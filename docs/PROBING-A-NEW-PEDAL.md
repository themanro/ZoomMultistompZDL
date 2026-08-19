# Probing a Zoom pedal this project has never seen

Everything in this repository was reverse-engineered against **one** pedal: an
MS-70CDR on firmware 2.10, device id `0x61`. The `.ZDL` container, the 146-byte
patch layout, every bit offset in the editor's tables — all of it is specific to
that machine.

The `+` models (**MS-50G+, MS-60B+, MS-70CDR+**) use a different container,
`ZD2`, so nothing here loads on them. People ask fairly often whether they could
ever be targeted, and the honest answer is that nobody in this project has held
one. What follows is how to find out, in about an evening, without risking your
pedal.

## The question that actually decides it

Not "can the format be understood" — it probably can. `docs/STATE-ABI-PROGRESS.md`
records a **hand-decoded ZD2 `Fx_SFX_LineSel`**, meaning someone has already read
ZD2 machine code and made sense of it. It is not an opaque blob.

The real question is different:

> The MS-70CDR is hackable because **Zoom themselves ship a tool that writes
> effect binaries to it.** None of this work is an exploit — Effect Manager is
> the loading mechanism, and this project just learned to speak its file format.

So for a `+` pedal, what matters is whether an official write path still exists.
If it does, custom effects are a reverse-engineering job of the same shape as
this one. If it does not, they become firmware modification, which is a
completely different and much harder proposition.

The three checks below establish, in order: whether the payload is readable,
whether the pedal talks, and whether it will hand over a patch.

---

## Check 1 — Is the effect binary readable, or encrypted?

Do this one first. If a `.ZD2` is encrypted or signed, nothing else matters.

**Get a file.** `.ZD2` effects live wherever Zoom's own software keeps them on
your machine — search your drive for `*.ZD2`. Any one will do; two or three is
better, because a single file is not a sample.

**Run the inspector.** No install beyond Python 3:

```bash
python3 build/inspect_zd2.py /path/to/Effect.ZD2
```

**Read the spread, not the single number.** Real code is *uneven*: headers and
tables are low entropy, instruction streams middling. Encryption and compression
are flat and high everywhere, and that flatness is the tell.

A known-plain file — one of this repo's own ZDLs — looks like this:

```
  FOUND b'\x7fELF' at offset 76 — ELF object — same family as ZDL; disassemblable
  entropy, whole file: 5.94 bits/byte  (8.00 = indistinguishable from random)
  per-block (1024B, 10 blocks): min 1.97  max 6.54  spread 4.57
  -> UNEVEN. Typical of real code and data. Analysable.
  zero-run bytes: 18.8%  (encryption leaves almost none)
```

Encrypted or compressed data looks like this instead:

```
  entropy, whole file: 8.00 bits/byte  (8.00 = indistinguishable from random)
  per-block (1024B, 39 blocks): min 7.79  max 7.85  spread 0.06
  -> FLAT AND HIGH everywhere. Consistent with encryption or compression.
  zero-run bytes: 0.0%  (encryption leaves almost none)
```

The tool is validated against three controls: a known-plain ZDL, `/dev/urandom`,
and a gzip stream.

| Result | What it means |
|---|---|
| **UNEVEN**, ELF or other structure found | Plain code. This is the expected answer, and the encouraging one. |
| **FLAT AND HIGH**, no structure | Encrypted or compressed. Note that compression is *not* a lock — if a container tag or a `gzip` magic shows up, unpack it and inspect what is inside before concluding anything. |
| inconclusive | Try more files. |

Report the **output**, not a conclusion. The breakdown is what decides this.

---

## Check 2 — Does the pedal talk at all?

Open the patch editor:

**https://themanro.github.io/ZoomMultistompZDL/tools/patch_editor.html**

Connect the pedal over USB, put it in normal mode with Effect Manager **closed**,
and click **Connect MIDI**. Then open **Show diagnostics** and look for the
identity line in the log:

```
identity: f0 7e 00 06 02 52 61 00 00 00 32 2e 31 30 f7
                          ^^ device id      ^^^^^^^^^^^ firmware, ASCII
```

`52` is Zoom. The next byte is the device id — `61` is the MS-70CDR. A `+` model
will report something else.

**The editor is safe here.** When it sees a device id other than `0x61` it:

* adopts that id so reads are addressed correctly — otherwise the pedal ignores
  every request and the editor merely looks broken;
* switches to **read-only** and blocks every write command (`0x28`, `0x31`,
  `0x32`, `0x33`) outright.

Writes are blocked rather than warned about, because every byte offset in this
editor was mapped against one pedal and writing them elsewhere would scramble
patches on hardware nobody has mapped.

| Result | What it means |
|---|---|
| An identity reply appears | The pedal speaks the universal MIDI identity request. Note the device id and firmware. |
| Nothing | Either it does not answer identity requests, or it does not present a MIDI port at all. Both are useful facts. |

---

## Check 3 — Will it hand over a patch?

Still in the editor, click **Re-read patch**, or **Reload bank** for all 50.

> Reload bank makes the pedal cycle through every patch **out loud**. Mute your
> amp first.

If a dump comes back, the log says so and the diagnostics will contain the raw
146 bytes. **On an unmapped model that is the single most valuable thing you can
produce.** It tells us the patch protocol is shared, and the bytes themselves are
the start of mapping the layout.

Do not be discouraged if the slots decode as nonsense — the editor is applying
MS-70CDR bit tables to a pedal they were not built for. The raw bytes are the
point, not the interpretation.

| Result | What it means |
|---|---|
| A 146-byte dump | The patch protocol carries over. Editor support is plausible well before custom effects are. |
| A dump of a different length | Related protocol, different patch format. Still very useful — report the length. |
| Silence | The command differs or is gone. |

---

## Reporting

Open an issue with:

1. the pedal model and firmware version,
2. `inspect_zd2.py` output for two or three effect files,
3. the editor's **Copy diag** output after connecting and attempting a read.

Raw output beats a summary. Several dead ends in this project came from someone
(usually the author) reporting a conclusion instead of the data, and the
conclusion being wrong.

## What none of this tells you

Even if all three checks come back positive, that establishes only that the
pedal is *analysable and talks*. It says nothing about whether effect binaries
can be written to it, which is the part that decides whether custom effects are
possible at all. That needs someone to look at what Zoom's own tool for the `+`
models does — and if it turns out there is no write path, the honest answer is
that this approach does not transfer.
