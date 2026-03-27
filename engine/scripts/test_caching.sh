#!/bin/bash
# Sprint 6 Phase 3 Step 5: End-to-End Cache Tests
# Validates that op-level and plan-level caching work correctly

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ENGINE_DIR/build"
SCRATCH_DIR="$ENGINE_DIR/../scratch"
PRAXIS_BIN="$BUILD_DIR/praxis-cad"
VMC_RECIPE="$ENGINE_DIR/../recipes/vmc/vmc_v0.json"

echo "===================================="
echo "Praxis-CAD Cache Validation Tests"
echo "===================================="
echo ""

# Check if binary exists
if [ ! -f "$PRAXIS_BIN" ]; then
    echo "❌ Binary not found: $PRAXIS_BIN"
    echo "Run: cd $BUILD_DIR && cmake .. && ninja"
    exit 1
fi

# Set PRAXIS_RECIPE_PATH to vmc recipe
export PRAXIS_RECIPE_PATH="$VMC_RECIPE"

# Create fresh test directory
TEST_DIR="$SCRATCH_DIR/cache_test"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

# Test counters
total=0
passed=0

# Helper: Parse numeric JSON field from report with validation
# Only works for numeric fields (executed_count, reused_count, cache_hit_rate)
parse_json_number_field() {
    local json_file=$1
    local field=$2
    
    # Count occurrences of field
    local count=$(grep -c "\"$field\":" "$json_file" 2>/dev/null || echo "0")
    
    if [ "$count" -eq 0 ]; then
        echo "ERROR: Field '$field' not found in $json_file" >&2
        return 1
    elif [ "$count" -gt 1 ]; then
        echo "ERROR: Field '$field' appears $count times in $json_file (expected exactly 1)" >&2
        return 1
    fi
    
    # Extract value (works for numeric fields)
    grep "\"$field\":" "$json_file" | sed 's/.*: \([0-9.]*\).*/\1/'
}

echo "Test 1: First run with --clear-cache (cold start)"
echo "------------------------------------------------"
total=$((total + 1))

RUN1_DIR="$TEST_DIR/run1"
mkdir -p "$RUN1_DIR"

# Run with --clear-cache to ensure clean state
"$PRAXIS_BIN" --intent GenerateMachine3AxisVMC \
    --param travel_x=800 --param travel_y=600 --param travel_z=500 \
    --out "$RUN1_DIR" \
    --clear-cache > "$RUN1_DIR/output.log" 2>&1

if [ ! -f "$RUN1_DIR/report.json" ]; then
    echo "  ❌ FAIL: report.json not created"
else
    executed=$(parse_json_number_field "$RUN1_DIR/report.json" "executed_count")
    reused=$(parse_json_number_field "$RUN1_DIR/report.json" "reused_count")
    
    if [ -z "$executed" ] || [ "$executed" -eq 0 ]; then
        echo "  ❌ FAIL: Expected executed_count > 0, got: $executed"
        echo "     (First run should execute all nodes)"
    elif [ "$reused" -ne 0 ]; then
        echo "  ❌ FAIL: Expected reused_count = 0, got: $reused"
        echo "     (First run should have empty cache)"
    else
        echo "  ✓ PASS: First run executed $executed nodes, 0 reused (cold start)"
        passed=$((passed + 1))
    fi
fi

echo ""
echo "Test 2: Second run with identical inputs (warm start)"
echo "------------------------------------------------------"
total=$((total + 1))

RUN2_DIR="$TEST_DIR/run2"
mkdir -p "$RUN2_DIR"

# Run again with same params (should hit plan cache)
"$PRAXIS_BIN" --intent GenerateMachine3AxisVMC \
    --param travel_x=800 --param travel_y=600 --param travel_z=500 \
    --out "$RUN2_DIR" > "$RUN2_DIR/output.log" 2>&1

if [ ! -f "$RUN2_DIR/report.json" ]; then
    echo "  ❌ FAIL: report.json not created"
else
    cache_hit=$(grep '"hit": true' "$RUN2_DIR/report.json" | wc -l)
    executed=$(parse_json_number_field "$RUN2_DIR/report.json" "executed_count")
    reused=$(parse_json_number_field "$RUN2_DIR/report.json" "reused_count")
    
    if [ "$cache_hit" -gt 0 ]; then
        # Plan cache hit - fastest path
        if [ "$executed" -ne 0 ] || [ "$reused" -ne 0 ]; then
            echo "  ❌ FAIL: Plan cache hit but op_cache shows activity"
            echo "     executed=$executed, reused=$reused (both should be 0)"
        else
            echo "  ✓ PASS: Plan cache hit, no execution needed (fastest path)"
            passed=$((passed + 1))
        fi
    else
        # Plan cache miss (shouldn't happen but validate op cache worked)
        if [ "$reused" -eq 0 ]; then
            echo "  ❌ FAIL: No plan cache hit and no op reuse"
            echo "     executed=$executed, reused=$reused"
        else
            echo "  ✓ PASS: Plan cache miss but op cache reused $reused nodes"
            passed=$((passed + 1))
        fi
    fi
