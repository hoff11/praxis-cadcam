#!/usr/bin/env pwsh
# Stable ID Referential Integrity Test (v2.0 - structured fields + symmetry)
# Validates that cross-references point to existing entities using:
# - Structured JSON fields (faces[], vertices[], edges[]) - preferred
# - Legacy stable_id suffix parsing - fallback for backwards compatibility
# - Bidirectional symmetry: edge↔vertex relationships must be consistent
# Exit code 0 = PASS, 1 = FAIL

param(
    [Parameter(Mandatory=$true)]
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

Write-Host "`n=== STABLE_ID REFERENTIAL INTEGRITY TEST (v2.0) ===" -ForegroundColor Cyan
Write-Host "Report: $ReportPath`n"

if (-not (Test-Path $ReportPath)) {
    Write-Host "✗ FAIL: Report file not found" -ForegroundColor Red
    exit 1
}

$json = Get-Content $ReportPath | ConvertFrom-Json
$sel = $json.interaction.selectables

# ============================================================================
# Helper Functions: Parse stable_id prefixes (identity only)
# ============================================================================
function Get-EdgeIndex($sid)   { if ($sid -match '^edge:(\d+)')   { return [int]$matches[1] } return $null }
function Get-VertIndex($sid)   { if ($sid -match '^vertex:(\d+)') { return [int]$matches[1] } return $null }
function Get-BodyIndex($sid)   { if ($sid -match '^body:(\d+)$')  { return [int]$matches[1] } return $null }
function Get-FaceKey($sid)     { if ($sid -match '^face:(\d+):(\d+)$') { return "$($matches[1]):$($matches[2])" } return $null }

# ============================================================================
# Helper Functions: Get adjacency (structured fields first, legacy fallback)
# ============================================================================
function Get-EdgeFaces($edgeItem) {
    # Structured: faces: [{body:<b>, face:<f>}, ...]
    if ($null -ne $edgeItem.faces -and $edgeItem.faces.Count -gt 0) {
        return @($edgeItem.faces | ForEach-Object { "$($_.body):$($_.face)" })
    }

    # Legacy: edge:E:faces:b0:f0,b1:f1
    if ($edgeItem.stable_id -match '^edge:\d+:faces:(.+)$') {
        return @($matches[1] -split ',' | ForEach-Object { $_.Trim() })
    }

    return @()
}

function Get-EdgeVertices($edgeItem) {
    # Structured: vertices: [v1, v2]
    if ($null -ne $edgeItem.vertices -and $edgeItem.vertices.Count -gt 0) {
        return @($edgeItem.vertices | ForEach-Object { [int]$_ })
    }

    # Legacy: edge:E:vertices:v1,v2 (rarely used)
    if ($edgeItem.stable_id -match ':vertices:(.+)$') {
        return @($matches[1] -split ',' | ForEach-Object { [int]($_.Trim()) })
    }

    return @()
}

function Get-VertexEdges($vertexItem) {
    # Structured: edges: [e1,e2,e3]
    if ($null -ne $vertexItem.edges -and $vertexItem.edges.Count -gt 0) {
        return @($vertexItem.edges | ForEach-Object { [int]$_ })
    }

    # Legacy: vertex:V:edges:e1,e2,e3
    if ($vertexItem.stable_id -match '^vertex:\d+:edges:(.+)$') {
        return @($matches[1] -split ',' | ForEach-Object { [int]($_.Trim()) })
    }

    return @()
}

# ============================================================================
# Build index sets for fast lookup
# ============================================================================
$edgeIndices = @{}
$faceKeys = @{}  # Key = "bodyIdx:localFaceIdx" since faces are per-body
$bodyIndices = @{}
$vertexIndices = @{}
$facesPerBody = @{}  # Track face indices per body for continuity checks

foreach ($item in $sel) {
    $sid = $item.stable_id
    switch ($item.ref_type) {
        "EdgeRef" {
            $e = Get-EdgeIndex $sid
            if ($null -ne $e) { $edgeIndices[$e] = $true }
        }
        "FaceRef" {
            $fk = Get-FaceKey $sid
            if ($null -ne $fk) {
                $faceKeys[$fk] = $true
                if ($sid -match '^face:(\d+):(\d+)$') {
                    $bodyIdx = [int]$matches[1]
                    $localFaceIdx = [int]$matches[2]
                    if (-not $facesPerBody.ContainsKey($bodyIdx)) {
                        $facesPerBody[$bodyIdx] = @{}
                    }
                    $facesPerBody[$bodyIdx][$localFaceIdx] = $true
                }
            }
        }
        "BodyRef" {
            $b = Get-BodyIndex $sid
            if ($null -ne $b) { $bodyIndices[$b] = $true }
        }
        "VertexRef" {
            $v = Get-VertIndex $sid
            if ($null -ne $v) { $vertexIndices[$v] = $true }
        }
    }
}

Write-Host "Entity counts:"
Write-Host "  Bodies:   $($bodyIndices.Count)"
Write-Host "  Faces:    $($faceKeys.Count)"
Write-Host "  Edges:    $($edgeIndices.Count)"
Write-Host "  Vertices: $($vertexIndices.Count)"
Write-Host ""

$errors = 0

# ============================================================================
# Validate EdgeRef -> FaceRef references (structured + legacy)
# ============================================================================
Write-Host "Checking EdgeRef -> FaceRef references..." -NoNewline
$edgeRefs = $sel | Where-Object {$_.ref_type -eq "EdgeRef"}
foreach ($edge in $edgeRefs) {
    $edgeFaces = Get-EdgeFaces $edge
    foreach ($fk in $edgeFaces) {
        if (-not $faceKeys.ContainsKey($fk)) {
            Write-Host ""
            Write-Host "  X $($edge.stable_id) references missing face:$fk" -ForegroundColor Red
            $errors++
        }
    }
}
if ($errors -eq 0) {
    Write-Host " OK" -ForegroundColor Green
}

# ============================================================================
# Validate VertexRef -> EdgeRef references (structured + legacy)
# ============================================================================
Write-Host "Checking VertexRef -> EdgeRef references..." -NoNewline
$vertexRefs = $sel | Where-Object {$_.ref_type -eq "VertexRef"}
foreach ($vertex in $vertexRefs) {
    $vEdges = Get-VertexEdges $vertex
    foreach ($e in $vEdges) {
        if (-not $edgeIndices.ContainsKey($e)) {
            Write-Host ""
            Write-Host "  X $($vertex.stable_id) references missing edge:$e" -ForegroundColor Red
            $errors++
        }
    }
}
if ($errors -eq 0) {
    Write-Host " OK" -ForegroundColor Green
}

# ============================================================================
# Validate FaceRef -> BodyRef references
# ============================================================================
Write-Host "Checking FaceRef -> BodyRef references..." -NoNewline
$faceRefs = $sel | Where-Object {$_.ref_type -eq "FaceRef"}
foreach ($face in $faceRefs) {
    if ($face.stable_id -match '^face:(\d+):(\d+)$') {
        $bodyIdx = [int]$matches[1]
        if (-not $bodyIndices.ContainsKey($bodyIdx)) {
            Write-Host ""
            Write-Host "  X $($face.stable_id) references missing body:$bodyIdx" -ForegroundColor Red
            $errors++
        }
    }
}
if ($errors -eq 0) {
    Write-Host " OK" -ForegroundColor Green
}

# ============================================================================
# NEW: Bidirectional edge↔vertex symmetry validation
# ============================================================================
Write-Host "Checking EdgeRef <-> VertexRef symmetry..." -NoNewline

# Build maps
$edgeToVerts = @{}  # e -> set(v)
$vertToEdges = @{}  # v -> set(e)

$sel | Where-Object {$_.ref_type -eq "EdgeRef"} | ForEach-Object {
    $e = Get-EdgeIndex $_.stable_id
    if ($null -ne $e) {
        if (-not $edgeToVerts.ContainsKey($e)) { $edgeToVerts[$e] = @{} }
        foreach ($v in (Get-EdgeVertices $_)) { $edgeToVerts[$e][$v] = $true }
    }
}

$sel | Where-Object {$_.ref_type -eq "VertexRef"} | ForEach-Object {
    $v = Get-VertIndex $_.stable_id
    if ($null -ne $v) {
        if (-not $vertToEdges.ContainsKey($v)) { $vertToEdges[$v] = @{} }
        foreach ($e in (Get-VertexEdges $_)) { $vertToEdges[$v][$e] = $true }
    }
}

# v -> e -> v
foreach ($v in $vertToEdges.Keys) {
    foreach ($e in $vertToEdges[$v].Keys) {
        if (-not $edgeToVerts.ContainsKey($e) -or -not $edgeToVerts[$e].ContainsKey($v)) {
            Write-Host ""
            Write-Host "  X vertex:$v references edge:$e but edge does not reference vertex:$v" -ForegroundColor Red
            $errors++
        }
    }
}

# e -> v -> e
foreach ($e in $edgeToVerts.Keys) {
    foreach ($v in $edgeToVerts[$e].Keys) {
        if (-not $vertToEdges.ContainsKey($v) -or -not $vertToEdges[$v].ContainsKey($e)) {
            Write-Host ""
            Write-Host "  X edge:$e references vertex:$v but vertex does not reference edge:$e" -ForegroundColor Red
            $errors++
        }
    }
}

if ($errors -eq 0) {
    Write-Host " OK" -ForegroundColor Green
}

# ============================================================================
# Index continuity checks
# ============================================================================
Write-Host "`nChecking index continuity..."

# Bodies should be 0..N-1
$bodyMax = ($bodyIndices.Keys | Measure-Object -Maximum).Maximum
$expectedBodies = 0..$bodyMax
$missingBodies = $expectedBodies | Where-Object {-not $bodyIndices.ContainsKey($_)}
if ($missingBodies) {
    Write-Host "  X Missing body indices: $($missingBodies -join ',')" -ForegroundColor Red
    $errors++
} else {
    Write-Host "  Bodies: 0..$bodyMax (continuous)" -ForegroundColor Green
}

# Faces should be 0..N-1 PER BODY (per-body face indices)
foreach ($bodyIdx in ($bodyIndices.Keys | Sort-Object)) {
    if ($facesPerBody.ContainsKey($bodyIdx)) {
        $faceIndicesForBody = $facesPerBody[$bodyIdx].Keys
        $faceMax = ($faceIndicesForBody | Measure-Object -Maximum).Maximum
        $expectedFaces = 0..$faceMax
        $missingFaces = $expectedFaces | Where-Object {-not $facesPerBody[$bodyIdx].ContainsKey($_)}
        if ($missingFaces) {
            Write-Host "  X Body ${bodyIdx}: Missing face indices: $($missingFaces -join ',')" -ForegroundColor Red
            $errors++
        } else {
            Write-Host "  Body ${bodyIdx}: Faces 0..$faceMax (continuous)" -ForegroundColor Green
        }
    } else {
        Write-Host "  ! Body ${bodyIdx}: No faces found" -ForegroundColor Yellow
    }
}

# Edges should be 0..N-1
$edgeMax = ($edgeIndices.Keys | Measure-Object -Maximum).Maximum
$expectedEdges = 0..$edgeMax
$missingEdges = $expectedEdges | Where-Object {-not $edgeIndices.ContainsKey($_)}
if ($missingEdges) {
    Write-Host "  X Missing edge indices: $($missingEdges -join ',')" -ForegroundColor Red
    $errors++
} else {
    Write-Host "  Edges: 0..$edgeMax (continuous)" -ForegroundColor Green
}

# Vertices should be 0..N-1
$vertexMax = ($vertexIndices.Keys | Measure-Object -Maximum).Maximum
$expectedVertices = 0..$vertexMax
$missingVertices = $expectedVertices | Where-Object {-not $vertexIndices.ContainsKey($_)}
if ($missingVertices) {
    Write-Host "  X Missing vertex indices: $($missingVertices -join ',')" -ForegroundColor Red
    $errors++
} else {
    Write-Host "  Vertices: 0..$vertexMax (continuous)" -ForegroundColor Green
}

Write-Host ""
if ($errors -eq 0) {
    Write-Host "PASS: All referential integrity checks passed" -ForegroundColor Green
    exit 0
} else {
    Write-Host "FAIL: $errors integrity errors found" -ForegroundColor Red
    exit 1
}
