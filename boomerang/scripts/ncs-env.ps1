# NCS environment activation for -driven builds.
# Sets the PATH and env vars the nRF Connect SDK / Zephyr build needs
# without requiring nrfutil toolchain-manager or the VSCode extension.
#
# Dot-source: . scripts\ncs-env.ps1
# Or run inline: . scripts\ncs-env.ps1; west build ...

$ncsToolchain = "C:\ncs\toolchains\b8b84efebd"
$ncsVersion   = "C:\ncs\v3.1.0"

# west entry point. Prefer the console-script shim, but fall back to
# `python -m west` when it is missing (2026-08-27: the west.exe shim under
# opt\bin\Scripts had gone even though the west 1.4.0 package was still
# installed -- a partial toolchain update). A global `west` function makes the
# fallback transparent, so the build one-liners do not change. NOTE the shim
# function eats a bare `--`, so pass compiler defines via an env var, not
# `west build ... -- -Dfoo`.
$westExe = "$ncsToolchain\opt\bin\Scripts\west.exe"
$westPy  = "$ncsToolchain\opt\bin\python.exe"
if (Test-Path $westExe) {
    Remove-Item function:west -ErrorAction SilentlyContinue
} elseif (Test-Path $westPy) {
    $env:CBPM_WEST_PY = $westPy
    function global:west { & $env:CBPM_WEST_PY -m west @args }
    Write-Warning "west.exe shim missing under $ncsToolchain -- using 'python -m west' fallback"
} else {
    Write-Error "neither west.exe nor python.exe found under $ncsToolchain"
    return
}
if (-not (Test-Path "$ncsVersion\zephyr")) {
    Write-Error "zephyr source tree not found under $ncsVersion"
    return
}

# Idempotent: repeated dot-sourcing in one long-lived shell must not grow
# PATH (2026-08-25: several same-window activations grew it until Windows
# child-process resolution broke -- ninja could no longer find cmd.exe).
if ($env:Path -notlike "*$ncsToolchain\opt\bin*") {
    $env:Path = (@(
        "$ncsToolchain\opt\bin",
        "$ncsToolchain\opt\bin\Scripts",
        "$ncsToolchain\opt\zephyr-sdk\arm-zephyr-eabi\bin"
    ) -join ";") + ";" + $env:Path
}

# The flash runner defaults to J-Link since 2026-08-17 (board.cmake fix:
# the set_ifndef was dead code below the runner includes, so nrfjprog had
# silently claimed the slot). Zephyr's jlink runner resolves JLink.exe via
# PATH, and the SEGGER installer does not add itself there (only Nordic's
# nrfjprog installer did) -- so put the newest installed SEGGER dir on
# PATH, mirroring the macOS install where the tools are symlinked into
# /usr/local/bin. Auto-detection matches flash-and-monitor.ps1.
$seggerRoots = @("C:\Program Files\SEGGER", "C:\Program Files (x86)\SEGGER")
$seggerDir = $null
foreach ($root in $seggerRoots) {
    if (Test-Path "$root\JLink\JLink.exe") { $seggerDir = "$root\JLink"; break }
    if (Test-Path $root) {
        $vers = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "JLink*" } | Sort-Object Name -Descending
        foreach ($d in $vers) {
            if (Test-Path (Join-Path $d.FullName "JLink.exe")) {
                $seggerDir = $d.FullName; break
            }
        }
        if ($seggerDir) { break }
    }
}
if ($seggerDir) {
    if ($env:Path -notlike "*$seggerDir*") {
        $env:Path = "$seggerDir;" + $env:Path
    }
} else {
    Write-Warning "SEGGER JLink.exe not found; 'west flash' (jlink runner) will fail. Install the J-Link tools or flash with --runner nrfjprog."
}

$env:ZEPHYR_BASE             = "$ncsVersion\zephyr"
$env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
$env:ZEPHYR_SDK_INSTALL_DIR  = "$ncsToolchain\opt\zephyr-sdk"
