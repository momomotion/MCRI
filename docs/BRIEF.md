# Brief: the CBPM dongle

> Placeholders in **[square brackets]** are for your supervisor to fill in
> before handing this over. Everything else is written and ready.

Welcome. This document is the context the code deliberately does not carry: what
the thing is for, why it is built the way it is, and what a good outcome looks
like. Read it once now and once again after milestone M2, when it will make more
sense.

---

## The problem

Blood pressure is measured with a cuff that squeezes an arm. It works, it is
uncomfortable, and it gives you one number every few minutes at best. For an
adult in a clinic that is usually fine. For a child, and particularly for a
child who needs monitoring over hours rather than seconds, it is not: the cuff
wakes them, the readings are sparse, and a distressed child does not give you a
representative blood pressure.

**Cuffless** measurement estimates blood pressure from signals you can record
continuously and unobtrusively: the timing between the heart's electrical
activity (ECG) and the arrival of the resulting pulse at the skin (measured
optically, PPG), corrected for movement (an accelerometer) and temperature. Get
it right and you get a continuous trace instead of occasional numbers, from a
device a child can wear and forget.

That is what this project is building, at the Murdoch Children's Research
Institute. It is a real medical device programme, not an exercise, and the data
it collects will be used in research with actual paediatric participants.

---

## The system

Four pieces of firmware, of which the dongle is one:

```
   [Joey]  --wired UART-->  [Roo]  --BLE-->  [DONGLE]  --USB-->  laptop
   worn sensor node         control box                          researcher
                            + SD card
                                              [reference pressure unit]
                                              validation measurements
```

- **Joey** is the wearable. An nRF52840 reading the accelerometer, the optical
  front end and the ECG, streaming samples over a wire. It has no radio.
- **Roo** is the control box on the participant's belt. It takes Joey's stream,
  writes it to an SD card, and is the only thing that talks to the outside
  world.
- **The reference pressure unit** records a conventional blood pressure
  alongside, so the cuffless estimate can be validated against ground truth.
- **The dongle** -- yours -- is how a researcher's laptop reaches Roo.

## What the dongle is for, and why it exists

During a study session, a researcher needs to do ordinary things: check the
signals look sane before starting, set a configuration, start and stop a
recording, and afterwards pull the data off. Some of that happens across the
room over Bluetooth; some of it happens with a cable, at full speed, when
downloading a session.

Without a dongle, the laptop needs to be good at both, and laptops are
inconsistent about Bluetooth in ways you cannot control on a study day. With
one, the laptop sees **a single serial port**, always, and the dongle decides
whether the bytes go out over the radio or down a wire.

That is the whole idea, and it is worth holding on to, because every design
decision in the repo falls out of it: one port, two links, mode is a property of
the dongle rather than of the host software.

---

## What you are building

Seven milestones, described in [MILESTONES.md](MILESTONES.md), each ending in a
command that prints PASS:

| | | |
|---|---|---|
| M0 | It boots | given to you working -- prove it before changing anything |
| M1 | The host can see you | USB CDC-ACM, ring buffers, interrupt handlers |
| M2 | Speak the protocol | binary framing, CRC, unit tests |
| M3 | Bridge the wire | UART with DMA, the async API |
| M4 | Never lie about loss | bounded buffers, counters, honest failure |
| M5 | Radio | BLE central: scan, connect, discover, subscribe |
| M6 | Survive | watchdog, reset causes, retained RAM |

The skeleton is complete: every module has a header stating its contract in
full, and a `.c` file with the structure laid out and the bodies left as
`TODO(Mn)` blocks that describe the algorithm, the ordering constraints, and the
mistake people usually make. You are not being asked to guess. You are being
asked to implement, and to understand what you implemented.

**Timeframe: [fill in -- start date, end date, days per week].**
**Bench access: [fill in -- where the hardware lives, when you can use it].**

---

## Status of this work: learning first

Be clear about where you stand, because it cuts both ways.

