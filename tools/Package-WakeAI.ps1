param([string]$Destination)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$cfg=Get-Content (Join-Path $repo 'wakeai.windows.local.json') -Raw|ConvertFrom-Json
$source=Join-Path $repo 'out\handoff-msvc\ui-added\Release\alarm_ui.exe'
$model=Join-Path $repo 'models\yolov8n-pose.onnx'
foreach($p in @($source,$model)){if(!(Test-Path $p)){throw "Missing: $p"}}
if(!$Destination){$Destination=Join-Path $repo ('dist\WakeAI-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
if(Test-Path $Destination){throw 'Choose a new empty destination; existing releases are never overwritten.'}
New-Item -ItemType Directory -Path $Destination|Out-Null
$Destination=(Resolve-Path $Destination).Path
Copy-Item $source $Destination
$env:PATH="$($cfg.QtRoot)\bin;$($cfg.OpenCVBin);$env:PATH"
& "$($cfg.QtRoot)\bin\windeployqt.exe" --release --no-compiler-runtime "$Destination\alarm_ui.exe"
if($LASTEXITCODE -ne 0){throw 'Qt deployment failed.'}
# Official OpenCV Windows packages: copy Release libraries and video plugins.
Get-ChildItem -LiteralPath $cfg.OpenCVBin -Filter '*.dll' |
    Where-Object {$_.Name -notmatch '\d+d\.dll$'} |
    Copy-Item -Destination $Destination
New-Item -ItemType Directory -Path "$Destination\models"|Out-Null
Copy-Item $model "$Destination\models"
if(!(Test-Path "$Destination\platforms\qwindows.dll")){throw 'Qt Windows platform plugin missing.'}
if(!(Test-Path "$Destination\sqldrivers\qsqlite.dll")){throw 'Qt SQLite driver missing.'}
if(!(Test-Path "$Destination\multimedia") -or
   !(Get-ChildItem -LiteralPath "$Destination\multimedia" -Filter '*.dll' -ErrorAction SilentlyContinue)){
    throw 'Qt Multimedia backend plugin missing; ringtone playback will not work.'
}
Write-Host "Package created: $Destination"
Write-Host 'Target PC needs the official Microsoft Visual C++ x64 Redistributable.'
Write-Host 'Verify launch, audio, camera, records and restart on the target PC.'
