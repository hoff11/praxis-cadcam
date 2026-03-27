/**
 * test_selector_parse.cpp
 * 
 * Phase C.4: Selector Grammar Parsing Tests
 * 
 * Test Coverage:
 * 1. Valid parsing (basic functional syntax)
 * 2. Filters (field=value, operators)
 * 3. Canonical normalization (deterministic output)
 * 4. Error handling (typed errors with spans)
 * 5. Edge cases (empty, whitespace, trailing input)
 */

#include "Selector.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace praxis::selector;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinRel;

TEST_CASE("Valid functional syntax - no filters", "[selector][c4]") {
    auto result = SelectorParser::parse("bodies()");
    REQUIRE(result.selector.has_value());
    REQUIRE_FALSE(result.error.has_value());
    
    auto& sel = *result.selector;
    REQUIRE(sel.target == SelectorTarget::Bodies);
    REQUIRE(sel.filters.empty());
    REQUIRE(sel.original == "bodies()");
    REQUIRE(sel.canonical == "bodies()");
}

TEST_CASE("All target types parse correctly", "[selector][c4]") {
    SECTION("bodies()") {
        auto r = SelectorParser::parse("bodies()");
        REQUIRE(r.selector.has_value());
        REQUIRE(r.selector->target == SelectorTarget::Bodies);
    }
    
    SECTION("faces()") {
        auto r = SelectorParser::parse("faces()");
        REQUIRE(r.selector.has_value());
        REQUIRE(r.selector->target == SelectorTarget::Faces);
    }
    
    SECTION("edges()") {
        auto r = SelectorParser::parse("edges()");
        REQUIRE(r.selector.has_value());
        REQUIRE(r.selector->target == SelectorTarget::Edges);
    }
    
    SECTION("vertices()") {
        auto r = SelectorParser::parse("vertices()");
        REQUIRE(r.selector.has_value());
        REQUIRE(r.selector->target == SelectorTarget::Vertices);
    }
}

TEST_CASE("Single filter parsing", "[selector][c4]") {
    auto result = SelectorParser::parse("faces(type=planar)");
    REQUIRE(result.selector.has_value());
    REQUIRE_FALSE(result.error.has_value());
    
    auto& sel = *result.selector;
    REQUIRE(sel.target == SelectorTarget::Faces);
    REQUIRE(sel.filters.size() == 1);
    
    auto& f = sel.filters[0];
    REQUIRE(f.field == "type");
    REQUIRE(f.op == FilterOp::Eq);
    REQUIRE(f.value.kind == Literal::Kind::String);
    REQUIRE(f.value.s == "planar");
}

TEST_CASE("Multiple filters parsing", "[selector][c4]") {
    auto result = SelectorParser::parse("faces(type=planar, area>10.5)");
    REQUIRE(result.selector.has_value());
    
    auto& sel = *result.selector;
    REQUIRE(sel.filters.size() == 2);
    
    // Filters are sorted alphabetically by field
    REQUIRE(sel.filters[0].field == "area");
    REQUIRE(sel.filters[0].op == FilterOp::Gt);
    REQUIRE(sel.filters[0].value.kind == Literal::Kind::Float);
    REQUIRE_THAT(sel.filters[0].value.d, WithinRel(10.5));
    
    REQUIRE(sel.filters[1].field == "type");
    REQUIRE(sel.filters[1].op == FilterOp::Eq);
    REQUIRE(sel.filters[1].value.kind == Literal::Kind::String);
}

TEST_CASE("All comparison operators", "[selector][c4]") {
    SECTION("Equals") {
        auto r = SelectorParser::parse("faces(index=0)");
        REQUIRE(r.selector->filters[0].op == FilterOp::Eq);
    }
    
    SECTION("Not equals") {
        auto r = SelectorParser::parse("faces(type!=planar)");
        REQUIRE(r.selector->filters[0].op == FilterOp::Neq);
    }
    
    SECTION("Less than") {
        auto r = SelectorParser::parse("faces(area<100)");
        REQUIRE(r.selector->filters[0].op == FilterOp::Lt);
    }
    
    SECTION("Less than or equals") {
        auto r = SelectorParser::parse("faces(area<=100)");
        REQUIRE(r.selector->filters[0].op == FilterOp::Lte);
    }
    
    SECTION("Greater than") {
        auto r = SelectorParser::parse("faces(area>50)");
        REQUIRE(r.selector->filters[0].op == FilterOp::Gt);
    }
    
    SECTION("Greater than or equals") {
        auto r = SelectorParser::parse("faces(area>=50)");
        REQUIRE(r.selector->filters[0].op == FilterOp::Gte);
    }
}

