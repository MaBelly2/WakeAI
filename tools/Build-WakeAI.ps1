param(
    [string]$QtRoot,
    [string]$OpenCVDir,
    [string]$OpenCVBin,
    [switch]$ConfigureOnly
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo
$configFile = Join-Path $repo 'wakeai.windows.local.json'
if (Test-Path $configFile) {
    $old = Get-Content $configFile -Raw | ConvertFrom-Json
    if (!$QtRoot) { $QtRoot = $old.QtRoot }
    if (!$OpenCVDir) { $OpenCVDir = $old.OpenCVDir }
    if (!$OpenCVBin) { $OpenCVBin = $old.OpenCVBin }
}
if (!$QtRoot) { $QtRoot = Read-Host 'Qt MSVC x64 root (example C:\Qt\6.11.2\msvc2022_64)' }
if (!$OpenCVDir) { $OpenCVDir = Read-Host 'Directory containing OpenCVConfig.cmake' }
if (!$OpenCVBin) { $OpenCVBin = Read-Host 'OpenCV MSVC x64 bin directory' }
foreach ($p in @(
    "$QtRoot\lib\cmake\Qt6\Qt6Config.cmake",
    "$QtRoot\lib\cmake\Qt6Multimedia\Qt6MultimediaConfig.cmake",
    "$QtRoot\lib\cmake\Qt6Sql\Qt6SqlConfig.cmake",
    "$QtRoot\bin\windeployqt.exe",
    "$OpenCVDir\OpenCVConfig.cmake",$OpenCVBin)) {
    if (!(Test-Path -LiteralPath $p)) { throw "Missing dependency/path: $p" }
}
if (!(Get-ChildItem -LiteralPath $OpenCVBin -Filter 'opencv*.dll')) {throw 'No OpenCV runtime DLLs found.'}
$QtRoot = (Resolve-Path $QtRoot).Path
$OpenCVDir = (Resolve-Path $OpenCVDir).Path
$OpenCVBin = (Resolve-Path $OpenCVBin).Path
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$utf8 = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($configFile,(@{QtRoot=$QtRoot;OpenCVDir=$OpenCVDir;OpenCVBin=$OpenCVBin}|ConvertTo-Json),$utf8)
# Preserve unrelated user presets; replace only the named WakeAI preset.
$presetFile = Join-Path $repo 'CMakeUserPresets.json'
$preset = [ordered]@{
    name='wakeai-msvc-release'; displayName='WakeAI MSVC x64 Release';
    generator='Visual Studio 17 2022'; architecture='x64';
    binaryDir='${sourceDir}/out/handoff-msvc';
    cacheVariables=@{
        CMAKE_PREFIX_PATH=$QtRoot.Replace('\','/'); OpenCV_DIR=$OpenCVDir.Replace('\','/');
        WAKEAI_OPENCV_RUNTIME_DIR=$OpenCVBin.Replace('\','/');
        WAKEAI_BUILD_GUI='ON'; WAKEAI_BUILD_VISION='ON'; WAKEAI_BUILD_TESTS='ON'
    }
}
if(Test-Path $presetFile) {
    $presets = Get-Content $presetFile -Raw | ConvertFrom-Json
    if (!$presets.PSObject.Properties['configurePresets']) {
        $presets | Add-Member -NotePropertyName configurePresets -NotePropertyValue @()
    }
    $presets.configurePresets = @($presets.configurePresets | Where-Object {$_.name -ne 'wakeai-msvc-release'}) + @($preset)
} else { $presets = [ordered]@{version=3;configurePresets=@($preset)} }
[IO.File]::WriteAllText($presetFile,($presets|ConvertTo-Json -Depth 30),$utf8)
& $cmake --preset wakeai-msvc-release
if($LASTEXITCODE -ne 0){throw 'CMake configuration failed; read the first error above.'}
if($ConfigureOnly){return}
& $cmake --build out/handoff-msvc --config Release --parallel
if($LASTEXITCODE -ne 0){throw 'Build failed; read the first error above.'}
$env:PATH="$QtRoot\bin;$OpenCVBin;$env:PATH"
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $ctest --test-dir out/handoff-msvc -C Release --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Tests failed.'}
Write-Host 'Build and tests passed. Run tools\Start-WakeAI.ps1 (or add -TestMode).'
