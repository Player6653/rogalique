# Форматирует все .h/.cpp в Engine/, Rogalique/, EngineTest/ по правилам из .clang-format в корне репозитория.
# Не трогает SFML/ (сторонний код) и папки сборки (x64/, packages/).

$candidates = @(
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin\clang-format.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\x64\bin\clang-format.exe"
)
$clangFormat = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $clangFormat) {
    $onPath = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($onPath) { $clangFormat = $onPath.Source }
}
if (-not $clangFormat) {
    Write-Error "clang-format.exe не найден. Установите компонент Visual Studio ""Clang tools for Windows"" или добавьте clang-format в PATH."
    exit 1
}

Write-Output "Использую: $clangFormat"

$root = $PSScriptRoot
$files = Get-ChildItem -Path (Join-Path $root 'Engine'), (Join-Path $root 'Rogalique'), (Join-Path $root 'EngineTest') `
    -Recurse -Include *.h, *.cpp -File

$count = 0
foreach ($file in $files) {
    Write-Output $file.FullName
    & $clangFormat -i -style=file $file.FullName
    $count++
}
Write-Output "Готово, отформатировано файлов: $count"