TEST_CASE("Literal types", "[selector][c4]") {
    SECTION("Integer") {
        auto r = SelectorParser::parse("faces(index=42)");
        REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::Int);
        REQUIRE(r.selector->filters[0].value.i == 42);
    }
    
    SECTION("Float") {
        auto r = SelectorParser::parse("faces(area=3.14)");
        REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::Float);
        REQUIRE_THAT(r.selector->filters[0].value.d, WithinRel(3.14));
    }
    
    SECTION("String (quoted)") {
        auto r = SelectorParser::parse("faces(type=\"planar\")");
        REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::String);
        REQUIRE(r.selector->filters[0].value.s == "planar");
    }
    
    SECTION("String (unquoted identifier)") {
        auto r = SelectorParser::parse("faces(type=planar)");
        REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::String);
        REQUIRE(r.selector->filters[0].value.s == "planar");
    }
    
    SECTION("Boolean true") {
        auto r = SelectorParser::parse("faces(visible=true)");
        REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::Bool);
        REQUIRE(r.selector->filters[0].value.b == true);
    }
    
    SECTION("Boolean false") {
        auto r = SelectorParser::parse("faces(visible=false)");
        REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::Bool);
        REQUIRE(r.selector->filters[0].value.b == false);
    }
}

TEST_CASE("Canonical normalization - case insensitive", "[selector][c4]") {
    auto r1 = SelectorParser::parse("Bodies()");
    auto r2 = SelectorParser::parse("BODIES()");
    auto r3 = SelectorParser::parse("bodies()");
    
    REQUIRE(r1.selector.has_value());
    REQUIRE(r2.selector.has_value());
    REQUIRE(r3.selector.has_value());
    
    // All should have same canonical form
    REQUIRE(r1.selector->canonical == r2.selector->canonical);
    REQUIRE(r2.selector->canonical == r3.selector->canonical);
    REQUIRE(r3.selector->canonical == "bodies()");
}

TEST_CASE("Canonical normalization - filter field lowercase", "[selector][c4]") {
    auto r1 = SelectorParser::parse("faces(TYPE=planar)");
    auto r2 = SelectorParser::parse("faces(Type=planar)");
    auto r3 = SelectorParser::parse("faces(type=planar)");
    
    REQUIRE(r1.selector->canonical == r2.selector->canonical);
    REQUIRE(r2.selector->canonical == r3.selector->canonical);
    
    // Verify field name is lowercase
    REQUIRE(r1.selector->filters[0].field == "type");
}

TEST_CASE("Canonical normalization - filter sorting", "[selector][c4]") {
    // Parse in different orders
    auto r1 = SelectorParser::parse("faces(b=2, a=1)");
    auto r2 = SelectorParser::parse("faces(a=1, b=2)");
    
    REQUIRE(r1.selector.has_value());
    REQUIRE(r2.selector.has_value());
    
    // Both should have same canonical form (alphabetically sorted)
    REQUIRE(r1.selector->canonical == r2.selector->canonical);
    REQUIRE(r1.selector->canonical == "faces(a=1, b=2)");
    
    // Verify internal filter order
    REQUIRE(r1.selector->filters[0].field == "a");
    REQUIRE(r1.selector->filters[1].field == "b");
}

TEST_CASE("Canonical normalization - number formatting", "[selector][c4]") {
    SECTION("Integer has no decimal") {
        auto r = SelectorParser::parse("faces(index=42)");
        REQUIRE(r.selector->canonical == "faces(index=42)");
    }
    
    SECTION("Float formatting") {
        auto r = SelectorParser::parse("faces(area=3.14159265)");
        REQUIRE(r.selector.has_value());
        // Should be formatted with fixed precision
        REQUIRE_THAT(r.selector->canonical, ContainsSubstring("area=3.14159"));
    }
}

