# Glossary

Project words and protocol words, in one place, because looking up an acronym
is not the same as learning anything and you should not have to.

## The system

**Joey** -- the wearable sensor node. An nRF52840 that reads the accelerometer,
the optical (PPG) sensor and the ECG front end, and streams samples over a wired
UART. It has no radio of its own.

**Roo** -- the control box. Takes Joey's stream, writes it to an SD card, and
talks to the outside world over Bluetooth or USB. Also an nRF52840.

**Dongle** -- this device. The laptop-side end of the link to Roo.

**Reference pressure unit** -- a fourth board, used to record a reference blood
pressure alongside the wearable's data for validation.

**COMPACT** -- the record format the sensor stream is carried in. The dongle
never parses it; it just moves the bytes.

**Verb** -- one command from the laptop, such as ping, start a session, or set a
configuration. Verbs travel in the 0xC0 framing described in
[../src/ctrl/verb_frame.h](../src/ctrl/verb_frame.h).

## Protocol and transport

**CDC-ACM** -- the USB device class that makes hardware appear as a serial port.
`/dev/cu.usbmodemXXXX` on macOS, `COMn` on Windows. There is no UART behind it,
so the baud rate you set on the host is ignored.

**UART / UARTE** -- an asynchronous serial link: two data wires, no clock, both
ends agreeing on a bit rate in advance. UARTE is Nordic's version with DMA
("EasyDMA"), which is what makes 1 Mbaud practical.

**Baud** -- bits per second on the wire. At 1 Mbaud with the usual framing you
get about 100 000 bytes per second, because each byte costs ten bit-times.

**Flow control** -- extra signalling that lets a receiver say "stop". This link
does not have it, which is why buffer sizing and honest drop counting matter.

**CRC** -- a checksum that detects corruption. CRC-16/CCITT-FALSE here: poly
0x1021, initial value 0xFFFF, no reflection, no final XOR. All four details are
part of the definition and getting one wrong yields numbers that look right and
never match.

**Framing** -- the rules that turn a stream of bytes back into messages: where
one starts, how long it is, how you recover after noise.

**Re-synchronisation** -- finding the start of the next message after losing
track. Here: skip one byte and look for the sync byte again.

## Bluetooth

**BLE** -- Bluetooth Low Energy. A different protocol from classic Bluetooth
despite the name.

**Central / peripheral** -- the central scans and initiates connections; the
peripheral advertises and waits. The dongle is central, Roo is peripheral.

**GATT** -- the data model on a BLE connection: services contain
characteristics, each with a UUID and a value.

**Characteristic** -- one value you can read, write or subscribe to.

**Notify** -- the peripheral pushes a characteristic value to the central when
it changes. How the sensor stream flows.

**CCC** -- the small descriptor attached to a characteristic that you write to
turn notifications on. Subscribing means writing to the CCC.

**MTU** -- the largest payload one ATT packet can carry, negotiated per
connection. Bigger than the 23-byte minimum in practice, but never assume.

**RSSI** -- received signal strength, in dBm. Always negative; closer to zero is
stronger.

## Zephyr and the toolchain

**Zephyr** -- the real-time operating system this firmware runs on.

**NCS** -- nRF Connect SDK, Nordic's distribution of Zephyr plus their own
drivers, the Bluetooth controller and tooling. We pin v3.1.0.

**west** -- the command-line tool that drives builds, flashing and the test
runner.

**Kconfig** -- the build-time configuration system that decides which code is
compiled. `prj.conf`.

**Devicetree** -- the description of what hardware exists and how it is wired.
`.dts` and `.overlay`.

**Twister** -- Zephyr's test runner. `unit_testing` is its host platform, which
compiles test code for your laptop rather than the board.

**ztest** -- the assertion framework the tests are written in.

**SWD** -- Serial Wire Debug, the two-wire debug interface on ARM chips. What
the J-Link connects to.

**J-Link** -- the SEGGER debug probe used to flash and to read logs.

**RTT** -- Real Time Transfer, SEGGER's mechanism for shipping log text out
through the debug probe with almost no cost to the running firmware. A buffer in
RAM that the probe reads over SWD.

**UF2** -- the drag-and-drop firmware format the XIAO bootloader accepts. An
alternative to SWD flashing, and the reason the application starts at flash
offset 0x27000 rather than 0.

**MBR / UICR** -- Nordic-specific regions at the bottom of flash holding the
master boot record and one-time chip configuration. Both are destroyed by a chip
erase, which is why this project forbids one on this board.

**Task watchdog** -- Zephyr's software layer over the hardware watchdog timer.
Several threads each check in on their own channel; the hardware timer is fed
only while all of them are healthy.