The project needs a working dongle, and this repo is written to the real
contract, with the real protocol, on the real hardware. If your firmware is good
and lands in time, it can become the dongle the study uses. That is a genuine
possibility and not a consolation prize.

It is also **not a promise**, and the schedule does not depend on you. There is
no scenario where a study is delayed because a milestone slipped, and you should
not feel that pressure. Take the time to understand what you are doing. Working
slowly and understanding it beats working fast and not.

---

## How to work

**Follow the ladder in order.** It is not arbitrary. The wire comes before the
radio because a wire can be probed with an oscilloscope and a radio cannot. The
status channel comes early because every milestone after it is easier when the
board can tell you what it thinks is happening.

**Watch each check fail before you make it pass.** A test you have never seen
fail might not be testing anything. This takes ten seconds and it is the single
habit most worth building now.

**Batch your changes.** A build and flash cycle is about five minutes. Make
several related changes, then flash once. The host-side unit tests run in about
a second, so anything you can test there, test there.

**Use the log.** `scripts/flash.sh` leaves an RTT log streaming. When something
does not work, read it before you start changing code. Most embedded debugging
is reading evidence you already have.

**Ask early.** A question costs someone five minutes. A day lost to a setup
problem or a misread datasheet costs you a day and teaches you nothing. There is
no expectation that you work anything out alone, and asking is not a sign you
are struggling -- it is how everyone here works.

**Ask about it: [fill in -- who to ask, how, and what response time to expect].**
**Review cadence: [fill in -- e.g. a walkthrough of your code every Friday].**

---

## House rules

These are the project's, not mine, and they apply to everything you write:

- **British English** in comments, log strings, documentation and commit
  messages. `initialise`, `behaviour`, `colour`. ASCII only: `--` rather than an
  em dash, `->` rather than an arrow glyph.
- **Never `west flash --erase`** on this board. It wipes the bootloader and the
  chip's one-time configuration, and recovery is a job for someone with the
  right kit. The flash scripts refuse the flag; do not work around them.
- **The contract is not yours to change.** [contract.md](contract.md) is copied
  from the firmware repository and describes an interface that other people's
  code already implements. If you think something in it is wrong, you may well
  be right -- say so, and we will change it in the right place. Do not change it
  locally, because then your dongle works and nobody else's does.
- **Comment the why, not the what.** That `k_msgq_put` was called is obvious
  from the line. Why its timeout is `K_NO_WAIT` is not, and that is the comment
  worth writing.
- **Commit in small pieces with real messages.** One milestone is several
  commits. "fix stuff" tells the next person nothing, and the next person is
  usually you in three weeks.

---

## What good looks like

Nobody is counting lines. What is actually valued here, in order:

1. **You can explain what your code does and why.** If you cannot walk someone
   through a function you wrote yesterday, it is not finished, however green the
   check is.
2. **You are honest about what is not proven.** "M3 passes the loopback but I
   have never tested it against a real Roo" is a better sentence than silence.
   In a regulated medical project, claiming more than you have evidence for is
   the actual sin -- more so than a bug.
3. **Your failures are visible.** Code that drops data and says so is better
   than code that drops data quietly. That is what M4 is about, and it is the
   most important idea in the whole ladder.
4. **You read before you asked, and asked before you were stuck for a day.**
5. **The next person can pick it up.** Comments, commit messages, and a note in
   the repository of anything you found out the hard way.

Things that are explicitly fine: not finishing all seven milestones; getting
something wrong and fixing it; rewriting a milestone you had already passed
because you understood it better the second time.

---

## Day one

1. Read this document.
2. [setup.md](setup.md) -- get your machine to `M0 PASS`.
3. [zephyr-crib.md](zephyr-crib.md) -- twenty concepts, nothing else. Skim it,
   then come back when a word bites.
4. [../src/main.c](../src/main.c) -- top to bottom. It is the map.
5. [MILESTONES.md](MILESTONES.md) -- and start M1.

[glossary.md](glossary.md) is there for every acronym in the building. Use it
freely; nobody was born knowing what a CCC descriptor is.
