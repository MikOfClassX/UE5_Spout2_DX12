[CmdletBinding()]
param(
    [string]$EngineDir = "C:\Program Files\Epic Games\UE_5.8",
    [string]$PackageDir = (Join-Path $PSScriptRoot "Build\Spout2_DX12")
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsAdministrator)) {
    Write-Host "Administrator rights are required to replace the Engine plugin. Requesting elevation..."

    $elevatedArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", ('"{0}"' -f $PSCommandPath),
        "-EngineDir", ('"{0}"' -f $EngineDir),
        "-PackageDir", ('"{0}"' -f $PackageDir)
    ) -join " "

    $process = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -ArgumentList $elevatedArguments
    exit $process.ExitCode
}

$EngineDir = [IO.Path]::GetFullPath($EngineDir)
$PackageDir = [IO.Path]::GetFullPath($PackageDir)
$targetDir = Join-Path $EngineDir "Engine\Plugins\Marketplace\Spout2_DX12"
$stageDir = Join-Path $env:TEMP "Spout2_DX12.new"
$backupDir = Join-Path $env:TEMP "Spout2_DX12.previous"

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw "Unreal Editor is running. Close it before deploying Spout2_DX12."
}

$requiredFiles = @(
    "Spout2_DX12.uplugin",
    "Binaries\Win64\UnrealEditor-Spout2_DX12.dll",
    "Binaries\Win64\UnrealEditor.modules",
    "Binaries\Win64\Spout.dll",
    "Binaries\Win64\SpoutDX12.dll",
    "Intermediate\Build\Win64\UnrealEditor\Inc\Spout2_DX12\UHT\SpoutSenderComponent.generated.h"
)

foreach ($relativePath in $requiredFiles) {
    if (-not (Test-Path (Join-Path $PackageDir $relativePath))) {
        throw "Incomplete package: missing '$relativePath' in '$PackageDir'. Run Build.bat first."
    }
}

$descriptor = Get-Content (Join-Path $PackageDir "Spout2_DX12.uplugin") -Raw | ConvertFrom-Json
if (-not $descriptor.Installed) {
    throw "The package descriptor is not marked Installed=true."
}
if ($descriptor.EngineVersion -ne "5.8.0") {
    throw "Unexpected packaged EngineVersion '$($descriptor.EngineVersion)' (expected 5.8.0)."
}

Write-Host "Package: $PackageDir"
Write-Host "Target:  $targetDir"
Write-Host "Backup:  $backupDir"

Remove-Item $stageDir -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $backupDir -Recurse -Force -ErrorAction SilentlyContinue

& robocopy.exe $PackageDir $stageDir /E /R:2 /W:1
if ($LASTEXITCODE -ge 8) {
    throw "Package staging failed with robocopy exit code $LASTEXITCODE."
}

$movedPreviousDeployment = $false
try {
    if (Test-Path $targetDir) {
        Move-Item $targetDir $backupDir
        $movedPreviousDeployment = $true
    }

    Move-Item $stageDir $targetDir

    foreach ($relativePath in $requiredFiles) {
        $packageHash = (Get-FileHash (Join-Path $PackageDir $relativePath) -Algorithm SHA256).Hash
        $targetHash = (Get-FileHash (Join-Path $targetDir $relativePath) -Algorithm SHA256).Hash
        if ($packageHash -ne $targetHash) {
            throw "Hash mismatch after deployment: $relativePath"
        }
    }

    & robocopy.exe $PackageDir $targetDir /MIR /L /R:0 /W:0 /NFL /NDL /NJH /NJS /NP
    if ($LASTEXITCODE -ne 0) {
        throw "The deployed tree differs from the package (robocopy dry-run exit code $LASTEXITCODE)."
    }
}
catch {
    Write-Warning "Deployment failed; restoring the previous deployment. $($_.Exception.Message)"
    Remove-Item $targetDir -Recurse -Force -ErrorAction SilentlyContinue
    if ($movedPreviousDeployment -and (Test-Path $backupDir)) {
        Move-Item $backupDir $targetDir
    }
    throw
}

Write-Host "DEPLOY SUCCESSFUL"
Write-Host "Previous deployment retained at '$backupDir' for rollback."
