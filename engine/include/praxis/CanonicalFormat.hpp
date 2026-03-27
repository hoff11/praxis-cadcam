/**
 * CanonicalFormat.hpp
 * 
 * Sprint 9 - EPIC 1: Deterministic Serialization (Foundational)
 * 
 * Purpose: Single source of truth for float formatting and JSON escaping
 * Guarantee: Byte-for-byte identical serialization across platforms and runs
 * 
 * Rules (non-negotiable):
 * - Normalize -0.0 → 0.0
 * - Reject NaN/Inf (fail fast)
 * - Locale-independent formatting (std::locale::classic())
 * - Precision policy: %.9g equivalent (defaultfloat + setprecision(9))
 * 
 * Usage: Every double in every JSON file MUST go through format_float()
 */

#pragma once
#include <string>

namespace praxis {
namespace canonical {

/**
 * Format a double for deterministic JSON serialization
 * 
 * Normalizes -0.0 to 0.0, rejects NaN/Inf
 * Uses locale-independent formatting with precision 9
 * 
 * @param v The double value to format
 * @return Canonical string representation
 * @throws std::runtime_error if v is NaN or Inf
 */
std::string format_float(double v);

/**
 * Escape a string for JSON serialization
 * 
 * Handles quotes, backslashes, control characters
 * Ensures deterministic output for all string values
 * 
 * @param s The string to escape
 * @return JSON-escaped string (without surrounding quotes)
 */
std::string escape_json(const std::string& s);

} // namespace canonical
} // namespace praxis
