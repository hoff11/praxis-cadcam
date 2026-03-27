/**
 * ParseSelectorCommand.cpp
 * 
 * Phase C.4: Parse selector expression (no topology resolution)
 * 
 * Usage:
 *   praxis-cad parse-selector "faces(type=planar)" --json
 */

#include "Selector.hpp"
#include <iostream>
#include <iomanip>

namespace praxis {
namespace commands {

int handle_parse_selector(const std::string& selector_expr, bool json_output) {
    using namespace praxis::selector;
    
    // Parse the selector
    ParseResult result = SelectorParser::parse(selector_expr);
    
    if (json_output) {
        // JSON output
        std::cout << "{\n";
        std::cout << "  \"success\": " << (result.selector.has_value() ? "true" : "false") << ",\n";
        
        if (result.selector) {
            std::cout << "  \"selector\": ";
            std::cout << SelectorParser::to_json(*result.selector);
            std::cout << "\n";
        }
        
        if (result.error) {
            std::cout << "  \"error\": ";
            std::cout << SelectorParser::error_to_json(*result.error);
            std::cout << "\n";
        }
        
        std::cout << "}\n";
    } else {
        // Human-readable output
        if (result.selector) {
            const Selector& sel = *result.selector;
            std::cout << "✓ Parse successful\n\n";
            std::cout << "Target:     " << to_string(sel.target) << "\n";
            std::cout << "Filters:    " << sel.filters.size() << "\n";
            
            if (!sel.filters.empty()) {
                std::cout << "\nFilters:\n";
                for (const Filter& f : sel.filters) {
                    std::cout << "  " << f.field << " " << to_string(f.op) << " " 
                             << f.value.canon << "\n";
                }
            }
            
            std::cout << "\nOriginal:   " << sel.original << "\n";
            std::cout << "Canonical:  " << sel.canonical << "\n";
        } else if (result.error) {
            const SelectorError& err = *result.error;
            std::cout << "✗ Parse failed\n\n";
            std::cout << "Error:   " << to_string(err.kind) << "\n";
            std::cout << "Message: " << err.message << "\n";
            std::cout << "Span:    [" << err.span.start << ", " << err.span.end << ")\n";
            
            // Show the error position in context
            std::cout << "\nInput:   " << selector_expr << "\n";
            std::cout << "         ";
            for (int i = 0; i < err.span.start; ++i) std::cout << " ";
            std::cout << "^";
            for (int i = err.span.start + 1; i < err.span.end; ++i) std::cout << "~";
            std::cout << "\n";
        }
    }
    
    return result.selector.has_value() ? 0 : 1;
}

}  // namespace commands
}  // namespace praxis
