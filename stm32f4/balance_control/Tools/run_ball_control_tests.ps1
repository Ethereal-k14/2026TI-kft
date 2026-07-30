$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gccCandidates = @('D:\Program Files\mingw64\bin\gcc.exe', 'gcc.exe')
$gcc = $gccCandidates | Where-Object { Get-Command $_ -ErrorAction SilentlyContinue } | Select-Object -First 1
if (-not $gcc) { throw 'Host GCC was not found.' }
$output = Join-Path $env:TEMP 'balance_ball_control_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror `
    "-I$(Join-Path $root 'User\App\Inc')" `
    (Join-Path $root 'User\App\Src\ball_control_core.c') `
    (Join-Path $PSScriptRoot 'test_ball_control_core.c') `
    -o $output
if ($LASTEXITCODE -ne 0) { throw 'Ball control test build failed.' }
& $output
if ($LASTEXITCODE -ne 0) { throw 'Ball control tests failed.' }

$safetyOutput = Join-Path $env:TEMP 'balance_safety_policy_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror `
    "-I$(Join-Path $root 'User\App\Inc')" `
    (Join-Path $root 'User\App\Src\app_safety_policy.c') `
    (Join-Path $PSScriptRoot 'test_safety_policy.c') `
    -o $safetyOutput
if ($LASTEXITCODE -ne 0) { throw 'Safety policy test build failed.' }
& $safetyOutput
if ($LASTEXITCODE -ne 0) { throw 'Safety policy tests failed.' }
