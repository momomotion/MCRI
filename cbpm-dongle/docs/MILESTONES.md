# Milestones

Seven rungs. Each one is roughly a sitting, each ends in a command that prints
PASS, and each makes the next one debuggable. Do them in order even where the
order looks arbitrary -- it is not. The wire comes before the radio because a
wire can be probed; the status channel comes early because every later
milestone is easier when the board can tell you what it thinks is happening.

Run `./scripts/check.sh` (or `.\scripts\check.ps1`) at any point to see where
you are. Rows above your current milestone are expected to fail.

A note on how to work: **watch each check fail before you make it pass.** A
test you have never seen fail might not be testing anything. This is a habit
worth building now, on a small system, rather than later on a large one.

---

## M0 -- It boots

**Goal:** prove your toolchain, your board and your logging path, so that every
later failure is your code rather than your setup.

Nothing to implement. Build, flash, and read the log:

```sh
source scripts/ncs-env.sh
west build -b xiao_ble/nrf52840 -d build-dongle
./scripts/flash.sh
```

**Check:** the RTT log contains `CBPM dongle v0.1 starting`, the LED blinks
twice a second, and `./scripts/check.sh` shows `M0 PASS`.

**Read while you are here:** [src/main.c](../src/main.c) top to bottom. It is
the map of everything else. Then [zephyr-crib.md](zephyr-crib.md) if any of the
vocabulary is new.

**If it does not work:** [hardware.md](hardware.md) has the wiring and the
flashing detail, and the four failures that account for most first days.

**Troubleshooting:**
1. If ```west build -b xiao_ble/nrf52840 -d build-dongle``` causes errors like "FATAL ERROR: command exited with status 1:", there may be an issue with the device tree label which is dependent on your version of Zephyr. \
  Under src/cdc/cdc_link.c, change ```DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));``` to ```DEVICE_DT_GET(DT_NODELABEL(board_cdc_acm_uart));``` or vice versa.

2. If running ```./scripts/flash.sh``` causes "zsh: permission denied: ./scripts/flash.sh", the script hasn't been given execute permission. \
   Run ```chmod +x scripts/flash.sh``` and try again. \
   This also applies for ```./scripts/check.sh```.

---

## M1 -- The host can see you

**Goal:** the board appears as a serial port on the laptop, and echoes.

**Implement:** the three `TODO(M1)` blocks in
[src/cdc/cdc_link.c](../src/cdc/cdc_link.c) -- the interrupt handler, the rest
of `cdc_link_init()`, and `cdc_link_write()`/`cdc_link_read()`. Then wire an
echo temporarily into the bridge pump: read a chunk, write it straight back.

**Check:**

```sh
python tools/dongle_probe.py --list          # find the port
python tools/dongle_probe.py --port <port> --echo
```

**What you are actually learning:** the split between an interrupt handler that
only moves bytes and a thread that does the thinking. Every driver you meet in
Zephyr is shaped like this.

**Two failures nearly everyone hits:**

- *The port appears and then the board seems dead.* You enabled the TX
  interrupt permanently. With an empty buffer it re-fires immediately and
  forever, and nothing else gets to run. Disable it in the handler when the
  ring is empty.
- *Nothing comes back at all.* Check `cdc_link_host_present()`. A CDC port that
  no program has opened swallows everything you write.

---

## M2 -- Speak the protocol

**Goal:** the dongle answers questions about itself, in the same framing the
rest of the system already uses.

**Implement:** all of [src/ctrl/verb_frame.c](../src/ctrl/verb_frame.c) (the
CRC, the parser, the encoder), then the `TODO(M2)` blocks in
[src/ctrl/dongle_ctrl.c](../src/ctrl/dongle_ctrl.c) and the intercept call in
[src/bridge/bridge.c](../src/bridge/bridge.c).

**Check:**

```sh
west twister -T tests/verb_frame --platform unit_testing --inline-logs
python tools/dongle_probe.py --port <port> --status
```

The unit tests need no board and take about a second. Use them: a protocol bug
found on the laptop costs a minute, and the same bug found on hardware costs an
afternoon.

**What you are actually learning:** that a wire format is a contract with
someone you cannot talk to. The test vectors come from the host implementation,
so passing them means agreeing with a program written by someone else, months
ago, in another language. That is the ordinary condition of embedded work.

**Watch out for:** the re-synchronisation rule. When the first byte is not the
sync byte you skip exactly ONE byte, not the whole buffer. Sensor data contains
0xC0 bytes that are not headers, and a parser that throws away everything after
a false start loses real data.

---

## M3 -- Bridge the wire

**Goal:** bytes from the host go out of the UART, and bytes arriving on the
UART go back to the host.

**Implement:** the `TODO(M3)` blocks in
[src/uart/uart_link.c](../src/uart/uart_link.c) (the async callback and
`uart_link_write()`), and the forwarding block in
[src/bridge/bridge.c](../src/bridge/bridge.c).

**Check:** join pad **D6** to pad **D7** with a jumper wire, so the board's
transmitter feeds its own receiver, then:

```sh
python tools/dongle_probe.py --port <port> --loopback 65536
```

64 KiB, byte-for-byte. The loopback trick means you can finish this milestone
without a Roo anywhere near you.

**What you are actually learning:** DMA and double buffering. At 1 Mbaud a byte
arrives every 10 microseconds and there is no flow control on this link -- if
you are not ready, the byte is simply gone. The alternating-buffer dance in the
callback is what keeps the receiver running without gaps.

**The characteristic failure:** the first 512 bytes work and then everything
stops. You did not answer `UART_RX_BUF_REQUEST`, so the driver ran out of
buffer and disabled reception. Once you have seen this symptom you will
recognise it for the rest of your career.

---

## M4 -- Never lie about loss

**Goal:** when the bridge cannot keep up, it says so.

**Implement:** the counters and the sticky lossy flag in
[src/bridge/bridge.c](../src/bridge/bridge.c) -- both directions, including the
bytes the CDC ring refuses.

**Check:**

```sh
python tools/dongle_probe.py --port <port> --flood
```

The probe deliberately sends faster than the far end can take, then checks that
the drop counter moved and the flag is set. If nothing drops, shrink
`CONFIG_CBPM_DONGLE_RING_SIZE` and try again -- a counter that never moves has
not been proven to work.

**Also write a test.** Create `tests/bridge/` in the shape of
`tests/verb_frame/`: pull the accounting out into a small pure function
(`offered`, `accepted`, counters in, counters out) and test it on the host. The
useful skill is not the test itself, it is noticing which piece of a hardware
module can be made pure and therefore testable.

**Why this milestone exists at all:** the device this dongle serves records
physiological data from children. A bridge that quietly thins a data stream
produces a recording that looks complete and is not, and nobody finds out until
someone tries to analyse it months later. Losing data under overload is
sometimes unavoidable. Hiding it never is.

---

## M5 -- Radio

**Goal:** the same bridge, over Bluetooth, with no change on the host side.

**Implement:** the `TODO(M5)` blocks in
[src/ble/ble_central.c](../src/ble/ble_central.c) -- scan by name, connect,
discover the service, subscribe to TX, write to RX. Then fix the deliberate lie
in [src/ctrl/dongle_ctrl.c](../src/ctrl/dongle_ctrl.c): the contract says
MODE_BLE is the power-up default.

Build with the radio on:

```sh
west build -b xiao_ble/nrf52840 -d build-dongle -- -DCONFIG_CBPM_DONGLE_BLE=y
```

**Check:** with a Roo powered nearby,
`python tools/dongle_probe.py --port <port> --status` shows `BLE connected` and
a plausible RSSI. With no Roo present it must show `scanning` and keep saying so
-- a status that claims a link it does not have is worse than no status.

**What you are actually learning:** that BLE is a database, not a pipe. You
connect, then you look up handles, then you subscribe. Most of the code is
discovery, and once you have written it once the whole subsystem stops looking
mysterious.

**Do not start this before M3 works.** Debugging a radio and a bridge together
is two unknowns.

---

## M6 -- Survive

**Goal:** if a thread wedges, the device reboots, comes back, and admits what
happened.

**Implement:** the `TODO(M6)` blocks in
[src/sys/sys_health.c](../src/sys/sys_health.c), the watchdog registration and
feeding in the bridge pump, and the `DONGLE_VERB_DEBUG_STALL` handler.
Uncomment `CONFIG_WATCHDOG` and `CONFIG_TASK_WDT` in
[prj.conf](../prj.conf).

**Check:**

```sh
python tools/dongle_probe.py --port <port> --wedge
```

The board should reset within the watchdog period, re-enumerate, and report a
watchdog reset cause with the breadcrumb intact.

**Think before you choose the period.** The bridge pump blocks waiting for host
traffic, which can legitimately be silent for minutes. A watchdog keyed to "how
long since bytes arrived" reboots an idle, healthy dongle -- and a watchdog that
fires on correct behaviour gets switched off, which is how you end up with
neither. Feed on every loop iteration including the timeout path.

This project has a real scar from the opposite mistake: on the sensor node a
liveness heartbeat advanced only on a *successful* queue write, so ordinary
back-pressure read as a dead sensor and reset the board every few seconds. The
fix was one line; finding it was a fortnight.

---

## Afterwards

If you want more, in rough order of usefulness:

1. **Make the mode change safe.** What happens to bytes already in flight when
   the host switches from BLE to UART mid-burst? The contract says the inactive
   link is torn down cleanly; define what "cleanly" means and implement it.
2. **A proper board definition.** Replace the overlay with
   `boards/mcri/dongle/`, in the shape of the Roo V5 board root. Board
   definitions are a distinct skill and this is a small, safe one to learn on.
3. **Write the ADR.** The contract has four open questions
   ([contract.md](contract.md)). Pick one, decide it, and write up why in the
   form the rest of the project uses. Being able to argue a design decision in
   writing matters more than any of the code above.