fi

echo ""
echo "Test 3: Selective invalidation (parameter change)"
echo "--------------------------------------------------"
total=$((total + 1))

RUN3_DIR="$TEST_DIR/run3"
mkdir -p "$RUN3_DIR"

# Run with slightly different param to test selective invalidation
"$PRAXIS_BIN" --intent GenerateMachine3AxisVMC \
    --param travel_x=805 --param travel_y=600 --param travel_z=500 \
    --out "$RUN3_DIR" > "$RUN3_DIR/output.log" 2>&1

if [ ! -f "$RUN3_DIR/report.json" ]; then
    echo "  ❌ FAIL: report.json not created"
else
    executed=$(parse_json_number_field "$RUN3_DIR/report.json" "executed_count")
    reused=$(parse_json_number_field "$RUN3_DIR/report.json" "reused_count")
    hit_rate=$(parse_json_number_field "$RUN3_DIR/report.json" "cache_hit_rate")
    
    if [ -z "$executed" ] || [ -z "$reused" ]; then
        echo "  ❌ FAIL: Could not parse op_cache stats"
    elif [ "$executed" -eq 0 ]; then
        echo "  ❌ FAIL: Expected some nodes to execute (param changed)"
        echo "     executed=$executed, reused=$reused"
    elif [ "$reused" -eq 0 ]; then
        echo "  ❌ FAIL: Expected some nodes to be reused (independent ops)"
        echo "     executed=$executed, reused=$reused"
    else
        echo "  ✓ PASS: Selective invalidation working"
        echo "     executed=$executed, reused=$reused, hit_rate=$hit_rate"
        passed=$((passed + 1))
    fi
fi

echo ""
echo "Test 4: --no-cache flag disables caching"
echo "-----------------------------------------"
total=$((total + 1))

RUN4_DIR="$TEST_DIR/run4"
mkdir -p "$RUN4_DIR"

# Run with --no-cache (should execute everything)
"$PRAXIS_BIN" --intent GenerateMachine3AxisVMC \
    --param travel_x=800 --param travel_y=600 --param travel_z=500 \
    --out "$RUN4_DIR" \
    --no-cache > "$RUN4_DIR/output.log" 2>&1

if [ ! -f "$RUN4_DIR/report.json" ]; then
    echo "  ❌ FAIL: report.json not created"
else
    executed=$(parse_json_number_field "$RUN4_DIR/report.json" "executed_count")
    reused=$(parse_json_number_field "$RUN4_DIR/report.json" "reused_count")
    cache_hit=$(grep '"hit": true' "$RUN4_DIR/report.json" | wc -l)
    
    if [ "$cache_hit" -gt 0 ]; then
        echo "  ❌ FAIL: --no-cache flag ignored (plan cache hit)"
    elif [ "$reused" -ne 0 ]; then
        echo "  ❌ FAIL: --no-cache flag ignored (op cache reused $reused)"
    elif [ -z "$executed" ] || [ "$executed" -eq 0 ]; then
        echo "  ❌ FAIL: Expected all nodes to execute with --no-cache"
    else
        echo "  ✓ PASS: --no-cache disabled caching, executed all $executed nodes"
        passed=$((passed + 1))
    fi
fi

echo ""
echo "Test 5: Cache persists across runs"
echo "-----------------------------------"
total=$((total + 1))

RUN5_DIR="$TEST_DIR/run5"
mkdir -p "$RUN5_DIR"

# Run yet again with original params (should still hit cache)
"$PRAXIS_BIN" --intent GenerateMachine3AxisVMC \
    --param travel_x=800 --param travel_y=600 --param travel_z=500 \
    --out "$RUN5_DIR" > "$RUN5_DIR/output.log" 2>&1

if [ ! -f "$RUN5_DIR/report.json" ]; then
    echo "  ❌ FAIL: report.json not created"
else
    cache_hit=$(grep '"hit": true' "$RUN5_DIR/report.json" | wc -l)
    executed=$(parse_json_number_field "$RUN5_DIR/report.json" "executed_count")
    
    if [ "$cache_hit" -eq 0 ] && [ ! -z "$executed" ] && [ "$executed" -gt 0 ]; then
        echo "  ❌ FAIL: Cache did not persist (executed $executed nodes again)"
    else
        echo "  ✓ PASS: Cache persisted across multiple runs"
        passed=$((passed + 1))
    fi
fi

echo ""
echo "===================================="
echo "Cache Validation Results"
echo "===================================="
echo "Tests passed: $passed/$total"
echo ""

if [ "$passed" -eq "$total" ]; then
    echo "✓ All cache tests passed!"
    exit 0
else
    echo "❌ Some cache tests failed"
    exit 1
fi
