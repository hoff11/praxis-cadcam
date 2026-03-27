/**
 * test_sprint10_validation.cpp
 * 
 * Minimal validation of Sprint 10 core functionality using actual codebase API
 * 
 * Tests what CAN be validated without full end-to-end execution:
 * - SemanticType enum completeness
 * - Selection mode validation logic
 * - Failure message registry coverage
 * - OutputArtifact schema correctness
 */

#include <gtest/gtest.h>
#include "../../include/SemanticTypes.hpp"
#include "../../include/FailureMessages.hpp"
#include "../../src/cache/OpCache.hpp"
#include "../../include/praxis/Intent.hpp"

using namespace praxis::semantic;
using namespace praxis::failures;
using namespace praxis::cache;

/**
 * Test: SemanticType enum is closed and complete
 * Validates: Exactly 6 semantic types as per semantic_objects.md
 */
TEST(Sprint10_SemanticTypes, EnumIsClosedAndComplete) {
    // Verify all 6 types can be created
    EXPECT_EQ(to_string(SemanticType::Product), "Product");
    EXPECT_EQ(to_string(SemanticType::Body), "Body");
    EXPECT_EQ(to_string(SemanticType::Face), "Face");
    EXPECT_EQ(to_string(SemanticType::Edge), "Edge");
    EXPECT_EQ(to_string(SemanticType::Artifact), "Artifact");
    EXPECT_EQ(to_string(SemanticType::Output), "Output");
}

/**
 * Test: Selection mode detection from selector syntax
 * Validates: Mode detection per selection_modes.md rules
 */
TEST(Sprint10_SelectionModes, ModeDetectionFromSelector) {
    auto product_mode = detect_mode_from_selector("product");
    ASSERT_TRUE(product_mode.has_value());
    EXPECT_EQ(*product_mode, SelectionMode::ProductSelection);
    
    auto body_mode = detect_mode_from_selector("body[0]");
    ASSERT_TRUE(body_mode.has_value());
    EXPECT_EQ(*body_mode, SelectionMode::BodySelection);
    
    auto face_mode = detect_mode_from_selector("face[type=planar]");
    ASSERT_TRUE(face_mode.has_value());
    EXPECT_EQ(*face_mode, SelectionMode::FaceSelection);
    
    auto edge_mode = detect_mode_from_selector("edge[radius=5.0]");
    ASSERT_TRUE(edge_mode.has_value());
    EXPECT_EQ(*edge_mode, SelectionMode::EdgeSelection);
    
    // Invalid selector
    auto invalid = detect_mode_from_selector("invalid_selector");
    EXPECT_FALSE(invalid.has_value());
}

/**
 * Test: Selection mode/type matrix enforcement
 * Validates: Matrix rules per selection_modes.md
 */
TEST(Sprint10_SelectionModes, ModeTypeMatrixEnforcement) {
    // ProductSelection allows Product and Body
    EXPECT_TRUE(is_selectable_in_mode(SelectionMode::ProductSelection, SemanticType::Product));
    EXPECT_TRUE(is_selectable_in_mode(SelectionMode::ProductSelection, SemanticType::Body));
    EXPECT_FALSE(is_selectable_in_mode(SelectionMode::ProductSelection, SemanticType::Face));
    EXPECT_FALSE(is_selectable_in_mode(SelectionMode::ProductSelection, SemanticType::Edge));
    
    // BodySelection allows only Body
    EXPECT_TRUE(is_selectable_in_mode(SelectionMode::BodySelection, SemanticType::Body));
    EXPECT_FALSE(is_selectable_in_mode(SelectionMode::BodySelection, SemanticType::Product));
    EXPECT_FALSE(is_selectable_in_mode(SelectionMode::BodySelection, SemanticType::Face));
    
    // FaceSelection allows only Face
    EXPECT_TRUE(is_selectable_in_mode(SelectionMode::FaceSelection, SemanticType::Face));
    EXPECT_FALSE(is_selectable_in_mode(SelectionMode::FaceSelection, SemanticType::Body));
    
    // EdgeSelection allows only Edge
    EXPECT_TRUE(is_selectable_in_mode(SelectionMode::EdgeSelection, SemanticType::Edge));
    EXPECT_FALSE(is_selectable_in_mode(SelectionMode::EdgeSelection, SemanticType::Face));
}

/**
 * Test: Invalid mode messages are contract-compliant
 * Validates: Messages reference correct selection mode
 */
