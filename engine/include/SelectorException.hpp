/**
 * SelectorException.hpp
 * 
 * Typed exception hierarchy for selector operations
 * 
 * Purpose: Replace string-based error handling with structured exceptions
 * that can be caught and handled specifically.
 */

#pragma once

#include <stdexcept>
#include <string>

namespace praxis {
namespace selector {

/**
 * Base exception for all selector operations
 */
class SelectorException : public std::runtime_error {
public:
    explicit SelectorException(const std::string& message)
        : std::runtime_error(message) {}
    
    virtual ~SelectorException() = default;
};

/**
 * Property does not exist on the target entity type
 * Example: faces.foo (foo is not a valid face property)
 */
class PropertyNotFoundException : public SelectorException {
public:
    PropertyNotFoundException(const std::string& property_name, const std::string& entity_type)
        : SelectorException("Property '" + property_name + "' not found on " + entity_type)
        , property_name_(property_name)
        , entity_type_(entity_type) {}
    
    const std::string& property_name() const { return property_name_; }
    const std::string& entity_type() const { return entity_type_; }
    
private:
    std::string property_name_;
    std::string entity_type_;
};

/**
 * Type mismatch between property and filter value
 * Example: faces[area == "large"] (area is numeric, "large" is string)
 */
class TypeMismatchException : public SelectorException {
public:
    TypeMismatchException(const std::string& property_type, const std::string& filter_type)
        : SelectorException("Type mismatch: cannot compare " + property_type + " property with " + filter_type + " filter")
        , property_type_(property_type)
        , filter_type_(filter_type) {}
    
    const std::string& property_type() const { return property_type_; }
    const std::string& filter_type() const { return filter_type_; }
    
private:
    std::string property_type_;
    std::string filter_type_;
};

/**
 * No entities matched the selector
 * Note: This is NOT an exception in normal flow - it's a valid result
 * This exception is only thrown if zero matches are considered an error condition
 */
class NoMatchesException : public SelectorException {
public:
    explicit NoMatchesException(const std::string& selector_canonical)
        : SelectorException("Selector matched no entities: " + selector_canonical)
        , selector_canonical_(selector_canonical) {}
    
    const std::string& selector_canonical() const { return selector_canonical_; }
    
private:
    std::string selector_canonical_;
};

/**
 * Inspector query failed
 * Example: Kernel returned invalid data, artifact not loaded
 */
class InspectorException : public SelectorException {
public:
    explicit InspectorException(const std::string& message)
        : SelectorException("Inspector error: " + message) {}
};

}  // namespace selector
}  // namespace praxis
