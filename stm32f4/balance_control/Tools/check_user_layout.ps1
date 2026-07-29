param(
    [string]$ProjectPath = (Join-Path $PSScriptRoot '..\MDK-ARM\balance_control.uvprojx'),
    [string]$RootPath = (Join-Path $PSScriptRoot '..')
)

$ErrorActionPreference = 'Stop'
$project = Resolve-Path -LiteralPath $ProjectPath
$root = (Resolve-Path -LiteralPath $RootPath).Path
[xml]$xml = Get-Content -LiteralPath $project
$errors = [System.Collections.Generic.List[string]]::new()

$userSources = @(Get-ChildItem -LiteralPath (Join-Path $root 'User') -Recurse -Filter '*.c' |
    ForEach-Object { $_.FullName.Substring($root.Length + 1).Replace('\', '/') })
$allProjectSources = @($xml.Project.Targets.Target.Groups.Group.Files.File.FilePath |
    Where-Object { $_ } |
    ForEach-Object { $_.Replace('\', '/') })
$projectSources = @($allProjectSources |
    Where-Object { $_ -and ($_ -like '../User/*') } |
    ForEach-Object { $_ })

foreach ($source in $userSources) {
    $projectPath = '../' + $source
    if ($projectSources -notcontains $projectPath) {
        $errors.Add("User source is not present in Keil project: $source")
    }
}

foreach ($requiredSource in @('../Core/Src/adc.c',
                              '../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c',
                              '../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c')) {
    if ($allProjectSources -notcontains $requiredSource) {
        $errors.Add("Required ADC source is not present in Keil project: $requiredSource")
    }
}

foreach ($legacyDir in @('App', 'Bsp')) {
    $legacySources = @(Get-ChildItem -LiteralPath (Join-Path $root $legacyDir) -Recurse -Filter '*.c' -ErrorAction SilentlyContinue)
    if ($legacySources.Count -ne 0) {
        $errors.Add("Legacy directory contains implementation sources: $legacyDir")
    }
}

$main = Get-Content -Raw -LiteralPath (Join-Path $root 'Core/Src/main.c')
$isr = Get-Content -Raw -LiteralPath (Join-Path $root 'Core/Src/stm32f4xx_it.c')
if ($main -notmatch '#include\s+"user_runtime\.h"') {
    $errors.Add('Core/Src/main.c must include user_runtime.h')
}
if ($isr -notmatch '#include\s+"user_isr\.h"') {
    $errors.Add('Core/Src/stm32f4xx_it.c must include user_isr.h')
}

if ($errors.Count -ne 0) {
    Write-Error ("User layout validation failed:`n - " + ($errors -join "`n - "))
    exit 1
}

Write-Output "User layout validation passed: sources=$($userSources.Count), project=$project"
