# Homework - CSE 320 - MP3 Parser

## Introduction

In this assignment, you will write a program to interact with MP3 audio files.
The goals for this homework include: practice C programming with file I/O, binary
parsing, bitwise manipulation, and pointers by working with a real binary format.

**YOU MUST WORK ON THIS ASSINGMENT ON YOUR OWN. IF YOU SUBMIT CODE FROM ANOTHER STUDENT YOU WILL BE BROUGHT UP ON ACADEMIC DISHONESTY CHARGES!**

**THIS ASSIGNEMNT IS EXPECTED TO TAKE A LOT OF TIME, JUST BECAUSE YOU HAVE 4 WEEKS DOES NOT MEAN YOU SHOULD PROCRASTINATE**

Your assignment will be graded by codegrade and will feature some of the tests provided in this repository as well as other tests that will be hidden until after the deadline for the assignment has passed. Make sure to test your code above and beyond the provided tests.

Your program is a command-line utility that opens MP3 files and processes queries
specified as command-line arguments. Results are printed to standard output.
On success, exit with `EXIT_SUCCESS` (0). On error, print a one-line message to
stderr and exit with `EXIT_FAILURE` (1).

For simplicity, **do not put library functions in `main.c`**. That file
should only contain `#include`s, local `#define`s, and `main`.

## Features at a Glance


| CLI flag                   | Feature                                      | Module          |
| -------------------------- | -------------------------------------------- | --------------- |
| `-f`                       | Input file (required)                        | —               |
| `-h`                       | Help message                                 | `global.h`      |
| `-s`                       | Section summary (ID3H, ID3F, MPEG, ID3V1)    | `mp3_reader.c`  |
| `-t`                       | ID3v1/v2 metadata (title, artist, album, …)  | `mp3_id3.c`     |
| `-i`                       | First MPEG frame header fields               | `mp3_reader.c`  |
| `-l`                       | Audio duration in seconds                    | `mp3_trim.c`    |
| `-p`                       | Timestamp of loudest audio                   | `mp3_trim.c`    |
| `-c start end -o out`      | Extract audio between timestamps             | `mp3_trim.c`    |
| `-m file2 -a start -o out` | Replace base audio with overlay at timestamp | `mp3_overlay.c` |


**High-level pipeline for audio analysis/editing (`-l`, `-p`, `-c`, `-m`):**

1. Read the file and locate the MPEG audio region (skip ID3v2 prefix and ID3v1 suffix).
2. Decode MPEG bytes to PCM using the instructor-provided `mp3_codec.c` (minimp3 + LAME).
3. Manipulate PCM samples (measure length, copy a range, or splice in overlay audio).
4. Re-encode PCM to MPEG and write a new file, preserving the original ID3 tag bytes.

Parsing features (`-s`, `-t`, `-i`) operate directly on the file bytes and do not
require decoding audio. See the sections below for format details.

## Technical Details (Where to Read More)


