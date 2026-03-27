#include "KinematicLoader.hpp"
#include "KinematicValidator.hpp"
#include "praxis/CanonicalFormat.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// For v0, we'll use a simple JSON library or manual parsing
// For now, let's create a minimal JSON parser for PKM files
// In production, you'd use nlohmann/json or similar

namespace praxis {
namespace kinematics {

// Simple JSON value extractor (very basic, not production-ready)
static std::string extract_string_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    
    size_t start = json.find("\"", pos + search.length());
    if (start == std::string::npos) return "";
    start++;
    
    size_t end = json.find("\"", start);
    if (end == std::string::npos) return "";
    
    return json.substr(start, end - start);
}

PKMLoadResult KinematicLoader::load_from_json(const std::string& file_path) {
    PKMLoadResult result;
    
    // Read file
    std::ifstream file(file_path);
    if (!file.good()) {
        result.success = false;
        result.error_message = "File does not exist: " + file_path;
        return result;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    try {
        result.pkm = parse_json(content);
        
        // Validate after loading
        auto validation = KinematicValidator::validate(result.pkm);
        if (!validation.valid) {
            result.success = false;
            result.error_message = "PKM validation failed: " + 
                (validation.errors.empty() ? "unknown error" : validation.errors[0]);
            result.warnings = validation.warnings;
            return result;
        }
        
        result.warnings = validation.warnings;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Failed to parse JSON: ") + e.what();
        return result;
    }
    
    return result;
}

PKM KinematicLoader::parse_json(const std::string& json_content) {
    PKM pkm;
    
    // Minimal JSON parser for PKM files (Sprint 2)
    // Handles: strings, arrays of strings, objects with known structure
    // Does NOT handle: nested objects, arbitrary structure, escape sequences
    
    // Helper lambda: find value after key
    auto find_value = [&](const std::string& key) -> std::pair<size_t, size_t> {
        std::string search = "\"" + key + "\"";
        size_t key_pos = json_content.find(search);
        if (key_pos == std::string::npos) return {std::string::npos, 0};
        
        size_t colon = json_content.find(":", key_pos);
        if (colon == std::string::npos) return {std::string::npos, 0};
        
        // Skip whitespace
        size_t start = colon + 1;
        while (start < json_content.size() && std::isspace(json_content[start])) start++;
        
        return {start, 0};
    };
    
    // Helper: extract string value
    auto extract_string = [&](const std::string& key) -> std::string {
        auto [pos, _] = find_value(key);
        if (pos == std::string::npos) return "";
        
        if (json_content[pos] != '"') return "";
        size_t end = json_content.find('"', pos + 1);
        if (end == std::string::npos) return "";
        
        return json_content.substr(pos + 1, end - pos - 1);
    };
    
    // Helper: extract string array
    auto extract_string_array = [&](const std::string& key) -> std::vector<std::string> {
        std::vector<std::string> result;
        auto [pos, _] = find_value(key);
        if (pos == std::string::npos) return result;
        
        if (json_content[pos] != '[') return result;
        size_t end = json_content.find(']', pos);
        if (end == std::string::npos) return result;
        
        // Parse array content
        size_t current = pos + 1;
        while (current < end) {
            // Skip whitespace
            while (current < end && std::isspace(json_content[current])) current++;
            if (current >= end) break;
            
            if (json_content[current] == '"') {
                size_t str_end = json_content.find('"', current + 1);
                if (str_end == std::string::npos || str_end > end) break;
                result.push_back(json_content.substr(current + 1, str_end - current - 1));
                current = str_end + 1;
            } else if (json_content[current] == ',') {
                current++;
            } else {
                break;
            }
        }
        
        return result;
    };
    
    // Helper: extract numeric array (for vectors/limits)
    auto extract_number_array = [&](const std::string& text, size_t start) -> std::vector<double> {
        std::vector<double> result;
        size_t end = text.find(']', start);
        if (end == std::string::npos) return result;
        
        size_t current = start + 1;
        std::string num_str;
        
        while (current < end) {
            char c = text[current];
            if (std::isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
                num_str += c;
            } else if (c == ',' || std::isspace(c)) {
                if (!num_str.empty()) {
                    result.push_back(std::stod(num_str));
                    num_str.clear();
                }
            }
            current++;
        }
        
        if (!num_str.empty()) {
            result.push_back(std::stod(num_str));
        }
        
        return result;
    };
    
    // Parse top-level fields
    pkm.pkm_version = extract_string("pkm_version");
    pkm.machine = extract_string("machine");
    pkm.units = extract_string("units");
    pkm.bodies = extract_string_array("bodies");
    pkm.chain = extract_string_array("chain");
    
    // Parse joints array (more complex)
    size_t joints_key = json_content.find("\"joints\"");
    if (joints_key != std::string::npos) {
        size_t joints_start = json_content.find('[', joints_key);
        
        if (joints_start != std::string::npos) {
            // Find matching closing bracket for joints array
            size_t joints_end = joints_start + 1;
            int bracket_depth = 1;
            while (joints_end < json_content.size() && bracket_depth > 0) {
                if (json_content[joints_end] == '[') bracket_depth++;
                else if (json_content[joints_end] == ']') bracket_depth--;
                joints_end++;
            }
            joints_end--;  // Back up to the closing bracket
            
            // Find each joint object using brace matching
            size_t current = joints_start + 1;
            
            while (current < joints_end) {
                // Skip whitespace and commas
                while (current < joints_end && (std::isspace(json_content[current]) || json_content[current] == ',')) {
                    current++;
                }
                if (current >= joints_end) break;
                
                // Find opening brace
                if (json_content[current] != '{') break;
                
                size_t obj_start = current;
                size_t obj_end = obj_start + 1;
                int brace_depth = 1;
                
                // Find matching closing brace
                while (obj_end < joints_end && brace_depth > 0) {
                    if (json_content[obj_end] == '{') brace_depth++;
                    else if (json_content[obj_end] == '}') brace_depth--;
                    obj_end++;
                }
                obj_end--;  // Back up to the closing brace
                
                if (brace_depth != 0) break;  // Malformed JSON
                
                std::string joint_str = json_content.substr(obj_start, obj_end - obj_start + 1);
                
                // Extract from joint substring
                auto extract_from_substr = [](const std::string& substr, const std::string& key) -> std::string {
                    size_t kpos = substr.find("\"" + key + "\"");
                    if (kpos == std::string::npos) return "";
                    size_t colon = substr.find(":", kpos);
                    if (colon == std::string::npos) return "";
                    
                    // Skip whitespace after colon
                    size_t val_start = colon + 1;
                    while (val_start < substr.size() && std::isspace(substr[val_start])) val_start++;
                    
                    if (val_start >= substr.size()) return "";
                    
                    // Check if it's a quoted string
                    if (substr[val_start] == '"') {
                        size_t quote2 = substr.find('"', val_start + 1);
                        if (quote2 == std::string::npos) return "";
                        return substr.substr(val_start + 1, quote2 - val_start - 1);
                    }
                    
                    return "";
                };
                
                Joint joint;
                joint.id = extract_from_substr(joint_str, "id");
                std::string type_str = extract_from_substr(joint_str, "type");
                joint.type = (type_str == "revolute") ? JointType::Revolute : JointType::Prismatic;
                joint.parent_body = extract_from_substr(joint_str, "parent_body");
                joint.child_body = extract_from_substr(joint_str, "child_body");
                
                // Parse axis_origin
                size_t origin_pos = joint_str.find("\"axis_origin\"");
                if (origin_pos != std::string::npos) {
                    size_t bracket = joint_str.find('[', origin_pos);
                    auto vals = extract_number_array(joint_str, bracket);
                    if (vals.size() >= 3) {
                        joint.axis_origin = Vec3(vals[0], vals[1], vals[2]);
                    }
                }
                
                // Parse axis_direction
                size_t dir_pos = joint_str.find("\"axis_direction\"");
                if (dir_pos != std::string::npos) {
                    size_t bracket = joint_str.find('[', dir_pos);
                    auto vals = extract_number_array(joint_str, bracket);
                    if (vals.size() >= 3) {
                        joint.axis_direction = Vec3(vals[0], vals[1], vals[2]);
                    }
                }
                
                // Parse limits (supports both array [min, max] and object {min:..., max:...})
                size_t limits_pos = joint_str.find("\"limits\"");
                if (limits_pos != std::string::npos) {
                    size_t bracket = joint_str.find('[', limits_pos);
                    size_t brace = joint_str.find('{', limits_pos);
                    
                    // Determine which comes first (or if only one exists)
                    bool use_array = false;
                    if (bracket != std::string::npos && (brace == std::string::npos || bracket < brace)) {
                        use_array = true;
                    }
                    
                    if (use_array) {
                        // Array format
                        auto vals = extract_number_array(joint_str, bracket);
                        if (vals.size() >= 2) {
                            joint.limits.min = vals[0];
                            joint.limits.max = vals[1];
                        }
                    } else if (brace != std::string::npos) {
                        // Object format
                        size_t min_pos = joint_str.find("\"min\"", brace);
                        size_t max_pos = joint_str.find("\"max\"", brace);
                        if (min_pos != std::string::npos && max_pos != std::string::npos) {
                            size_t min_colon = joint_str.find(':', min_pos);
                            size_t max_colon = joint_str.find(':', max_pos);
                            if (min_colon != std::string::npos && max_colon != std::string::npos) {
                                std::string min_str, max_str;
                                for (size_t i = min_colon + 1; i < joint_str.size(); i++) {
                                    if (std::isdigit(joint_str[i]) || joint_str[i] == '.' || joint_str[i] == '-') {
                                        min_str += joint_str[i];
                                    } else if (!min_str.empty()) break;
                                }
                                for (size_t i = max_colon + 1; i < joint_str.size(); i++) {
                                    if (std::isdigit(joint_str[i]) || joint_str[i] == '.' || joint_str[i] == '-') {
                                        max_str += joint_str[i];
                                    } else if (!max_str.empty()) break;
                                }
                                if (!min_str.empty() && !max_str.empty()) {
                                    joint.limits.min = std::stod(min_str);
                                    joint.limits.max = std::stod(max_str);
                                }
                            }
                        }
                    }
                }
                
                pkm.joints.push_back(joint);
                current = obj_end + 1;
            }
        }
    }
    
    // Parse frames object (optional in v0)
    size_t frames_key = json_content.find("\"frames\"");
    if (frames_key != std::string::npos) {
        size_t frames_brace = json_content.find('{', frames_key);
        
        if (frames_brace != std::string::npos) {
            // Find matching closing brace
            size_t frames_end = frames_brace + 1;
            int brace_depth = 1;
            
            while (frames_end < json_content.size() && brace_depth > 0) {
                if (json_content[frames_end] == '{') brace_depth++;
                else if (json_content[frames_end] == '}') brace_depth--;
                frames_end++;
            }
            frames_end--;  // Back up to closing brace
            
            size_t current = frames_brace + 1;
            
            // Parse each frame entry
            while (current < frames_end) {
                // Skip whitespace and commas
                while (current < frames_end && (std::isspace(json_content[current]) || json_content[current] == ',')) {
                    current++;
                }
                
                if (current >= frames_end || json_content[current] == '}') break;
                
                // Expect a quoted frame name
                if (json_content[current] != '"') {
                    current++;
                    continue;
                }
                
                // Extract frame name
                size_t name_start = current + 1;
                size_t name_end = json_content.find('"', name_start);
                if (name_end == std::string::npos) break;
                
                std::string frame_name = json_content.substr(name_start, name_end - name_start);
                
                // Find opening brace of frame object
                size_t obj_start = json_content.find('{', name_end);
                if (obj_start == std::string::npos || obj_start >= frames_end) break;
                
                size_t obj_end = obj_start + 1;
                int obj_depth = 1;
                
                while (obj_end < frames_end && obj_depth > 0) {
                    if (json_content[obj_end] == '{') obj_depth++;
                    else if (json_content[obj_end] == '}') obj_depth--;
                    obj_end++;
                }
                obj_end--;
                
                if (obj_depth != 0) break;
                
                std::string frame_str = json_content.substr(obj_start, obj_end - obj_start + 1);
                
                Frame frame;
                frame.name = frame_name;
                
                // Parse origin, x_axis, y_axis, z_axis as Vec3 arrays
                auto parse_vec3 = [&](const std::string& key) -> Vec3 {
                    Vec3 result = {0, 0, 0};
                    size_t vec_key = frame_str.find("\"" + key + "\"");
                    if (vec_key != std::string::npos) {
                        size_t bracket = frame_str.find('[', vec_key);
                        if (bracket != std::string::npos) {
                            size_t bracket_end = frame_str.find(']', bracket);
                            if (bracket_end != std::string::npos) {
                                std::string vec_str = frame_str.substr(bracket + 1, bracket_end - bracket - 1);
                                std::istringstream iss(vec_str);
                                char comma;
                                iss >> result.x;
                                iss >> comma;  // consume comma
                                iss >> result.y;
                                iss >> comma;
                                iss >> result.z;
                            }
                        }
                    }
                    return result;
                };
                
                frame.origin = parse_vec3("origin");
                frame.x_axis = parse_vec3("x_axis");
                frame.y_axis = parse_vec3("y_axis");
                frame.z_axis = parse_vec3("z_axis");
                
                pkm.frames.push_back(frame);
                current = obj_end + 1;
            }
        }
    }
    
    return pkm;
}

bool KinematicLoader::save_to_json(const PKM& pkm, const std::string& file_path) {
    try {
        std::string json = generate_json(pkm);
        
        std::ofstream file(file_path);
        if (!file.good()) {
            return false;
        }
        
        file << json;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string KinematicLoader::generate_json(const PKM& pkm) {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "  \"pkm_version\": \"" << canonical::escape_json(pkm.pkm_version) << "\",\n";
    ss << "  \"machine\": \"" << canonical::escape_json(pkm.machine) << "\",\n";
    ss << "  \"units\": \"" << canonical::escape_json(pkm.units) << "\",\n";
    
    // Bodies array
    ss << "  \"bodies\": [";
    for (size_t i = 0; i < pkm.bodies.size(); i++) {
        ss << "\"" << canonical::escape_json(pkm.bodies[i]) << "\"";
        if (i < pkm.bodies.size() - 1) ss << ", ";
    }
    ss << "],\n";
    
    // Joints array
    ss << "  \"joints\": [\n";
    for (size_t i = 0; i < pkm.joints.size(); i++) {
        const auto& j = pkm.joints[i];
        ss << "    {\n";
        ss << "      \"id\": \"" << canonical::escape_json(j.id) << "\",\n";
        ss << "      \"type\": \"" << canonical::escape_json(joint_type_to_string(j.type)) << "\",\n";
        ss << "      \"parent_body\": \"" << canonical::escape_json(j.parent_body) << "\",\n";
        ss << "      \"child_body\": \"" << canonical::escape_json(j.child_body) << "\",\n";
        ss << "      \"axis_origin\": [" << j.axis_origin.x << ", " 
           << j.axis_origin.y << ", " << j.axis_origin.z << "],\n";
        ss << "      \"axis_direction\": [" << j.axis_direction.x << ", " 
           << j.axis_direction.y << ", " << j.axis_direction.z << "],\n";
        ss << "      \"limits\": [" << j.limits.min << ", " << j.limits.max << "]\n";
        ss << "    }";
        if (i < pkm.joints.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Chain array
    ss << "  \"chain\": [";
    for (size_t i = 0; i < pkm.chain.size(); i++) {
        ss << "\"" << canonical::escape_json(pkm.chain[i]) << "\"";
        if (i < pkm.chain.size() - 1) ss << ", ";
    }
    ss << "],\n";
    
    // Frames object
    ss << "  \"frames\": {";
    if (!pkm.frames.empty()) {
        ss << "\n";
        for (size_t i = 0; i < pkm.frames.size(); i++) {
            const auto& f = pkm.frames[i];
            ss << "    \"" << canonical::escape_json(f.name) << "\": {\n";
            ss << "      \"origin\": [" << f.origin.x << ", " << f.origin.y << ", " << f.origin.z << "],\n";
            ss << "      \"x_axis\": [" << f.x_axis.x << ", " << f.x_axis.y << ", " << f.x_axis.z << "],\n";
            ss << "      \"y_axis\": [" << f.y_axis.x << ", " << f.y_axis.y << ", " << f.y_axis.z << "],\n";
            ss << "      \"z_axis\": [" << f.z_axis.x << ", " << f.z_axis.y << ", " << f.z_axis.z << "]\n";
            ss << "    }";
            if (i < pkm.frames.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << "  ";
    }
    ss << "}\n";
    
    ss << "}\n";
    
    return ss.str();
}

} // namespace kinematics
} // namespace praxis
