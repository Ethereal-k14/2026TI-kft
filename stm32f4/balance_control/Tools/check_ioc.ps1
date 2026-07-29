param(
    [string]$IocPath = (Join-Path $PSScriptRoot '..\balance_control.ioc')
)

$ErrorActionPreference = 'Stop'
$resolvedPath = (Resolve-Path -LiteralPath $IocPath).Path
$lines = Get-Content -LiteralPath $resolvedPath
$entries = @{}
$errors = [System.Collections.Generic.List[string]]::new()

foreach ($line in $lines) {
    if (($line.Length -eq 0) -or $line.StartsWith('#')) {
        continue
    }

    $parts = $line.Split('=', 2)
    if ($parts.Count -ne 2) {
        $errors.Add("Invalid line without key/value separator: $line")
        continue
    }

    if ($entries.ContainsKey($parts[0])) {
        $errors.Add("Duplicate key: $($parts[0])")
    } else {
        $entries.Add($parts[0], $parts[1])
    }
}

function Assert-Equal {
    param(
        [string]$Key,
        [string]$Expected
    )

    if (-not $entries.ContainsKey($Key)) {
        $errors.Add("Missing key: $Key")
    } elseif ($entries[$Key] -ne $Expected) {
        $errors.Add("$Key expected '$Expected', got '$($entries[$Key])'")
    }
}

function Assert-ContainsValue {
    param(
        [string]$KeyPattern,
        [string]$Expected
    )

    $matches = @($entries.Keys |
        Where-Object { $_ -like $KeyPattern } |
        Where-Object { $entries[$_] -eq $Expected })
    if ($matches.Count -eq 0) {
        $errors.Add("No key matching '$KeyPattern' has value '$Expected'")
    }
}

$ipCount = ($entries.Keys | Where-Object { $_ -match '^Mcu\.IP\d+$' }).Count
$pinCount = ($entries.Keys | Where-Object { $_ -match '^Mcu\.Pin\d+$' }).Count
$dmaRequestCount = ($entries.Keys | Where-Object { $_ -match '^Dma\.Request\d+$' }).Count

$declaredPins = $entries.Keys |
    Where-Object { $_ -match '^Mcu\.Pin\d+$' } |
    ForEach-Object { $entries[$_] }
$duplicatePins = $declaredPins | Group-Object | Where-Object { $_.Count -gt 1 }
foreach ($duplicatePin in $duplicatePins) {
    $errors.Add("Pin is declared more than once: $($duplicatePin.Name)")
}

Assert-Equal 'Mcu.CPN' 'STM32F407VET6'
Assert-Equal 'Mcu.IPNb' $ipCount.ToString()
Assert-Equal 'Mcu.PinsNb' $pinCount.ToString()
Assert-Equal 'Dma.RequestsNb' $dmaRequestCount.ToString()
Assert-Equal 'RCC.HSE_VALUE' '8000000'
Assert-Equal 'RCC.PLLSourceVirtual' 'RCC_PLLSOURCE_HSE'
Assert-Equal 'RCC.PLLM' '4'
Assert-Equal 'RCC.PLLN' '168'
Assert-Equal 'RCC.PLLQ' '7'
Assert-Equal 'RCC.SYSCLKFreq_VALUE' '168000000'
Assert-Equal 'PH0-OSC_IN.Mode' 'HSE-External-Oscillator'
Assert-Equal 'PH1-OSC_OUT.Mode' 'HSE-External-Oscillator'
Assert-Equal 'PC0.Signal' 'ADCx_IN10'
Assert-ContainsValue 'Mcu.IP*' 'ADC1'
Assert-Equal 'ADC1.Channel-10\#ChannelRegularConversion' 'ADC_CHANNEL_10'
Assert-Equal 'ADC1.ExternalTrigConv' 'ADC_EXTERNALTRIGCONV_T3_TRGO'
Assert-Equal 'PA5.Signal' 'SPI1_SCK'
Assert-Equal 'PA6.Signal' 'SPI1_MISO'
Assert-Equal 'PA7.Signal' 'SPI1_MOSI'
Assert-Equal 'PB8.Signal' 'I2C1_SCL'
Assert-Equal 'PB9.Signal' 'I2C1_SDA'
Assert-Equal 'I2C1.ClockSpeed' '400000'
Assert-Equal 'PC6.Signal' 'S_TIM8_CH1'
Assert-Equal 'PA15.Signal' 'S_TIM2_CH1_ETR'
Assert-Equal 'PB3.Signal' 'S_TIM2_CH2'
Assert-ContainsValue 'Mcu.IP*' 'TIM2'
Assert-ContainsValue 'Mcu.IP*' 'TIM5'
Assert-Equal 'TIM2.EncoderMode' 'TIM_ENCODERMODE_TI12'
Assert-Equal 'TIM2.Period' '4294967295'
Assert-Equal 'TIM5.Channel-PWM\ Input1' 'TIM_CHANNEL_1'
Assert-Equal 'TIM5.Prescaler' '84-1'
Assert-Equal 'SH.S_TIM5_CH1.0' 'TIM5_CH1,PWM Input1'
Assert-Equal 'SH.S_TIM5_CH2.0' 'TIM5_CH2,PWM Input2'
Assert-Equal 'NVIC.TIM5_IRQn' 'true\:3\:0\:false\:false\:true\:true\:true\:true'
Assert-Equal 'PB4.Signal' 'GPXTI4'
Assert-Equal 'PB4.GPIO_ModeDefaultEXTI' 'GPIO_MODE_IT_RISING'
Assert-Equal 'PC8.GPIO_Label' 'TMC_ENN'
Assert-Equal 'PC8.PinState' 'GPIO_PIN_SET'
Assert-Equal 'PC9.GPIO_ModeDefaultEXTI' 'GPIO_MODE_IT_RISING'
Assert-Equal 'PE0.GPIO_ModeDefaultEXTI' 'GPIO_MODE_IT_FALLING'
Assert-Equal 'PE5.GPIO_ModeDefaultEXTI' 'GPIO_MODE_IT_RISING_FALLING'
Assert-Equal 'PE6.GPIO_ModeDefaultEXTI' 'GPIO_MODE_IT_RISING_FALLING'
Assert-Equal 'PA9.Signal' 'USART1_TX'
Assert-Equal 'PA10.Signal' 'USART1_RX'
Assert-Equal 'PD5.Signal' 'USART2_TX'
Assert-Equal 'PD6.Signal' 'USART2_RX'
Assert-Equal 'PD8.Signal' 'USART3_TX'
Assert-Equal 'PD9.Signal' 'USART3_RX'
Assert-Equal 'PC10.Signal' 'UART4_TX'
Assert-Equal 'PC11.Signal' 'UART4_RX'
Assert-Equal 'UART4.BaudRate' '921600'
Assert-Equal 'USART2.BaudRate' '115200'
Assert-Equal 'USART3.BaudRate' '921600'
if ($entries['ProjectManager.functionlistsort'] -notmatch 'MX_ADC1_Init-ADC1' -or
    $entries['ProjectManager.functionlistsort'] -notmatch 'MX_TIM2_Init-TIM2') {
    $errors.Add('ProjectManager.functionlistsort must include ADC1 and TIM2 generation entries')
}

