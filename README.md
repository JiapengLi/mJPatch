# mJPatch

**Embedded JojoPatch library**. This library should work with [JojoDiff -  diff utility for binary files](<http://jojodiff.sourceforge.net/>).

mJPatch is inspired by [JAPatch](<https://github.com/janjongboom/janpatch>), the goal is to rewrite JAPatch and decouple the dependencies of file management APIs to make the library could work with MCU easily no mater it supports file system or not.

## Features

- Parse JojoDiff patch file and log it for human reading to understand jojopatch file structure

- Simplify patch file data feeding, make it a single access point, do not call `getc` everywhere.
- Be friendly to MCU development
- Minimize RAM and ROM cost
- User defined write buffer size, minimum zero. (Buffer could help to decrease write flash times to improve performance)

## JojoDiff Patch File Structure

- `ESC A7` Escape character, (`A7 A6` stands for `MOD` action, `A7 A5` stands for `INS` action and so on)
- `MOD A6` Modify original file content (pretending) and write to destination file, original and destination file address cursors increase together with same steps 
- `INS A5` Insert new data to destination file. Original file address cursor stays unchanged, destination cursor increase accordingly. 
- `DEL A4` Delete data from original file (pretending), original file address cursor plus offset. 
- `EQL A3` Copy from original file to destination file, both address cursors increasing accordingly
- `BKT A2` Set original file address cursor backward, subtract offset. 

| Action | Original File Address | Destination File Address |   Payload    |
| :----: | :-------------------: | :----------------------: | :----------: |
|  MOD   |         `+++`         |          `+++`           | Data stream  |
|  INS   |         `===`         |          `+++`           | Data stream  |
|  DEL   |         `+++`         |          `===`           | Offset value |
|  EQL   |         `+++`         |          `+++`           | Offset value |
|  BKT   |         `---`         |          `===`           | Offset value |

```
+++ : address cursor move forward
--- : address cursor move backward
=== : address cursor stay unchanged
```

### Offset Value Encoding

`DEL, EQL, BKT` are followed by an offset value. It is coded in below principle. 

- Big endian
- Encoding method is decided by the first offset byte

| buf[0] First Byte | Offset Value Encoding                                        |
| :---------------: | ------------------------------------------------------------ |
|      0 - 251      | `buf[0] + 1`                                                 |
|        252        | `buf[0] + buf[1] + 1`                                        |
|        253        | `buf[1]  buf[2]` (Big endian)                                |
|        254        | `buf[1]  buf[2] buf[3] buf[4]` (Big endian)                  |
|        255        | `buf[1] buf[2] buf[3] buf[4] buf[5] buf[6] buf[7] buf[8]` (Big endian) |

## How To Parse Patch File

1. Each valid patch file must start with `A7`
2. Check and wait `A7`, each `A7` must be followed by `A2 - A7`, 
   - if other characters follows `A7`, parse program may ignore `A7` and process the character
3. if `A2 - A6` is received, change to new action, reset context and sync address cursor
4. Keep do the same action until next action change event

### Hints

- `A7 A7` will be merged to a single `A7`
- `A7 (A2 - A6) `trigger action change event
- single `A2 - A6` will be itself, nothing changed

## Example

### Patch File Example

```
 A7 A3 FC 17 
 A7 A6 A7 A7 A7 A7 A7 A7 A7 A7 A7 A7 A7 A7 A7 A7 A7 A7 
 A7 A3 0F 
 A7 A6 A7 A7 A7 A7 A7 A7 A7 A7 
 A7 A3 13 
 A7 A6 A7 A7 A7 A7 A7 A7 A7 A7 
 A7 A3 5B 
 A7 A6 A3 A7 A7 
 A7 A3 59 
```

### Parse Patch File

```
ADDRESS

      0 | EQL A7 A3 FC 17
EQL: Copy from 0(O) to 0(D) for 276 bytes

      4 | MOD A7 A6 A7 A7 A7 A7 A7 A7 A7 A7
MOD: Modify from 276(O) 276(D) for 8 bytes

     22 | EQL A7 A3 0F
EQL: Copy from 284(O) to 284(D) for 16 bytes

     25 | MOD A7 A6 A7 A7 A7 A7
MOD: Modify from 300(O) 300(D) for 4 bytes

     35 | EQL A7 A3 13
EQL: Copy from 304(O) to 304(D) for 20 bytes

     38 | MOD A7 A6 A7 A7 A7 A7
MOD: Modify from 324(O) 324(D) for 4 bytes

     48 | EQL A7 A3 5B
EQL: Copy from 328(O) to 328(D) for 92 bytes

     51 | MOD A7 A6 A3 A7
MOD: Modify from 420(O) 420(D) for 2 bytes

     56 | EQL A7 A3 59
EQL: Copy from 422(O) to 422(D) for 90 bytes

Patch file size: 59
Destination file size: 512
Original file size: 512 (used)
```

## Reference

- <http://jojodiff.sourceforge.net/>
- <https://github.com/janjongboom/janpatch>
- <https://os.mbed.com/blog/entry/towards-fota-lora-crypto-delta-updates/>



