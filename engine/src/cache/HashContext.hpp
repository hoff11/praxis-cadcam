#pragma once
#include <string>

namespace praxis {
namespace cache {

// Context for computing deterministic op hashes (Sprint 6 Phase 2)
struct HashContext {
    int schema_version = 1;
    
    std::string engine_version;     // From CMake PRAXIS_ENGINE_VERSION
    std::string kernel_version;     // From OCCT Standard_Version::GetVersion()
    
    int arg_precision = 6;          // Numeric arg serialization precision
    
    // Output contract (part of hash payload; bumpable if contract changes)
    std::string output_contract = "out";
};

} // namespace cache
} // namespace praxis
