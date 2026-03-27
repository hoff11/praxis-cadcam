#!/usr/bin/env pwsh
# Stable ID Determinism Test
# Validates that stable_ids are identical across independent builds of the same geometry
# Exit code 0 = PASS, 1 = FAIL

param(
    [string]$RecipePath = "/mnt/c/dev/praxis/recipes/vmc/vmc_v0.json",
    [string]$OutputBase = "engine/test_determinism"
)

$ErrorActionPreference = "Stop"

Write-Host "`n=== STABLE_ID DETERMINISM TEST ===" -ForegroundColor Cyan
Write-Host "Testing: GenerateMachine3AxisVMC"
Write-Host "Generating two independent builds...`n"

# Run 1
$run1Dir = "$OutputBase`_run1"
Write-Host "[1/2] Build 1..." -NoNewline
wsl bash -c "cd /mnt/c/dev/praxis && rm -rf $run1Dir && PRAXIS_RECIPE_PATH=$RecipePath ./engine/build/praxis-cad --intent GenerateMachine3AxisVMC --out $run1Dir --no-cache" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host " FAILED" -ForegroundColor Red
    exit 1
}
Write-Host " OK" -ForegroundColor Green

# Run 2
$run2Dir = "$OutputBase`_run2"
Write-Host "[2/2] Build 2..." -NoNewline
wsl bash -c "cd /mnt/c/dev/praxis && rm -rf $run2Dir && PRAXIS_RECIPE_PATH=$RecipePath ./engine/build/praxis-cad --intent GenerateMachine3AxisVMC --out $run2Dir --no-cache" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host " FAILED" -ForegroundColor Red
    exit 1
}
Write-Host " OK" -ForegroundColor Green

# Load reports
$report1 = Join-Path $run1Dir "report.json"
$report2 = Join-Path $run2Dir "report.json"

Write-Host "`nVerifying report files..."
if (-not (Test-Path $report1)) {
    Write-Host "X FAIL: Missing report.json in run1 output: $run1Dir" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $report2)) {
    Write-Host "X FAIL: Missing report.json in run2 output: $run2Dir" -ForegroundColor Red
    exit 1
}

$j1 = Get-Content $report1 | ConvertFrom-Json
$j2 = Get-Content $report2 | ConvertFrom-Json

Write-Host "`nComparing stable_ids..."

# Compare each ref_type
$refTypes = @("BodyRef", "FaceRef", "EdgeRef", "VertexRef")
$allPass = $true

foreach ($refType in $refTypes) {
    Write-Host "  $refType..." -NoNewline
    $ids1 = ($j1.interaction.selectables | Where-Object {$_.ref_type -eq $refType} | ForEach-Object {$_.stable_id}) | Sort-Object
    $ids2 = ($j2.interaction.selectables | Where-Object {$_.ref_type -eq $refType} | ForEach-Object {$_.stable_id}) | Sort-Object
    
    $diff = Compare-Object $ids1 $ids2
    if ($diff) {
        Write-Host " DIFFER" -ForegroundColor Red
        $diff | ForEach-Object {
            if ($_.SideIndicator -eq "<=") {
                Write-Host "    - Only in run1: $($_.InputObject)" -ForegroundColor Red
            } else {
                Write-Host "    - Only in run2: $($_.InputObject)" -ForegroundColor Red
            }
        }
        $allPass = $false
    } else {
        Write-Host " IDENTICAL ($($ids1.Count) items)" -ForegroundColor Green
    }
}

Write-Host ""
if ($allPass) {
    Write-Host "PASS: All stable_ids are deterministic" -ForegroundColor Green
    exit 0
} else {
    Write-Host "FAIL: Stable IDs differ between builds" -ForegroundColor Red
    exit 1
}
