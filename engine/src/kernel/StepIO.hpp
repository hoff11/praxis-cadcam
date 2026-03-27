#pragma once
#include "ShapeHandle.hpp"
#include <string>
#include <vector>
#include <map>

namespace praxis {
namespace kernel {

struct StepReadResult {
    bool success = true;
    ShapeHandle shape;
    std::string error_message;
    std::vector<std::string> operations;
    std::map<std::string, std::string> metrics;
};

struct StepWriteResult {
    bool success = true;
    std::string error_message;
    std::vector<std::string> operations;
    std::map<std::string, std::string> metrics;
};

class StepIO {
public:
    // Read STEP file
    static StepReadResult read_step(const std::string& file_path);
    
    // Write STEP file
    static StepWriteResult write_step(
        const ShapeHandle& shape, 
        const std::string& file_path
    );
};

} // namespace kernel
} // namespace praxis
