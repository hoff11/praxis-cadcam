# ==============================
# Praxis Backup Script (HARDENED)
# Excludes all build artifacts and test outputs
# ==============================

$timestamp = Get-Date -Format "yyyy-MM-dd-HHmmss"
# Use the script location so backups work regardless of current shell directory.
$source    = (Resolve-Path $PSScriptRoot).Path

# Put backups OUTSIDE the repo to avoid recursive copying
$backupDir = Join-Path (Split-Path $source -Parent) "_Praxis-cadcam-Backups"
$dest      = Join-Path $backupDir "Praxis-cadcam-backup-$timestamp"

New-Item -ItemType Directory -Force -Path $backupDir | Out-Null

# ------------------------------
# PATH-SPECIFIC EXCLUDES
# Directories that should never be backed up
# ------------------------------
$excludePaths = @(
    # Version control and IDE
    (Join-Path $source ".git"),
    (Join-Path $source ".idea"),
    (Join-Path $source ".vscode"),
    (Join-Path $source ".vs"),
    
    # Package managers and dependencies
    (Join-Path $source ".pnpm-store"),
    (Join-Path $source "node_modules"),
    
    # Build directories
    (Join-Path $source "engine\build"),
    (Join-Path $source "engine\_build"),
    (Join-Path $source "engine\out"),
    (Join-Path $source "build"),
    (Join-Path $source "dist"),
    (Join-Path $source ".next"),
    (Join-Path $source ".turbo"),
    
    # Cache directories
    (Join-Path $source "engine\.cache"),
    (Join-Path $source "cache"),
    (Join-Path $source "coverage"),
    
    # Temporary and test output
    (Join-Path $source "tmp"),
    (Join-Path $source "scratch"),
    
    # Engine root CMake artifacts (from in-source builds)
    (Join-Path $source "engine\CMakeFiles"),
    (Join-Path $source "engine\_deps"),
    
    # Engine test output directories (pattern-based)
    (Join-Path $source "engine\test_cache_dir"),
    (Join-Path $source "engine\test_cache_flags_dir"),
    (Join-Path $source "engine\test_perbody"),
    (Join-Path $source "engine\test_quick"),
    (Join-Path $source "engine\test_v2"),
    (Join-Path $source "engine\test_iteration_order")
)

# Add any test_cache_* and test_run* directories dynamically
Get-ChildItem -Path (Join-Path $source "engine") -Directory -Filter "test_cache_*" -ErrorAction SilentlyContinue | ForEach-Object {
    $excludePaths += $_.FullName
}
Get-ChildItem -Path (Join-Path $source "engine") -Directory -Filter "test_run*" -ErrorAction SilentlyContinue | ForEach-Object {
    $excludePaths += $_.FullName
}
Get-ChildItem -Path (Join-Path $source "engine") -Directory -Filter "test_*_out*" -ErrorAction SilentlyContinue | ForEach-Object {
    $excludePaths += $_.FullName
}

# Convert excludes to robocopy arguments
$excludeArgs = @()
foreach ($path in $excludePaths) {
    if (Test-Path $path) {
        $excludeArgs += "/XD"
        $excludeArgs += $path
    }
}

# ------------------------------
# FILE PATTERN EXCLUDES
# Files that should never be backed up
# ------------------------------
$excludeFiles = @(
    # Compiled binaries and object files
    "*.obj", "*.o", "*.pdb", "*.ilk", "*.exe", "*.dll", "*.so", "*.dylib", "*.a",
    
    # CMake/Make artifacts in engine root
    "CMakeCache.txt", "Makefile", "cmake_install.cmake", "CTestTestfile.cmake",
    
    # Compiled engine binary (regenerated on build)
    "praxis-cad",
    
    # Dependency files
    "*.d", "*.o.d",
    
    # Backup files and temp files
    "*~", "*.swp", "*.bak", "*.tmp",
    
    # OS-specific
    "Thumbs.db", ".DS_Store"
)

# ------------------------------
# RUN ROBOCOPY
# ------------------------------
Write-Host "Starting backup: $dest"
Write-Host "Excluding $($excludePaths.Count) directories and $($excludeFiles.Count) file patterns..."

robocopy `
    $source `
    $dest `
    /MIR `
    /R:2 `
    /W:1 `
    /NFL `
    /NDL `
    /NP `
    /XJ `
    /XF $excludeFiles `
    @excludeArgs

$rc = $LASTEXITCODE

# Robocopy "success" is 0..7. 8+ indicates failure.
if ($rc -ge 8) {
    throw "Robocopy failed with exit code $rc"
}

# ------------------------------
# VERIFY NO BUILD ARTIFACTS
# ------------------------------
Write-Host "Verifying backup integrity..."

$buildArtifacts = @(
    "CMakeCache.txt",
    "CMakeFiles",
    "praxis-cad",
    "_deps",
    "node_modules",
    ".git"
)

$foundArtifacts = @()
foreach ($artifact in $buildArtifacts) {
    $searchPath = Join-Path $dest $artifact
    if (Test-Path $searchPath) {
        $foundArtifacts += $artifact
    }
    
    # Check in engine subdirectory too
    $engineSearchPath = Join-Path $dest "engine\$artifact"
    if (Test-Path $engineSearchPath) {
        $foundArtifacts += "engine\$artifact"
    }
}

if ($foundArtifacts.Count -gt 0) {
    Write-Warning "⚠️  Build artifacts found in backup:"
    $foundArtifacts | ForEach-Object { Write-Warning "   - $_" }
    Write-Warning "Backup may be larger than necessary."
}

# ------------------------------
# ZIP IT
# ------------------------------
$zipPath = "$dest.zip"

if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Write-Host "Creating compressed archive..."

# Zip contents so the ZIP root is the repo snapshot (not a nested folder)
Compress-Archive -Path (Join-Path $dest "*") -DestinationPath $zipPath

# Optional: remove unzipped backup after zip
Remove-Item $dest -Recurse -Force

$zipSize = (Get-Item $zipPath).Length / 1MB
Write-Host ""
Write-Host "Backup complete:"
Write-Host "   Archive: $zipPath"
Write-Host "   Size: $([math]::Round($zipSize, 2)) MB"
Write-Host ""
