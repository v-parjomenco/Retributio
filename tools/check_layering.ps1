# =================================================================================================
# File: tools/check_layering.ps1
# Purpose: Verify layering rule — engine must not depend on game layer.
# Rule: engine/include/* and engine/src/* must not reference atrapacielos:: or
#       include game headers. games/* may depend on engine (allowed).
# Usage: Run from repo root: .\tools\check_layering.ps1
# =================================================================================================

param(
    [switch]$VerboseOutput
)

$ErrorActionPreference = "Stop"

$repoRoot   = Split-Path -Parent $PSScriptRoot
$engineRoot = Join-Path $repoRoot "engine"

Write-Host "Repo root:    $repoRoot"
Write-Host "Engine root:  $engineRoot"
Write-Host ""

$enginePaths = @(
    (Join-Path $engineRoot "include"),
    (Join-Path $engineRoot "src")
)

if ($VerboseOutput) {
    Write-Host "Проверяемые пути:"
    $enginePaths | ForEach-Object { Write-Host "  $_" }
    Write-Host ""
}

# Запрещены любые прямые зависимости от game-слоя в коде движка
$patterns = @(
    '#\s*include\s*["<]\s*atrapacielos/',
    '#\s*include\s*["<]\s*atrapacielos\\',
    'atrapacielos::'
)

$hasViolations = $false

foreach ($pattern in $patterns) {
    if ($VerboseOutput) {
        Write-Host "Проверяем паттерн: $pattern"
    }

    $matches = Get-ChildItem -Recurse -Path $enginePaths `
                   -Include *.h,*.hpp,*.inl,*.cpp -ErrorAction SilentlyContinue |
               Select-String -Pattern $pattern -ErrorAction SilentlyContinue

    if ($matches) {
        $hasViolations = $true
        Write-Host "" 
        Write-Host "================================================" -ForegroundColor Red
        Write-Host " LAYERING VIOLATION: pattern '$pattern' found"   -ForegroundColor Red
        Write-Host "================================================" -ForegroundColor Red
        $matches | ForEach-Object {
            Write-Host ("{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim()) `
                -ForegroundColor Red
        }
    }
}

if (-not $hasViolations) {
    Write-Host ""
    Write-Host "Layering check passed: engine/* does not depend on game/*." -ForegroundColor Green
    exit 0
}

Write-Host ""
Write-Host "Layering check FAILED. See violations above." -ForegroundColor Red
exit 1