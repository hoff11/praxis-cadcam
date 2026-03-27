/**
 * AttachFaceToFace.cpp
 * Phase D.1 (v0.6.0) - First geometry-mutating assembly operation
 * 
 * Accepts resolved FaceRef inputs from selector resolution (Phase C).
 * Computes rigid transform to mate two faces and applies to moving body.
 */

#include "praxis/Intent.hpp"
#include "../kernel/KernelOps.hpp"
#include "../kernel/StepIO.hpp"
#include "../kernel/StlIO.hpp"
#include "../kernel/ShapeHandleInternal.hpp"
#include "OCCTInspector.hpp"
#include "Reference.hpp"
#include "InteractionEmit.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <cmath>

// OCCT headers for transform computation
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Precision.hxx>
#include <STEPControl_Writer.hxx>
#include <IFSelect_ReturnStatus.hxx>

namespace praxis {

using namespace inspection;
using namespace reference;

namespace {

// Extract face normal (for planar faces only)
std::optional<gp_Dir> get_face_normal(const TopoDS_Face& face) {
    try {
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane) {
            return std::nullopt;  // v0 only supports planar faces
        }
        
        // Get normal at face center
        double umin, umax, vmin, vmax;
        BRepTools::UVBounds(face, umin, umax, vmin, vmax);
        double u = (umin + umax) / 2.0;
        double v = (vmin + vmax) / 2.0;
        
        gp_Pnt pt;
        gp_Vec du, dv;
        surface.D1(u, v, pt, du, dv);
        
        gp_Dir normal = gp_Dir(du.Crossed(dv));
        
        // Respect face orientation
        if (face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
        }
        
        return normal;
    } catch (...) {
        return std::nullopt;
    }
}

// Compute face centroid
gp_Pnt get_face_centroid(const TopoDS_Face& face) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.CentreOfMass();
}

// Compute rigid transform to mate two planar faces (flush mode)
std::optional<gp_Trsf> compute_flush_transform(
    const TopoDS_Face& moving_face,
    const TopoDS_Face& fixed_face,
    double offset_mm
) {
    // Get normals
    auto moving_normal_opt = get_face_normal(moving_face);
    auto fixed_normal_opt = get_face_normal(fixed_face);
    
    if (!moving_normal_opt || !fixed_normal_opt) {
        return std::nullopt;  // Non-planar faces not supported in v0
    }
    
    gp_Dir moving_normal = *moving_normal_opt;
    gp_Dir fixed_normal = *fixed_normal_opt;
    
    // Get centroids
    gp_Pnt moving_center = get_face_centroid(moving_face);
    gp_Pnt fixed_center = get_face_centroid(fixed_face);
    
    // Compute transform
    gp_Trsf transform;
    
    // Step 1: Translate moving_center to origin
    gp_Trsf t1;
    t1.SetTranslation(gp_Vec(moving_center, gp_Pnt(0, 0, 0)));
    
    // Step 2: Rotate moving_normal to align with -fixed_normal (opposing for flush mate)
    gp_Dir target_normal = fixed_normal.Reversed();
    gp_Trsf t2;
    
    if (!moving_normal.IsParallel(target_normal, Precision::Angular())) {
        // Compute rotation axis and angle
        gp_Vec rotation_axis = gp_Vec(moving_normal).Crossed(gp_Vec(target_normal));
        double angle = moving_normal.Angle(target_normal);
        
        if (rotation_axis.Magnitude() > Precision::Confusion()) {
            t2.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(rotation_axis)), angle);
        }
    } else if (moving_normal.IsEqual(fixed_normal, Precision::Angular())) {
        // Special case: normals are parallel and same direction - need 180° rotation
        // Pick any perpendicular axis for rotation (use Y if X-normal, X if Y-normal, etc.)
        gp_Dir rotation_axis;
        if (std::abs(moving_normal.X()) > 0.5) {
            rotation_axis = gp_Dir(0, 1, 0);  // Rotate around Y
        } else if (std::abs(moving_normal.Y()) > 0.5) {
            rotation_axis = gp_Dir(1, 0, 0);  // Rotate around X
        } else {
            rotation_axis = gp_Dir(1, 0, 0);  // Rotate around X
        }
        t2.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), rotation_axis), M_PI);
    }
    // If normals are already opposed (anti-parallel), no rotation needed
    
    // Step 3: Translate to fixed face centroid with offset along normal
    // For flush mate: moving face centroid -> fixed face centroid (with offset)
    gp_Vec offset_vec = gp_Vec(fixed_normal) * offset_mm;
    gp_Pnt target_pos = fixed_center.Translated(offset_vec);
    gp_Trsf t3;
    t3.SetTranslation(gp_Vec(gp_Pnt(0, 0, 0), target_pos));
    
    // Combine transforms: t3 * t2 * t1
    // This moves moving_center to origin, rotates normal, then moves to target
    transform = t3 * t2 * t1;
    
    return transform;
}

