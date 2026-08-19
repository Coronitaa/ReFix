# =============================================================================
# ReFix Digital Authenticode Code Signing Script
# =============================================================================

$certName = "CN=ReFix Trusted Engine, O=ReFix Security"

# Find or create self-signed code signing certificate
$cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -match "ReFix Trusted Engine" } | Select-Object -First 1

if (-not $cert) {
    Write-Host "[*] Generating trusted Code Signing Certificate..."
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $certName -CertStoreLocation "Cert:\CurrentUser\My" -NotAfter (Get-Date).AddYears(10)
}

# Add certificate to Trusted Root Certification Authorities so Windows marks the signature as VALID
$rootCert = Get-ChildItem Cert:\CurrentUser\Root | Where-Object { $_.Subject -match "ReFix Trusted Engine" } | Select-Object -First 1
if (-not $rootCert) {
    Write-Host "[*] Registering certificate in Trusted Root Certification Authorities store..."
    $tmpCertPath = "$env:TEMP\refix_root.crt"
    Export-Certificate -Cert $cert -FilePath $tmpCertPath | Out-Null
    Import-Certificate -FilePath $tmpCertPath -CertStoreLocation Cert:\CurrentUser\Root | Out-Null
    Remove-Item $tmpCertPath -Force -ErrorAction SilentlyContinue
}

# Sign all ReFix proxy DLLs
$dlls = @("build\winmm.dll", "build\EOSSDK-Win64-Shipping.dll", "build\RedboneEOS.dll", "build\steam_api64.dll")

foreach ($dll in $dlls) {
    if (Test-Path $dll) {
        Write-Host "[*] Digitally signing $dll..."
        $sigResult = Set-AuthenticodeSignature -FilePath $dll -Certificate $cert
        Write-Host "[OK] $dll signature status: $($sigResult.Status)"
    }
}
