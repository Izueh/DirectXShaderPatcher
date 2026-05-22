param(
  [Parameter(Mandatory = $true)]
  [string]$InputShader,

  [string]$DecompilerPath = "F:\software\decompiler\bin\cmd_Decompiler.exe",

  [string]$OutputPath = "",

  [switch]$Force
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputShader)) {
  throw "Input shader not found: $InputShader"
}

if (-not (Test-Path -LiteralPath $DecompilerPath)) {
  throw "Decompiler not found: $DecompilerPath"
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
  $repoRoot = Split-Path -Parent $PSScriptRoot
  $artifactDir = Join-Path $repoRoot "artifacts\disasm"
  $shaderName = [System.IO.Path]::GetFileNameWithoutExtension($InputShader)
  $OutputPath = Join-Path $artifactDir ($shaderName + ".disasm.txt")
}

if ((Test-Path -LiteralPath $OutputPath) -and -not $Force) {
  throw "Output already exists: $OutputPath (use -Force to overwrite)"
}

$parent = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
  New-Item -ItemType Directory -Path $parent | Out-Null
}

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $DecompilerPath
$psi.Arguments = "-d `"$InputShader`""
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$proc = New-Object System.Diagnostics.Process
$proc.StartInfo = $psi
$null = $proc.Start()

$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()

$expectedAsmPath = [System.IO.Path]::ChangeExtension((Resolve-Path -LiteralPath $InputShader).Path, ".asm")

if (Test-Path -LiteralPath $expectedAsmPath) {
  Copy-Item -LiteralPath $expectedAsmPath -Destination $OutputPath -Force
  Write-Host "Wrote disassembly to $OutputPath"
  exit 0
}

if ($proc.ExitCode -ne 0) {
  throw "Decompiler failed (exit $($proc.ExitCode)): $stderr $stdout"
}

Set-Content -LiteralPath $OutputPath -Value $stdout -Encoding UTF8
Write-Host "Wrote disassembly to $OutputPath"
