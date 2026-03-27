#include "StepIO.hpp"
#include "ShapeHandleInternal.hpp"
#include <TopoDS_Shape.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <fstream>

namespace praxis {
namespace kernel {

StepReadResult StepIO::read_step(const std::string& file_path) {
    StepReadResult result;
    
    try {
        // Check file exists
        std::ifstream test_file(file_path);
        if (!test_file.good()) {
            result.success = false;
            result.error_message = "File does not exist: " + file_path;
            return result;
        }
        test_file.close();
        
        // Read STEP file
        STEPControl_Reader reader;
        IFSelect_ReturnStatus read_status = reader.ReadFile(file_path.c_str());
        result.operations.push_back("STEPControl_Reader.ReadFile()");
        
        if (read_status != IFSelect_RetDone) {
            result.success = false;
            result.error_message = "Failed to read STEP file: " + file_path;
            return result;
        }
        
        // Transfer to shape
        int num_roots = reader.TransferRoots();
        result.operations.push_back("STEPControl_Reader.TransferRoots()");
        result.metrics["num_roots"] = std::to_string(num_roots);
        
        if (num_roots == 0) {
            result.success = false;
            result.error_message = "No roots found in STEP file";
            return result;
        }
        
        TopoDS_Shape temp_shape = reader.OneShape();
        
        if (temp_shape.IsNull()) {
            result.success = false;
            result.error_message = "Failed to extract shape from STEP file";
            return result;
        }
        
        result.shape = ShapeHandle();
        set_shape_internal(result.shape, temp_shape);
        result.metrics["file_path"] = file_path;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("read_step failed: ") + e.what();
    }
    
    return result;
}

StepWriteResult StepIO::write_step(
    const ShapeHandle& shape,
    const std::string& file_path
) {
    StepWriteResult result;
    
    try {
        if (shape.is_null()) {
            result.success = false;
            result.error_message = "Cannot write null shape to STEP";
            return result;
        }
        
        const TopoDS_Shape& occt_shape = get_shape_internal(shape);
        
        STEPControl_Writer writer;
        IFSelect_ReturnStatus transfer_status = writer.Transfer(occt_shape, STEPControl_AsIs);
        result.operations.push_back("STEPControl_Writer.Transfer()");
        
        if (transfer_status != IFSelect_RetDone) {
            result.success = false;
            result.error_message = "Failed to transfer shape to STEP writer";
            return result;
        }
        
        IFSelect_ReturnStatus write_status = writer.Write(file_path.c_str());
        result.operations.push_back("STEPControl_Writer.Write()");
        
        if (write_status != IFSelect_RetDone) {
            result.success = false;
            result.error_message = "Failed to write STEP file: " + file_path;
            return result;
        }
        
        result.metrics["file_path"] = file_path;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("write_step failed: ") + e.what();
    }
    
    return result;
}

} // namespace kernel
} // namespace praxis
