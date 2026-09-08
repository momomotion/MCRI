<!--
  Copied from the CBPM firmware repo, docs/design/dongle-transport-contract.md,
  on 2026-09-08. This is the specification you are implementing.

  Treat it as read-only here. If something in it needs to change, change it in
  the firmware repo and copy it across again, so the two do not drift.

  Links below point at files in that repo, not this one.
-->

# Dongle transport contract: PC/Mac <-> dongle <-> Roo

Status: **DRAFT** (2026-09-03). First writing of the previously
unwritten dongle contract. The dongle is the single physical host
interface to Roo: one USB serial port on the host side, two links to
Roo on the device side. Everything marked PROPOSED needs team review;
byte-level ids are TBC until checked against the allocated verb space
in [scripts/gui/verbs.py](../../scripts/gui/verbs.py).

Related: [roo-pc-contract.md](roo-pc-contract.md) (record + verb
payloads, unchanged here),
[roo-ble-gatt-contract.md](roo-ble-gatt-contract.md) (the GATT service
the dongle consumes), [ble-scope.md](ble-scope.md) (topology),
[sys-fmea.md](sys-fmea.md) rows D01-D03 (design-FMEA controls this
contract bakes in).

## Topology

    PC / Mac  --USB-CDC-->  DONGLE  --BLE GATT (client)-->   Roo
                                    --UART (wired bridge)->  Roo

The host always talks to ONE serial port: the dongle's USB-CDC. The
BLE-vs-UART distinction is a **dongle mode**, not a different host
transport. The GUI's existing Transport abstraction maps directly:
`ble_inspect` -> dongle BLE mode; `usb_sd_download` -> dongle UART
mode. A thin dongle-protocol layer (mode select + status) sits on top
of the existing UsbTransport; the record and verb payloads themselves
are untouched.

## Hardware and stack

XIAO nRF52840 Plus under NCS/Zephyr v3.1.0 (same chip family and
board-root pattern as the Roo Zephyr port). USB device stack with one
CDC-ACM instance; BLE central via the SoftDevice Controller; one UARTE
for the wired Roo link.

## Modes

### MODE_BLE (default at power-up) -- PROPOSED

The everyday operator path during collection: spot check (status/SQI
lane), set config, start/stop measurement.

- Dongle connects as **central/client** to the `CBPM-Roo` GATT server
  per [roo-ble-gatt-contract.md](roo-ble-gatt-contract.md): subscribes
  to TX (Notify), writes verbs to RX.
- Notify payloads (COMPACT records, verbatim) are forwarded to CDC
  unchanged; host verb frames from CDC are forwarded to the RX
  characteristic unchanged. The host-side decoder needs no change:
  the length-framed COMPACT decoder and the re-sent-header re-sync
  rule apply as written in the GATT contract.
- BLE connect/disconnect events are surfaced to the host via the
  dongle status channel (below), never silently.

### MODE_UART

The debug/download path: live debug streaming, and post-session SD
download, wired to Roo.

- Transparent bidirectional bridge: Roo UART bytes -> CDC, CDC bytes
  -> Roo UART. No re-framing; the existing stream and verb framings
  pass through untouched. Post-session download uses the existing
  drain verbs end-to-end; parsing to a researcher-ready format runs
  host-side (decision on CSV-per-sensor vs HDF5 vs parquet remains
  open and is out of scope here).
- UART parameters mirror the Roo link configuration (1 Mbaud today);
  the dongle does not translate rates.

## Dongle-local control channel -- PROPOSED

Mode select and dongle status must not collide with traffic destined
for Roo, and the one-port model rules out a second CDC instance for
control. Proposal: a **dongle-reserved verb family** inside the
existing verb framing. The dongle intercepts verbs in its reserved id
band and never forwards them; every other byte is bridged. Roo-bound
verbs are unaffected because the band is allocated from unused id
space (TBC against verbs.py).

Verbs (ids TBC):

- `DONGLE_GET_STATUS` -> reply carries: firmware version, current
  mode, BLE link state (disconnected / scanning / connected + RSSI),
  UART link state, bridge counters (bytes each way, drops each way),
  reset cause of the dongle itself.
- `DONGLE_SET_MODE (BLE | UART)` -> ack; mode change tears down the
  inactive link cleanly and is reflected in the next status reply.

Open design question: interception requires the dongle to parse verb
boundaries on the CDC side. In MODE_UART pure transparency is
preferred for robustness; option (a) parse only opportunistically and
guarantee interception only when the host pauses Roo-bound traffic,
option (b) full verb-boundary tracking. PROPOSED: (a) -- mode changes
are operator actions, not in-band mid-stream events.

## Flow control and buffering

- Bounded bridge buffers in both directions, sized for the COMPACT
  decimated stream (BLE mode, low rate) and the 1 Mbaud drain burst
  (UART mode) against USB-CDC service latency.
- **Never silently drop**: overflow increments a per-direction drop
  counter reported in `DONGLE_GET_STATUS`; a drop event also raises a
  flag bit so a spot-check view can show "bridge lossy" rather than
  quietly thinning data (FMEA row D01).
- Download integrity does not depend on the bridge: drained records
  carry their own CRC and the drain protocol is ack-driven
  (ADR-0050), so a lossy bridge degrades throughput, not correctness.

## Reliability mechanisms (designed in from the first commit)

Per the FMEA design rows D01-D03 and the Joey mechanism-batch
boilerplate:

- Task watchdog on the bridge and BLE threads; reset-cause read at
  boot and reported in `DONGLE_GET_STATUS`.
- Retained-RAM breadcrumb (last mode, last fault) across resets.
- Mode is explicit in every status reply -- no implicit mode state on
  the host (FMEA row D02).

## Deliberately deferred

- **Security**: no bonding/encryption for the bench, matching the
  GATT contract's posture; LE Secure Connections before any
  human-subject radio use.
- **Power**: dongle is USB-powered; no budget work needed now.
- **Composite USB alternatives** (second CDC for control, DFU
  interface): revisit only if the in-band control channel proves
  awkward in practice.

## Open questions

1. Verb id band allocation (check verbs.py allocated space).
2. MODE_UART interception guarantee: option (a) vs (b) above.
3. Whether MODE_BLE should auto-reconnect to the last Roo address or
   always scan by name (`CBPM-Roo`) -- multi-Roo bench days argue for
   name + address pinning via a future `DONGLE_SET_TARGET`.
4. Researcher-ready download format (host-side; out of scope here but
   the download UX depends on it).

## Decision

Pending. Lands as an ADR once the team reviews the PROPOSED items and
the first dongle firmware implements the two modes end-to-end.
