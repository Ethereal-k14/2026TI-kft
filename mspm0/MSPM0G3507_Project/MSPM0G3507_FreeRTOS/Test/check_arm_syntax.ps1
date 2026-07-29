$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = (Resolve-Path (Join-Path $root '..\source')).Path
$gccCandidates = @(
    'D:\Program Files (x86)\arm-gnu-toolchain-14.2\bin\arm-none-eabi-gcc.exe',
    'arm-none-eabi-gcc.exe'
)
$gcc = $gccCandidates | Where-Object { Get-Command $_ -ErrorAction SilentlyContinue } | Select-Object -First 1
if (-not $gcc) { throw 'arm-none-eabi-gcc was not found.' }
$includeDirs = @(
    $sdk,
    (Join-Path $sdk 'third_party\CMSIS\Core\Include'),
    'Application', 'Application\Task', 'Application\test', 'Application\Algorithm',
    'Application\Algorithm\Filter', 'BSP', 'BSP\Peripherals', 'BSP\Input',
    'BSP\IMU', 'BSP\IMU\Ports\mspm0g3507', 'Config', 'Lib\OSAL',
    'Lib\AxiomTrace', 'Lib\Math', 'Lib\FreeRTOS\include',
    'Lib\FreeRTOS\portable\GCC\ARM_CM0', 'Lib\FreeRTOS\portable\MemMang',
    'Lib\FreeRTOS\src', 'Protocol\Common', 'Protocol\Transport'
) | ForEach-Object { if ([IO.Path]::IsPathRooted($_)) { '-I' + $_ } else { '-I' + (Join-Path $root $_) } }
$sources = @(
    'Application\Algorithm\app_line_follower.c',
    'Application\Algorithm\app_imu_kinematics.c',
    'Application\app_line_track.c',
    'Application\app_balance_link.c',
    'Application\app_main.c',
    'Application\Task\task_control.c',
    'Application\Task\task_imu.c'
) | ForEach-Object { Join-Path $root $_ }
& $gcc -mcpu=cortex-m0plus -mthumb -std=c11 -fsyntax-only `
    -D__MSPM0G3507__ -DLOG_LEVEL=3 -DPRJ_LINE_TRACK_ENABLE=1 `
    @includeDirs @sources
if ($LASTEXITCODE -ne 0) { throw 'MSPM0 ARM integration syntax failed.' }
Write-Output "MSPM0 ARM integration syntax passed: $($sources.Count) changed integration sources"
