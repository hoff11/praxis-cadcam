// ExtractBodies.cpp  (v3 — XDE-based)
// Reads a STEP file via STEPCAFControl_Reader, preserving the full product name
// hierarchy and spatial transforms.  Outputs:
//   assembly.step              — full machine re-written via STEPCAFControl_Writer
//                                (preserves XDE product hierarchy + names)
//   {subasm}/assembly.step     — per-level-1-subassembly compound (located)
//   {subasm}/{part}.step       — individual healed solid
//   extraction_manifest.json
//   h2_metadata_template.json

#include "praxis/Intent.hpp"
#include "../kernel/KernelOps.hpp"
#include "../kernel/ShapeHandleInternal.hpp"
#include "nlohmann/json.hpp"

// XDE / XCAF
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TCollection_ExtendedString.hxx>

// Shape tools
#include <TopoDS_Solid.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <TopExp_Explorer.hxx>
#include <IFSelect_ReturnStatus.hxx>

// stdlib
#include <openssl/sha.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <map>
#include <regex>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace praxis {

namespace {

// Logical rename map: maps sanitized vendor subassembly name → human-readable label.
// Keys must exactly match the sanitized form (spaces→_, slashes→_, etc.).
// This table is specific to the Haas VF-4 STEP model (VF-4_STEP_2019-01).
// Extend or override via --param rename_map=<json_file> in a future version.
static const std::map<std::string, std::string> SUBASM_RENAME = {
    {"10000085453_37-3666",                                    "z_column_assembly"},
    {"10000086642_37-3682",                                    "y_axis_assembly"},
    {"10000087996_37-3703",                                    "spindle_head_assembly"},
    {"10000086728",                                            "z_slide_assembly"},
    {"10000078323_37-3641_SMTC_30_PCKT_30T_BT_20_M_VF_2018",  "tool_changer_assembly"},
    {"10000005210_MED_VMC",                                    "base_casting_assembly"},
    {"10000086576_37-3652",                                    "coolant_chip_assembly"},
    {"10000075385",                                            "operator_pendant_assembly"},
    {"10000095056_ASSEMBLED",                                  "work_enclosure_assembly"},
    {"MA-0004_37-0699",                                        "sheet_metal_base_assembly"},
    {"30-4024A",                                               "work_table_assembly"},
    {"25-1696ind",                                             "chip_conveyor_assembly"},
    {"Electrical_Cabinet-1",                                   "electrical_cabinet"},
    {"pneumatic_cabinet",                                      "pneumatic_cabinet"},
    {"10000029424fixed",                                       "fixed_guard"},
    {"25-1692-1",                                              "guard_panel_1"},
    {"25-1694",                                                "guard_panel_2"},
    {"10000075549",                                            "misc_hardware_1"},
    {"10000062081",                                            "misc_hardware_2"},
    {"10000067570",                                            "misc_hardware_3"},
};

static std::string apply_rename(const std::string& sanitized) {
    auto it = SUBASM_RENAME.find(sanitized);
    return (it != SUBASM_RENAME.end()) ? it->second : sanitized;
}

static std::string body_hash(double volume, double cx, double cy, double cz) {
    auto fmt = [](double v) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4) << v;
        return ss.str();
    };
    std::string sig = "V:" + fmt(volume) + "|C:" + fmt(cx) + "," + fmt(cy) + "," + fmt(cz);
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(sig.data()), sig.size(), digest);
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; ++i) ss << std::setw(2) << static_cast<int>(digest[i]);
    return ss.str();
}

static std::string sanitize_name(const std::string& raw) {
    if (raw.empty()) return "unnamed";
    std::string s = raw;
    for (char& c : s) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<'  || c == '>' || c == '|' || c == '\'' ||
            c == ' ' || c == '\t')
            c = '_';
    }
    std::regex multi_us("_+");
    s = std::regex_replace(s, multi_us, "_");
    size_t start = s.find_first_not_of('_');
    size_t end   = s.find_last_not_of('_');
    return (start == std::string::npos) ? "unnamed" : s.substr(start, end - start + 1);
}

static std::string label_name(const TDF_Label& label) {
    Handle(TDataStd_Name) attr;
    if (!label.FindAttribute(TDataStd_Name::GetID(), attr)) return "";
    TCollection_ExtendedString ext = attr->Get();
    std::string result;
    for (int i = 1; i <= ext.Length(); ++i) {
        Standard_ExtCharacter ch = ext.Value(i);
        result += (ch < 128) ? static_cast<char>(ch) : '?';
    }
    size_t s = result.find_first_not_of(" \t\r\n");
    size_t e = result.find_last_not_of(" \t\r\n");
    return (s == std::string::npos) ? "" : result.substr(s, e - s + 1);
}

