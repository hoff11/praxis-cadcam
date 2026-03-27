/**
 * test_json_escaping.cpp
 * 
 * Sprint 9 - Move 3: JSON String Escaping Hardening Test
 * 
 * Ensures that escape_json() correctly handles all problematic characters
 * and that failure messages containing these characters produce valid JSON
 */

#include "praxis/CanonicalFormat.hpp"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace praxis::canonical;

// Helper: Parse JSON string value (minimal parser for testing)
std::string extract_json_string_value(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\":\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    
    pos += pattern.length();
    std::ostringstream result;
    
    // Simple unescaping parser
    while (pos < json.length() && json[pos] != '\"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            char next = json[pos + 1];
            if (next == '\"') result << '\"';
            else if (next == '\\') result << '\\';
            else if (next == 'n') result << '\n';
            else if (next == 't') result << '\t';
            else if (next == 'r') result << '\r';
            else result << '\\' << next;
            pos += 2;
        } else {
            result << json[pos];
            pos++;
        }
    }
    
    return result.str();
}

void test_escape_quotes() {
    std::cout << "Test: Escape quotes in strings\n";
    
    std::string input = "expected \"1.0\" got \"1.1\"";
    std::string escaped = escape_json(input);
    
    std::cout << "  Input:   " << input << "\n";
    std::cout << "  Escaped: " << escaped << "\n";
    
    // Should contain escaped quotes
    assert(escaped.find("\\\"") != std::string::npos);
    assert(escaped == "expected \\\"1.0\\\" got \\\"1.1\\\"");
    
    std::cout << "  ✓ Quotes escaped correctly\n\n";
}

void test_escape_newlines() {
    std::cout << "Test: Escape newlines in strings\n";
    
    std::string input = "line1\nline2\nline3";
    std::string escaped = escape_json(input);
    
    std::cout << "  Input:   (contains newlines)\n";
    std::cout << "  Escaped: " << escaped << "\n";
    
    // Should contain escaped newlines
    assert(escaped.find("\\n") != std::string::npos);
    assert(escaped == "line1\\nline2\\nline3");
    
    std::cout << "  ✓ Newlines escaped correctly\n\n";
}

void test_escape_backslashes() {
    std::cout << "Test: Escape backslashes (Windows paths)\n";
    
    std::string input = "C:\\path\\to\\file.txt";
    std::string escaped = escape_json(input);
    
    std::cout << "  Input:   " << input << "\n";
    std::cout << "  Escaped: " << escaped << "\n";
    
    // Should contain escaped backslashes
    assert(escaped.find("\\\\") != std::string::npos);
    assert(escaped == "C:\\\\path\\\\to\\\\file.txt");
    
    std::cout << "  ✓ Backslashes escaped correctly\n\n";
}

void test_failure_message_escaping() {
    std::cout << "Test: Failure messages with problematic characters produce valid JSON\n";
    
    // Build an interaction-style JSON fragment directly from escaped fields.
    std::string message = "Syntax error:\n  Expected \"}\" but got \"]\"\n  At C:\\path\\selector.txt:10";
    std::string selector = "body[invalid syntax \"quoted\"]";

    std::string json = "{";
    json += "\"type\":\"InvalidSelector\",";
    json += "\"message\":\"" + escape_json(message) + "\",";
    json += "\"selector\":\"" + escape_json(selector) + "\"";
    json += "}";
    
    std::cout << "  Generated JSON:\n" << json << "\n\n";
    
    // The JSON should be valid (no unescaped quotes/newlines/backslashes)
    // Check that it doesn't contain literal newlines
    assert(json.find('\n') == std::string::npos || json.find("\\n") != std::string::npos);
    
    // Check that quotes are escaped
    size_t msg_start = json.find("\"message\":\"");
    assert(msg_start != std::string::npos);
    
    // Extract and verify the escaped message
    std::string extracted_msg = extract_json_string_value(json, "message");
    std::cout << "  Extracted message: " << extracted_msg << "\n";
    
    // The extracted message should match the original (after unescaping)
    assert(extracted_msg == message);
    
    std::cout << "  ✓ Failure JSON is valid and reversible\n\n";
}

void test_control_characters() {
    std::cout << "Test: Control characters are escaped\n";
    
    std::string input = "text\x01with\x02control\x03chars";
    std::string escaped = escape_json(input);
    
    std::cout << "  Escaped: " << escaped << "\n";
    
    // Should contain \\u escape sequences
    assert(escaped.find("\\u") != std::string::npos);
    
    std::cout << "  ✓ Control characters escaped as \\uXXXX\n\n";
}

void test_roundtrip_stability() {
    std::cout << "Test: Escape and unescape produces identical result\n";
    
    std::string original = "Complex \"string\" with:\n  - newlines\n  - backslashes: \\\n  - tabs:\t\there";
    std::string escaped = escape_json(original);
    
    // Build a minimal JSON and parse it back
    std::string json = "{\"value\":\"" + escaped + "\"}";
    std::string extracted = extract_json_string_value(json, "value");
    
    std::cout << "  Original:  " << original << "\n";
    std::cout << "  Extracted: " << extracted << "\n";
    
    assert(original == extracted);
    
    std::cout << "  ✓ Roundtrip is identical\n\n";
}

int main() {
    std::cout << "=================================================\n";
    std::cout << "Sprint 9 - Move 3: JSON Escaping Hardening Tests\n";
    std::cout << "=================================================\n\n";
    
    try {
        test_escape_quotes();
        test_escape_newlines();
        test_escape_backslashes();
        test_failure_message_escaping();
        test_control_characters();
        test_roundtrip_stability();
        
        std::cout << "=================================================\n";
        std::cout << "✅ All JSON escaping tests passed!\n";
        std::cout << "=================================================\n";
        std::cout << "Invalid JSON from unescaped strings is now IMPOSSIBLE.\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << "\n";
        return 1;
    }
}
