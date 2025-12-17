param(
    # Применять авто-фиксы clang-tidy (осторожно: это массовое форматирование/правки)
    [switch]$Fix,

    # Падать с ненулевым кодом, если найдены warning/error
    [switch]$FailOnIssues,

    # Кол-во параллельных задач (работает только в PowerShell 7+)
    [int]$Jobs = 0,

    # Путь к include SFML
    [string]$SfmlInclude = "C:\dev\SFML-3.0.2-64\include",

    # Имя подпапки проекта в репозитории (где лежат src/ и include/)
    [string]$ProjectFolderName = "SFML1",

     # Доп. include пути (если надо) — массив строк
     [string[]]$ExtraIncludes = @()

    # Если задано — прогоняем clang-tidy только на .cpp, путь которых содержит эту подстроку.
    # Примеры: -SingleFile "render_system.cpp" или -SingleFile "\src\core\ecs\"
    [string]$SingleFile = ""
)

 Set-StrictMode -Version Latest
 $ErrorActionPreference = "Stop"

if ($Jobs -gt 1) {
    throw "Parallel mode (-Jobs > 1) is not implemented yet. Run without -Jobs or set -Jobs 0/1."
}

function Require-Path([string]$Path, [string]$What) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Не найдено: $What => $Path"
    }
}

function Find-CompilationDbDir([string[]]$Candidates) {
    foreach ($dir in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($dir)) { continue }
        $cc = Join-Path $dir "compile_commands.json"
        if (Test-Path -LiteralPath $cc) {
            return $dir
        }
    }
    return $null
}

function Measure-Issues([string[]]$Lines) {
    $warn = 0
    $err = 0

    foreach ($l in $Lines) {
        if ($l -match ":\d+:\d+:\s+warning:") { $warn++ }
        elseif ($l -match ":\d+:\d+:\s+error:") { $err++ }
    }

    return [pscustomobject]@{
        Warnings = $warn
        Errors   = $err
    }
}

# tools/ -> repo root
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectDir = Join-Path $repoRoot $ProjectFolderName
$srcDir = Join-Path $projectDir "src"
$includeDir = Join-Path $projectDir "include"
$thirdPartyDir = Join-Path $includeDir "third_party"

Require-Path $projectDir "ProjectDir"
Require-Path $srcDir "SourceDir"
Require-Path $includeDir "IncludeDir"
Require-Path $thirdPartyDir "ThirdPartyDir"
Require-Path $SfmlInclude "SFML include"

$clangTidyCmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
if ($null -eq $clangTidyCmd) {
    throw "clang-tidy не найден в PATH. Проверь установку LLVM/Clang и переменную PATH."
}

Write-Host "Repo root:       $repoRoot"
Write-Host "Project dir:     $projectDir"
Write-Host "Source dir:      $srcDir"
Write-Host "Include dir:     $includeDir"
Write-Host "SFML include:    $SfmlInclude"
Write-Host "Third-party dir: $thirdPartyDir"
Write-Host "clang-tidy:      $($clangTidyCmd.Source)"
Write-Host ""

& $clangTidyCmd.Source --version | ForEach-Object { Write-Host $_ }
Write-Host ""

# Если вдруг где-то всё же лежит compile_commands.json — используем автоматически.
$ccCandidates = @(
    $repoRoot,
    $projectDir,
    (Join-Path $repoRoot "build"),
    (Join-Path $repoRoot "out"),
    (Join-Path $repoRoot "out\build"),
    (Join-Path $repoRoot "cmake-build"),
    (Join-Path $repoRoot "cmake-build-debug"),
    (Join-Path $repoRoot "cmake-build-release")
)

$compDbDir = Find-CompilationDbDir $ccCandidates
if ($null -ne $compDbDir) {
    Write-Host "Compilation database найден: $compDbDir" -ForegroundColor Green
} else {
    Write-Host "Compilation database НЕ найден. Работаем в fallback-режиме (без compile_commands.json)." -ForegroundColor Yellow
}
Write-Host ""

# ВАЖНО: header-filter должен матчиться по абсолютным путям.
# Мы разрешаем диагностику из:
#   SFML1/src/**
#   SFML1/include/core/**
#   SFML1/include/game/**
$headerFilter = ".*[\\/]$ProjectFolderName[\\/]src[\\/].*|.*[\\/]$ProjectFolderName[\\/]include[\\/](core|game)[\\/].*"