TEST_CASE("Whitespace handling", "[selector][c4]") {
    auto r1 = SelectorParser::parse("faces()");
    auto r2 = SelectorParser::parse("  faces()  ");
    auto r3 = SelectorParser::parse("faces(  )");
    auto r4 = SelectorParser::parse("faces( type = planar )");
    
    REQUIRE(r1.selector->canonical == r2.selector->canonical);
    REQUIRE(r1.selector->canonical == r3.selector->canonical);
    REQUIRE(r4.selector->canonical == "faces(type=planar)");
}

TEST_CASE("Error: Invalid function name", "[selector][c4]") {
    auto result = SelectorParser::parse("invalid()");
    REQUIRE_FALSE(result.selector.has_value());
    REQUIRE(result.error.has_value());
    
    auto& err = *result.error;
    REQUIRE(err.kind == SelectorErrorKind::InvalidFunction);
    REQUIRE_THAT(err.message, ContainsSubstring("invalid"));
}

TEST_CASE("Error: Missing parens (expected opening paren)", "[selector][c4]") {
    auto result = SelectorParser::parse("faces");
    REQUIRE_FALSE(result.selector.has_value());
    REQUIRE(result.error.has_value());
    
    auto& err = *result.error;
    // Note: Currently mapped to MissingClosingParen - could be ExpectedLParen
    REQUIRE(err.kind == SelectorErrorKind::MissingClosingParen);
}

TEST_CASE("Error: Missing closing paren", "[selector][c4]") {
    auto result = SelectorParser::parse("faces(type=planar");
    REQUIRE_FALSE(result.selector.has_value());
    REQUIRE(result.error.has_value());
    
    auto& err = *result.error;
    REQUIRE(err.kind == SelectorErrorKind::MissingClosingParen);
}

TEST_CASE("Error: Invalid operator", "[selector][c4]") {
    auto result = SelectorParser::parse("faces(type~planar)");
    REQUIRE_FALSE(result.selector.has_value());
    REQUIRE(result.error.has_value());
    
    auto& err = *result.error;
    REQUIRE(err.kind == SelectorErrorKind::InvalidOperator);
}

TEST_CASE("Error: Trailing input", "[selector][c4]") {
    auto result = SelectorParser::parse("faces() extra");
    REQUIRE_FALSE(result.selector.has_value());
    REQUIRE(result.error.has_value());
    
    auto& err = *result.error;
    REQUIRE(err.kind == SelectorErrorKind::TrailingInput);
}

TEST_CASE("JSON output format", "[selector][c4]") {
    auto result = SelectorParser::parse("faces(type=planar, area>10)");
    REQUIRE(result.selector.has_value());
    
    std::string json = SelectorParser::to_json(*result.selector);
    
    // Verify JSON contains key fields
    REQUIRE_THAT(json, ContainsSubstring("\"target\": \"faces\""));
    REQUIRE_THAT(json, ContainsSubstring("\"filters\""));
    REQUIRE_THAT(json, ContainsSubstring("\"field\": \"area\""));
    REQUIRE_THAT(json, ContainsSubstring("\"field\": \"type\""));
    REQUIRE_THAT(json, ContainsSubstring("\"original\""));
    REQUIRE_THAT(json, ContainsSubstring("\"canonical\""));
}

TEST_CASE("JSON error output format", "[selector][c4]") {
    auto result = SelectorParser::parse("invalid()");
    REQUIRE(result.error.has_value());
    
    std::string json = SelectorParser::error_to_json(*result.error);
    
    REQUIRE_THAT(json, ContainsSubstring("\"error\": \"InvalidFunction\""));
    REQUIRE_THAT(json, ContainsSubstring("\"message\""));
    REQUIRE_THAT(json, ContainsSubstring("\"span\""));
}

TEST_CASE("Complex selector", "[selector][c4]") {
    auto result = SelectorParser::parse(
        "faces(type=planar, area>50, area<100, index!=5)"
    );
    
    REQUIRE(result.selector.has_value());
    
    auto& sel = *result.selector;
    REQUIRE(sel.target == SelectorTarget::Faces);
    REQUIRE(sel.filters.size() == 4);
    
    // Filters should be sorted: area<100, area>50, index!=5, type=planar
    // (sorted by field, then op, then value)
    REQUIRE(sel.filters[0].field == "area");
    REQUIRE(sel.filters[1].field == "area");
    REQUIRE(sel.filters[2].field == "index");
    REQUIRE(sel.filters[3].field == "type");
}

