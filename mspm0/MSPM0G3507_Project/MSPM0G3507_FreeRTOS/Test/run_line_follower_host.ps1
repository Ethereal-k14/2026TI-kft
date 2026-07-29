$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$gccCandidates = @(
    'D:\Program Files\mingw64\bin\gcc.exe',
    'gcc.exe'
)
$gcc = $gccCandidates | Where-Object { Get-Command $_ -ErrorAction SilentlyContinue } | Select-Object -First 1
if (-not $gcc) { throw 'Host GCC was not found.' }

$output = Join-Path $env:TEMP 'axiomtrace_line_follower_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror `
    "-I$(Join-Path $root 'Application\Algorithm')" `
    (Join-Path $root 'Application\Algorithm\app_line_follower.c') `
    (Join-Path $PSScriptRoot 'test_line_follower_host.c') `
    -o $output
if ($LASTEXITCODE -ne 0) { throw 'Line follower host test build failed.' }
& $output
if ($LASTEXITCODE -ne 0) { throw 'Line follower host tests failed.' }
