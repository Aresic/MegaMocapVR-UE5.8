$ErrorActionPreference = 'Stop'
$sdkRoot = Join-Path $PSScriptRoot 'third_party\openvr-1.5.17'
$files = @(
    @{ Path='lib/win64/openvr_api.lib'; Hash='c745c0adcf1093ff5d447387028086f307a63b92e2a63bc82554c9e524e77cb1' },
    @{ Path='bin/win64/openvr_api.dll'; Hash='6c5e18a3c12ddb9618c1edb36cd4834115735c1f3096bd4cb2022c3593af28b8' }
)
foreach ($file in $files) {
    $destination = Join-Path $sdkRoot $file.Path
    if ((Test-Path -LiteralPath $destination) -and ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -eq $file.Hash)) {
        Write-Host "Verified $($file.Path)"
        continue
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    $temporary = "$destination.download"
    try {
        Invoke-WebRequest -Uri "https://raw.githubusercontent.com/ValveSoftware/openvr/v1.5.17/$($file.Path)" -OutFile $temporary
        if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $file.Hash) {
            throw "OpenVR hash mismatch: $($file.Path)"
        }
        Move-Item -LiteralPath $temporary -Destination $destination -Force
        Write-Host "Downloaded and verified $($file.Path)"
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}
