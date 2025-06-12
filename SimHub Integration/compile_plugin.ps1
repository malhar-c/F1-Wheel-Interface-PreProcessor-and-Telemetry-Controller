# F1 Wheel Clutch Plugin PowerShell Compilation Script
# Enhanced version with better error handling and automatic path detection
#
# Parameters:
#   -SimHubPath: Path to SimHub installation (auto-detected if not provided)
#   -OutputPath: Directory for compiled DLL (default: .\bin)
#   -SourceFile: C# source file to compile (default: F1WheelClutchPlugin.cs)
#   -OutputFile: Name of output DLL file (default: F1WheelClutchPlugin.dll)
#   -OpenOutput: Switch to open output directory after compilation

param(
    [string]$SimHubPath = "",
    [string]$OutputPath = ".\bin",
    [string]$SourceFile = "F1WheelClutchPlugin.cs",
    [string]$OutputFile = "F1WheelClutchPlugin.dll",
    [switch]$OpenOutput = $false
)

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "F1 Wheel Clutch Plugin Compilation" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# Function to find SimHub installation
function Find-SimHubPath {    $possiblePaths = @(
        "C:\Program Files (x86)\SimHub",
        "C:\Program Files\SimHub",
        "D:\Program Files (x86)\SimHub",
        "D:\Program Files\SimHub"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path "$path\SimHub.exe") {
            return $path
        }
    }
    
    # Check registry for installation path
    try {
        $regPath = Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*" | 
                   Where-Object { $_.DisplayName -like "*SimHub*" }
        if ($regPath -and $regPath.InstallLocation) {
            return $regPath.InstallLocation.TrimEnd('\')
        }
    } catch {
        # Registry check failed, continue
    }
    
    return $null
}