# Опции clang-tidy
$tidyOptions = @(
    "-header-filter=$headerFilter"

    "-extra-arg=-std=c++20"

    # include нашего проекта
    "-extra-arg=-I"
    "-extra-arg=$includeDir"

    # SFML и third_party как system headers
    "-extra-arg=-isystem"
    "-extra-arg=$SfmlInclude"

    "-extra-arg=-isystem"
    "-extra-arg=$thirdPartyDir"

    # Отключаем обычные clang warnings — оставляем clang-tidy
    "-extra-arg=-Wno-everything"
)

foreach ($inc in $ExtraIncludes) {
    if (-not [string]::IsNullOrWhiteSpace($inc)) {
        $tidyOptions += "-extra-arg=-I"
        $tidyOptions += "-extra-arg=$inc"
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

# Шум, который безопасно выкинуть (но НЕ режем реальные warning/error строки)
$noisePatterns = @(
    "Error while trying to load a compilation database"
    "Could not auto-detect compilation database"
    "No compilation database found in"
    "Running without flags\."
    "fixed-compilation-database"
    "json-compilation-database"
    "warnings generated"
    "Suppressed \d+ warnings"
    "Use -header-filter=.*"
    "Use -system-headers to display errors from system headers as well\."
)

function Filter-Noise([string[]]$Lines) {
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($l in $Lines) {
        $isNoise = $false
        foreach ($p in $noisePatterns) {
            if ($l -match $p) { $isNoise = $true; break }
        }
        if (-not $isNoise) {
            $out.Add($l)
        }
    }
    return $out.ToArray()
}

# Список файлов
 $files = Get-ChildItem -LiteralPath $srcDir -Recurse -File -Filter "*.cpp" |
     Sort-Object FullName

if (-not [string]::IsNullOrWhiteSpace($SingleFile)) {
    $needle = [regex]::Escape($SingleFile)
    $files = $files | Where-Object { $_.FullName -match $needle } | Sort-Object FullName
}

if ($files.Count -eq 0) {
    Write-Host "В $srcDir не найдено ни одного .cpp" -ForegroundColor Yellow
    exit 0
}

Write-Host "Файлов .cpp: $($files.Count)"
Write-Host ""

$totalWarnings = 0
$totalErrors = 0
$filesWithIssues = New-Object System.Collections.Generic.List[string]
$failedFiles = New-Object System.Collections.Generic.List[string]

function Run-Tidy-OnFile([string]$filePath) {
    $relative = $filePath.Substring($repoRoot.Length + 1)

    Write-Host "=== clang-tidy: $relative ===" -ForegroundColor Cyan

    try {
        $raw = & $clangTidyCmd.Source $filePath @tidyOptions 2>&1
        $lines = @()
        if ($null -ne $raw) { $lines = $raw | ForEach-Object { "$_" } }

        $filtered = Filter-Noise $lines

        foreach ($l in $filtered) {
            Write-Host $l
        }

        $m = Measure-Issues $filtered
        return [pscustomobject]@{
            File      = $relative
            Warnings  = $m.Warnings
            Errors    = $m.Errors
            Failed    = $false
        }
    }
    catch {
        Write-Host $_ -ForegroundColor Red
        return [pscustomobject]@{
            File      = $relative
            Warnings  = 0
            Errors    = 0
            Failed    = $true
        }
    }
}

$results = @()

# Последовательный прогон (надёжный и предсказуемый)
foreach ($f in $files) {
    $r = Run-Tidy-OnFile $f.FullName
    $results += $r

    $totalWarnings += $r.Warnings
    $totalErrors += $r.Errors

    if ($r.Failed) {
        $failedFiles.Add($r.File)
    } elseif ($r.Warnings -gt 0 -or $r.Errors -gt 0) {
        $filesWithIssues.Add($r.File)
    }

    Write-Host ""
}

Write-Host "==================== SUMMARY ====================" -ForegroundColor White
Write-Host "Files processed:   $($files.Count)"
Write-Host "Files failed:      $($failedFiles.Count)"
Write-Host "Files w/ issues:   $($filesWithIssues.Count)"
Write-Host "Total warnings:    $totalWarnings"
Write-Host "Total errors:      $totalErrors"
Write-Host "=================================================" -ForegroundColor White

if ($failedFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "FAILED FILES:" -ForegroundColor Red
    $failedFiles | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
}

if ($filesWithIssues.Count -gt 0) {
    Write-Host ""
    Write-Host "FILES WITH WARN/ERR:" -ForegroundColor Yellow
    $filesWithIssues | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
}

if ($FailOnIssues -and (($totalWarnings + $totalErrors) -gt 0 -or $failedFiles.Count -gt 0)) {
    exit 2
}

exit 0
