/**
 * Reference.cpp
 * Minimal ReferenceEncoder and ReferenceResolver implementation for Epic 2
 * Sprint 9: Migrated to canonical float formatting for deterministic serialization
 */

#include "../include/Reference.hpp"
#include "praxis/CanonicalFormat.hpp"
#include <sstream>
#include <cmath>
#include <iostream>
#include <unordered_map>

namespace praxis {
namespace reference {

// ========================= Utilities =========================

std::string resolution_status_to_string(ResolutionStatus status) {
    switch (status) {
        case ResolutionStatus::Success: return "Success";
        case ResolutionStatus::Missing: return "Missing";
        case ResolutionStatus::Drifted: return "Drifted";
        case ResolutionStatus::Ambiguous: return "Ambiguous";
        case ResolutionStatus::ContractMismatch: return "ContractMismatch";
        default: return "Unknown";
    }
}

// ========================= Encoder =========================

ReferenceEncoder::ReferenceEncoder(inspection::Inspector* inspector)
    : inspector_(inspector) {}

BodyRef ReferenceEncoder::encode_body(const inspection::BodyProperties& body) {
    BodyRef ref;
    ref.index = body.index;
    ref.signature.volume = body.volume;
    ref.signature.centroid = body.centroid;
    ref.signature.bbox = body.bbox;
    return ref;
}

FaceRef ReferenceEncoder::encode_face(const inspection::FaceProperties& face) {
    FaceRef ref;
    // Parent body
    auto bodies = inspector_->enumerate_bodies();
    if (face.body_index >= 0 && face.body_index < (int)bodies.size()) {
        ref.parent = encode_body(bodies[(size_t)face.body_index]);
    }
    ref.index = face.index;
    ref.signature.surface_type = face.surface_type;
    ref.signature.area = face.area;
    ref.signature.centroid = face.centroid;
    ref.signature.bbox = face.bbox;
    // Only include normal if present/non-zero
    if (face.normal.x != 0.0 || face.normal.y != 0.0 || face.normal.z != 0.0) {
        ref.signature.normal = face.normal;
    }
    return ref;
}

EdgeRef ReferenceEncoder::encode_edge(const inspection::EdgeProperties& edge) {
    EdgeRef ref;
    ref.index = edge.index;
    ref.signature.edge_type = edge.edge_type;
    ref.signature.length = edge.length > 0 ? std::optional<double>(edge.length) : std::nullopt;
    ref.signature.radius = edge.radius > 0 ? std::optional<double>(edge.radius) : std::nullopt;
    ref.signature.midpoint = edge.midpoint;
    ref.signature.bbox = edge.bbox;
    // Populate parent faces (if inspector can enumerate)
    if (inspector_) {
        auto all_faces = inspector_->enumerate_all_faces();
        // Build map by (body_index, local_face_index) since face indices are now per-body
        std::map<std::pair<int,int>, inspection::FaceProperties> face_by_location;
        for (const auto& f : all_faces) {
            face_by_location.emplace(std::make_pair(f.body_index, f.index), f);
        }
        ReferenceEncoder faceEncoder(inspector_);
        for (const auto& floc : edge.adjacent_faces) {
            auto key = std::make_pair(floc.body_index, floc.local_face_index);
            auto it = face_by_location.find(key);
            if (it != face_by_location.end()) {
                ref.parent_faces.push_back(faceEncoder.encode_face(it->second));
            }
        }
    }
    return ref;
}

// ========================= Resolver =========================

ReferenceResolver::ReferenceResolver(inspection::Inspector* inspector)
    : inspector_(inspector) {}

void ReferenceResolver::set_tolerance(double tolerance) { tolerance_ = tolerance; }

bool ReferenceResolver::matches_within_tolerance(double a, double b) const {
    return std::abs(a - b) <= tolerance_;
}

bool ReferenceResolver::vector_matches(const inspection::Vector3& a, const inspection::Vector3& b) const {
    return matches_within_tolerance(a.x, b.x)
        && matches_within_tolerance(a.y, b.y)
        && matches_within_tolerance(a.z, b.z);
}

bool ReferenceResolver::bbox_matches(const inspection::BoundingBox& a, const inspection::BoundingBox& b) const {
    return matches_within_tolerance(a.min_x, b.min_x)
        && matches_within_tolerance(a.min_y, b.min_y)
        && matches_within_tolerance(a.min_z, b.min_z)
        && matches_within_tolerance(a.max_x, b.max_x)
        && matches_within_tolerance(a.max_y, b.max_y)
        && matches_within_tolerance(a.max_z, b.max_z);
}

ResolutionResult ReferenceResolver::resolve(const BodyRef& ref) {
    ResolutionResult result{ResolutionStatus::Missing, std::nullopt, "", {}};
    if (ref.contract_version != CONTRACT_VERSION) {
        result.status = ResolutionStatus::ContractMismatch;
        result.message = "Contract version mismatch";
        return result;
    }
    auto bodies = inspector_->enumerate_bodies();

    // Prefer exact index first
    if (ref.index >= 0 && ref.index < (int)bodies.size()) {
        const auto& b = bodies[(size_t)ref.index];
        bool sig_ok = matches_within_tolerance(ref.signature.volume, b.volume)
                   && vector_matches(ref.signature.centroid, b.centroid)
                   && bbox_matches(ref.signature.bbox, b.bbox);
        if (sig_ok) {
            result.status = ResolutionStatus::Success;
            BodyRef resolved = ref; // keep original signature
            resolved.index = b.index;
            result.resolved = resolved;
            return result;
        }
    }

    // Search for unique match by signature
    std::vector<int> candidates;
    for (size_t i = 0; i < bodies.size(); ++i) {
        const auto& b = bodies[i];
        if (matches_within_tolerance(ref.signature.volume, b.volume)
            && vector_matches(ref.signature.centroid, b.centroid)
            && bbox_matches(ref.signature.bbox, b.bbox)) {
            candidates.push_back((int)i);
        }
    }
    if (candidates.empty()) {
        result.status = ResolutionStatus::Drifted; // signature didn't match anything
        result.message = "No body matched signature (drift)";
        return result;
    }
    if (candidates.size() > 1) {
        result.status = ResolutionStatus::Ambiguous;
        result.message = "Multiple bodies matched signature";
        return result;
    }
    result.status = ResolutionStatus::Success;
    BodyRef resolved = ref;
    resolved.index = candidates.front();
    result.resolved = resolved;
    return result;
}

ResolutionResult ReferenceResolver::resolve(const FaceRef& ref) {
    ResolutionResult result{ResolutionStatus::Missing, std::nullopt, "", {}};
    
    if (ref.contract_version != CONTRACT_VERSION || ref.parent.contract_version != CONTRACT_VERSION) {
        result.status = ResolutionStatus::ContractMismatch;
        result.message = "Contract version mismatch";
        return result;
    }
    // Resolve parent body first
    auto parent_res = resolve(ref.parent);
    if (parent_res.status != ResolutionStatus::Success) {
        result.status = parent_res.status;
        result.message = "Parent body failed to resolve";
        return result;
    }
    int body_index = std::get<BodyRef>(*parent_res.resolved).index;
    
    // Enumerate faces for that body deterministically
    auto faces = inspector_->enumerate_faces(body_index);

    // FaceRef.index is a LOCAL face index (TopExp order within the resolved body).
    // Because enumerate_faces() may return a sorted list for determinism, never treat ref.index
    // as the vector position. Instead, locate the face with f.index == ref.index.
    auto find_by_local_index = [&](int local_idx) -> const inspection::FaceProperties* {
        for (const auto& f : faces) {
            if (f.index == local_idx) return &f;
        }
        return nullptr;
    };

    // Prefer same local index if present
    if (ref.index >= 0) {
        const auto* fp = find_by_local_index(ref.index);
        if (fp) {
            const auto& f = *fp;
            bool sig_ok = (f.surface_type == ref.signature.surface_type)
                       && matches_within_tolerance(f.area, ref.signature.area)
                       && vector_matches(f.centroid, ref.signature.centroid)
                       && bbox_matches(f.bbox, ref.signature.bbox)
                       && (!ref.signature.normal.has_value() || vector_matches(f.normal, *ref.signature.normal));
            if (sig_ok) {
                result.status = ResolutionStatus::Success;
                FaceRef resolved = ref;
                resolved.parent = std::get<BodyRef>(*parent_res.resolved);
                resolved.index = ref.index; // preserve local index contract
                result.resolved = resolved;
                return result;
            }
        }
    }

    // Search for unique match by signature
    std::vector<int> candidates;
    for (size_t i = 0; i < faces.size(); ++i) {
        const auto& f = faces[i];
        bool matches = (f.surface_type == ref.signature.surface_type)
            && matches_within_tolerance(f.area, ref.signature.area)
            && vector_matches(f.centroid, ref.signature.centroid)
            && bbox_matches(f.bbox, ref.signature.bbox)
            && (!ref.signature.normal.has_value() || vector_matches(f.normal, *ref.signature.normal));
        if (matches) {
            candidates.push_back(f.index); // store LOCAL face index
        }
    }
    if (candidates.empty()) {
        result.status = ResolutionStatus::Drifted;
        result.message = "No face matched signature (drift)";
        return result;
    }
    if (candidates.size() > 1) {
        result.status = ResolutionStatus::Ambiguous;
        result.message = "Multiple faces matched signature";
        return result;
    }
    result.status = ResolutionStatus::Success;
    FaceRef resolved = ref;
    resolved.parent = std::get<BodyRef>(*parent_res.resolved);
    resolved.index = candidates.front(); // LOCAL face index
    result.resolved = resolved;
    return result;
}

ResolutionResult ReferenceResolver::resolve(const EdgeRef& ref) {
    ResolutionResult result{ResolutionStatus::Missing, std::nullopt, "", {}};
    if (ref.contract_version != CONTRACT_VERSION) {
        result.status = ResolutionStatus::ContractMismatch;
        result.message = "Contract version mismatch";
        return result;
    }

    // Build candidate edge set
    std::vector<inspection::EdgeProperties> candidates;
    auto add_unique = [&](const std::vector<inspection::EdgeProperties>& src) {
        for (const auto& e : src) {
            bool exists = false;
            for (const auto& c : candidates) {
                if (c.index == e.index) { exists = true; break; }
            }
            if (!exists) candidates.push_back(e);
        }
    };

    if (!ref.parent_faces.empty()) {
        // Resolve each parent face and aggregate edges
        for (const auto& pf : ref.parent_faces) {
            auto pres = resolve(pf);
            if (pres.status != ResolutionStatus::Success) {
                result.status = pres.status;
                result.message = "Parent face failed to resolve";
                return result;
            }
            auto resolved_face = std::get<FaceRef>(*pres.resolved);
            auto face_edges = inspector_->enumerate_edges(resolved_face.index);
            add_unique(face_edges);
        }
    } else {
        candidates = inspector_->enumerate_all_edges();
    }

    auto sig_matches = [&](const inspection::EdgeProperties& e)->bool{
        if (e.edge_type != ref.signature.edge_type) return false;
        if (ref.signature.length.has_value() && !matches_within_tolerance(*ref.signature.length, e.length)) return false;
        if (ref.signature.radius.has_value() && !matches_within_tolerance(*ref.signature.radius, e.radius)) return false;
        if (!vector_matches(ref.signature.midpoint, e.midpoint)) return false;
        if (!bbox_matches(e.bbox, ref.signature.bbox)) return false;
        return true;
    };

    // Prefer exact index match if present among candidates and signature matches
    for (const auto& e : candidates) {
        if (e.index == ref.index && sig_matches(e)) {
            result.status = ResolutionStatus::Success;
            EdgeRef resolved = ref;
            resolved.index = e.index;
            result.resolved = resolved;
            return result;
        }
    }

    // Search for unique match by signature
    std::vector<int> idxs;
    for (const auto& e : candidates) {
        if (sig_matches(e)) idxs.push_back(e.index);
    }
    if (idxs.empty()) {
        result.status = ResolutionStatus::Drifted;
        result.message = "No edge matched signature (drift)";
        return result;
    }
    if (idxs.size() > 1) {
        result.status = ResolutionStatus::Ambiguous;
        result.message = "Multiple edges matched signature";
        return result;
    }
    result.status = ResolutionStatus::Success;
    EdgeRef resolved = ref;
    resolved.index = idxs.front();
    result.resolved = resolved;
    return result;
}

ResolutionResult ReferenceResolver::resolve(const Reference& ref) {
    if (std::holds_alternative<BodyRef>(ref)) return resolve(std::get<BodyRef>(ref));
    if (std::holds_alternative<FaceRef>(ref)) return resolve(std::get<FaceRef>(ref));
    if (std::holds_alternative<EdgeRef>(ref)) return resolve(std::get<EdgeRef>(ref));
    return {ResolutionStatus::Missing, std::nullopt, "Unknown reference type", {}};
}

std::vector<ResolutionResult> ReferenceResolver::resolve_all(const std::vector<Reference>& refs) {
    std::vector<ResolutionResult> results;
    results.reserve(refs.size());
    for (const auto& r : refs) results.push_back(resolve(r));
    return results;
}

// ========================= JSON Serialization =========================
// Sprint 9: All numeric serialization uses canonical::format_float() for determinism

static std::string vec3_to_json(const inspection::Vector3& v) {
    return "[" + canonical::format_float(v.x) + "," 
         + canonical::format_float(v.y) + "," 
         + canonical::format_float(v.z) + "]";
}

static std::string bbox_to_json(const inspection::BoundingBox& b) {
    return "[" + canonical::format_float(b.min_x) + "," 
         + canonical::format_float(b.min_y) + "," 
         + canonical::format_float(b.min_z) + "," 
         + canonical::format_float(b.max_x) + "," 
         + canonical::format_float(b.max_y) + "," 
         + canonical::format_float(b.max_z) + "]";
}

// JSON parsing helpers
static std::optional<std::string> extract_string_field(const std::string& json, const std::string& field) {
    std::string pattern = "\"" + field + "\":\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    pos += pattern.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return std::nullopt;
    return json.substr(pos, end - pos);
}

static std::optional<int> extract_int_field(const std::string& json, const std::string& field) {
    std::string pattern = "\"" + field + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    pos += pattern.length();
    size_t end = pos;
    while (end < json.length() && (std::isdigit(json[end]) || json[end] == '-')) ++end;
    if (end == pos) return std::nullopt;
    try { return std::stoi(json.substr(pos, end - pos)); }
    catch (...) { return std::nullopt; }
}

static std::optional<double> extract_double_field(const std::string& json, const std::string& field) {
    std::string pattern = "\"" + field + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    pos += pattern.length();
    // Skip whitespace
    while (pos < json.length() && std::isspace(json[pos])) ++pos;
    size_t end = pos;
    // Accept digits, dot, minus, plus, e/E for scientific notation
    bool has_e = false;
    while (end < json.length()) {
        char c = json[end];
        if (std::isdigit(c) || c == '.' || c == '-' || c == '+') {
            ++end;
        } else if ((c == 'e' || c == 'E') && !has_e) {
            has_e = true;
            ++end;
        } else {
            break;
        }
    }
    if (end == pos) return std::nullopt;
    try { return std::stod(json.substr(pos, end - pos)); }
    catch (...) { return std::nullopt; }
}

static std::optional<inspection::Vector3> extract_vec3_field(const std::string& json, const std::string& field) {
    std::string pattern = "\"" + field + "\":[";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    pos += pattern.length();
    size_t end = json.find("]", pos);
    if (end == std::string::npos) return std::nullopt;
    std::string arr = json.substr(pos, end - pos);
    
    // Parse three comma-separated doubles, handling scientific notation
    std::vector<double> vals;
    size_t start = 0;
    for (int i = 0; i < 3; ++i) {
        while (start < arr.length() && std::isspace(arr[start])) ++start;
        size_t num_end = start;
        bool has_e = false;
        while (num_end < arr.length()) {
            char c = arr[num_end];
            if (c == ',') break;
            if (std::isdigit(c) || c == '.' || c == '-' || c == '+') {
                ++num_end;
            } else if ((c == 'e' || c == 'E') && !has_e) {
                has_e = true;
                ++num_end;
            } else {
                break;
            }
        }
        if (num_end == start) return std::nullopt;
        try { vals.push_back(std::stod(arr.substr(start, num_end - start))); }
        catch (...) { return std::nullopt; }
        start = num_end + 1; // skip comma
    }
    if (vals.size() != 3) return std::nullopt;
    return inspection::Vector3{vals[0], vals[1], vals[2]};
}

static std::optional<inspection::BoundingBox> extract_bbox_field(const std::string& json, const std::string& field) {
    std::string pattern = "\"" + field + "\":[";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    pos += pattern.length();
    size_t end = json.find("]", pos);
    if (end == std::string::npos) return std::nullopt;
    std::string arr = json.substr(pos, end - pos);
    
    // Parse six comma-separated doubles, handling scientific notation
    std::vector<double> vals;
    size_t start = 0;
    while (start < arr.length() && vals.size() < 6) {
        while (start < arr.length() && std::isspace(arr[start])) ++start;
        if (start >= arr.length()) break;
        size_t num_end = start;
        bool has_e = false;
        while (num_end < arr.length()) {
            char c = arr[num_end];
            if (c == ',') break;
            if (std::isdigit(c) || c == '.' || c == '-' || c == '+') {
                ++num_end;
            } else if ((c == 'e' || c == 'E') && !has_e) {
                has_e = true;
                ++num_end;
            } else {
                break;
            }
        }
        if (num_end == start) break;
        try { vals.push_back(std::stod(arr.substr(start, num_end - start))); }
        catch (...) { return std::nullopt; }
        start = num_end + 1; // skip comma
    }
    if (vals.size() != 6) return std::nullopt;
    inspection::BoundingBox bbox;
    bbox.min_x = vals[0]; bbox.min_y = vals[1]; bbox.min_z = vals[2];
    bbox.max_x = vals[3]; bbox.max_y = vals[4]; bbox.max_z = vals[5];
    return bbox;
}

static std::optional<std::string> extract_object_field(const std::string& json, const std::string& field) {
    std::string pattern = "\"" + field + "\":{";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    pos += pattern.length() - 1; // include opening brace
    int depth = 0;
    size_t start = pos;
    while (pos < json.length()) {
        if (json[pos] == '{') ++depth;
        else if (json[pos] == '}') {
            --depth;
            if (depth == 0) return json.substr(start, pos - start + 1);
        }
        ++pos;
    }
    return std::nullopt;
}

std::string BodyRef::to_json() const {
    std::ostringstream os;
    os << "{\"ref_type\":\"" << ref_type << "\",";
    os << "\"contract_version\":\"" << contract_version << "\",";
    os << "\"index\":" << index << ",";
    os << "\"signature\":{";
    os << "\"volume\":" << canonical::format_float(signature.volume) << ",";
    os << "\"centroid\":" << vec3_to_json(signature.centroid) << ",";
    os << "\"bbox\":" << bbox_to_json(signature.bbox);
    os << "}}";
    return os.str();
}

std::optional<BodyRef> BodyRef::from_json(const std::string& json) {
    BodyRef ref;
    
    // Extract and validate contract_version
    auto cv = extract_string_field(json, "contract_version");
    if (!cv || *cv != CONTRACT_VERSION) return std::nullopt;
    ref.contract_version = *cv;
    
    // Extract ref_type
    auto rt = extract_string_field(json, "ref_type");
    if (!rt || *rt != "BodyRef") return std::nullopt;
    ref.ref_type = *rt;
    
    // Extract index
    auto idx = extract_int_field(json, "index");
    if (!idx) return std::nullopt;
    ref.index = *idx;
    
    // Extract signature object
    auto sig_obj = extract_object_field(json, "signature");
    if (!sig_obj) return std::nullopt;
    
    // Extract signature fields
    auto vol = extract_double_field(*sig_obj, "volume");
    if (!vol) return std::nullopt;
    ref.signature.volume = *vol;
    
    auto cen = extract_vec3_field(*sig_obj, "centroid");
    if (!cen) return std::nullopt;
    ref.signature.centroid = *cen;
    
    auto bbox = extract_bbox_field(*sig_obj, "bbox");
    if (!bbox) return std::nullopt;
    ref.signature.bbox = *bbox;
    
    return ref;
}

std::string FaceRef::to_json() const {
    using namespace inspection;
    std::ostringstream os;
    os << "{\"ref_type\":\"" << ref_type << "\",";
    os << "\"contract_version\":\"" << contract_version << "\",";
    os << "\"index\":" << index << ",";
    os << "\"parent\":" << parent.to_json() << ",";
    os << "\"signature\":{";
    os << "\"surface_type\":\"" << surface_type_to_string(signature.surface_type) << "\",";
    os << "\"area\":" << canonical::format_float(signature.area) << ",";
    os << "\"centroid\":" << vec3_to_json(signature.centroid);
    os << ",\"bbox\":" << bbox_to_json(signature.bbox);
    if (signature.normal.has_value()) {
        os << ",\"normal\":" << vec3_to_json(*signature.normal);
    }
    os << "}}";
    return os.str();
}

std::optional<FaceRef> FaceRef::from_json(const std::string& json) {
    FaceRef ref;
    
    // Extract and validate contract_version
    auto cv = extract_string_field(json, "contract_version");
    if (!cv || *cv != CONTRACT_VERSION) return std::nullopt;
    ref.contract_version = *cv;
    
    // Extract ref_type
    auto rt = extract_string_field(json, "ref_type");
    if (!rt || *rt != "FaceRef") return std::nullopt;
    ref.ref_type = *rt;
    
    // Extract index
    auto idx = extract_int_field(json, "index");
    if (!idx) return std::nullopt;
    ref.index = *idx;
    
    // Extract and parse parent BodyRef
    auto parent_obj = extract_object_field(json, "parent");
    if (!parent_obj) return std::nullopt;
    auto parent = BodyRef::from_json(*parent_obj);
    if (!parent) return std::nullopt;
    ref.parent = *parent;
    
    // Extract signature object (need to skip past parent's signature)
    // Find the parent object end first
    auto parent_start = json.find("\"parent\":{");
    if (parent_start == std::string::npos) return std::nullopt;
    size_t parent_end = parent_start + 10; // skip past "parent":{ 
    int depth = 1;
    while (parent_end < json.length() && depth > 0) {
        if (json[parent_end] == '{') ++depth;
        else if (json[parent_end] == '}') --depth;
        ++parent_end;
    }
    // Now search for "signature" after parent_end
    std::string remaining = json.substr(parent_end);
    auto sig_obj_in_remaining = extract_object_field(remaining, "signature");
    if (!sig_obj_in_remaining) return std::nullopt;
    
    // Extract signature fields
    auto st = extract_string_field(*sig_obj_in_remaining, "surface_type");
    if (!st) return std::nullopt;
    ref.signature.surface_type = inspection::string_to_surface_type(*st);
    
    auto area = extract_double_field(*sig_obj_in_remaining, "area");
    if (!area) return std::nullopt;
    ref.signature.area = *area;
    
    auto cen = extract_vec3_field(*sig_obj_in_remaining, "centroid");
    if (!cen) return std::nullopt;
    ref.signature.centroid = *cen;

    auto bbox = extract_bbox_field(*sig_obj_in_remaining, "bbox");
    if (!bbox) return std::nullopt;
    ref.signature.bbox = *bbox;
    
    // Normal is optional
    auto norm = extract_vec3_field(*sig_obj_in_remaining, "normal");
    if (norm) ref.signature.normal = *norm;
    
    return ref;
}

std::string EdgeRef::to_json() const {
    using namespace inspection;
    std::ostringstream os;
    os << "{\"ref_type\":\"" << ref_type << "\",";
    os << "\"contract_version\":\"" << contract_version << "\",";
    os << "\"index\":" << index << ",";
    if (!parent_faces.empty()) {
        os << "\"parent_faces\":[";
        for (size_t i = 0; i < parent_faces.size(); ++i) {
            if (i) os << ",";
            os << parent_faces[i].to_json();
        }
        os << "],";
    }
    os << "\"signature\":{";
    os << "\"edge_type\":\"" << edge_type_to_string(signature.edge_type) << "\",";
    if (signature.length.has_value()) os << "\"length\":" << canonical::format_float(*signature.length) << ",";
    if (signature.radius.has_value()) os << "\"radius\":" << canonical::format_float(*signature.radius) << ",";
    os << "\"midpoint\":" << vec3_to_json(signature.midpoint) << ",";
    os << "\"bbox\":" << bbox_to_json(signature.bbox) << "}}";
    return os.str();
}

std::optional<EdgeRef> EdgeRef::from_json(const std::string& json) {
    EdgeRef ref;
    
    // Extract and validate contract_version
    auto cv = extract_string_field(json, "contract_version");
    if (!cv || *cv != CONTRACT_VERSION) return std::nullopt;
    ref.contract_version = *cv;
    
    // Extract ref_type
    auto rt = extract_string_field(json, "ref_type");
    if (!rt || *rt != "EdgeRef") return std::nullopt;
    ref.ref_type = *rt;
    
    // Extract index
    auto idx = extract_int_field(json, "index");
    if (!idx) return std::nullopt;
    ref.index = *idx;
    
    // Extract parent_faces array (optional)
    std::string pf_pattern = "\"parent_faces\":[";
    size_t pf_pos = json.find(pf_pattern);
    if (pf_pos != std::string::npos) {
        size_t array_start = json.find('[', pf_pos);
        if (array_start != std::string::npos) {
            int array_depth = 1;
            size_t pos = array_start + 1;
            while (pos < json.size() && array_depth > 0) {
                if (json[pos] == '[') ++array_depth;
                else if (json[pos] == ']') --array_depth;
                ++pos;
            }
            if (array_depth == 0 && pos <= json.size()) {
                std::string arr = json.substr(array_start + 1, (pos - 1) - (array_start + 1));
            // Simple split by top-level braces
            size_t start = 0;
            while (start < arr.length()) {
                size_t brace_start = arr.find('{', start);
                if (brace_start == std::string::npos) break;
                int depth = 0;
                size_t obj_pos = brace_start;
                while (obj_pos < arr.length()) {
                    if (arr[obj_pos] == '{') ++depth;
                    else if (arr[obj_pos] == '}') {
                        --depth;
                        if (depth == 0) {
                            std::string face_json = arr.substr(brace_start, obj_pos - brace_start + 1);
                            auto face = FaceRef::from_json(face_json);
                            if (face) ref.parent_faces.push_back(*face);
                            start = obj_pos + 1;
                            break;
                        }
                    }
                    ++obj_pos;
                }
                if (depth != 0) break;
            }
        }
        }
    }
    
    // Extract signature object after parent_faces (if present) to avoid
    // accidentally parsing nested FaceRef signatures.
    std::string sig_source = json;
    size_t pf_sig_pos = json.find("\"parent_faces\":[");
    if (pf_sig_pos != std::string::npos) {
        size_t array_start = json.find('[', pf_sig_pos);
        if (array_start != std::string::npos) {
            int depth = 1;
            size_t pos = array_start + 1;
            while (pos < json.size() && depth > 0) {
                if (json[pos] == '[') ++depth;
                else if (json[pos] == ']') --depth;
                ++pos;
            }
            if (depth == 0 && pos <= json.size()) {
                sig_source = json.substr(pos);
            }
        }
    }

    // Extract signature object
    auto sig_obj = extract_object_field(sig_source, "signature");
    if (!sig_obj) return std::nullopt;
    
    // Extract signature fields
    auto et = extract_string_field(*sig_obj, "edge_type");
    if (!et) return std::nullopt;
    ref.signature.edge_type = inspection::string_to_edge_type(*et);
    
    // Length and radius are optional
    auto len = extract_double_field(*sig_obj, "length");
    if (len) ref.signature.length = *len;
    
    auto rad = extract_double_field(*sig_obj, "radius");
    if (rad) ref.signature.radius = *rad;
    
    auto mid = extract_vec3_field(*sig_obj, "midpoint");
    if (!mid) return std::nullopt;
    ref.signature.midpoint = *mid;
    
    auto bbox = extract_bbox_field(*sig_obj, "bbox");
    if (!bbox) return std::nullopt;
    ref.signature.bbox = *bbox;
    
    return ref;
}

} // namespace reference
} // namespace praxis
