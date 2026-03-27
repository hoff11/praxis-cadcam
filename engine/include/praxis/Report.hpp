#pragma once
#include "Intent.hpp"
#include <string>
#include <fstream>

namespace praxis {

class Report {
public:
    static bool writeReport(const IntentRequest& request, 
                           const IntentResult& result,
                           const std::string& outputPath);
};

} // namespace praxis
