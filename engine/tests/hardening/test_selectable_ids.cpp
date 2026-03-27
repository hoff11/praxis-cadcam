// Sprint 10/EPIC 1.6: Stable selectable IDs must not depend on label text

#include "../../include/InteractionEmit.hpp"
#include <cassert>
#include <iostream>

using namespace praxis;

static void test_label_change_does_not_change_stable_id() {
    std::cout << "[Test] label_change_does_not_change_stable_id\n";
    IntentResult r;

    const std::string sid = stable_id::face(3, 17);
    emit_selectable(r, "FaceRef", sid, "Face 17 (planar)");
    emit_selectable(r, "FaceRef", sid, "Top face");

    assert(r.interaction.selectables.size() == 2);
    assert(r.interaction.selectables[0].stable_id == sid);
    assert(r.interaction.selectables[1].stable_id == sid);
    // Labels may differ; IDs must not.
    assert(r.interaction.selectables[0].label != r.interaction.selectables[1].label);
}

static void test_edge_id_sorts_faces() {
    std::cout << "[Test] edge_id_sorts_faces\n";
    std::vector<inspection::FaceLocation> a_faces = {{0, 10}, {1, 2}, {2, 7}};
    std::vector<inspection::FaceLocation> b_faces = {{2, 7}, {0, 10}, {1, 2}};
    const std::string a = stable_id::edge(5, a_faces);
    const std::string b = stable_id::edge(5, b_faces);
    assert(a == b);
}

int main() {
    test_label_change_does_not_change_stable_id();
    test_edge_id_sorts_faces();
    std::cout << "OK\n";
    return 0;
}