// Extract a specific body from compound shape by index
std::optional<TopoDS_Shape> extract_body(const TopoDS_Shape& compound, int body_index) {
    if (compound.ShapeType() != TopAbs_COMPOUND) {
        if (body_index == 0) return compound;
        return std::nullopt;
    }
    
    int current_index = 0;
    for (TopExp_Explorer exp(compound, TopAbs_SOLID); exp.More(); exp.Next()) {
        if (current_index == body_index) {
            return exp.Current();
        }
        current_index++;
    }
    
    return std::nullopt;
}

// Extract a specific face from body by index (using local TopExp order)
// This now works correctly because OCCTInspector::enumerate_faces preserves local indices
std::optional<TopoDS_Face> extract_face(const TopoDS_Shape& body, int face_index) {
    int current_index = 0;
    for (TopExp_Explorer exp(body, TopAbs_FACE); exp.More(); exp.Next(), ++current_index) {
        if (current_index == face_index) {
            return TopoDS::Face(exp.Current());
        }
    }
    return std::nullopt;
}

// Rebuild compound with transformed body
TopoDS_Shape rebuild_compound_with_transformed_body(
    const TopoDS_Shape& original_compound,
    int moving_body_index,
    const TopoDS_Shape& transformed_body
) {
    TopoDS_Compound result;
    TopoDS_Builder builder;
    builder.MakeCompound(result);
    
    int current_index = 0;
    for (TopExp_Explorer exp(original_compound, TopAbs_SOLID); exp.More(); exp.Next()) {
        if (current_index == moving_body_index) {
            builder.Add(result, transformed_body);
        } else {
            builder.Add(result, exp.Current());
        }
        current_index++;
    }
    
    return result;
}

} // anonymous namespace

