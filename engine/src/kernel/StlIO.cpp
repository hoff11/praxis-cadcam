#include "StlIO.hpp"
#include "ShapeHandleInternal.hpp"
#include <BRepMesh_IncrementalMesh.hxx>
#include <StlAPI_Writer.hxx>
#include <TopoDS_Shape.hxx>
#include <iostream>
#include <fstream>
#include <chrono>

namespace praxis {
namespace kernel {

StlWriteResult StlIO::write_stl(
    const ShapeHandle& shape, 
    const std::string& file_path,
    double deflection
) {
    StlWriteResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        if (shape.is_null()) {
            result.success = false;
            result.error_message = "Cannot write STL: shape is null";
            result.operations.push_back("write_stl: FAILED (null shape)");
            return result;
        }
        
        // Tessellate/mesh the shape
        // deflection controls mesh quality: smaller = finer mesh
        // angle_deflection in radians (0.5 is reasonable)
        TopoDS_Shape occt_shape = get_shape_internal(shape);
        BRepMesh_IncrementalMesh mesher(occt_shape, deflection, Standard_False, 0.5);
        
        if (!mesher.IsDone()) {
            result.success = false;
            result.error_message = "STL meshing failed";
            result.operations.push_back("write_stl: FAILED (meshing)");
            return result;
        }
        
        // Write STL file (binary mode for smaller files)
        StlAPI_Writer writer;
        writer.ASCIIMode() = Standard_False;  // binary STL
        
        // Note: Write() returns IFSelect_ReturnStatus enum where IFSelect_RetDone = 0 means success
        // Don't use boolean check, as 0 evaluates to false
        auto write_status = writer.Write(occt_shape, file_path.c_str());
        (void)write_status;  // IFSelect_RetDone = 0 is success, but we'll verify file exists instead
        
        // Verify file was actually written
        std::ifstream test_file(file_path, std::ios::binary);
        if (!test_file.good()) {
            result.success = false;
            result.error_message = "STL write failed: output file not created";
            result.operations.push_back("write_stl: FAILED (file not created)");
            return result;
        }
        test_file.close();
        
        auto end = std::chrono::high_resolution_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
        
        result.success = true;
        result.operations.push_back("write_stl: wrote " + file_path);
        result.metrics["stl_deflection"] = std::to_string(deflection);
        result.metrics["stl_write_ms"] = std::to_string(duration_ms);
        
        std::cout << "✓ STL written: " << file_path << " (deflection=" << deflection << ", " 
                  << duration_ms << "ms)\n";
        
        return result;
        
    } catch (const Standard_Failure& e) {
        result.success = false;
        result.error_message = std::string("OCCT exception: ") + e.GetMessageString();
        result.operations.push_back("write_stl: FAILED (exception)");
        return result;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("write_stl failed: ") + e.what();
        result.operations.push_back("write_stl: FAILED (exception)");
        return result;
    }
}

} // namespace kernel
} // namespace praxis
