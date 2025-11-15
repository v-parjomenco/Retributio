param(
    # Если указать -Fix, скрипт добавит к вызову clang-tidy ключ -fix
    [switch]$Fix,

    # Путь к include-директории SFML (можно переопределить параметром)
    [string]$SfmlInclude = "C:\dev\SFML-3.0.2-64\include",

    # Имя подпапки с проектом внутри репозитория (там, где src и include)
    [string]$ProjectFolderName = "SFML1"
)

# Не делаем Stop на любую мелочь, пусть скрипт идет дальше
$ErrorActionPreference = "Continue"

# Корень репозитория = папка, где лежит сам скрипт
$repoRoot   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Join-Path $repoRoot $ProjectFolderName
$srcDir     = Join-Path $projectDir "src"
$includeDir = Join-Path $projectDir "include"
$thirdPartyDir = Join-Path $includeDir "third_party"

if (-not (Test-Path $srcDir)) {
    Write-Error "Не найден каталог с исходниками: $srcDir"
    exit 1
}

# Общие опции для clang-tidy
$tidyOptions = @(
    # Показывать диагностику только для файлов в:
    #  SFML1/src/**,
    #  SFML1/include/core/**,
    #  SFML1/include/entities/**,
    #  SFML1/include/graphics/**,
    #  SFML1/include/utils/**
    "-header-filter=$ProjectFolderName[\\/]src[\\/].*|$ProjectFolderName[\\/]include[\\/](core|entities|graphics|utils)[\\/].*"

    # Добавляем флаги компилятора ко всем запускам
    "-extra-arg=-std=c++17"

    # Наши хедеры
    "-extra-arg=-I$includeDir"

    # SFML как system-headers, чтобы clang-tidy не лез туда с проверками
    "-extra-arg=-isystem$SfmlInclude"
	
    # Чтобы clang-tidy не перелопачивал сторонние вещи, типа json в include\third_party
    "-extra-arg=-isystem$thirdPartyDir"

    # Выключаем обычные clang warning'и – оставляем только clang-tidy
    "-extra-arg=-Wno-everything"
)

if ($Fix) {
    $tidyOptions += "-fix"
    $tidyOptions += "-format-style=file"
}

Write-Host "Repo root:       $repoRoot"
Write-Host "Project dir:     $projectDir"
Write-Host "Source dir:      $srcDir"
Write-Host "Include dir:     $includeDir"
Write-Host "SFML include:    $SfmlInclude"
Write-Host "Third-party dir: $thirdPartyDir"
Write-Host ""

# Обходим все .cpp в SFML1/src
Get-ChildItem $srcDir -Recurse -Filter *.cpp | ForEach-Object {
    $file     = $_.FullName
    # относительный путь от корня репозитория
    $relative = $file.Substring($repoRoot.Length + 1)

    Write-Host "=== clang-tidy: $relative ===" -ForegroundColor Cyan

    & clang-tidy $file @tidyOptions 2>&1 | ForEach-Object {
        if ($_ -notmatch "Error while trying to load a compilation database" -and
            $_ -notmatch "fixed-compilation-database" -and
            $_ -notmatch "json-compilation-database" -and
            $_ -notmatch "Could not auto-detect compilation database" -and
            $_ -notmatch "No compilation database found in" -and
            $_ -notmatch "Running without flags." -and
            $_ -notmatch "warnings generated" -and
            $_ -notmatch "Suppressed [0-9]+ warnings" -and
            $_ -notmatch "Use -header-filter=.*" -and
            $_ -notmatch "Use -system-headers to display errors from system headers as well.") {

            Write-Host $_
        }
    }
}