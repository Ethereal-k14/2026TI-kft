$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gccCandidates = @(
    'D:\Program Files (x86)\arm-gnu-toolchain-14.2\bin\arm-none-eabi-gcc.exe',
    'arm-none-eabi-gcc.exe'
)
$gcc = $gccCandidates | Where-Object { Get-Command $_ -ErrorAction SilentlyContinue } | Select-Object -First 1
if (-not $gcc) { throw 'arm-none-eabi-gcc was not found.' }

$includeDirs = @(
    'Core\Inc',
    'Drivers\STM32F4xx_HAL_Driver\Inc',
    'Drivers\STM32F4xx_HAL_Driver\Inc\Legacy',
    'Drivers\CMSIS\Device\ST\STM32F4xx\Include',
    'Drivers\CMSIS\Include',
    'User',
    'User\App\Inc',
    'User\Bsp\Inc',
    'User\Config',
    'User\Protocol\Inc'
) | ForEach-Object { '-I' + (Join-Path $root $_) }
$sources = @(
    Get-ChildItem -LiteralPath (Join-Path $root 'User') -Recurse -Filter '*.c';
    Get-ChildItem -LiteralPath (Join-Path $root 'Core\Src') -Filter '*.c'
) | ForEach-Object { $_.FullName }

& $gcc -mcpu=cortex-m4 -mthumb -std=c11 -fsyntax-only `
    -DUSE_HAL_DRIVER -DSTM32F407xx @includeDirs @sources
if ($LASTEXITCODE -ne 0) { throw 'ARM source syntax validation failed.' }
Write-Output "ARM source syntax validation passed: $($sources.Count) sources"
