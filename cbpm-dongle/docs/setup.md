# Setting up your machine

From a laptop with nothing installed to a green `M0 PASS`. Budget an afternoon
for the first time, most of it waiting for downloads.

Windows, macOS and Linux all work. Where they differ, the differences are
marked. If you get stuck for more than about twenty minutes on any step here,
ask -- setup problems are not a test of anything and there is no prize for
suffering through one alone.

Once you are installed, [hardware.md](hardware.md) is the day-to-day reference
for the board, the probe and flashing.

---

## 0. What you need

**Hardware**

- A Seeed XIAO nRF52840 Plus board and a USB cable.
- A SEGGER J-Link probe with four jumper wires (SWDIO, SWCLK, GND, 3V3).
- One extra jumper wire for the M3 loopback check.

**Access**

- The repository, either from MCRI GitLab or as a bundle handed to you.
- Roughly 15 GB of disk for the SDK and toolchain.

**Time**

An afternoon for the install. After that a clean build is about four minutes
and a flash under two, so a full edit-build-flash-test cycle is roughly five
minutes. That number shapes how you work: batch your changes, and lean on the
host-side unit tests, which run in about a second.

---

## 1. Git and the repository

Install git if you do not have it (`xcode-select --install` on macOS, your
package manager on Linux, <https://git-scm.com> on Windows), then set your
identity so your commits are attributable:

```sh
git config --global user.name "Your Name"
git config --global user.email "your.address@example.org"
```

Clone the repository, or unpack the bundle you were given:

```sh
git clone <repository url> cbpm-dongle
# or, from a bundle:
git clone cbpm-dongle.bundle cbpm-dongle
cd cbpm-dongle
```

Do not run `west init` anywhere. This repository is a freestanding Zephyr
application: it builds against an SDK you install separately, and creating a
west workspace around it causes a confusing failure where every `west build`
command stops existing.

---

## 2. VS Code and the nRF Connect SDK

The SDK is installed **through the editor extension**, which is the supported
route on all three platforms now. The old nRF Connect for Desktop Toolchain
Manager is deprecated.

1. Install **VS Code** from <https://code.visualstudio.com>.
2. Install the **nRF Connect for VS Code Extension Pack** from the marketplace.
3. On macOS, run "Shell Command: Install 'code' in PATH" from the command
   palette while you are there.
4. Open the nRF Connect side panel. Use **Manage toolchains** and **Manage
   SDKs** to install **nRF Connect SDK v3.1.0**. Take the matching toolchain
   when it offers.

This lays down two things:

- the SDK itself (Zephyr, Nordic drivers, the Bluetooth controller), and
- a self-contained toolchain bundle carrying its own west, Python, CMake,
  Ninja and the ARM compiler.

Where they land:

| OS | SDK | Toolchain bundle |
|----|-----|------------------|
| Windows | `C:\ncs\v3.1.0\` | `C:\ncs\toolchains\<id>\` |
| macOS, Linux | `/opt/nordic/ncs/v3.1.0/` | `/opt/nordic/ncs/toolchains/<id>/` |

The `<id>` is a content hash that differs per platform and per release, which
is why nothing in this repo hardcodes it.

**Version matters.** v3.1.0 is what the whole project is pinned to. A newer SDK
will mostly work and will occasionally fail in ways that waste a day, so match
it exactly.

### macOS only: one extra package

Nordic's bundled `nrfutil` links against Homebrew's `liblzma`, which macOS does
not ship. Without it you get `Library not loaded: .../liblzma.5.dylib`:

```sh
brew install xz
```

### Linux only: device permissions

The SEGGER installer ships udev rules for the J-Link; make sure they are in
place. For serial ports, add yourself to the group that owns them and then log
out and back in:

```sh
sudo usermod -aG dialout $USER      # or `uucp` on Arch
```

Symptom if you skip this: the port appears in the list and refuses to open with
a permissions error, which is easy to misread as a broken board.

---

## 3. Activating the toolchain

Nothing from that install lands on your `PATH` automatically. Activate it at
the top of **every terminal** that builds or flashes:

```sh
source scripts/ncs-env.sh          # macOS, Linux (bash or zsh)
```

```powershell
. .\scripts\ncs-env.ps1            # Windows PowerShell
```

You should see `NCS v3.1.0 activated`. Confirm it took:

```sh
west --version                     # West version: v1.4.0
arm-zephyr-eabi-gcc --version      # (Zephyr SDK 0.17.0) 12.2.0
```

The activation applies to the current shell only. A new terminal needs its own,
every time, forever. Most people put the command in their shell history and
press up-arrow; some add an alias.

---

## 4. Python for the host tools

You need Python 3.10 or newer for [tools/dongle_probe.py](../tools/dongle_probe.py),
which is half of nearly every milestone check.

```sh
python3 -m venv .venv
source .venv/bin/activate                  # Windows: .\.venv\Scripts\Activate.ps1
pip install -r tools/requirements.txt
deactivate
```

**Then leave the virtual environment deactivated.** Two traps, both of which
have cost real days on this project:

1. **Never build with a venv active.** CMake prefers `$VIRTUAL_ENV` when it
   looks for Python, whatever your `PATH` says, and your venv does not have
   Zephyr's build dependencies. The build dies partway through with
   `ModuleNotFoundError: No module named 'pykwalify'`, which looks nothing like
   the actual cause. Run `deactivate` first.

2. **Never re-activate a venv that is already active.** Its activation script
   begins by restoring `PATH` from a snapshot taken at the *first* activation,
   which was before the toolchain was added, so every toolchain entry silently
   disappears and you get `command not found: west` moments after a successful
   activation. Re-source `ncs-env.sh` to repair it; that is safe to repeat.

The simplest habit that avoids both: never activate the venv at all, and call
its interpreter by path when you need it.

```sh
.venv/bin/python tools/dongle_probe.py --selftest
```

```powershell
.\.venv\Scripts\python.exe tools\dongle_probe.py --selftest
```

That should print `PASS`. It needs no hardware, and it proves your Python side
agrees with the protocol before you have anything to talk to.

---

## 5. The J-Link probe

Install the **J-Link Software and Documentation Pack** from
<https://www.segger.com/downloads/jlink/>. It provides the drivers plus the
`JLinkRTTLogger` command-line tool that [scripts/flash.sh](../scripts/flash.sh)
uses to read logs.

Wire it to the board: SWDIO, SWCLK, GND, and 3V3 (the probe needs the target's
supply voltage to detect it, so all four matter). The XIAO nRF52840 Plus brings
the SWD pads out on the underside.

Confirm the probe is seen:

```sh
JLinkExe -NoGui 1
# then type: ShowEmuList
# then:      exit
```

On Windows the equivalent is `JLink.exe` from the Start menu.

---

## 6. First build

```sh
source scripts/ncs-env.sh
west build -b xiao_ble/nrf52840 -d build-dongle
```

The first build fetches nothing but compiles a lot -- about four minutes. Later
builds of unchanged code are seconds.

Then, with the board and probe connected:

```sh
./scripts/flash.sh                 # Windows: .\scripts\flash.ps1
```

It builds, flashes, and leaves a log streaming to `.rtt/rtt-<timestamp>.log`.
You are looking for:

```
*** Booting Zephyr OS ***
[00:00:00.001,000] <inf> main: CBPM dongle v0.1 starting
```

and the on-board LED blinking twice a second.

---

## 7. Prove the whole setup

```sh
./scripts/check.sh                 # Windows: .\scripts\check.ps1
```

Expect `M0 PASS`, `host PASS`, and either `M2 PASS` or `M2 SKIP`. Everything
else is skipped until you have implemented it -- that is what the ladder in
[MILESTONES.md](MILESTONES.md) is for.

**About that M2 row.** The unit tests compile for your laptop rather than the
board, so they need a host C compiler, and the platform builds 32-bit. On
Windows, MinGW-w64 cannot link that, and the row shows `SKIP`. Options, easiest
first:

1. Run the tests under WSL with `gcc-multilib` installed.
2. Run them on macOS or Linux if you have one.
3. Compile the pure logic by hand into a small program with `main()` and a few
   `assert()` calls, which needs nothing but an ordinary 64-bit gcc.

The firmware itself builds fine on Windows either way. Do not let a skipped M2
row stop you, but do find one of the three routes before you start M2 in
earnest: the tests are the fastest feedback you will get on the whole project.

---

## 8. Editor comfort (optional, worth ten minutes)

- **Code navigation.** The nRF Connect extension generates
  `build-dongle/compile_commands.json`. Point the C/C++ or clangd extension at
  it and "go to definition" starts working through the whole Zephyr tree, which
  changes how quickly you can answer your own questions.
- **The nRF Connect panel** gives you build, flash and a devicetree viewer from
  the sidebar. The command line stays the reference in this project because it
  is what the scripts and the docs use, but the devicetree viewer in particular
  is genuinely useful when a node is not resolving.
- **A serial terminal.** Any will do. `python -m serial.tools.miniterm <port>`
  is already installed with pyserial.

---

## When it goes wrong

The four failures that account for most first days are listed at the end of
[hardware.md](hardware.md). Beyond those:

| Symptom | Cause |
|---------|-------|
| `west: command not found` after activating | you re-activated an already-active venv; re-source `ncs-env.sh` |
| `ModuleNotFoundError: pykwalify` mid-build | a venv is active; `deactivate` |
| Board target not found | the spelling is `xiao_ble/nrf52840`, with a slash |
| Port opens then reads nothing | Linux permissions (section 2), or no program has opened the port |
| `bad interpreter: ...^M` | a shell script got CRLF line endings; check `.gitattributes` came with the clone |

Ask early. A question costs someone five minutes; a day lost to a setup problem
costs you a day and teaches you nothing about embedded systems.
