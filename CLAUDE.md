# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

mJPatch is a minimal embedded C library that applies [JojoDiff](http://jojodiff.sourceforge.net/) binary patch files. It is designed for MCU (microcontroller) environments where filesystem APIs may not exist, so all I/O is abstracted through user-supplied callbacks. The library has no external dependencies beyond `<stdint.h>` and `<stdbool.h>`.

## Building

There is no Makefile. The project uses a Code::Blocks project file (`mjp.cbp`), but the simplest way to build the CLI test harness is:

```sh
gcc -Wall -o mjp main.c mjp.c
```

For the optional write-buffer feature, define `MJP_WR_BUF_SIZE` at compile time:

```sh
gcc -Wall -DMJP_WR_BUF_SIZE=32 -o mjp main.c mjp.c
```

## Running the CLI

The `main.c` harness takes four positional arguments:

```sh
./mjp <original_file> <patch_file> <destination_file> <comparison_file>
```

It first parses and logs the patch structure to stdout, then applies the patch and writes `<destination_file>`, then byte-compares it against `<comparison_file>`, printing `OK` or `NG`.

## Architecture

The library has two parallel API surfaces that share the same state machine and `mjp_t` struct:

| Purpose | Feed bytes with | Finish with |
|---|---|---|
| Apply patch (generate output file) | `mjp_apply(int dt)` | `mjp_apply_done()` |
| Parse/log patch structure to stdout | `mjp_parse(int dt)` | `mjp_parse_done()` |

Both surfaces are driven byte-by-byte, making the library suitable for streaming environments (e.g., reading over UART on a device with no buffered filesystem).

### State machine (`mjp.c`)

- A single global `mjp_t mjp` holds all state.
- `mjp_start()` resets the struct and registers the three I/O callbacks.
- `mjp.cmd` tracks the current active opcode. It starts as `MJP_EOF` (no active command).
- `mjp.last_dt` is used to detect the two-byte ESC sequences (`A7 A7` → literal `A7`; `A7 <cmd>` → command switch).
- Opcodes `BKT`, `EQL`, and `DEL` consume an encoded offset immediately after the command byte. These are parsed incrementally by `mjp_parse_oft()` / `mjp_get_oft()` into `mjp.oft`.
- `MOD` and `INS` consume a raw data stream until the next ESC sequence.

### Offset encoding (`mjp_parse_oft` / `mjp_get_oft`)

Offset values are variable-length big-endian integers determined by the first byte:

| First byte | Total bytes consumed | Value |
|---|---|---|
| 0–251 | 1 | `buf[0] + 1` |
| 252 | 2 | `buf[0] + buf[1] + 1` |
| 253 | 3 | 16-bit big-endian from `buf[1..2]` |
| 254 | 5 | 32-bit big-endian from `buf[1..4]` |
| 255 | 9 | 64-bit — **not supported**, returns error |

### Write buffer (optional)

When `MJP_WR_BUF_SIZE` is defined, `mjp_write()` batches writes into `mjp.wr.buf` and flushes via `des_wr_cb` only when the buffer is full or `mjp_flush()` is called. This reduces flash-write cycles on MCUs. When the macro is not defined (the default), every byte is written immediately via `des_wr_cb`.

### Callbacks

```c
// Write len bytes from buf to destination at addr
typedef int (*mjp_des_wr_t)(int addr, uint8_t *buf, int len);

// Read one byte from original file at addr
typedef int (*mjp_org_rd_t)(int addr);

// Optional: bulk copy src->des for len bytes (used by EQL to avoid byte-by-byte reads)
typedef int (*mjp_copy_t)(int src, int des, int len);
```

`copy_cb` may be `NULL`; if so, EQL falls back to calling `org_rd_cb` + `mjp_write()` in a loop.

## Key Constants (`mjp.h`)

```c
MJP_BKT = 0xA2  // Backtrace original address
MJP_EQL = 0xA3  // Copy original → destination
MJP_DEL = 0xA4  // Advance original address (skip)
MJP_INS = 0xA5  // Insert new bytes into destination
MJP_MOD = 0xA6  // Overwrite original bytes in destination
MJP_ESC = 0xA7  // Escape / command prefix byte
```

Error codes returned by `mjp_apply` / `mjp_parse`:

```c
MJP_ERR_FORMAT   = -1
MJP_ERR_BKT_OFT = -2
MJP_ERR_EQL_OFT = -3
MJP_ERR_DEL_OFT = -4
```
