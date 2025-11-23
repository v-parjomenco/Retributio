# ============================================================================
# File: tools/check_layering.ps1
# Purpose: Verify layering rules between core/ and game/ in SFML1 project
# Rules:
#   - core/* (и pch.h) не имеют права зависеть от game/* (ни прямых include, ни namespace)
#   - game/* может зависеть от core/* (это нормально)
# Usage:
#   - Запускать из корня репозитория SFML1:
#       .\tools\check_layering.ps1
# ============================================================================

param (
    [switch]$VerboseOutput
)

$ErrorActionPreference = "Stop"

# Определяем корень репозитория как родитель папки tools/
$repoRoot   = Split-Path -Parent $PSScriptRoot
$projectRoot = Join-Path $repoRoot "SFML1"

Write-Host "Repo root:     $repoRoot"
Write-Host "Project root:  $projectRoot"
Write-Host ""

# Пути, которые считаются "core-слоем"
$corePaths = @(
    (Join-Path $projectRoot "include\core"),
    (Join-Path $projectRoot "src\core"),
    (Join-Path $projectRoot "include\pch.h")
)

if ($VerboseOutput) {
    Write-Host "Core paths:"
    $corePaths | ForEach-Object { Write-Host "  $_" }
    Write-Host ""
}

# Паттерны, которые запрещены в core-слое
$patterns = @(
    '#\s*include\s*["<]\s*game/',
    '#\s*include\s*["<]\s*game\\',
    'game::skyguard'
)

$hasViolations = $false

foreach ($pattern in $patterns) {
    if ($VerboseOutput) {
        Write-Host "Checking pattern: $pattern"
    }

    $matches = Get-ChildItem -Recurse -Path $corePaths -Include *.h,*.hpp,*.inl,*.cpp -ErrorAction SilentlyContinue |
               Select-String -Pattern $pattern -ErrorAction SilentlyContinue

    if ($matches) {
        $hasViolations = $true
        Write-Host ""
        Write-Host "===============================================" -ForegroundColor Red
        Write-Host " LAYERING VIOLATION: pattern '$pattern' found" -ForegroundColor Red
        Write-Host "===============================================" -ForegroundColor Red

        $matches | ForEach-Object {
            Write-Host ("{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim()) -ForegroundColor Red
        }
    }
}

if (-not $hasViolations) {
    Write-Host ""
    Write-Host "Layering check passed: core/* does not depend on game/*." -ForegroundColor Green
    exit 0
}

Write-Host ""
Write-Host "Layering check FAILED. See violations above." -ForegroundColor Red
exit 1