| Topic                               | Location in this README                                            |
| ----------------------------------- | ------------------------------------------------------------------ |
| ID3v2/v1 layout, synchsafe integers | [Reading Hex Values](#debugging-and-reading-hex-values)            |
| MPEG frame header bit fields        | [MPEG Frame Header (`-i`)](#mpeg-frame-header--i)                  |
| Section types and offsets           | [Section Summary (`-s`)](#section-summary--s)                      |
| Frame size and side-info offsets    | [Section Parsing](#section-parsing-mp3_sectionsc)                  |
| PCM decode/encode API               | [Instructor-Provided Codec](#instructor-provided-codec-mp3_codecc) |
| Trim, duration, and loudest peak | [Audio Duration and Trim](#audio-duration-and-trim-mp3_trimc)      |
| Overlay replacement logic           | [Audio Overlay](#audio-overlay-mp3_overlayc)                       |
| Output and error macros             | `[global.h` macros](#globalh-output-and-error-macros)              |
| Debug logging (stderr only)         | `[debug.h` macros](#debugh-debug-macros)                           |


**External references:**

- [MP3 file structure (Wikipedia)](https://en.wikipedia.org/wiki/MP3#File_structure)
- [ID3v2.3 specification](https://id3.org/id3v2.3.0)
- [minimp3](https://github.com/lieff/minimp3) (used inside `mp3_codec.c`)

## Project Structure

```
.
├── include
│   ├── debug.h
│   ├── global.h
│   ├── mp3_codec.h      (instructor-provided API)
│   ├── mp3_id3.h
│   ├── mp3_overlay.h
│   ├── mp3_reader.h
│   ├── mp3_sections.h
│   ├── mp3_trim.h
│   └── util.h
├── third_party
│   ├── minimp3.h        (vendored; used by mp3_codec.c)
│   └── minimp3_ex.h
├── Makefile
├── src
│   ├── main.c
│   ├── mp3_codec.c      (instructor-provided)
│   ├── mp3_id3.c
│   ├── mp3_overlay.c
│   ├── mp3_reader.c
│   ├── mp3_sections.c
│   ├── mp3_trim.c
│   └── util.c
└── tests
    ├── test_reader.c
    ├── test_codec.c
    ├── test_trim.c
    ├── test_overlay.c
    └── data
        └── [MP3 fixtures]

```

Build with `make` or `make debug`. Generate test fixtures with `make fixtures`.

- `make` / `make all` — release build (debug macros are no-ops)
- `make debug` — adds `-g`, enables colored `debug.h` output, and defines `DEBUG`, `INFO`, `WARN`, `ERROR`, and `SUCCESS`
- `make clean` — remove build artifacts; combine targets as needed (e.g. `make clean debug`)

## Program Usage

```
Usage: bin/mp3 -f mp3_file [options]
Options:
  -f mp3_file               Input MP3 file (required)
  -h                        Print this help message
  -s                        Print section summary
  -t                        Print ID3 metadata
  -i                        Print MPEG frame header fields
  -l                        Print audio duration in seconds
  -p                        Print timestamp of loudest audio
  -c start end -o out_file  Extract audio from start to end seconds
  -m file2 -a start -o out  Overlay file2 at start seconds in input

```

Your program should process arguments in two passes: first validate `-f` and `-h`, then 
dispatch the feature flags in a second pass. Use `PRINT_USAGE` from `global.h` for the 
help text.

## Program Output, `global.h`, and `debug.h`

Graded output must match the specifications exactly. Feature results (`-s`, `-t`, `-i`,
`-l`, and success messages for `-c`/`-m`) go to **stdout**. User-facing error messages
go to **stderr** and the program should exit with `EXIT_FAILURE` (1).

Do not use `printf` for ad hoc debugging during normal runs — that output will break
automated tests. Use the macros below instead.

### `global.h` output and error macros

`global.h` is provided so all graded messages use a consistent format. **Use these
macros** for anything the user or grader sees:


| Category               | Examples                                                                                       | Stream |
| ---------------------- | ---------------------------------------------------------------------------------------------- | ------ |
| Usage                  | `PRINT_USAGE(prog_name)`                                                                       | stdout |
| Errors                 | `PRINT_ERROR_MISSING_F_FLAG()`, `PRINT_ERROR_TRIM_FAILED()`, `PRINT_ERROR_UNKNOWN_OPTION(opt)` | stderr |
| Section summary (`-s`) | `PRINT_SECTION_SUMMARY_HEADER`, `PRINT_SECTION_INFO`                                           | stdout |
| Frame header (`-i`)    | `PRINT_FRAME_HEADER`                                                                           | stdout |
| Metadata (`-t`)        | `PRINT_METADATA_HEADER`, `PRINT_METADATA_V2_FRAME`, `PRINT_METADATA_V1_TITLE`, …               | stdout |
| Duration (`-l`)        | `PRINT_DURATION(seconds)`                                                                      | stdout |
| Loudest peak (`-p`)    | `PRINT_LOUDEST_TIMESTAMP(seconds)`                                                             | stdout |
| Trim / overlay success | `PRINT_TRIM_SUCCESS`, `PRINT_OVERLAY_SUCCESS`                                                  | stdout |


Error macros follow the form `Error: <description>` on a single line. When argument
parsing fails, use the matching `PRINT_ERROR_*_REQUIRES()` macro; when a library call
fails, use the corresponding `PRINT_ERROR_*_FAILED()` macro.

`main.c` already includes `global.h` and uses these macros for CLI errors and help.
Your library code should call the feature-specific `PRINT_`* macros from `main.c`
helpers (as in the template), not invent new format strings.

### `debug.h` debug macros

`debug.h` provides optional **diagnostic** logging that never goes to stdout:


| Macro          | Purpose                                 | Enabled when               |
| -------------- | --------------------------------------- | -------------------------- |
| `debug(...)`   | Verbose tracing while developing        | `make debug` (`-DDEBUG`)   |
| `info(...)`    | Informational notes                     | `make debug` (`-DINFO`)    |
| `warn(...)`    | Warnings                                | `make debug` (`-DWARN`)    |
| `error(...)`   | Internal error traces (not user errors) | `make debug` (`-DERROR`)   |
| `success(...)` | Internal success traces                 | `make debug` (`-DSUCCESS`) |


Each active macro prints to **stderr** with file, function, and line number. In a
plain `make` build, all of these expand to nothing — zero overhead and no extra output.

Example:

```c
#include "debug.h"

debug("frame_size=%zu offset=%zu", frame_size, offset);
warn("unexpected padding bit set");
```

With `make debug`, messages are colorized when `COLOR` is defined. User-visible
errors shown to graders should still use `PRINT_ERROR_*` from `global.h`, not the
`error()` macro from `debug.h`.

> Unless compiled with `make debug`, a successful run that exits with `EXIT_SUCCESS`
> must not produce any output other than what the assignment specifies.

## Part 1: MP3 File Structure and Sections

An MP3 file may contain:

1. **ID3v2 tag** (optional) at the start, beginning with the bytes `ID3`
2. **MPEG audio frames**, each starting with an 11-bit sync word (`0xFFE` or `0xFFF`)
3. **ID3v1 tag** (optional) at the end, 128 bytes starting with `TAG`

### Section Summary (`-s`)

The section summary lists every logical section in the file:

```
Section Summary for <filename>:
  Section 0: Type=ID3H, Offset=0, Length=10
  Section 1: Type=ID3F, ID=TIT2, Offset=10, Length=<n>
  Section 2: Type=ID3F, ID=TPE1, Offset=<n>, Length=<n>
  Section 3: Type=MPEG, Offset=<n>, Length=<n>
  ...
  Section N: Type=ID3V1, Offset=<n>, Length=128
```

Section types:

- `ID3H` — ID3v2 tag header (10 bytes)
- `ID3F` — ID3v2 frame (metadata such as title or artist)
- `MPEG` — one MPEG audio frame
- `ID3V1` — legacy 128-byte tag at end of file

### MPEG Frame Header (`-i`)

The first MPEG frame header in the file is parsed and printed:

```
MPEG Frame Header for <filename>:
  MPEG Version: MPEG 1
  Layer: Layer III
  Protection: none
  Bitrate: 128 kbps
  Sample Rate: 44100 Hz
  Padding: 0
  Channel Mode: Stereo
  Copyright: 0
  Original: 0
  Emphasis: none
  Frame Size: 417 bytes
```

The 4-byte MPEG frame header layout:

```
AAAAAAAA AAABBCCD EEEEFFGH IIJJKLMM
```

- **A** — 11-bit sync (all ones)
- **B** — MPEG version (2 bits)
- **C** — Layer (01 = Layer III)
- **D** — Protection bit (0 = CRC follows header)
- **E** — Bitrate index (4 bits)
- **F** — Sample rate index (2 bits)
- **G** — Padding bit
- **H** — Private bit
- **I** — Channel mode (2 bits)
- **J** — Mode extension (2 bits)
- **K** — Copyright
- **L** — Original
- **M** — Emphasis (2 bits)

Frame size for Layer III:

- MPEG-1: `144 * bitrate / sample_rate + padding`
- MPEG-2/2.5: `72 * bitrate / sample_rate + padding`

## Debugging and Reading Hex Values

As test input for your code, several MP3 files are provided in the `tests/data/`
directory. MP3 files are binary files that may contain ID3 metadata tags and a
sequence of MPEG audio frames. There is no single fixed header at the
very start of every file — some files begin with an ID3v2 tag, while others (such
as `frames_only.mp3`) begin directly with MPEG frame data.

Multi-byte integer fields in ID3 tags are stored in **big-endian** format (most
significant byte first). ID3v2 tag *sizes* use a special **synchsafe** encoding
where the high bit of each byte is always zero. MPEG frame headers, by contrast,
are bit-packed into exactly 4 bytes and must be decoded with shifts and masks
rather than by reading a whole 32-bit integer.

The Intel x64 architecture is **little-endian**, so when you load multi-byte values
into C integers you will often need to rearrange bytes (or use the provided
`read_u32_be()` helper) to interpret them correctly.

### Using `hd` (hex dump)

The `hd` tool displays the raw bytes of a binary file. You can (and should) read
its manual page, but for now here is the start of `tests/data/with_id3v2.mp3`:

```
$ hd tests/data/with_id3v2.mp3 | less
00000000  49 44 33 03 00 00 00 00  00 2c 54 49 54 32 00 00  |ID3......,TIT2..|
00000010  00 0b 00 00 00 00 54 65  73 74 20 53 6f 6e 67 54  |......Test SongT|
00000020  50 45 31 00 00 00 0d 00  00 00 00 54 65 73 74 20  |PE1........Test |
00000030  41 72 74 69 73 74 ff fb  90 00 00 01 02 03 04 05  |Artist..........|
00000040  06 07 08 09 0a 0b 0c 0d  0e 0f 10 11 12 13 14 15  |................|
```

> :nerd_face: Pipe long output through `less` to scroll one screen at a time.
> Type `h` at the `less` prompt for help; type `q` to quit.

The `hd` output uses canonical hex+ASCII format:

- The **first column** is the file offset in hexadecimal.
- The **middle columns** show sixteen bytes in hex.
- The **rightmost column** shows the same bytes as ASCII (non-printable bytes appear as dots).

### Reading the ID3v2 Tag Header

The first ten bytes of this file are the ID3v2 tag header:

```
00000000  49 44 33 03 00 00 00 00  00 2c                    |ID3......,|
```

Breaking these bytes down:

- **Identifier** (3 bytes): `0x49`, `0x44`, `0x33` = `ID3` in ASCII
- **Version** (2 bytes): `0x03`, `0x00` = ID3v2.3.0
- **Flags** (1 byte): `0x00` = no extended header, no experimental tag, no footer
- **Tag size** (4 bytes, synchsafe): `0x00`, `0x00`, `0x00`, `0x2C` = `44` decimal

The synchsafe size is computed as:
`(0x00 << 21) | (0x00 << 14) | (0x00 << 7) | 0x2C` = **44 bytes** of frame data
follow the 10-byte header. The total ID3v2 tag occupies bytes `0` through `53`
(offset 0 + 10 + 44).

### Reading ID3v2 Frames

ID3v2 frames begin immediately after the 10-byte tag header. Each frame has:

1. **Frame ID** (4 bytes ASCII)
2. **Frame size** (4 bytes; big-endian for ID3v2.3, synchsafe for ID3v2.4)
3. **Frame flags** (2 bytes)
4. **Frame data** (variable length)

The first frame in this file starts at offset `0x0A` (10 decimal):

```
00000000  49 44 33 03 00 00 00 00  00 2c 54 49 54 32 00 00  |ID3......,TIT2..|
00000010  00 0b 00 00 00 00 54 65  73 74 20 53 6f 6e 67 54  |......Test SongT|
                      -- first frame data ends here --^
```

- **Frame ID** (4 bytes at offset `0x0A`): `0x54`, `0x49`, `0x54`, `0x32` = `TIT2` (title)
- **Frame size** (4 bytes, big-endian): `0x00`, `0x00`, `0x00`, `0x0B` = **11** bytes
- **Frame flags** (2 bytes): `0x00`, `0x00`
- **Frame data** (11 bytes): begins at offset `0x14` with `0x00`, `0x00`, followed by
the ASCII text `Test Song` (`0x54`, `0x65`, `0x73`, `0x74`, `0x20`, `0x53`, `0x6f`, `0x6e`, `0x67`)

The next frame begins at offset `0x1F`:

- **Frame ID**: `0x54`, `0x50`, `0x45`, `0x31` = `TPE1` (artist/lead performer)
- **Frame size**: `0x00`, `0x00`, `0x00`, `0x0D` = **13** bytes
- **Frame data**: begins with `Test Artist`

After all ID3v2 frames, the MPEG audio data starts at offset `0x36` (54 decimal).

### Reading an MPEG Frame Header

At offset `0x36` we see the start of the first MPEG audio frame:

```
00000030  41 72 74 69 73 74 ff fb  90 00 00 01 02 03 04 05  |Artist..........|
                      ^^^^^^^^^^
```

The four frame-header bytes are: `0xFF`, `0xFB`, `0x90`, `0x00`.

MPEG frame headers are **not** stored as a simple 32-bit integer. Each field
occupies a specific range of bits across these four bytes:

```
Byte 0:  11111111                          (sync, part 1)
Byte 1:  11111011                          (sync part 2 + version/layer/protection)
Byte 2:  10010000                          (bitrate/sample rate/padding)
Byte 3:  00000000                          (channel mode/emphasis)
```

Decoding `0xFF 0xFB 0x90 0x00`:


| Field             | Bits    | Value      | Meaning                           |
| ----------------- | ------- | ---------- | --------------------------------- |
| Sync              | 11 bits | `0x7FF`    | Valid MPEG sync word              |
| MPEG version      | 2 bits  | `11` (3)   | MPEG-1                            |
| Layer             | 2 bits  | `01` (1)   | Layer III                         |
| Protection        | 1 bit   | `1`        | No CRC after header               |
| Bitrate index     | 4 bits  | `1001` (9) | 128 kbps (MPEG-1 Layer III table) |
| Sample rate index | 2 bits  | `00` (0)   | 44100 Hz                          |
| Padding           | 1 bit   | `0`        | No padding byte                   |
| Channel mode      | 2 bits  | `00` (0)   | Stereo                            |


With **128 kbps** and **44100 Hz** on MPEG-1 Layer III, the frame size is:

`144 × 128000 / 44100 + 0` = **417 bytes**

So the first MPEG frame occupies offsets `0x36` through `0x1D6` (54 + 417 − 1).
The next frame begins at offset `0x1D7` with another `0xFF` sync byte, and the
pattern repeats until the end of the audio stream (or until an ID3v1 tag is found).

A real-world file without an ID3v2 prefix looks like this:

```
$ hd tests/data/frames_only.mp3 | head -3
00000000  ff fb 90 00 00 01 02 03  04 05 06 07 08 09 0a 0b  |................|
```

Here the file begins directly with `0xFF 0xFB` (the MPEG sync word) because no 
ID3v2 tag is present. The four header bytes are identical to the earlier example, 
so the frame decodes the same way: 128 kbps, 44100 Hz, Stereo.

### Reading the ID3v1 Tag at End of File

Some files also contain a legacy **ID3v1** tag: exactly 128 bytes at the very end
of the file, starting with the ASCII bytes `TAG`. In `tests/data/full_tags.mp3`:

```
$ hd -s 0xD40 tests/data/full_tags.mp3
00000d40  54 41 47 46 75 6c 6c 20  54 72 61 63 6b 00 00 00  |TAGFull Track...|
00000d50  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000d60  00 46 69 78 74 75 72 65  20 42 61 6e 64 00 00 00  |.Fixture Band...|
```

Breaking down the ID3v1 layout:

- **Tag** (3 bytes): `TAG`
- **Title** (30 bytes): `Full Track` (null-padded)
- **Artist** (30 bytes): `Fixture Band` (null-padded)
- **Album**, **Year**, **Comment**, and **Genre** fields follow in the remaining bytes

### Putting It Together

When developing your parser, use `hd` to verify that the offsets and lengths
reported by `bin/mp3 -s` match what you see in the raw bytes:

```
$ bin/mp3 -f tests/data/with_id3v2.mp3 -s
Section Summary for tests/data/with_id3v2.mp3:
  Section 0: Type=ID3H, Offset=0, Length=10
  Section 1: Type=ID3F, ID=TIT2, Offset=10, Length=11
  Section 2: Type=ID3F, ID=TPE1, Offset=31, Length=13
  Section 3: Type=MPEG, Offset=54, Length=417
  ...
```

Cross-check each section against the hex dump: the ID3H section ends where the
first `TIT2` frame begins, the ID3 frames end where `0xFF 0xFB` appears, and
each MPEG section spans exactly the number of bytes predicted by the frame header.

> :relieved: If you can correctly implement section traversal and MPEG frame-header
> parsing so that your offsets and field values match `hd`, you will have the
> foundation needed for the metadata and audio-editing parts of the assignment.

### ID3 Metadata (`-t`)

The `-t` flag prints human-readable metadata extracted from ID3v2 frames and/or
the trailing ID3v1 tag. Supported ID3v2 text frames include `TIT2` (title),
`TPE1` (artist), `TALB` (album), `TYER`/`TDRC` (year), and `COMM` (comment).
Unrecognized ID3v2 frames are printed using an ASCII fallback decoder.

Functions in `mp3_id3.c`:

- `mp3_extract_metadata()` — parse all tags from a file path
- `mp3_free_metadata()` — release allocated strings
- `mp3_mpeg_audio_start()` / `mp3_mpeg_audio_end()` — byte offsets of MPEG data

## Part 2: Core Library Functions

### Data Structures (`mp3_sections.h`)

```c
typedef struct {
    int mpeg_version;
    int layer;
    int protection;
    int bitrate_kbps;
    int sample_rate_hz;
    int padding;
    int private_bit;
    int channel_mode;
    int mode_extension;
    int copyright;
    int original;
    int emphasis;
    size_t frame_size;
} mp3_frame_header_t;

typedef struct {
    size_t offset;
    char type[8];
    char id[5];
    uint32_t length;
    uint8_t *data;
} mp3_section_t;
```

### Section Parsing (`mp3_sections.c`)

- `mp3_is_sync()` — detect MPEG sync word
- `mp3_parse_frame_header()` — parse 4-byte frame header

### Reader (`mp3_reader.c`)

- `mp3_open()` — open file for reading
- `mp3_summary()` — enumerate all sections
- `mp3_extract_frame_header()` — parse first MPEG frame header
- `mp3_free_sections()` — free section array

## Instructor-Provided Codec (`mp3_codec.c`)

Full MP3 encode/decode is **not** part of the student implementation. The course
distributes `mp3_codec.c`, which wraps:

- **Decode:** [minimp3](https://github.com/lieff/minimp3) (`third_party/minimp3.h`)
- **Encode:** [LAME](https://lame.sourceforge.io/) (`libmp3lame`)

```c
typedef struct {
    int16_t *samples;       /* interleaved PCM */
    size_t sample_count;    /* total int16 values (frames × channels) */
    int channels;
    int sample_rate_hz;
    int bitrate_kbps;
} mp3_pcm_buffer_t;

int mp3_codec_decode(const uint8_t *mpeg_data, size_t mpeg_size, mp3_pcm_buffer_t *out);
int mp3_codec_encode(const mp3_pcm_buffer_t *pcm, uint8_t **out_data, size_t *out_size);
void mp3_codec_free_pcm(mp3_pcm_buffer_t *pcm);
```

Pass only raw MPEG frame bytes (not ID3 tags) to `mp3_codec_decode()`.

## Audio Duration and Trim (`mp3_trim.c`)

### Duration (`-l`)

Decodes the MPEG audio region and reports:

`duration = (sample_count / channels) / sample_rate_hz`

- `mp3_get_duration(path, &seconds)`

### Loudest timestamp (`-p`)

Decodes the MPEG audio region and scans PCM frames for the highest peak amplitude.
For each frame, the peak level is the maximum absolute sample value across all
channels. The timestamp of the first frame with the highest peak is returned:

`timestamp = loudest_frame / sample_rate_hz`

If multiple frames tie for the loudest level, the earliest frame wins.

- `mp3_get_loudest_timestamp(path, &seconds)`

### Trim (`-c start end -o out`)

Extracts the half-open interval **[start, end)** in seconds:

1. Decode input to PCM.
2. Convert timestamps to sample indices: `frame = (size_t)(sec * sample_rate_hz)`.
3. Copy samples `[start_frame × channels, end_frame × channels)`.
4. Re-encode and write: `ID3v2 prefix + new MPEG + ID3v1 suffix`.

- `mp3_trim_audio(input, start_sec, end_sec, output)`

Re-encoding changes the MPEG byte stream and output file size; ID3 tag regions
from the input file are copied verbatim.

## Audio Overlay (`mp3_overlay.c`)

### Overlay (`-m file2 -a start -o out`)

Inserts `file2` into `file` at `start` seconds by **replacing** the base audio
during the overlay (the original audio in that window is skipped, not mixed).
When the overlay ends, the base track resumes if audio remains.

1. Decode both files to PCM.
2. Resample the overlay to the base sample rate if needed (linear interpolation).
3. Convert mono/stereo if channel counts differ.
4. For each output frame: use base samples before `start`, overlay samples during
  the overlay window, then base samples again afterward.
5. Re-encode and preserve ID3 regions from the base file.

- `mp3_overlay_audio(base_path, overlay_path, start_sec, output_path)`

If the overlay runs past the end of the base track, the output is extended to
include the full overlay clip.

## Testing

```bash
make fixtures
make
bin/mp3_tests
```

The test suite includes reader/section tests, codec round-trip checks, trim
tests, and overlay tests.

Example CLI usage:

```bash
bin/mp3 -f tests/data/Batman.mp3 -s -i -t
bin/mp3 -f tests/data/Batman.mp3 -l
bin/mp3 -f tests/data/Batman.mp3 -p
bin/mp3 -f tests/data/Batman.mp3 -c 10 20 -o /tmp/clip.mp3
bin/mp3 -f tests/data/Batman.mp3 -m tests/data/short_mono.mp3 -a 36 -o /tmp/mixed.mp3
```

## Allowed Libraries

**Student code:**

- `glibc` (standard C library, including `malloc`/`free` and stdio)

**Instructor-provided (do not reimplement):**

- `mp3_codec.c` — minimp3 decode + LAME encode (`-lmp3lame` on the link line)
- Vendored headers in `third_party/` (minimp3)

Students should **not** use other external audio libraries (`libmpg123`, `libmad`,
`ffmpeg`, etc.) in their own source files.

You may need to install LAME and Criterion to run the provided code. Here are the commands I ran to install them in my system:

- sudo apt install libmp3lame-dev libcriterion-dev

## References

- [MP3 frame format (Wikipedia)](https://en.wikipedia.org/wiki/MP3#File_structure)
- [ID3 tag specification](https://id3.org/id3v2.3.0)
- [minimp3](https://github.com/lieff/minimp3)
- [LAME MP3 encoder](https://lame.sourceforge.io/)