// AttachFaceToFace intent implementation
IntentResult attachFaceToFaceIntent(const IntentRequest& request) {
    IntentResult result;
    result.summary.intent = "AttachFaceToFace";
    
    // Parse required arguments
    auto moving_face_json_it = request.params.find("moving_face");
    auto fixed_face_json_it = request.params.find("fixed_face");
    auto mode_it = request.params.find("mode");
    
    if (moving_face_json_it == request.params.end()) {
        result.success = false;
        result.errors.push_back("Missing required parameter: moving_face");
        return result;
    }
    
    if (fixed_face_json_it == request.params.end()) {
        result.success = false;
        result.errors.push_back("Missing required parameter: fixed_face");
        return result;
    }
    
    std::string mode = (mode_it != request.params.end()) ? mode_it->second : "flush";
    if (mode != "flush") {
        result.success = false;
        result.errors.push_back("Only 'flush' mode is supported in v0.6.0");
        return result;
    }
    
    // Parse offset (optional, default 0)
    double offset_mm = 0.0;
    auto offset_it = request.params.find("offset");
    if (offset_it != request.params.end()) {
        try {
            offset_mm = std::stod(offset_it->second);
        } catch (...) {
            result.warnings.push_back("Invalid offset value, using 0.0");
        }
    }
    
    result.resolved_params["offset_mm"] = offset_mm;
    
    // Parse FaceRef JSON
    auto moving_face_ref_opt = FaceRef::from_json(moving_face_json_it->second);
    auto fixed_face_ref_opt = FaceRef::from_json(fixed_face_json_it->second);
    
    if (!moving_face_ref_opt) {
        result.success = false;
        result.errors.push_back("Failed to parse moving_face FaceRef JSON");
        return result;
    }
    
    if (!fixed_face_ref_opt) {
        result.success = false;
        result.errors.push_back("Failed to parse fixed_face FaceRef JSON");
        return result;
    }
    
    FaceRef moving_face_ref = *moving_face_ref_opt;
    FaceRef fixed_face_ref = *fixed_face_ref_opt;
    
    // Validate: faces must belong to different bodies
    if (moving_face_ref.parent.index == fixed_face_ref.parent.index) {
        result.success = false;
        result.errors.push_back("Cannot attach faces from the same body (self-attachment not supported)");
        return result;
    }
    
    // Validate: only planar faces supported in v0
    if (moving_face_ref.signature.surface_type != SurfaceType::PLANAR ||
        fixed_face_ref.signature.surface_type != SurfaceType::PLANAR) {
        result.success = false;
        result.errors.push_back("Only planar faces are supported in v0.6.0");
        return result;
    }
    
    // Load input artifact (must be provided as input_step_path)
    if (request.input_step_path.empty()) {
        result.success = false;
        result.errors.push_back("AttachFaceToFace requires input_step_path (artifact to modify)");
        return result;
    }
    
    // Load artifact for inspection and modification
    auto inspector = inspection::create_inspector();
    if (!inspector->load_artifact(request.input_step_path)) {
        result.success = false;
        result.errors.push_back("Failed to load input artifact: " + request.input_step_path);
        return result;
    }
    
    // Load the OCCT shape for transformation
    auto step_res = kernel::StepIO::read_step(request.input_step_path);
    if (!step_res.success) {
        result.success = false;
        result.errors.push_back("Failed to read STEP file: " + step_res.error_message);
        return result;
    }
    
    // Get internal TopoDS_Shape for manipulation
    const TopoDS_Shape& loaded_shape = get_shape_internal(step_res.shape);
    
    // Resolve references using ReferenceResolver (D.1 contract requirement)
    ReferenceResolver resolver(inspector.get());
    
    auto moving_res = resolver.resolve(moving_face_ref);
    if (moving_res.status != ResolutionStatus::Success) {
        result.success = false;
        result.errors.push_back("Failed to resolve moving_face: " + 
                                resolution_status_to_string(moving_res.status) + 
                                " - " + moving_res.message);
        return result;
    }
    
    auto fixed_res = resolver.resolve(fixed_face_ref);
    if (fixed_res.status != ResolutionStatus::Success) {
        result.success = false;
        result.errors.push_back("Failed to resolve fixed_face: " + 
                                resolution_status_to_string(fixed_res.status) + 
                                " - " + fixed_res.message);
        return result;
    }
    
    // Extract resolved references
    FaceRef resolved_moving = std::get<FaceRef>(*moving_res.resolved);
    FaceRef resolved_fixed = std::get<FaceRef>(*fixed_res.resolved);
    
    // Extract bodies using resolved indices
    auto moving_body_opt = extract_body(loaded_shape, resolved_moving.parent.index);
    auto fixed_body_opt = extract_body(loaded_shape, resolved_fixed.parent.index);
    
    if (!moving_body_opt) {
        result.success = false;
        result.errors.push_back("Moving body not found at resolved index " + std::to_string(resolved_moving.parent.index));
        return result;
    }
    
    if (!fixed_body_opt) {
        result.success = false;
        result.errors.push_back("Fixed body not found at resolved index " + std::to_string(resolved_fixed.parent.index));
        return result;
    }
    
    // Extract faces using resolved indices
    auto moving_face_opt = extract_face(*moving_body_opt, resolved_moving.index);
    auto fixed_face_opt = extract_face(*fixed_body_opt, resolved_fixed.index);
    
    if (!moving_face_opt) {
        result.success = false;
        result.errors.push_back("Moving face not found at resolved index " + std::to_string(resolved_moving.index) +
                                " in body " + std::to_string(resolved_moving.parent.index));
        return result;
    }
    
    if (!fixed_face_opt) {
        result.success = false;
        result.errors.push_back("Fixed face not found at resolved index " + std::to_string(resolved_fixed.index) +
                                " in body " + std::to_string(resolved_fixed.parent.index));
        return result;
    }
    
    // Compute attachment transform
    auto transform_opt = compute_flush_transform(*moving_face_opt, *fixed_face_opt, offset_mm);
    if (!transform_opt) {
        result.success = false;
        result.errors.push_back("Failed to compute attachment transform (non-planar faces?)");
        return result;
    }
    
    // Apply transform to moving body
    BRepBuilderAPI_Transform transformer(*moving_body_opt, *transform_opt, Standard_True);
    if (!transformer.IsDone()) {
        result.success = false;
        result.errors.push_back("Failed to apply transformation to moving body");
        return result;
    }
    
    TopoDS_Shape transformed_body = transformer.Shape();
    
    // Rebuild compound with transformed body
    TopoDS_Shape final_shape = rebuild_compound_with_transformed_body(
        loaded_shape,
        resolved_moving.parent.index,
        transformed_body
    );
    
    // Validate result
    bool is_valid = kernel::KernelOps::validate(kernel::ShapeHandle());  // Will use actual validation below
    
    // Write output artifacts (STEP write validates internally)
    const std::filesystem::path out_root = std::filesystem::path(request.output_dir);
    std::filesystem::create_directories(out_root);
    const auto step_path = (out_root / "attached.step").string();
    const auto stl_path  = (out_root / "attached.stl").string();
    
    // For writing, we need to create a temporary STEP file and reload it
    // This is necessary because ShapeHandle is opaque and we can't directly construct it
    const auto temp_step_path = (out_root / "temp_attached.step").string();
    
    // Write using OCCT directly
    try {
        STEPControl_Writer writer;
        IFSelect_ReturnStatus transfer_status = writer.Transfer(final_shape, STEPControl_AsIs);
        if (transfer_status != IFSelect_RetDone) {
            result.success = false;
            result.errors.push_back("Failed to transfer attached shape to STEP writer");
            return result;
        }
        
        IFSelect_ReturnStatus write_status = writer.Write(temp_step_path.c_str());
        if (write_status != IFSelect_RetDone) {
            result.success = false;
            result.errors.push_back("Failed to write temporary STEP file");
            return result;
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back(std::string("STEP write error: ") + e.what());
        return result;
    }
    
    // Reload as ShapeHandle for validation and STL export
    auto reload_res = kernel::StepIO::read_step(temp_step_path);
    if (!reload_res.success) {
        result.success = false;
        result.errors.push_back("Failed to reload attached shape: " + reload_res.error_message);
        return result;
    }
    
    is_valid = kernel::KernelOps::validate(reload_res.shape);
    result.metrics["shape_valid"] = is_valid ? "true" : "false";
    
    if (!is_valid) {
        result.warnings.push_back("Attached shape reported invalid by kernel analyzer");
    }
    
    // Write final STEP file
    auto step_write = kernel::StepIO::write_step(reload_res.shape, step_path);
    if (!step_write.success) {
        result.success = false;
        result.errors.push_back(step_write.error_message);
        return result;
    }
    
    // Write STL
    auto stl_write = kernel::StlIO::write_stl(reload_res.shape, stl_path, 0.5);
    if (!stl_write.success) {
        result.warnings.push_back(stl_write.error_message);
    }
    
    // Remove temp file
    std::filesystem::remove(temp_step_path);
    
    result.artifacts.push_back(step_path);
    if (stl_write.success) {
        result.artifacts.push_back(stl_path);
    }
    
    // Populate summary
    result.summary.artifact_count = static_cast<int>(result.artifacts.size());
    result.summary.produced = {"Assembly", "Product"};
    
    // Count bodies/faces in result
    int body_count = 0;
    int face_count = 0;
    for (TopExp_Explorer exp(final_shape, TopAbs_SOLID); exp.More(); exp.Next()) {
        body_count++;
    }
    for (TopExp_Explorer exp(final_shape, TopAbs_FACE); exp.More(); exp.Next()) {
        face_count++;
    }
    
    result.summary.body_count = body_count;
    result.summary.face_count = face_count;
    
    // Previewable outputs
    PreviewableOutput step_out;
    step_out.filename = "attached.step";
    step_out.type = "step";
    step_out.semantic_type = "Assembly";
    step_out.previewable = true;
    result.summary.previewable_outputs.push_back(step_out);
    
    if (stl_write.success) {
        PreviewableOutput stl_out;
        stl_out.filename = "attached.stl";
        stl_out.type = "stl";
        stl_out.semantic_type = "Assembly";
        stl_out.previewable = true;
        result.summary.previewable_outputs.push_back(stl_out);
    }
    
    // Populate selectables from result artifact
    populate_selectables_from_artifact(result, step_path);
    
    result.success = true;
    result.confidence = 0.95;
    return result;
}

} // namespace praxis
