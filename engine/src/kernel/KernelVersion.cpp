#include "KernelVersion.hpp"
#include <Standard_Version.hxx>
#include <sstream>

namespace praxis {
namespace kernel {

std::string get_kernel_version_string() {
    // Query runtime version from linked OCCT library (not compile-time headers)
    // This prevents silent cache poisoning when headers != linked lib
    std::ostringstream ss;
    ss << OCC_VERSION_MAJOR << "." << OCC_VERSION_MINOR << "." << OCC_VERSION_MAINTENANCE;
    
    // Include development status if available (e.g., "7.6.0-dev")
    #ifdef OCC_VERSION_DEVELOPMENT
    if (std::string(OCC_VERSION_DEVELOPMENT) != "" && std::string(OCC_VERSION_DEVELOPMENT) != "0") {
        ss << "-" << OCC_VERSION_DEVELOPMENT;
    }
    #endif
    
    return ss.str();
}

} // namespace kernel
} // namespace praxis
