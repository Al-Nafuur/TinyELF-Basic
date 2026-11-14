# TinyELF Basic

**A real BASIC home computer on the Atari 2600.**

TinyELF Basic is a BASIC interpreter that runs as an ELF program on the ARM microcontroller of a PlusCart or UnoCart. The ARM runs the interpreter and feeds the 2600's 6507 the instructions to draw the screen and read the joystick ports. You type on a real PS/2 keyboard (through the KeyPortari) and save your programs on a SaveKey or AtariVox.

The language core is Stefan Lenz's [Basic2](https://github.com/slviajero/tinybasic), a modern C descendant of Palo Alto Tiny BASIC.

```
TinyELF Basic v0.20 65535 Bytes Free
READY
```

## Features

- **65,535 bytes** for programs and variables, 32-bit integer maths (no floating point)
- **Text:** 36 × 14 characters, or 18 large columns in `GRAPHICS 1`
- **Graphics:** `GRAPHICS 3` (40 × 14 blocks) and `GRAPHICS 8` (145 × 181 pixels) with `PLOT` and `DRAWTO`, optionally with a 3-row text window
- **Colour:** text colour per row, background colour per scanline, character set in RAM (via `POKE`)
- **Sound:** both TIA audio channels via `POKE`
- **Controllers:** joystick and paddles with `STICK`, `STRIG` and `PTRIG`
- **Storage:** `SAVE`/`LOAD` and `LIST`/`ENTER` on a SaveKey or AtariVox, with an Atari DOS 2.x-style file system: drives `E1`–`E8`, 8.3 file names, up to 64 files per drive, `CATALOG`, `FORMAT`, `DELETE`
- **Language:** strings with `LEFT$`, `MID$`, `RIGHT$` & co., long variable names, `DATA`/`READ`, `DEF FN`, `ON … GOTO`, two-dimensional and string arrays, `MILLIS` and `DELAY`, `ESC` to break, so classic type-ins are easy to port

## What you need

- an Atari 2600 or 7800
- a **PlusCart** or **UnoCart** with **UCA firmware v2.3.32** or newer
- a **KeyPortari** in both joystick ports, set to the **7-bit ASCII** protocol, and a PS/2 keyboard (or a USB keyboard that still speaks PS/2)
- optional: a **SaveKey**, **SaveKey Plus** or **AtariVox** for storage, in the KeyPortari's right pass-through port

## Getting started

1. Download `TinyELF-Basic-v0.20.bin` and the manual from the [latest release](https://github.com/Al-Nafuur/TinyELF-Basic/releases/latest).
2. Copy the `.bin` to your PlusCart or UnoCart like any other ROM and start it from the menu.
3. Type a program:

   ```
   10 PRINT "HELLO ATARI"
   20 GOTO 10
   RUN
   ```

   Press `ESC` to stop it.
4. Save it with `SAVE "E1:HELLO.BAS"` (after `FORMAT "E1:"` on a fresh SaveKey) and list the drive with `CATALOG "E1:"`.

## Manual

The [user manual](MANUAL.md) describes every statement, the graphics modes, the memory map for `PEEK` and `POKE`, the sound registers and the file system. A print-ready PDF is attached to each release.

## Example programs

About 100 ready-to-run listings are in [`examples/`](examples):

| Folder | Contents |
|---|---|
| [`14games`](examples/14games) | 53 classics from *BASIC Computer Games* and the roguelike *Dungeons of Ekileugor* |
| [`20demos`](examples/20demos) | small graphics demos |
| [`30tutorial`](examples/30tutorial) | short programs that show one feature each |
| [`40benchmarks`](examples/40benchmarks) | classic BASIC benchmarks |

Copy a listing to your SaveKey and load it with `ENTER`.

## SaveKey-Manager

[SaveKey-Manager](https://al-nafuur.github.io/SaveKey-Manager/) is a browser app (PWA) to back up, format and fill your SaveKey from a PC. It connects through the [PicoBridge](https://al-nafuur.github.io/SaveKey-Manager/firmware/pico-bridge), a Raspberry Pi Pico working as a USB-to-I²C bridge. It formats TinyELF drives, uploads, downloads and edits programs, and copies the example programs straight from this repository through its Public Library.

## Tools

[`tools/TinyELF-Image-Converter.html`](tools/TinyELF-Image-Converter.html) turns a picture into a TinyELF Basic program that draws it in `GRAPHICS 8`. Open the file in a browser.

## Emulation

TinyELF Basic runs in **Gopher2600** (recent version, with `-keyportari ASCII -right SAVEKEY`), with 48 KB of program memory. Gopher2600 emulates only one 32 KB SaveKey (drive `E1`), and storage commands can fail there with error 23 or 20.

Stella cannot run TinyELF Basic: its ELF support emulates a Cortex-M0, TinyELF Basic needs the Cortex-M4 of the PlusCart and UnoCart.

## Building

The release is built with **GCC 14.2** (Arm GNU Toolchain 14.2.Rel1). Other GCC versions give a different binary; GCC 15 does not compile the sources yet.

```
arm-none-eabi-gcc -r -mlong-calls -fno-exceptions -fno-builtin-memmove -march=armv7-m -Os -Wall \
  -Wl,-T,tinyelf.ld *.c -o TinyELF-Basic-v0.20.bin -lgcc
```

This gives a byte-identical copy of the released `TinyELF-Basic-v0.20.bin`. The linker script `tinyelf.ld` keeps all variables out of `.bss`, so the whole 64 KB CCM RAM of the STM32 is free for BASIC programs.

## Credits

- **Stefan Lenz**: the Basic2 interpreter and the manual this one is derived from
- **Li-Chen Wang**: Palo Alto Tiny BASIC, the ancestor of the core language
- **Omegamatrix**: the 36-character display kernel and its font, published on AtariAge ([36 Character Kernel Redux](https://forums.atariage.com/topic/317204-36-character-kernel-redux)). TinyELF Basic reimplements his kernel in C on the ARM.
- **Alex Herbert** and **Thomas Jentzsch**: the SaveKey/AtariVox I²C driver that the TinyELF driver is ported from
- **Zachary Scolaro**: vcsLib and the ELF support in the UCA firmware
- **JetSetIlly**: Gopher2600, used for testing

## Licence

TinyELF Basic is based on Stefan Lenz's Basic2, which is licensed under the GNU General Public License v3, and is distributed under the same licence. The example programs in `examples/` keep their own licences, see the `LICENSE-*.txt` files there.

## Discussion

- [TinyELF Basic thread on AtariAge](https://forums.atariage.com/topic/386052-tinyelf-basic-for-the-atari-2600/)
