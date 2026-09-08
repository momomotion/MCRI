# Hardware, toolchain and flashing

Everything you need to get from a bare board to a running image, and the four
failures that account for most first days.

## The board

**Seeed XIAO nRF52840 Plus.** An nRF52840 module: 64 MHz Cortex-M4F, 256 KiB
RAM, 1 MB flash, Bluetooth Low Energy radio, USB device controller. Thumb-sized,
castellated edges, and it ships with the Adafruit UF2 bootloader already in
flash.

The board target we build for is upstream Zephyr's `xiao_ble/nrf52840`. The
"Plus" variant breaks out more pads than the plain XIAO but the core pin map is
the same, which is why the stock board definition works. If you ever need pins
the stock definition does not describe, that is the moment to write a board
definition of your own -- not before.

Pads this firmware uses:

| Pad | nRF pin | Use |
|-----|---------|-----|
| D6 | P1.11 | UART TX to Roo |
| D7 | P1.12 | UART RX from Roo |
| -- | P0.26 | on-board red LED (heartbeat) |
| SWDIO / SWCLK | -- | debug probe (underside pads on the Plus) |

For the M3 loopback check, a single jumper wire from D6 to D7 makes the board
talk to itself.

## Toolchain

nRF Connect SDK **v3.1.0**. Installing it, Python and the J-Link software is
covered step by step in [setup.md](setup.md); this section is the part you use
every day afterwards.

Nothing from the SDK lands on your PATH by itself. Activate it in every
terminal that builds:

```sh
source scripts/ncs-env.sh          # macOS or Linux, bash or zsh
```

```powershell
. .\scripts\ncs-env.ps1            # Windows
```

You should see `NCS v3.1.0 activated`. Check it took:

```sh
west --version
arm-zephyr-eabi-gcc --version
```

**Do not have a Python virtual environment active while building.** CMake
prefers `$VIRTUAL_ENV` for `find_package(Python3)` whatever PATH says, and a
venv without Zephyr's build dependencies fails partway through with
`ModuleNotFoundError: No module named 'pykwalify'`. Run `deactivate` first. The
host tools do not need the venv activated either -- call the interpreter by
path: `.venv/bin/python tools/dongle_probe.py`.

## Debug probe and flashing

A SEGGER J-Link wired to SWDIO, SWCLK, GND and (for target detection) the
board's 3V3. Install the "J-Link Software and Documentation Pack" from
segger.com for the drivers and the `JLinkRTTLogger` command-line tool.

```sh
./scripts/flash.sh                 # build, flash, then stream the RTT log
./scripts/flash.sh --no-build      # flash what is already built
./scripts/flash.sh --no-rtt        # flash only
```

**Never chip-erase this board.** `west flash --erase` (and `--recover`) wipes
the whole of flash, which on this module includes the Adafruit UF2 bootloader,
the MBR, and the UICR configuration written at first boot. The board is then
mute -- no USB, no drag-drop recovery -- until someone reflashes the bootloader
over SWD. `flash.sh` refuses both flags on purpose. The application lives at
offset 0x27000, above the bootloader, and an ordinary flash writes only there.

## Reading logs

Logs go over RTT through the debug probe, not over the USB port, because the
USB port carries data and log text would corrupt it. `flash.sh` leaves a logger
running and writes to `.rtt/rtt-<timestamp>.log`.

Two things worth knowing about RTT:

- The J-Link is a single shared resource. A logger left running holds it and
  the next flash fails with "J-Link busy". `flash.sh` frees it for you.
- On macOS, `JLinkRTTLogger` has been seen to fail to find the RTT control
  block even when it is provably live. The sibling firmware repo works around
  this with a pylink-based capture script. If your log file stays empty on a
  Mac while the board is clearly running, that is the known bug, not you.

## Finding the serial port

```sh
python tools/dongle_probe.py --list
```

- macOS and Linux: `/dev/cu.usbmodemXXXX` (use the `cu.` form).
- Windows: `COMn`.

With both a J-Link and the dongle attached you will see several ports; the
J-Link presents its own. The probe identifies the dongle by its USB product
string (`CBPM dongle`, set in [prj.conf](../prj.conf)) and falls back to asking
you.

## Running the unit tests

```sh
west twister -T tests --platform unit_testing --inline-logs
```

The `unit_testing` platform compiles for your laptop rather than the board, so
it needs a working host C compiler, and it builds 32-bit. On Windows that is
awkward: MinGW-w64 cannot link the 32-bit harness. If `check.sh` reports
`M2 SKIP -- no usable host compiler`, that is why. Options, easiest first:

1. Run the tests under WSL with `gcc-multilib` installed.
2. Run them on macOS or Linux if you have one to hand.
3. Compile the pure logic by hand into a small program with `main()` and a few
   `assert()` calls, which needs nothing but a 64-bit gcc.

The firmware itself builds fine on Windows either way -- this affects only the
host-side tests.

## The four first-day failures

1. **`west: command not found`, seconds after a successful activation.** You
   re-sourced a virtual environment while it was already active; its
   `deactivate nondestructive` step restored PATH from a snapshot taken before
   the toolchain was added. Re-source `ncs-env.sh`; it is idempotent.

2. **The build cannot find the board.** Check the exact spelling:
   `xiao_ble/nrf52840`, with a slash. Board targets grew qualifiers in recent
   Zephyr versions and older tutorials use the bare name.

3. **The flash succeeds but nothing happens.** Look for the RTT banner first.
   If the log is empty, the probe may not be connected to a powered board --
   the J-Link needs the target's 3V3 to detect it, and a board powered only
   from USB with no ground to the probe is a classic.

4. **The port appears, then vanishes when you open it.** Usually a firmware
   crash on the first write. Check the RTT log for a fault dump; that is
   exactly the situation RTT logging exists for.
