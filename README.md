# CBPM dongle firmware

The dongle is the single physical interface between a researcher's laptop and
the **Roo** control box. One USB cable on the laptop side, two possible links
to Roo on the device side:

```
    PC / Mac  --USB-CDC-->  DONGLE  --BLE GATT (client)-->  Roo
                                    --UART (wired)------->  Roo
```

The laptop always sees **one serial port**. Whether the bytes reach Roo over
the radio or over a wire is a *dongle mode*, not a different host program.
That single idea is the whole design, and everything in this repo exists to
serve it.

Hardware: Seeed XIAO nRF52840 Plus. Stack: nRF Connect SDK (Zephyr) v3.1.0.

---

## You are here to implement this

The repo is a **skeleton with the answers removed**. Every module has a header
that states its contract in full, a `.c` file with the structure laid out and
the body left as `TODO(Mn)`, and a check that tells you when you have got it
right.

Work through [docs/MILESTONES.md](docs/MILESTONES.md) in order. Each milestone
is roughly one sitting and ends with a command that prints PASS or FAIL.

| # | Milestone | You will have learnt |
|---|---|---|
| M0 | It boots | Zephyr build, flash, RTT logs, devicetree, threads |
| M1 | The host can see you | USB device stack, CDC-ACM, ring buffers, interrupt-driven UART API |
| M2 | Speak the protocol | Binary framing, CRC16, parsing bytes into meaning, unit tests |
| M3 | Bridge the wire | UARTE with DMA, the async API, two-way data pumps |
| M4 | Never lie about loss | Bounded buffers, back-pressure, counters, honest failure |
| M5 | Radio | BLE central: scan, connect, discover, subscribe, write |
| M6 | Survive | Task watchdog, reset causes, retained RAM across a reset |

M0 already works. Prove it before you change anything.

---

## First five minutes

New here? Start with [docs/BRIEF.md](docs/BRIEF.md) for what this device is for
and what you are being asked to do, then [docs/setup.md](docs/setup.md) to get
your machine from nothing to a green M0.

If your machine is already set up, you need the XIAO board, a J-Link with SWD
wired to it, the NCS v3.1.0 toolchain, and Python 3.10 or newer.

```sh
source scripts/ncs-env.sh                 # Windows: . .\scripts\ncs-env.ps1
west build -b xiao_ble/nrf52840 -d build-dongle
./scripts/flash.sh                        # Windows: .\scripts\flash.ps1
./scripts/check.sh                        # the PASS/FAIL table
```

If the banner appears in the RTT log and `check.sh` prints `M0 PASS`, your
toolchain is sound and every later failure is your code rather than your setup.
That distinction is worth ten minutes of anyone's time.

Wiring, flashing and probe detail: [docs/hardware.md](docs/hardware.md). New to
Zephyr? Read [docs/zephyr-crib.md](docs/zephyr-crib.md) first -- it is the
twenty concepts this repo actually uses, and nothing else.

---

## Repo map

```
src/main.c        start here: init order, the thread map, the whole story
src/app.h         shared types: modes, status, the numbers on the wire
src/cdc/          USB CDC-ACM: the one port the host sees
src/uart/         UARTE to Roo, 1 Mbaud, DMA
src/ble/          BLE central: the radio path to Roo
src/bridge/       the two-way pump and its bounded buffers
src/ctrl/         verb framing, and the dongle's own control verbs
src/sys/          reset cause, breadcrumb, watchdog
tests/            host-compiled unit tests (no board needed)
tools/            dongle_probe.py: talk to your firmware from the laptop
docs/             brief, setup, milestones, hardware, crib sheet, glossary,
                  and the transport contract you are implementing
scripts/          environment, build, flash, check
```

## House rules

- **British English** in comments, log strings, docs and commit messages
  (`initialise`, `behaviour`, `colour`). ASCII only: `--` not an em dash,
  `->` not an arrow glyph.
- **Never `west flash --erase` this board.** It wipes the UF2 bootloader, the
  MBR and the UICR, and you will need an SWD recovery to get the board back.
  `scripts/flash.sh` refuses the flag for you.
- **Comment the why, not the what.** `k_msgq_put` is obvious; why the timeout
  is `K_NO_WAIT` is not.
- **A test that never fails is not a test.** When you implement a milestone,
  first watch its check fail, then make it pass.

## Where this fits

This dongle is one of four firmware components in the CBPM system (Joey the
sensor node, Roo the control box, the reference pressure unit, and this). The
protocol it carries is fixed by contracts written elsewhere and copied into
[docs/contract.md](docs/contract.md); the framing and verb ids are not yours to
invent, which is deliberate -- interoperating with someone else's spec is most
of embedded work.
