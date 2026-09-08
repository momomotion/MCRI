# Zephyr in twenty concepts

Not a tutorial. This is the subset of Zephyr this repo actually uses, in the
order you will meet it, so you can read the code on day one and look the rest
up when you need it.

If you have used Arduino or PlatformIO: the biggest adjustment is that Zephyr is
an operating system with a configuration system in front of it. Roughly half of
early confusion is not C at all, it is the build system deciding your code out
of existence.

---

## The two configuration systems, and how to tell them apart

**Devicetree** describes the hardware that exists: which peripherals are
enabled, on which pins, at what speed. It lives in `.dts` and `.overlay` files.
The board ships one; you add an overlay to change it
([boards/xiao_ble_nrf52840.overlay](../boards/xiao_ble_nrf52840.overlay)).

**Kconfig** decides which software is compiled: drivers, subsystems, your own
options. It lives in `prj.conf` and `Kconfig`.

Nearly every "the driver is not there" hour is one of these two: a Kconfig
option enabled for a peripheral whose devicetree node is still disabled, or a
devicetree node enabled with no driver compiled to run it. When something is
missing, ask *which of the two* is missing before you start reading code.

Useful when you are stuck: `build-dongle/zephyr/.config` is the final resolved
Kconfig, and `build-dongle/zephyr/zephyr.dts` is the final resolved devicetree.
Both are worth opening. They tell you what the build actually decided, rather
than what you meant.

---

## Getting at hardware from C

```c
static const struct device *const dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
```

`DT_NODELABEL(uart0)` names a devicetree node; `DEVICE_DT_GET` turns it into a
device pointer **at compile time**. If the node does not exist the build fails,
which is much better than a null pointer on a bench. Always check
`device_is_ready(dev)` before first use.

`DT_ALIAS(led0)` is the same idea through an alias, which is how portable
samples find "the LED" on boards that have different pins.

`DT_PROP(node, current_speed)` reads a property. Note the underscore: hyphens in
devicetree become underscores in C.

---

## Threads

```c
K_THREAD_DEFINE(tid, STACK_SIZE, entry_fn, NULL, NULL, NULL, PRIO, 0, DELAY);
```

Created at build time, with its own stack. A lower priority number is more
urgent. `DELAY` is milliseconds before it starts, or `K_TICKS_FOREVER` to leave
it stopped until someone calls `k_thread_start()` -- which is what
[bridge.c](../src/bridge/bridge.c) does, so that no traffic moves before the
links exist.

Threads that outlive their initialisation dependencies are a common source of
"works on the second run".

---

## Interrupt context

Driver callbacks -- the UART ISR, the async UART events, the Bluetooth
callbacks -- run in interrupt context. There, you must not:

- block (no `k_sleep`, no waiting on a mutex or a full queue),
- log more than sparingly,
- do anything slow.

What you *can* do is copy bytes into a ring buffer and signal a semaphore. That
constraint is why the code is shaped the way it is, and it is the single most
useful rule in the whole framework.

---

## Ring buffers and semaphores

```c
RING_BUF_DECLARE(rb, 4096);
K_SEM_DEFINE(sem, 0, 1);
```

The ring buffer is the shock absorber between a fast producer and a slow
consumer. `ring_buf_put()` returns how many bytes it took, which may be fewer
than you offered -- that return value is where honest loss accounting starts.

The semaphore is how the ISR wakes the thread. Initial count 0, limit 1, used as
a "something happened" flag rather than a counter.

Read before you wait, always. If you wait first you will eventually sleep on a
signal that arrived a microsecond earlier, with data already in the buffer.

---

## Logging

```c
LOG_MODULE_REGISTER(mymodule, LOG_LEVEL_INF);
LOG_INF("value %u", x);
```

One `LOG_MODULE_REGISTER` per `.c` file. Logging is deferred by default: the
call packs its arguments and returns, and a separate thread formats and emits
them. That makes it cheap enough for ordinary code and still too expensive for
an ISR in a hot path.

Deferred logging has one consequence worth remembering: if the device resets or
wedges, the last second or so of log lines may never be emitted. The evidence
you most want is the evidence most likely to be missing, which is why the
breadcrumb in [sys_health.c](../src/sys/sys_health.c) exists.

---

## Errors

Zephyr returns negative errno values: `-ENODEV`, `-EBUSY`, `-ETIMEDOUT`,
`-ENOTSUP`, `-EAGAIN`. Check them. `-ENOTSUP` from a driver call almost always
means a missing Kconfig option rather than a broken driver.

---

## `__noinit`, and RAM that survives a reset

An ordinary global is zeroed at every boot. A `__noinit` variable is not: the
linker puts it in a section the start-up code leaves alone, so after a soft
reset it still holds what it held before. That is how the breadcrumb works. It
is also uninitialised garbage after a *power-on* reset, which is why it carries
a magic word to say whether it means anything.

---

## Build outputs worth knowing

```
build-dongle/zephyr/zephyr.hex     what gets flashed
build-dongle/zephyr/zephyr.uf2     drag-drop image for the bootloader
build-dongle/zephyr/.config        the resolved Kconfig
build-dongle/zephyr/zephyr.dts     the resolved devicetree
```

`west build -t rom_report` and `-t ram_report` break down where your flash and
RAM went. Worth running once out of curiosity: the Bluetooth stack is large,
and seeing that is more instructive than being told.

---

## Where to look things up

- Zephyr API docs: <https://docs.zephyrproject.org> -- pick the version that
  matches (Zephyr 4.x for NCS v3.1.0), because APIs move.
- Samples: `$ZEPHYR_BASE/samples/`. `subsys/usb/cdc_acm`, `drivers/uart/echo_bot`
  and `bluetooth/central_ht` between them cover most of this project.
- The nRF Connect SDK adds its own subsystems under `nrf/`; the GATT Discovery
  Manager mentioned in [ble_central.c](../src/ble/ble_central.c) is one.
- The sibling firmware repos in this project are the best reference of all,
  because they solve these exact problems on this exact chip.
