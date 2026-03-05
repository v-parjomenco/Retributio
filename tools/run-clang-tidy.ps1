param(
    # Применять авто-фиксы clang-tidy (массовые правки — осторожно)
    [switch]$Fix,

    # Ненулевой exit code при наличии warning/error
    [switch]$FailOnIssues,

    # Фильтр по слою: "engine", "atrapacielos" или пусто = все слои
    [string]$Layer = "",

    # Путь к include SFML
    [string]$SfmlInclude = "C:\dev\SFML-3.0.2-64\include",

    # Прогнать только на .cpp, путь которых содержит эту подстроку
    [string]$SingleFile = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path([string]$Path, [string]$What) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Не найдено: $What => $Path"
    }
}

function Find-CompilationDbDir([string[]]$Candidates) {
    foreach ($dir in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($dir)) { continue }
        $cc = Join-Path $dir "compile_commands.json"
        if (Test-Path -LiteralPath $cc) { return $dir }
    }
    return $null
}

function Measure-Issues([string[]]$Lines) {
    $warn = 0; $err = 0
    foreach ($l in $Lines) {
        if ($l -match ":\d+:\d+:\s+warning:") { $warn++ }
        elseif ($l -match ":\d+:\d+:\s+error:") { $err++ }
    }
    return [pscustomobject]@{ Warnings = $warn; Errors = $err }
}

# tools/ -> repo root
$repoRoot       = Split-Path -Parent $PSScriptRoot
$engineSrc      = Join-Path $repoRoot "engine\src"
$engineInclude  = Join-Path $repoRoot "engine\include"
$atrSrc         = Join-Path $repoRoot "games\atrapacielos\src"
$atrInclude     = Join-Path $repoRoot "games\atrapacielos\include"
$thirdParty     = Join-Path $repoRoot "third_party"

Require-Path $engineSrc     "engine src"
Require-Path $engineInclude "engine include"
Require-Path $thirdParty    "third_party"
Require-Path $SfmlInclude   "SFML include"

# Определяем набор директорий для сканирования
$scanDirs = @()
if ($Layer -eq "" -or $Layer -eq "engine") {
    $scanDirs += $engineSrc
}
if ($Layer -eq "" -or $Layer -eq "atrapacielos") {
    if (Test-Path -LiteralPath $atrSrc) {
        $scanDirs += $atrSrc
    }
}

if ($scanDirs.Count -eq 0) {
    throw "Нет директорий для сканирования. Проверь параметр -Layer."
}

$clangTidyCmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
if ($null -eq $clangTidyCmd) {
    throw "clang-tidy не найден в PATH."
}

Write-Host "Repo root:   $repoRoot"
Write-Host "Scan dirs:   $($scanDirs -join ', ')"
Write-Host "SFML:        $SfmlInclude"
Write-Host "clang-tidy:  $($clangTidyCmd.Source)"
Write-Host ""
& $clangTidyCmd.Source --version | ForEach-Object { Write-Host $_ }
Write-Host ""

$ccCandidates = @(
    $repoRoot,
    (Join-Path $repoRoot "build"),
    (Join-Path $repoRoot "out"),
    (Join-Path $repoRoot "out\build"),
    (Join-Path $repoRoot "cmake-build"),
    (Join-Path $repoRoot "cmake-build-debug"),
    (Join-Path $repoRoot "cmake-build-release")
)

$compDbDir = Find-CompilationDbDir $ccCandidates
if ($null -ne $compDbDir) {
    Write-Host "Compilation database: $compDbDir" -ForegroundColor Green
} else {
    Write-Host "Compilation database не найден (fallback режим)." -ForegroundColor Yellow
}
Write-Host ""

$tidyOptions = @(
    "-extra-arg=-std=c++20"

    "-extra-arg=-I"
    "-extra-arg=$engineInclude"

    "-extra-arg=-isystem"
    "-extra-arg=$SfmlInclude"

    "-extra-arg=-isystem"
    "-extra-arg=$thirdParty"

    # Предупреждения clang-tidy, а не clang frontend
    "-extra-arg=-Wno-everything"
)