$expectedDma = @{
    'Dma.ADC1.0.Instance'      = 'DMA2_Stream0'
    'Dma.SPI1_TX.1.Instance'   = 'DMA2_Stream3'
    'Dma.USART1_RX.2.Instance' = 'DMA2_Stream2'
    'Dma.USART1_TX.3.Instance' = 'DMA2_Stream7'
    'Dma.USART2_RX.4.Instance' = 'DMA1_Stream5'
    'Dma.USART2_TX.5.Instance' = 'DMA1_Stream6'
    'Dma.USART3_RX.6.Instance' = 'DMA1_Stream1'
    'Dma.USART3_TX.7.Instance' = 'DMA1_Stream3'
    'Dma.UART4_RX.8.Instance'  = 'DMA1_Stream2'
    'Dma.UART4_TX.9.Instance'  = 'DMA1_Stream4'
}
foreach ($dmaKey in $expectedDma.Keys) {
    Assert-Equal $dmaKey $expectedDma[$dmaKey]
}

$dmaInstances = @{}
foreach ($key in ($entries.Keys | Where-Object { $_ -match '^Dma\..*\.Instance$' })) {
    $instance = $entries[$key]
    if ($dmaInstances.ContainsKey($instance)) {
        $errors.Add("DMA stream conflict: $instance is used by $($dmaInstances[$instance]) and $key")
    } else {
        $dmaInstances.Add($instance, $key)
    }
}

$pllInputHz = 8000000 / 4
$vcoHz = $pllInputHz * 168
$sysclkHz = $vcoHz / 2
$pll48Hz = $vcoHz / 7
if (($sysclkHz -ne 168000000) -or ($pll48Hz -ne 48000000)) {
    $errors.Add('Clock formula validation failed')
}

function Get-UartBaudErrorPercent {
    param(
        [double]$PeripheralClockHz,
        [double]$RequestedBaud
    )

    $divider16 = [math]::Round(($PeripheralClockHz / $RequestedBaud) * 16.0) / 16.0
    $actualBaud = $PeripheralClockHz / $divider16
    return [math]::Abs(($actualBaud - $RequestedBaud) / $RequestedBaud) * 100.0
}

$debugBaudError = Get-UartBaudErrorPercent 84000000 115200
$highSpeedBaudError = Get-UartBaudErrorPercent 42000000 921600
if ($debugBaudError -gt 1.0) {
    $errors.Add("USART1 baud error too high: $debugBaudError percent")
}
if ($highSpeedBaudError -gt 1.0) {
    $errors.Add("APB1 921600 baud error too high: $highSpeedBaudError percent")
}

if ($errors.Count -ne 0) {
    Write-Error ("IOC validation failed:`n - " + ($errors -join "`n - "))
    exit 1
}

Write-Output "IOC validation passed: $resolvedPath"
Write-Output "IP=$ipCount Pins=$pinCount DMA requests=$dmaRequestCount DMA streams=$($dmaInstances.Count)"
Write-Output 'Clock: HSE 8 MHz -> SYSCLK 168 MHz, PLL48CLK 48 MHz'
Write-Output ("UART baud error: USART1={0:N4}% APB1-high-speed={1:N4}%" -f $debugBaudError, $highSpeedBaudError)