struct StreamMute {
    std::streambuf* old_out = nullptr;
    std::streambuf* old_err = nullptr;
    std::ostringstream sink;
    bool enabled = false;
    explicit StreamMute(bool mute) : enabled(mute) {
        if (enabled) { old_out = std::cout.rdbuf(sink.rdbuf()); old_err = std::cerr.rdbuf(sink.rdbuf()); }
    }
    ~StreamMute() {
        if (enabled) { std::cout.rdbuf(old_out); std::cerr.rdbuf(old_err); }
    }
};

struct SolidEntry {
    TopoDS_Shape shape;           // original located solid (world frame)
    std::string  subasm_name;     // sanitized level-1 subassembly name
    std::string  part_name;       // sanitized leaf part name (may be disambiguated)
    int          index_global = 0;
};

// Recursively collect all SOLID shapes under 'label'.
// depth==0  → root assembly; depth==1 → first real subassembly (fixes subasm_name)
static void collect_solids(
    const Handle(XCAFDoc_ShapeTool)& stool,
    const TDF_Label& label,
    const std::string& subasm_name,
    const std::string& current_name,
    std::vector<SolidEntry>& out,
    int depth)
{
    TopoDS_Shape shape;
    if (!stool->GetShape(label, shape) || shape.IsNull()) return;

    if (stool->IsAssembly(label)) {
        TDF_LabelSequence comps;
        stool->GetComponents(label, comps, Standard_False);
        for (int i = 1; i <= comps.Length(); ++i) {
            const TDF_Label& comp = comps.Value(i);
            TDF_Label referred;
            const TDF_Label& target = stool->GetReferredShape(comp, referred) ? referred : comp;
            std::string inst = label_name(comp);
            std::string ref  = label_name(target);
            std::string child_name = !ref.empty() ? ref : (!inst.empty() ? inst : current_name);
            std::string child_subasm = (depth == 0)
                ? apply_rename(sanitize_name(child_name))
                : subasm_name;
            collect_solids(stool, target, child_subasm, child_name, out, depth + 1);
        }
    } else {
        // Simple shape: enumerate solids within it
        for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
            SolidEntry e;
            e.shape        = exp.Current();
            e.subasm_name  = subasm_name.empty() ? "root" : subasm_name;
            e.part_name    = sanitize_name(current_name);
            e.index_global = static_cast<int>(out.size());
            out.push_back(std::move(e));
        }
    }
}

} // namespace

