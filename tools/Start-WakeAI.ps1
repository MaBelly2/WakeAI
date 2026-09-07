param([switch]$TestMode)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $repo 'out\handoff-msvc\ui-added\Release\alarm_ui.exe'
if(!(Test-Path $exe)){throw 'Run tools\Build-WakeAI.ps1 first.'}
$cfgPath=Join-Path $repo 'wakeai.windows.local.json'
if(Test-Path $cfgPath){
    $cfg=Get-Content $cfgPath -Raw|ConvertFrom-Json
    $env:PATH="$($cfg.QtRoot)\bin;$($cfg.OpenCVBin);$env:PATH"
}
if($TestMode){& $exe --test-mode}else{& $exe}
