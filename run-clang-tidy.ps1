param(
    # Если указать -Fix, скрипт добавит к вызову clang-tidy ключ -fix
    [switch]$Fix,

    # Путь к include-директории SFML (можно переопределить параметром)
    [string]$SfmlInclude = "C:\dev\SFML-3.0.2-64\include"
)

# Не делаем Stop на любую мелочь, пусть скрипт идет дальше
$ErrorActionPreference = "Continue"

# Корень репозитория = папка, где лежит сам скрипт
$repoRoot   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Join-Path $repoRoot "SFML1"
$srcDir     = Join-Path $projectDir "src"
$includeDir = Join-Path $projectDir "include"
$thirdPartyDir = Join-Path $projectDir "include\third_party"

if (-not (Test-Path $srcDir)) {
    Write-Error "Не найден каталог с исходниками: $srcDir"
    exit 1
}

# Общие опции для clang-tidy
$tidyOptions = @(
    # Показывать диагностику только для файлов в SFML1/src и SFML1/include
    "-header-filter=SFML1[\\/](src|include)[\\/].*"

    # Добавляем флаги компилятора ко всем запускам
    "-extra-arg=-std=c++17"

    # Наши хедеры
    "-extra-arg=-I$includeDir"

    # SFML как system-headers, чтобы clang-tidy не лез туда с проверками
    "-extra-arg=-isystem$SfmlInclude"
	
	# Чтобы clang-tidy не перелопачивал сторонние вещи, типа json
    "-extra-arg=-isystem$thirdPartyDir"	

    # Выключаем обычные clang warning'и – оставляем только clang-tidy
    "-extra-arg=-Wno-everything"
)

if ($Fix) {
    $tidyOptions += "-fix"
    $tidyOptions += "-format-style=file"
}

Write-Host "Repo root:     $repoRoot"
Write-Host "Project dir:   $projectDir"
Write-Host "Source dir:    $srcDir"
Write-Host "Include dir:   $includeDir"
Write-Host "SFML include:  $SfmlInclude"
Write-Host ""

# Обходим все .cpp в SFML1/src
Get-ChildItem $srcDir -Recurse -Filter *.cpp | ForEach-Object {
    $file     = $_.FullName
    $relative = $file.Substring($repoRoot.Length + 1)

    Write-Host "=== clang-tidy: $relative ===" -ForegroundColor Cyan

    # Запускаем clang-tidy, объединяем stdout+stderr, фильтруем шум про compilation database
    & clang-tidy $file @tidyOptions 2>&1 | ForEach-Object {
        if ($_ -notmatch "Error while trying to load a compilation database") {
            Write-Host $_
        }
    }
}