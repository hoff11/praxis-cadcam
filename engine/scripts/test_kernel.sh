#!/bin/bash

# Run Layer 1 kernel tests

cd "$(dirname "$0")/.."

echo "========================================"
echo "Building Kernel Tests"
echo "========================================"

# Build test executable
cd build
cmake ..
ninja test_kernel_ops

if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

echo ""
echo "========================================"
echo "Running Kernel Tests"
echo "========================================"
echo ""

# Run tests
./test_kernel_ops
TEST_RESULT=$?

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo "✅ Layer 1 kernel tests passed"
else
    echo "❌ Layer 1 kernel tests failed"
fi

exit $TEST_RESULT

    std::cout << "  Operations tracked: make_box=" << metrics["make_box"] 
              << ", validate=" << metrics["validate"]
              << ", get_bounding_box=" << metrics["get_bounding_box"] << std::endl;
    
    return 0;
}
EOF

echo "  Compiling test..."
g++ /tmp/test_kernel.cpp \
    -I./include \
    -I/usr/include/oce \
    -L/usr/lib/x86_64-linux-gnu \
    -lTKernel -lTKMath -lTKG3d -lTKBRep -lTKTopAlgo -lTKPrim \
    -std=c++17 \
    -o /tmp/test_kernel 2>&1 | head -20

if [ -f /tmp/test_kernel ]; then
    echo "  Running test..."
    /tmp/test_kernel
    TEST_RESULT=$?
    rm /tmp/test_kernel /tmp/test_kernel.cpp
    exit $TEST_RESULT
else
    echo "  FAIL: Compilation failed"
    rm /tmp/test_kernel.cpp
    exit 1
fi
