#pragma once
#include "ShapeHandle.hpp"
#include <string>
#include <vector>
#include <map>

namespace praxis {
namespace kernel {

struct StlWriteResult {
    bool success = true;
    std::string error_message;
    std::vector<std::string> operations;
    std::map<std::string, std::string> metrics;
};

class StlIO {
public:
    // Write STL file (tessellated/meshed representation)
    // deflection: controls mesh quality (smaller = finer mesh, typical: 0.1-1.0)
    static StlWriteResult write_stl(
        const ShapeHandle& shape, 
        const std::string& file_path,
        double deflection = 0.5
    );
};

} // namespace kernel
} // namespace praxis