# Function to find .NET Framework compiler
function Find-DotNetCompiler {
    $possiblePaths = @(
        "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe",
        "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\Roslyn\csc.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\Roslyn\csc.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\Roslyn\csc.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\Roslyn\csc.exe"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    return $null
}

# Auto-detect SimHub path if not provided
if (-not $SimHubPath) {
    Write-Host "Searching for SimHub installation..." -ForegroundColor Yellow
    $SimHubPath = Find-SimHubPath
}

if (-not $SimHubPath -or -not (Test-Path $SimHubPath)) {
    Write-Host "ERROR: SimHub installation not found!" -ForegroundColor Red
    Write-Host "Please specify the SimHub path using -SimHubPath parameter" -ForegroundColor Red
    Write-Host "Example: .\compile_plugin.ps1 -SimHubPath 'C:\Program Files (x86)\SimHub'" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found SimHub at: $SimHubPath" -ForegroundColor Green

# Find .NET compiler
Write-Host "Searching for .NET Framework compiler..." -ForegroundColor Yellow
$cscPath = Find-DotNetCompiler

if (-not $cscPath) {
    Write-Host "ERROR: .NET Framework compiler not found!" -ForegroundColor Red
    Write-Host "Please install .NET Framework 4.7.2 or Visual Studio Build Tools" -ForegroundColor Red
    exit 1
}

Write-Host "Found .NET compiler at: $cscPath" -ForegroundColor Green

# Create output directory
if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
}

# Check if source file exists
if (-not (Test-Path $SourceFile)) {
    Write-Host "ERROR: Source file '$SourceFile' not found!" -ForegroundColor Red
    Write-Host "Make sure you're running this script in the correct directory" -ForegroundColor Red
    exit 1
}

# Required references
$potentialReferences = @(
    "$SimHubPath\SimHub.Plugins.dll",
    "$SimHubPath\GameReaderCommon.dll"
)

# Function to find WPF assemblies
function Find-WpfAssemblies {
    $wpfPaths = @{
        "PresentationCore" = @()
        "PresentationFramework" = @()
        "WindowsBase" = @()
    }    # Try reference assemblies first
    $refPath = "$env:ProgramFiles\Reference Assemblies\Microsoft\Framework\.NETFramework\v4.8"
    if (Test-Path $refPath) {
        $assemblies = @($wpfPaths.Keys)
        foreach ($assembly in $assemblies) {
            $fullPath = "$refPath\$assembly.dll"
            if (Test-Path $fullPath) {
                $wpfPaths[$assembly] += $fullPath
            }
        }
    }
      # Fallback to GAC and system locations
    $gacPaths = @(
        "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319",
        "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319",
        "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\WPF",
        "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\WPF",
        "$env:ProgramFiles\Reference Assemblies\Microsoft\Framework\.NETFramework\v4.8"
    )
      foreach ($gacPath in $gacPaths) {
        if (Test-Path $gacPath) {
            $assemblies = @($wpfPaths.Keys)
            foreach ($assembly in $assemblies) {
                $possiblePaths = @(
                    "$gacPath\$assembly.dll",
                    "$gacPath\WPF\$assembly.dll"
                )
                
                foreach ($possiblePath in $possiblePaths) {
                    if ((Test-Path $possiblePath) -and ($wpfPaths[$assembly].Count -eq 0)) {
                        $wpfPaths[$assembly] += $possiblePath
                        break
                    }
                }
            }
        }
    }
    
    return $wpfPaths
}

$systemReferences = @(
    "System.dll",
    "System.Core.dll",
    "System.Windows.Forms.dll",
    "System.Xaml.dll"
)

# Find WPF assemblies
$wpfAssemblies = Find-WpfAssemblies
foreach ($assembly in $wpfAssemblies.Keys) {
    if ($wpfAssemblies[$assembly].Count -gt 0) {
        $systemReferences += $wpfAssemblies[$assembly][0]
        Write-Host "Found WPF: $assembly.dll" -ForegroundColor Green
    } else {
        Write-Host "Warning: WPF assembly $assembly.dll not found - plugin UI may not work" -ForegroundColor Yellow
    }
}

# Filter out missing SimHub references and warn about them
$validSimHubRefs = @()
$missingRefs = @()

foreach ($ref in $potentialReferences) {
    if (Test-Path $ref) {
        $validSimHubRefs += $ref
        Write-Host "Found: $(Split-Path $ref -Leaf)" -ForegroundColor Green
    } else {
        $missingRefs += $ref
        Write-Host "Warning: Missing $(Split-Path $ref -Leaf) - continuing without it" -ForegroundColor Yellow
    }
}

# Combine valid references
$references = $validSimHubRefs + $systemReferences

if ($validSimHubRefs.Count -eq 0) {
    Write-Host "ERROR: No SimHub plugin references found!" -ForegroundColor Red
    Write-Host "Please verify SimHub installation at: $SimHubPath" -ForegroundColor Red
    exit 1
}

# Build reference string
$refString = ($references | ForEach-Object { "/reference:`"$_`"" }) -join " "

# Output file
$outputFile = Join-Path $OutputPath $OutputFile

Write-Host ""
Write-Host "Compiling plugin..." -ForegroundColor Yellow
Write-Host "Source: $SourceFile"
Write-Host "Output: $outputFile"

# Compile command
$compileCmd = "`"$cscPath`" /target:library /out:`"$outputFile`" $refString `"$SourceFile`""

try {
    # Execute compilation
    $result = cmd /c $compileCmd 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "============================================" -ForegroundColor Green
        Write-Host "SUCCESS: Plugin compiled successfully!" -ForegroundColor Green
        Write-Host "============================================" -ForegroundColor Green
        Write-Host "Output: $outputFile" -ForegroundColor Green
        
        # Get file info
        $fileInfo = Get-Item $outputFile
        Write-Host "Size: $([math]::Round($fileInfo.Length / 1KB, 2)) KB" -ForegroundColor Green
        Write-Host "Created: $($fileInfo.CreationTime)" -ForegroundColor Green
        
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "1. Copy the .dll to SimHub's Plugins folder:" -ForegroundColor White
        Write-Host "   $SimHubPath\Plugins\" -ForegroundColor Yellow
        Write-Host "2. Restart SimHub" -ForegroundColor White
        Write-Host "3. Enable the plugin in Additional Plugins" -ForegroundColor White
        Write-Host "4. Configure serial port in plugin settings" -ForegroundColor White
        
        if ($OpenOutput) {
            Start-Process explorer.exe -ArgumentList "/select,`"$outputFile`""
        }
    } else {
        Write-Host ""
        Write-Host "============================================" -ForegroundColor Red
        Write-Host "ERROR: Compilation failed!" -ForegroundColor Red
        Write-Host "============================================" -ForegroundColor Red
        Write-Host "Compiler output:" -ForegroundColor Yellow
        Write-Host $result -ForegroundColor Red
    }
} catch {
    Write-Host "ERROR: Failed to execute compiler" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
}

Write-Host ""
Read-Host "Press Enter to continue"
