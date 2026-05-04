#include "praxis/Intent.hpp"
#include "../kernel/KernelOps.hpp"
#include "../kernel/StepIO.hpp"
#include "../kernel/ShapeHandleInternal.hpp"
#include "nlohmann/json.hpp"
#include <TopExp_Explorer.hxx>
#include <TopoDS_Solid.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <STEPControl_Writer.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace praxis {

static std::string body_hash(double volume, double cx, double cy, double cz) {
    auto fmt = [](double v) -> std::string {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(4) << v;
        return ss.str();
    };
    std::string sig = "V:" + fmt(volume) + "|C:" + fmt(cx) + "," + fmt(cy) + "," + fmt(cz);
    unsigned long h = 5381;
    for (char c : sig) h = ((h << 5) + h) + static_cast<unsigned char>(c);
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(6) << (h & 0xFFFFFF);
    return ss.str();
}

IntentResult extractBodies(const IntentRequest& request) {
    IntentResult result;
    auto start = std::chrono::high_resolution_clock::now();

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
                result.errors.push_back("Input file does not exist: " + request.input_step_path);
                return result;
            }
        }

        std::cout << "Reading STEP file: " << request.input_step_path << "\n";
        auto read_result = kernel::StepIO::read_step(request.input_step_path);
        if (!read_result.success) {
            result.success = false;
            result.errors.push_back("Failed to read STEP: " + read_result.error_message);
            return result;
        }
        std::cout << "✓ STEP loaded\n";

        const TopoDS_Shape& full_shape = get_shape_internal(read_result.shape);

        // Count solids (fast pass — no geometry computed)
        int total_solids = 0;
        for (TopExp_Explorer exp(full_shape, TopAbs_SOLID); exp.More(); exp.Next()) {
            total_solids++;
        }
        std::cout << "Found " << total_solids << " solid(s)\n";
        result.metrics["body_count"] = std::to_string(total_solids);

        json manifest;
        manifest["machine_name"] = fs::path(request.input_step_path).stem().string();
        manifest["source_step"] = fs::path(request.input_step_path).filename().string();
        manifest["extraction_date"] = "2026-05-04";
        manifest["total_solids"] = total_solids;

        json bodies_array = json::array();
        int idx = 0;
        int extracted = 0;

        for (TopExp_Explorer exp(full_shape, TopAbs_SOLID); exp.More(); exp.Next(), ++idx) {
            const TopoDS_Shape& solid = exp.Current();

            // Geometric properties — volume, centroid, bbox only (no face/edge traversal)
            GProp_GProps gprops;
            BRepGProp::VolumeProperties(solid, gprops);
            double vol = gprops.Mass();
            gp_Pnt ctr = gprops.CentreOfMass();

            Bnd_Box bbox;
            BRepBndLib::Add(solid, bbox);
            double xmin, ymin, zmin, xmax, ymax, zmax;
            bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

            std::string hash = body_hash(vol, ctr.X(), ctr.Y(), ctr.Z());

            std::stringstream id_ss;
            id_ss << std::setfill('0') << std::setw(3) << idx;
            std::string body_id = "body_" + id_ss.str();
            std::string filename = body_id + "_" + hash + ".step";
            std::string filepath = request.output_dir + "/" + filename;

            std::cout << "  [" << idx + 1 << "/" << total_solids << "] " << filename;

            // Copy and heal
            BRepBuilderAPI_Copy copier(solid);
            kernel::ShapeHandle body_handle;
            set_shape_internal(body_handle, copier.Shape());
            auto healed = kernel::KernelOps::heal(body_handle);

            // Write via STEPControl_Writer directly (lighter than going through StepIO)
            const TopoDS_Shape& out_shape = get_shape_internal(healed.shape);
            STEPControl_Writer writer;
            writer.Transfer(out_shape, STEPControl_AsIs);
            IFSelect_ReturnStatus ws = writer.Write(filepath.c_str());

            if (ws != IFSelect_RetDone) {
                result.warnings.push_back("Failed to write " + filename);
                std::cout << " FAILED\n";
            } else {
                result.artifacts.push_back(filepath);
                extracted++;
                std::cout << " ok\n";
            }

            json entry;
            entry["id"] = body_id;
            entry["index"] = idx;
            entry["filename"] = filename;
            entry["volume"] = vol;
            entry["centroid"] = {{"x", ctr.X()}, {"y", ctr.Y()}, {"z", ctr.Z()}};
            entry["bbox"] = {
                {"min_x", xmin}, {"min_y", ymin}, {"min_z", zmin},
                {"max_x", xmax}, {"max_y", ymax}, {"max_z", zmax}
            };
            entry["write_status"] = (ws == IFSelect_RetDone) ? "ok" : "failed";
            bodies_array.push_back(entry);
        }

        manifest["bodies"] = bodies_array;
        manifest["extracted_count"] = extracted;

        std::string manifest_path = request.output_dir + "/extraction_manifest.json";
        std::ofstream mf(manifest_path);
        if (!mf.good()) {
            result.success = false;
            result.errors.push_back("Failed to write manifest");
            return result;
        }
        mf << manifest.dump(2) << "\n";
        mf.close();
        result.artifacts.push_back(manifest_path);

        std::cout << "✓ Extracted " << extracted << "/" << total_solids << " bodies\n";
        std::cout << "✓ Manifest: " << manifest_path << "\n";

        result.success = true;
        result.confidence = extracted > 0 ? 0.9 : 0.5;
        result.metrics["extracted_count"] = std::to_string(extracted);

        auto end = std::chrono::high_resolution_clock::now();
        result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back(std::string("ExtractBodies failed: ") + e.what());
    }

    return result;
}

}  // namespace praxis
