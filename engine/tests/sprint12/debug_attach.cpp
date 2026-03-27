#include "TestHarness.hpp"
#include "OCCTInspector.hpp"
#include <iostream>

using namespace praxis::testing;
using namespace praxis::inspection;

int main() {
    // Inspect attached.step
    auto inspector = create_inspector();
    if (!inspector->load_artifact("test_outputs/attach_gate/output/step_2/attached.step")) {
        std::cerr << "Failed to load\n";
        return 1;
    }
    
    auto bodies = inspector->enumerate_bodies();
    std::cout << "Bodies: " << bodies.size() << "\n\n";
    
    for (size_t i = 0; i < bodies.size(); i++) {
        const auto& b = bodies[i];
        std::cout << "Body " << i << ": volume=" << b.volume 
                  << ", bbox=(" << b.bbox.min_x << "," << b.bbox.min_y << "," << b.bbox.min_z
                  << ")-(" << b.bbox.max_x << "," << b.bbox.max_y << "," << b.bbox.max_z << ")\n";
        
        auto faces = inspector->enumerate_faces(i);
        for (size_t j = 0; j < faces.size(); j++) {
            const auto& f = faces[j];
            std::cout << "  Face " << j << ": centroid=(" << f.centroid.x << "," << f.centroid.y << "," << f.centroid.z
                      << "), normal=(" << f.normal.x << "," << f.normal.y << "," << f.normal.z << ")\n";
        }
        std::cout << "\n";
    }
    
    return 0;
}
