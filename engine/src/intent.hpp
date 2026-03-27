// Intent definitions for Praxis-CAD
// Day 2 - Intent Router Skeleton

#pragma once
#include <string>
#include <vector>

namespace praxis {

// Intent types
enum class IntentType {
    Unknown,
    GenerateMachine3AxisVMC,
    HealAndNormalize
};

// Base intent structure
struct Intent {
    IntentType type = IntentType::Unknown;
    virtual ~Intent() = default;
};

// GenerateMachine3AxisVMC intent
struct GenerateVMCIntent : public Intent {
    double travelX = 1000.0;  // mm
    double travelY = 500.0;   // mm
    double travelZ = 500.0;   // mm
    double tableX = 1200.0;   // mm
    double tableY = 600.0;    // mm
    std::string fidelity = "low";  // "low" or "medium"
    
    GenerateVMCIntent() { type = IntentType::GenerateMachine3AxisVMC; }
};

// HealAndNormalize intent
struct HealIntent : public Intent {
    std::string inputFile;
    double tolerance = 0.01;  // mm
    std::vector<std::string> goals;  // "watertight", "manifold", "simulation-ready"
    
    HealIntent() { type = IntentType::HealAndNormalize; }
};

// Result structure
struct CadResult {
    bool success = false;
    std::string message;
    std::string outputDir;
};

} // namespace praxis