# Добавляем include игры если сканируем её слой
if ($Layer -eq "" -or $Layer -eq "atrapacielos") {
    if (Test-Path -LiteralPath $atrInclude) {
        $tidyOptions += "-extra-arg=-I"
        $tidyOptions += "-extra-arg=$atrInclude"
    }
}

if ($null -ne $compDbDir) {
    $tidyOptions += "-p"
    $tidyOptions += $compDbDir
}

if ($Fix) {
    $tidyOptions += "-fix"
    $tidyOptions += "-format-style=file"
}

$noisePatterns = @(
    "Error while trying to load a compilation database",
    "Could not auto-detect compilation database",
    "No compilation database found in",
    "Running without flags\.",
    "fixed-compilation-database",
    "json-compilation-database",
    "warnings generated",
    "Suppressed \d+ warnings",
    "Use -header-filter=.*",
    "Use -system-headers to display errors from system headers as well\."
)

function Filter-Noise([string[]]$Lines) {
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($l in $Lines) {
        $isNoise = $false
        foreach ($p in $noisePatterns) {
            if ($l -match $p) { $isNoise = $true; break }
        }
        if (-not $isNoise) { $out.Add($l) }
    }
    return $out.ToArray()
}

# Собираем .cpp из всех scan dirs
$files = @()
foreach ($dir in $scanDirs) {
    $files += Get-ChildItem -LiteralPath $dir -Recurse -File -Filter "*.cpp"
}
$files = $files | Sort-Object FullName

if (-not [string]::IsNullOrWhiteSpace($SingleFile)) {
    $needle = [regex]::Escape($SingleFile)
    $files = $files | Where-Object { $_.FullName -match $needle } | Sort-Object FullName
}

if ($files.Count -eq 0) {
    Write-Host "Не найдено ни одного .cpp." -ForegroundColor Yellow
    exit 0
}

Write-Host "Файлов .cpp: $($files.Count)"
Write-Host ""

$totalWarnings   = 0
$totalErrors     = 0
$filesWithIssues = New-Object System.Collections.Generic.List[string]
$failedFiles     = New-Object System.Collections.Generic.List[string]

function Run-Tidy-OnFile([string]$filePath) {
    $relative = $filePath.Substring($repoRoot.Length + 1)
    Write-Host "=== clang-tidy: $relative ===" -ForegroundColor Cyan

    try {
        $raw      = & $clangTidyCmd.Source $filePath @tidyOptions 2>&1
        $lines    = @()
        if ($null -ne $raw) { $lines = $raw | ForEach-Object { "$_" } }
        $filtered = Filter-Noise $lines
        foreach ($l in $filtered) { Write-Host $l }
        $m = Measure-Issues $filtered
        return [pscustomobject]@{
            File = $relative; Warnings = $m.Warnings; Errors = $m.Errors; Failed = $false
        }
    }
    catch {
        Write-Host $_ -ForegroundColor Red
        return [pscustomobject]@{
            File = $relative; Warnings = 0; Errors = 0; Failed = $true
        }
    }
}

foreach ($f in $files) {
    $r = Run-Tidy-OnFile $f.FullName
    $totalWarnings += $r.Warnings
    $totalErrors   += $r.Errors
    if ($r.Failed) { $failedFiles.Add($r.File) }
    elseif ($r.Warnings -gt 0 -or $r.Errors -gt 0) { $filesWithIssues.Add($r.File) }
    Write-Host ""
}

Write-Host "==================== SUMMARY ====================" -ForegroundColor White
Write-Host "Files processed:  $($files.Count)"
Write-Host "Files failed:     $($failedFiles.Count)"
Write-Host "Files w/ issues:  $($filesWithIssues.Count)"
Write-Host "Total warnings:   $totalWarnings"
Write-Host "Total errors:     $totalErrors"
Write-Host "=================================================" -ForegroundColor White

if ($failedFiles.Count -gt 0) {
    Write-Host "`nFAILED FILES:" -ForegroundColor Red
    $failedFiles | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
}

if ($filesWithIssues.Count -gt 0) {
    Write-Host "`nFILES WITH WARN/ERR:" -ForegroundColor Yellow
    $filesWithIssues | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
}

if ($FailOnIssues -and (($totalWarnings + $totalErrors) -gt 0 -or $failedFiles.Count -gt 0)) {
    exit 2
}

exit 0