# Builds the loader and packs the release zip into dist/.
# Needs Visual Studio 2022 (or its Build Tools) with the C++ workload, and the .NET SDK 8 or newer.
#
#   powershell -ExecutionPolicy Bypass -File build.ps1 [-Version 1.0.0]

param(
    [string]$Version = "dev"
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found. Install Visual Studio 2022 or its Build Tools."
}
$msbuild = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) {
    throw "MSBuild with the C++ tools not found. Install the 'Desktop development with C++' workload."
}

& $msbuild (Join-Path $root "CSharpLoader.sln") -restore -p:Configuration=Release -p:Platform=x64 -v:minimal -nologo
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

# same layout as the game folder: unzip into BlackMythWukong/
$name = "B1CSharpLoader-$Version"
$pkg = Join-Path $root "dist\$name"
$win64 = Join-Path $pkg "b1\Binaries\Win64"
$loader = Join-Path $win64 "CSharpLoader"
if (Test-Path $pkg) {
    Remove-Item $pkg -Recurse -Force
}
foreach ($dir in "Data", "Mods", "Plugins") {
    New-Item -ItemType Directory -Force (Join-Path $loader $dir) | Out-Null
}

$managerOut = Join-Path $root "CSharpManager\bin\Release\net472"
$files = "0Harmony.dll", "CSharpManager.bin", "CSharpModBase.dll", "Mono.Cecil.dll", "Mono.Cecil.Mdb.dll",
         "Mono.Cecil.Pdb.dll", "Mono.Cecil.Rocks.dll", "SharpDX.dll", "SharpDX.XInput.dll"
foreach ($file in $files) {
    Copy-Item (Join-Path $managerOut $file) $loader
}
Copy-Item (Join-Path $root "CSharpManager\b1cs.ini") $loader
Copy-Item (Join-Path $root "x64\Release\version.dll") $win64
foreach ($file in "LICENSE", "README.md", "README.cn.md") {
    Copy-Item (Join-Path $root $file) $pkg
}

# write the zip entries by hand: forward slashes, and the empty folders stay in the zip
$zip = "$pkg.zip"
if (Test-Path $zip) {
    Remove-Item $zip -Force
}
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::Open($zip, "Create")
try {
    foreach ($item in Get-ChildItem $pkg -Recurse) {
        $entry = $item.FullName.Substring($pkg.Length + 1).Replace("\", "/")
        if ($item.PSIsContainer) {
            [void]$archive.CreateEntry("$entry/")
        } else {
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $item.FullName, $entry)
        }
    }
} finally {
    $archive.Dispose()
}
Write-Host "Release: $zip"
