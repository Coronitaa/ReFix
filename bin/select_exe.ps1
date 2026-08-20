param(
    [string]$InitialDir = ""
)
Add-Type -AssemblyName System.Windows.Forms
$dialog = New-Object System.Windows.Forms.OpenFileDialog
$dialog.Title = "Select Game Executable"
$dialog.Filter = "Executable Files (*.exe)|*.exe|All Files (*.*)|*.*"
if ($InitialDir -and (Test-Path $InitialDir)) {
    $dialog.InitialDirectory = $InitialDir
}
if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
    Write-Output $dialog.FileName
}