IntentResult extractBodies(const IntentRequest& request) {
    IntentResult result;
    auto start_t = std::chrono::high_resolution_clock::now();

    try {
        if (request.input_step_path.empty()) {
            result.success = false; result.errors.push_back("No input STEP file"); return result;
        }
        {
            std::ifstream f(request.input_step_path);
            if (!f.good()) {
                result.success = false;
                result.errors.push_back("Input not found: " + request.input_step_path);
                return result;
            }
        }
        const bool quiet = !(request.params.count("verbose_step_write") &&
                             request.params.at("verbose_step_write") == "true");

        // ---- 1. Read with XDE -----------------------------------------------
        std::cout << "Reading STEP (XDE): " << request.input_step_path << "\n";
        Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
        Handle(TDocStd_Document) doc;
        app->NewDocument("MDTV-XCAF", doc);

        STEPCAFControl_Reader reader;
        reader.SetNameMode(Standard_True);
        reader.SetColorMode(Standard_False);
        reader.SetLayerMode(Standard_False);

        IFSelect_ReturnStatus rs;
        { StreamMute m(quiet); rs = reader.ReadFile(request.input_step_path.c_str()); }
        if (rs != IFSelect_RetDone) {
            result.success = false; result.errors.push_back("ReadFile failed"); return result;
        }
        { StreamMute m(quiet); if (!reader.Transfer(doc)) {
            result.success = false; result.errors.push_back("Transfer failed"); return result;
        }}
        std::cout << "✓ XDE read complete\n";

        Handle(XCAFDoc_ShapeTool) stool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());

        // ---- 2. Walk tree and collect solid entries -------------------------
        std::vector<SolidEntry> solids;
        TDF_LabelSequence free_shapes;
        stool->GetFreeShapes(free_shapes);
        for (int i = 1; i <= free_shapes.Length(); ++i) {
            const TDF_Label& lbl = free_shapes.Value(i);
            collect_solids(stool, lbl, "", label_name(lbl), solids, 0);
        }

        // Disambiguate duplicate part names within the same subassembly
        {
            std::map<std::string, int> name_count;
            for (auto& e : solids) {
                std::string key = e.subasm_name + "/" + e.part_name;
                int& cnt = name_count[key];
                if (cnt > 0) e.part_name = e.part_name + "_" + std::to_string(cnt);
                cnt++;
            }
        }

        std::cout << "Found " << solids.size() << " solid(s)\n";
        result.metrics["body_count"] = std::to_string(solids.size());

        // ---- 3. Write full assembly STEP via STEPCAFControl_Writer ----------
        //         This preserves product hierarchy and names from the source
        {
            std::string asm_path = request.output_dir + "/assembly.step";
            std::cout << "Writing assembly.step (XDE hierarchy preserved)...\n";
            STEPCAFControl_Writer asm_writer;
            IFSelect_ReturnStatus aws = IFSelect_RetDone;
            {
                StreamMute m(quiet);
                if (!asm_writer.Transfer(doc, STEPControl_AsIs))
                    result.warnings.push_back("STEPCAFControl_Writer::Transfer returned false");
                aws = asm_writer.Write(asm_path.c_str());
            }
            if (aws == IFSelect_RetDone) {
                result.artifacts.push_back(asm_path);
                std::cout << "✓ assembly.step written (with product names)\n";
            } else {
                result.warnings.push_back("Failed to write assembly.step");
            }
        }

        // ---- 4. Per-subassembly and per-body STEP files ---------------------
        std::map<std::string, std::vector<const SolidEntry*>> subasm_groups;
        for (const auto& e : solids) subasm_groups[e.subasm_name].push_back(&e);

        json manifest;
        manifest["machine_name"]    = fs::path(request.input_step_path).stem().string();
        manifest["source_step"]     = fs::path(request.input_step_path).filename().string();
        manifest["intent"]          = "ExtractBodies";
        manifest["format_version"]  = "2.0";
        manifest["total_solids"]    = static_cast<int>(solids.size());
        manifest["assembly_step"]   = "assembly.step";

        json bodies_array = json::array();
        std::vector<json> body_rows;
        int extracted = 0;

        for (const auto& [subasm, entries] : subasm_groups) {
            std::string subasm_dir = request.output_dir + "/" + subasm;
            fs::create_directories(subasm_dir);

            // Per-subassembly compound (located → positions preserved)
            BRep_Builder sb;
            TopoDS_Compound subasm_cmp;
            sb.MakeCompound(subasm_cmp);

            for (const SolidEntry* ep : entries) {
                const SolidEntry& e = *ep;
                sb.Add(subasm_cmp, e.shape);

                GProp_GProps gprops;
                BRepGProp::VolumeProperties(e.shape, gprops);
                double vol = gprops.Mass();
                gp_Pnt ctr = gprops.CentreOfMass();

                Bnd_Box bbox;
                BRepBndLib::Add(e.shape, bbox);
                double xmin, ymin, zmin, xmax, ymax, zmax;
                bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

                std::string hash     = body_hash(vol, ctr.X(), ctr.Y(), ctr.Z());
                std::string filename = e.part_name + "_" + hash + ".step";
                std::string filepath = subasm_dir + "/" + filename;
                std::string rel_path = subasm + "/" + filename;

                std::cout << "  [" << (e.index_global + 1) << "/" << solids.size()
                          << "] " << rel_path;

                BRepBuilderAPI_Copy copier(e.shape);
                kernel::ShapeHandle body_handle;
                set_shape_internal(body_handle, copier.Shape());
                auto healed = kernel::KernelOps::heal(body_handle);

                STEPControl_Writer writer;
                IFSelect_ReturnStatus ws = IFSelect_RetError;
                {
                    StreamMute m(quiet);
                    writer.Transfer(get_shape_internal(healed.shape), STEPControl_AsIs);
                    ws = writer.Write(filepath.c_str());
                }

                if (ws == IFSelect_RetDone) { result.artifacts.push_back(filepath); extracted++; std::cout << " ok\n"; }
                else { result.warnings.push_back("Failed: " + rel_path); std::cout << " FAILED\n"; }

                json entry;
                entry["id"]          = e.part_name;
                entry["index"]       = e.index_global;
                entry["subassembly"] = subasm;
                entry["filename"]    = rel_path;
                entry["hash"]        = hash;
                entry["volume"]      = vol;
                entry["centroid"]    = {{"x", ctr.X()}, {"y", ctr.Y()}, {"z", ctr.Z()}};
                entry["bbox"]        = {{"min_x",xmin},{"min_y",ymin},{"min_z",zmin},
                                        {"max_x",xmax},{"max_y",ymax},{"max_z",zmax}};
                entry["dims_mm"]     = {{"x",xmax-xmin},{"y",ymax-ymin},{"z",zmax-zmin}};
                entry["write_status"] = (ws == IFSelect_RetDone) ? "ok" : "failed";
                bodies_array.push_back(entry);
                body_rows.push_back(entry);
            }

            // Per-subassembly positioned assembly
            {
                std::string sa_path = subasm_dir + "/assembly.step";
                STEPControl_Writer saw;
                IFSelect_ReturnStatus saws = IFSelect_RetError;
                { StreamMute m(quiet); saw.Transfer(subasm_cmp, STEPControl_AsIs); saws = saw.Write(sa_path.c_str()); }
                if (saws == IFSelect_RetDone) {
                    result.artifacts.push_back(sa_path);
                    std::cout << "  ✓ " << subasm << "/assembly.step (" << entries.size() << " solids)\n";
                }
            }
        }

        manifest["bodies"]         = bodies_array;
        manifest["extracted_count"] = extracted;
        manifest["subassemblies"]  = json::array();
        for (const auto& [k, v] : subasm_groups)
            manifest["subassemblies"].push_back({{"name", k}, {"solid_count", static_cast<int>(v.size())}});

        std::string manifest_path = request.output_dir + "/extraction_manifest.json";
        { std::ofstream mf(manifest_path); if (!mf.good()) {
            result.success = false; result.errors.push_back("Cannot write manifest"); return result;
        } mf << manifest.dump(2) << "\n"; }
        result.artifacts.push_back(manifest_path);

        // ---- 5. H2 seed template -------------------------------------------
        std::sort(body_rows.begin(), body_rows.end(), [](const json& a, const json& b) {
            return a.at("volume").get<double>() > b.at("volume").get<double>();
        });
        json h2;
        h2["machine_name"]     = manifest["machine_name"];
        h2["source_step"]      = manifest["source_step"];
        h2["template_version"] = "2.0";
        h2["axes"] = json::array({
            {{"name","X"},{"direction",{nullptr,nullptr,nullptr}},{"travel_min_mm",nullptr},{"travel_max_mm",nullptr},{"body_ids",json::array()}},
            {{"name","Y"},{"direction",{nullptr,nullptr,nullptr}},{"travel_min_mm",nullptr},{"travel_max_mm",nullptr},{"body_ids",json::array()}},
            {{"name","Z"},{"direction",{nullptr,nullptr,nullptr}},{"travel_min_mm",nullptr},{"travel_max_mm",nullptr},{"body_ids",json::array()}}
        });
        json priorities = json::array();
        for (size_t i = 0; i < std::min<size_t>(40, body_rows.size()); ++i) {
            const auto& b = body_rows[i];
            priorities.push_back({
                {"priority_rank", static_cast<int>(i+1)},
                {"id",          b.at("id")},
                {"subassembly", b.at("subassembly")},
                {"filename",    b.at("filename")},
                {"volume",      b.at("volume")},
                {"dims_mm",     b.at("dims_mm")},
                {"semantic_role", nullptr},
                {"notes", ""}
            });
        }
        h2["priority_bodies"] = priorities;
        std::string h2_path = request.output_dir + "/h2_metadata_template.json";
        { std::ofstream h2f(h2_path); if (h2f.good()) { h2f << h2.dump(2) << "\n"; result.artifacts.push_back(h2_path); } }

        std::cout << "✓ Extracted " << extracted << "/" << solids.size() << " bodies\n";
        result.success    = true;
        result.confidence = extracted > 0 ? 0.9 : 0.5;
        result.metrics["extracted_count"] = std::to_string(extracted);
        auto end_t = std::chrono::high_resolution_clock::now();
        result.duration_ms = std::chrono::duration<double, std::milli>(end_t - start_t).count();

    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back(std::string("ExtractBodies failed: ") + e.what());
    }

    return result;
}

}  // namespace praxis