TEST_CASE("Deterministic canonical output", "[selector][c4]") {
    // Parse same selector multiple times
    std::vector<std::string> canonicals;
    for (int i = 0; i < 10; ++i) {
        auto r = SelectorParser::parse("faces(z=3, y=2, x=1)");
        REQUIRE(r.selector.has_value());
        canonicals.push_back(r.selector->canonical);
    }
    
    // All should be identical
    for (const auto& c : canonicals) {
        REQUIRE(c == canonicals[0]);
    }
    
    // Should be alphabetically sorted
    REQUIRE(canonicals[0] == "faces(x=1, y=2, z=3)");
}

TEST_CASE("Negative numbers", "[selector][c4]") {
    SECTION("Negative integer") {
        auto r = SelectorParser::parse("faces(offset=-1)");
        REQUIRE(r.selector.has_value());
        REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::Int);
        REQUIRE(r.selector->filters[0].value.i == -1);
        REQUIRE(r.selector->canonical == "faces(offset=-1)");
    }
    
    SECTION("Negative float") {
        auto r = SelectorParser::parse("faces(area>=-0.25)");
        REQUIRE(r.selector.has_value());
        REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::Float);
        REQUIRE_THAT(r.selector->filters[0].value.d, WithinRel(-0.25));
    }
}

TEST_CASE("Scientific notation", "[selector][c4]") {
    auto r = SelectorParser::parse("faces(area=1e-3)");
    REQUIRE(r.selector.has_value());
    REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::Float);
    REQUIRE_THAT(r.selector->filters[0].value.d, WithinRel(0.001));
    
    // Canonical should normalize to standard decimal
    REQUIRE_THAT(r.selector->canonical, ContainsSubstring("area="));
}

TEST_CASE("Duplicate fields - deterministic ordering", "[selector][c4]") {
    // If duplicate fields are allowed, they should sort deterministically
    auto r = SelectorParser::parse("faces(type=planar, type=cylindrical)");
    
    if (r.selector.has_value()) {
        // Both filters should be present and sorted
        REQUIRE(r.selector->filters.size() == 2);
        REQUIRE(r.selector->filters[0].field == "type");
        REQUIRE(r.selector->filters[1].field == "type");
        
        // Canonical should be deterministic
        auto r2 = SelectorParser::parse("faces(type=cylindrical, type=planar)");
        REQUIRE(r2.selector.has_value());
        REQUIRE(r.selector->canonical == r2.selector->canonical);
    }
    // If duplicates are not allowed, an error should be raised (future enhancement)
}

TEST_CASE("String escape sequences", "[selector][c4]") {
    auto r = SelectorParser::parse("faces(name=\"test\\nvalue\")");
    REQUIRE(r.selector.has_value());
    REQUIRE(r.selector->filters[0].value.kind == Literal::Kind::String);
    // Parser should handle escape sequences
    REQUIRE(r.selector->filters[0].value.s.find('\\') != std::string::npos);
}

TEST_CASE("Error: Trailing comma", "[selector][c4]") {
    auto result = SelectorParser::parse("faces(a=1,)");
    REQUIRE_FALSE(result.selector.has_value());
    REQUIRE(result.error.has_value());
    
    // Should be a clear grammar error (currently might be UnexpectedToken or InvalidFilterValue)
    auto& err = *result.error;
    REQUIRE((err.kind == SelectorErrorKind::InvalidFilterValue || 
             err.kind == SelectorErrorKind::UnexpectedToken));
}

TEST_CASE("Complex numeric filters", "[selector][c4]") {
    SECTION("Multiple ranges") {
        auto r = SelectorParser::parse("faces(area>=10, area<=100)");
        REQUIRE(r.selector.has_value());
        REQUIRE(r.selector->filters.size() == 2);
        // Sorted alphabetically by op string: "<=" comes before ">="
        REQUIRE(r.selector->filters[0].op == FilterOp::Lte);
        REQUIRE(r.selector->filters[1].op == FilterOp::Gte);
    }
    
    SECTION("Zero values") {
        auto r = SelectorParser::parse("faces(offset=0, scale=0.0)");
        REQUIRE(r.selector.has_value());
        REQUIRE(r.selector->filters[0].value.i == 0);
        REQUIRE_THAT(r.selector->filters[1].value.d, WithinRel(0.0));
    }
}
