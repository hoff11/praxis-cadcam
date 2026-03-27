#pragma once
#include "Intent.hpp"
#include <memory>

namespace praxis {

class IntentRouter {
public:
    // Execute an intent and return results
    static IntentResult execute(const IntentRequest& request);

private:
    // Intent handlers
    static IntentResult executeGenerateVMC(const IntentRequest& request);
    static IntentResult executeHeal(const IntentRequest& request);
    static IntentResult executeBuildFromRecipe(const IntentRequest& request);
    
    // Helper to ensure output directory exists
    static bool ensureOutputDir(const std::string& path);
};

} // namespace praxis
