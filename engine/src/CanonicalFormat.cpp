/**
 * CanonicalFormat.cpp
 * 
 * Sprint 9 - EPIC 1: Deterministic Serialization
 * Implementation of canonical float formatting and JSON escaping
 */

#include "praxis/CanonicalFormat.hpp"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace praxis {
namespace canonical {

std::string format_float(double v) {
    // Reject non-finite values (NaN/Inf) - fail fast
    if (std::isnan(v)) {
        throw std::runtime_error("NaN is not allowed in canonical JSON serialization");
    }
    if (std::isinf(v)) {
        throw std::runtime_error("Infinity is not allowed in canonical JSON serialization");
    }
    
    // Normalize -0.0 to 0.0 (ensures deterministic representation)
    if (v == 0.0) {
        v = 0.0;
    }
    
    // Use locale-independent formatting with precision 9
    // defaultfloat provides %.9g equivalent behavior
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss.setf(std::ios::fmtflags(0), std::ios::floatfield);  // Clear float field flags
    oss << std::setprecision(9) << std::defaultfloat << v;
    
    return oss.str();
}

std::string escape_json(const std::string& s) {
    std::ostringstream oss;
    
    for (char c : s) {
        switch (c) {
            case '\"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\b':
                oss << "\\b";
                break;
            case '\f':
                oss << "\\f";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                // Escape control characters (0x00-0x1F)
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c))
                        << std::dec;
                } else {
                    oss << c;
                }
        }
    }
    
    return oss.str();
}

} // namespace canonical
} // namespace praxis
