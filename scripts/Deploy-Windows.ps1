param(
    [Parameter(Mandatory = $true)]
    [string]$QtRoot,

    [Parameter(Mandatory = $true)]
    [string]$VcpkgRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string]$VisualCppRedistDirectory
)

$ErrorActionPreference = 'Stop'

$output = [System.IO.Path]::GetFullPath($OutputDirectory)
$executable = Join-Path $output 'ShDisplayViewer.exe'
$winDeployQt = Join-Path $QtRoot 'bin\windeployqt.exe'
$vcpkgInstalled = Join-Path $VcpkgRoot 'installed\x64-windows'
$vcpkgBin = Join-Path $vcpkgInstalled 'bin'
$osgPluginSource = Join-Path $vcpkgInstalled 'plugins\osgPlugins-3.6.5'

foreach ($requiredPath in @($output, $executable, $winDeployQt, $vcpkgBin, $osgPluginSource)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required deployment input does not exist: $requiredPath"
    }
}

& $winDeployQt --release --no-translations --dir $output $executable
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Copy-Item -Path (Join-Path $vcpkgBin '*.dll') -Destination $output -Force
New-Item -ItemType Directory -Path (Join-Path $output 'platforms') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $QtRoot 'plugins\platforms\qoffscreen.dll') -Destination (Join-Path $output 'platforms') -Force

$osgPluginDestination = Join-Path $output 'osgPlugins-3.6.5'
New-Item -ItemType Directory -Path $osgPluginDestination -Force | Out-Null
$sdkPluginNames = @('osgdb_osgearth_flatgis.dll','osgdb_osgearth_hgbump.dll','osgdb_osgearth_wmts.dll','osgdb_osgearth_hgvector.dll')
foreach ($plugin in Get-ChildItem -LiteralPath $osgPluginSource -Filter '*.dll' -File) {
    $destination = Join-Path $osgPluginDestination $plugin.Name
    if ($plugin.Name -in $sdkPluginNames -and (Test-Path -LiteralPath $destination)) { continue }
    Copy-Item -LiteralPath $plugin.FullName -Destination $destination -Force
}

# Refresh packaged SDK runtimes even when no client source needed recompiling.
$cache = Join-Path $output 'CMakeCache.txt'
if (Test-Path -LiteralPath $cache) {
    $sdkEntry = Select-String -LiteralPath $cache -Pattern '^ShEarthSDK_DIR:[^=]+=(.+)$' | Select-Object -First 1
    if ($sdkEntry) {
        $sdkBin = [System.IO.Path]::GetFullPath((Join-Path $sdkEntry.Matches[0].Groups[1].Value '..\..\..\bin'))
        if (-not (Test-Path -LiteralPath (Join-Path $sdkBin 'FreeEarthRuntime.dll'))) { throw 'SDK runtime package is incomplete' }
        Copy-Item -Path (Join-Path $sdkBin '*.dll') -Destination $output -Force
        foreach ($name in $sdkPluginNames) {
            $source = Join-Path $sdkBin "osgPlugins-3.6.5\$name"
            if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $osgPluginDestination -Force }
        }
    }
}

$runtimeData = @(
    @{ Source = (Join-Path $vcpkgInstalled 'share\gdal'); Destination = (Join-Path $output 'gdal-data') },
    @{ Source = (Join-Path $vcpkgInstalled 'share\proj'); Destination = (Join-Path $output 'proj-data') },
    @{ Source = (Join-Path $vcpkgInstalled 'etc\fonts'); Destination = (Join-Path $output 'fontconfig') }
)
foreach ($item in $runtimeData) {
    if (Test-Path -LiteralPath $item.Source) {
        New-Item -ItemType Directory -Path $item.Destination -Force | Out-Null
        Copy-Item -Path (Join-Path $item.Source '*') -Destination $item.Destination -Recurse -Force
    }
}

# Keep the packaged Fontconfig cache with the application, using the process
# XDG_CACHE_HOME set by AppPaths instead of LocalAppData or the user's home.
$fontconfigFile = Join-Path $output 'fontconfig\fonts.conf'
if (Test-Path -LiteralPath $fontconfigFile) {
    [xml]$fontconfig = Get-Content -LiteralPath $fontconfigFile -Raw
    foreach ($node in @($fontconfig.SelectNodes('//cachedir'))) {
        [void]$node.ParentNode.RemoveChild($node)
    }
    $node = $fontconfig.CreateElement('cachedir')
    $node.SetAttribute('prefix', 'xdg')
    $node.InnerText = 'fontconfig'
    [void]$fontconfig.DocumentElement.AppendChild($node)
    $fontconfig.Save($fontconfigFile)
}

if ($VisualCppRedistDirectory) {
    if (-not (Test-Path -LiteralPath $VisualCppRedistDirectory)) {
        throw "VC++ redistributable directory does not exist: $VisualCppRedistDirectory"
    }
    Copy-Item -Path (Join-Path $VisualCppRedistDirectory '*.dll') -Destination $output -Force
}

Write-Host "Deployment completed: $output"