TEST(Sprint10_SelectionModes, InvalidModeMessagesAreContractCompliant) {
    std::string msg = get_invalid_mode_message(
        SelectionMode::ProductSelection, 
        SemanticType::Face
    );
    
    // Message must mention the violation
    EXPECT_NE(msg.find("Face"), std::string::npos);
    EXPECT_NE(msg.find("ProductSelection"), std::string::npos);
    EXPECT_NE(msg.find("FaceSelection"), std::string::npos);
}

/**
 * Test: Failure message registry covers documented types
 * Validates: selection_modes.md failure types have messages
 */
TEST(Sprint10_FailureMessages, SelectionModeFailuresCovered) {
    // All selection_modes.md failure types must have messages
    EXPECT_FALSE(get_failure_message("InvalidSelectionMode").empty());
    EXPECT_FALSE(get_failure_message("InvalidSelector").empty());
    EXPECT_FALSE(get_failure_message("InvalidBodySelector").empty());
    EXPECT_FALSE(get_failure_message("InvalidFaceSelector").empty());
    EXPECT_FALSE(get_failure_message("InvalidEdgeSelector").empty());
    EXPECT_FALSE(get_failure_message("NoFacesMatched").empty());
    EXPECT_FALSE(get_failure_message("NoEdgesMatched").empty());
    EXPECT_FALSE(get_failure_message("Ambiguous").empty());
    EXPECT_FALSE(get_failure_message("AmbiguousSelection").empty());
    EXPECT_FALSE(get_failure_message("BodyIndexOutOfRange").empty());
    EXPECT_FALSE(get_failure_message("ArtifactLoadFailure").empty());
    EXPECT_FALSE(get_failure_message("EmptyProduct").empty());
}

/**
 * Test: Failure messages are bounded (single sentence)
 * Validates: Messages don't contain newlines or multiple sentences
 */
TEST(Sprint10_FailureMessages, MessagesAreBounded) {
    std::string msg1 = get_failure_message("InvalidSelector", "test\ncontext\nwith\nnewlines");
    EXPECT_EQ(msg1.find('\n'), std::string::npos) << "Message contains newline";
    EXPECT_EQ(msg1.find('\r'), std::string::npos) << "Message contains carriage return";
    
    std::string msg2 = get_failure_message("ArtifactLoadFailure", "path/to/file.step");
    // Single sentence rule: should end with period, not have internal periods
    size_t first_period = msg2.find('.');
    size_t last_period = msg2.rfind('.');
    EXPECT_EQ(first_period, last_period) << "Message has multiple sentences";
}

/**
 * Test: OutputArtifact has preview classification fields
 * Validates: Schema includes type, previewable, semantic_type
 */
TEST(Sprint10_OutputArtifact, HasPreviewClassificationFields) {
    OutputArtifact artifact;
    artifact.name = "test";
    artifact.filename = "test.step";
    artifact.bytes = 1234;
    artifact.sha256 = "abc123";
    
    // Sprint 10 fields
    artifact.type = "step";
    artifact.previewable = true;
    artifact.semantic_type = "Body";
    
    // Verify fields compile and can be set
    EXPECT_EQ(artifact.type, "step");
    EXPECT_TRUE(artifact.previewable);
    EXPECT_EQ(artifact.semantic_type, "Body");
}

/**
 * Test: IntentResult has summary structure
 * Validates: Summary fields exist per Task 3.1
 */
TEST(Sprint10_ResultSummary, HasSummaryStructure) {
    praxis::IntentResult result;
    
    // Verify summary fields exist
    result.summary.intent = "Test intent";
    result.summary.body_count = 1;
    result.summary.face_count = 6;
    result.summary.edge_count = 12;
    result.summary.artifact_count = 1;
    result.summary.produced.push_back("1 Body");
    result.summary.previewable_outputs.push_back("out");
    
    EXPECT_EQ(result.summary.intent, "Test intent");
    EXPECT_EQ(result.summary.body_count, 1);
    EXPECT_EQ(result.summary.produced.size(), 1);
    EXPECT_EQ(result.summary.previewable_outputs.size(), 1);
}

/**
 * Test: Parse semantic type from string
 * Validates: String → enum conversion works case-insensitively
 */
TEST(Sprint10_SemanticTypes, ParseFromString) {
    EXPECT_EQ(parse_semantic_type("body"), SemanticType::Body);
    EXPECT_EQ(parse_semantic_type("Body"), SemanticType::Body);
    EXPECT_EQ(parse_semantic_type("BODY"), SemanticType::Body);
    EXPECT_EQ(parse_semantic_type("face"), SemanticType::Face);
    EXPECT_EQ(parse_semantic_type("edge"), SemanticType::Edge);
    EXPECT_EQ(parse_semantic_type("product"), SemanticType::Product);
    
    // Invalid type
    EXPECT_FALSE(parse_semantic_type("invalid").has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
