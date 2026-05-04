// InspectAssembly.cpp
// Reads a STEP file via STEPCAFControl_Reader (XDE) and dumps the full product
// name hierarchy to assembly_tree.json.  Use this to understand what names and
// subassembly groupings already exist in the file before re-running ExtractBodies.

#include "praxis/Intent.hpp"
#include "nlohmann/json.hpp"

#include <STEPCAFControl_Reader.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TopoDS_Shape.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Solid.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace praxis {

namespace {

static std::string label_name(const TDF_Label& label) {
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        TCollection_ExtendedString ext = nameAttr->Get();
        // Convert ExtendedString to UTF-8
        std::string result;
        for (int i = 1; i <= ext.Length(); ++i) {
            Standard_ExtCharacter ch = ext.Value(i);
            if (ch < 128) result += static_cast<char>(ch);
            else result += '?';
        }
        // Trim whitespace
        size_t start = result.find_first_not_of(" \t\r\n");
        size_t end = result.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return result.substr(start, end - start + 1);
    }
    return "";
}

static std::string make_label_id(const TDF_Label& label) {
    std::string path;
    TDF_Label cur = label;
    while (!cur.IsNull() && !cur.IsRoot()) {
        path = std::to_string(cur.Tag()) + (path.empty() ? "" : "." + path);
        cur = cur.Father();
    }
    return path;
}

struct NodeInfo {
    std::string label_id;
    std::string name;
    bool is_assembly = false;
    bool is_simple_shape = false;
    bool is_reference = false;
    int solid_count = 0;
    int depth = 0;
    std::string parent_id;
    json children = json::array();
};

// Count solids in a shape
static int count_solids(const TopoDS_Shape& shape) {
    int n = 0;
    for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) ++n;
    return n;
}

static json walk(const Handle(XCAFDoc_ShapeTool)& stool,
                 const TDF_Label& label,
                 int depth,
                 const std::string& parent_id,
                 int& total_nodes)
{
    json node;
    node["label_id"] = make_label_id(label);
    node["name"] = label_name(label);
    node["depth"] = depth;
    node["parent_id"] = parent_id;

    bool is_asm = stool->IsAssembly(label);
    bool is_ref = stool->IsReference(label);
    bool is_simple = stool->IsSimpleShape(label);
    node["is_assembly"] = is_asm;
    node["is_reference"] = is_ref;
    node["is_simple_shape"] = is_simple;

    // Solid count for this shape (non-recursive for speed)
    TopoDS_Shape shape;
    if (stool->GetShape(label, shape) && !shape.IsNull()) {
        int sc = count_solids(shape);
        node["solid_count"] = sc;
    } else {
        node["solid_count"] = 0;
    }

    total_nodes++;

    // Recurse into components (only for assemblies and up to depth 8 to avoid explosion)
    if (is_asm && depth < 8) {
        TDF_LabelSequence components;
        stool->GetComponents(label, components, /*recursive=*/Standard_False);
        json children = json::array();
        for (int i = 1; i <= components.Length(); ++i) {
            const TDF_Label& comp = components.Value(i);
            // Dereference if it's a reference to see the referred shape
            TDF_Label referred;
            bool has_ref = stool->GetReferredShape(comp, referred);
            json child_node = walk(stool, has_ref ? referred : comp, depth + 1, node["label_id"].get<std::string>(), total_nodes);
            // Also attach the instance name from comp (different from referred shape name)
            std::string inst_name = label_name(comp);
            if (!inst_name.empty() && inst_name != child_node["name"].get<std::string>()) {
                child_node["instance_name"] = inst_name;
            }
            children.push_back(child_node);
        }
        node["children"] = children;
    } else {
        node["children"] = json::array();
    }

    return node;
}

} // namespace

IntentResult inspectAssembly(const IntentRequest& request) {
    IntentResult result;

    try {
        if (request.input_step_path.empty()) {
            result.success = false;
            result.errors.push_back("No input STEP file specified");
            return result;
        }
        {
            std::ifstream f(request.input_step_path);
            if (!f.good()) {
                result.success = false;
                result.errors.push_back("Input file not found: " + request.input_step_path);
                return result;
            }
        }

        std::cout << "InspectAssembly: reading " << request.input_step_path << "\n";

        // Create XCAF document
        Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
        Handle(TDocStd_Document) doc;
        app->NewDocument("MDTV-XCAF", doc);

        // Read STEP into XDE
        STEPCAFControl_Reader reader;
        reader.SetNameMode(Standard_True);
        reader.SetColorMode(Standard_False);
        reader.SetLayerMode(Standard_False);

        IFSelect_ReturnStatus stat = reader.ReadFile(request.input_step_path.c_str());
        if (stat != IFSelect_RetDone) {
            result.success = false;
            result.errors.push_back("STEPCAFControl_Reader::ReadFile failed");
            return result;
        }

        std::cout << "  File read OK, transferring to XDE document...\n";
        if (!reader.Transfer(doc)) {
            result.success = false;
            result.errors.push_back("STEPCAFControl_Reader::Transfer failed");
            return result;
        }
        std::cout << "  XDE transfer complete\n";

        Handle(XCAFDoc_ShapeTool) stool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());

        // Get free (top-level) shapes
        TDF_LabelSequence free_shapes;
        stool->GetFreeShapes(free_shapes);
        std::cout << "  Top-level shapes: " << free_shapes.Length() << "\n";

        int total_nodes = 0;
        json tree_roots = json::array();
        for (int i = 1; i <= free_shapes.Length(); ++i) {
            json root = walk(stool, free_shapes.Value(i), 0, "", total_nodes);
            tree_roots.push_back(root);
        }

        json output;
        output["source_step"] = fs::path(request.input_step_path).filename().string();
        output["top_level_count"] = free_shapes.Length();
        output["total_nodes_visited"] = total_nodes;
        output["tree"] = tree_roots;

        // Also produce a flat summary: unique names at each depth
        std::cout << "  Total nodes visited: " << total_nodes << "\n";

        std::string out_path = request.output_dir + "/assembly_tree.json";
        std::ofstream out(out_path);
        if (!out.good()) {
            result.success = false;
            result.errors.push_back("Cannot write output: " + out_path);
            return result;
        }
        out << output.dump(2) << "\n";
        out.close();

        result.artifacts.push_back(out_path);
        result.success = true;
        result.confidence = 1.0;
        std::cout << "✓ Assembly tree written: " << out_path << "\n";

    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back(std::string("InspectAssembly failed: ") + e.what());
    }

    return result;
}

} // namespace praxis
