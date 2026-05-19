param(
    [Parameter(Mandatory = $true)]
    [string]$TestExe,

    [Parameter(Mandatory = $true)]
    [string]$Arg1,

    [Parameter(Mandatory = $true)]
    [string]$Arg2
)

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $TestExe
$psi.Arguments = ('"{0}" "{1}"' -f $Arg1, $Arg2)
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true

$process = [System.Diagnostics.Process]::Start($psi)
$stdout = $process.StandardOutput.ReadToEnd()
$stderr = $process.StandardError.ReadToEnd()
$process.WaitForExit()

if ($stdout.Length -gt 0) {
    [Console]::Out.Write($stdout)
}

if ($stderr.Length -gt 0) {
    [Console]::Error.Write($stderr)
}

Write-Host ("Child exit code: {0}" -f $process.ExitCode)
exit $process.ExitCode