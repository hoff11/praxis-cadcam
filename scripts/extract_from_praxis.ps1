param(
    [string]$PraxisRoot = "C:\dev\praxis",
    [string]$CadcamRoot = "C:\dev\praxis-cadcam"
)

$ErrorActionPreference = "Stop"

function Copy-Tree {
    param(
        [string]$Source,
        [string]$Dest
    )

    if (-not (Test-Path $Source)) {
        Write-Host "SKIP: source not found -> $Source"
        return
    }

    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
    robocopy $Source $Dest /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy failed copying $Source to $Dest (exit=$LASTEXITCODE)"
    }

    Write-Host "COPIED: $Source -> $Dest"
}

Write-Host "Extracting machine-truth core from $PraxisRoot to $CadcamRoot"

# Native CAD kernel (authoritative C++ core)
Copy-Tree -Source (Join-Path $PraxisRoot "engine") -Dest (Join-Path $CadcamRoot "engine")

# Machine-definition source assets
Copy-Tree -Source (Join-Path $PraxisRoot "recipes") -Dest (Join-Path $CadcamRoot "recipes")

# Machine and geometry domain packages (retain TS temporarily during transition)
Copy-Tree -Source (Join-Path $PraxisRoot "packages\machines") -Dest (Join-Path $CadcamRoot "packages\machines")
Copy-Tree -Source (Join-Path $PraxisRoot "packages\geometry") -Dest (Join-Path $CadcamRoot "packages\geometry")
Copy-Tree -Source (Join-Path $PraxisRoot "packages\planner") -Dest (Join-Path $CadcamRoot "packages\planner")
Copy-Tree -Source (Join-Path $PraxisRoot "packages\contracts") -Dest (Join-Path $CadcamRoot "packages\contracts")

Write-Host ""
Write-Host "Extraction complete."
Write-Host "Next:"
Write-Host "  1) Build native CLI in $CadcamRoot"
Write-Host "  2) Remove TS CLI dependency from workflows"
Write-Host "  3) Keep only machine-truth modules in praxis-cadcam"
