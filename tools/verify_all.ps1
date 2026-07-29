$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Invoke-VerificationStep {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )
    Write-Output "`n=== $Name ==="
    & $Action
    if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code $LASTEXITCODE" }
}

Invoke-VerificationStep 'Repository layout' {
    & (Join-Path $PSScriptRoot 'check_repository_layout.ps1')
}
Invoke-VerificationStep 'K230 protocol/runtime' {
    python (Join-Path $root 'k230\tests\test_ball_balance_link.py')
}
Invoke-VerificationStep 'MSPM0 portable algorithms' {
    & (Join-Path $root 'mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Test\run_line_follower_host.ps1')
}
Invoke-VerificationStep 'MSPM0 ARM integration syntax' {
    & (Join-Path $root 'mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Test\check_arm_syntax.ps1')
}
Invoke-VerificationStep 'STM32 balance-control algorithms' {
    & (Join-Path $root 'stm32f4\balance_control\Tools\run_ball_control_tests.ps1')
}
Invoke-VerificationStep 'STM32 CubeMX configuration' {
    & (Join-Path $root 'stm32f4\balance_control\Tools\check_ioc.ps1')
}
Invoke-VerificationStep 'STM32 user-code layout' {
    & (Join-Path $root 'stm32f4\balance_control\Tools\check_user_layout.ps1')
}
Invoke-VerificationStep 'STM32 ARM source syntax' {
    & (Join-Path $root 'stm32f4\balance_control\Tools\check_arm_syntax.ps1')
}

Write-Output "`nAll repository software verification steps passed."
