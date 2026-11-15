# TinyELF Basic – User Manual

**TinyELF Basic v0.21**

TinyELF Basic is a BASIC interpreter that runs entirely on the microcontroller of a modern Atari 2600
flash cartridge (PlusCart, UnoCart) as an ELF program. It drives a 36-character text kernel and adds
Atari-style graphics commands and a small DOS 2.x-style filesystem on I2C SaveKey-compatible EEPROMs.

The language core is Stefan Lenz's *Basic2* interpreter (<https://github.com/slviajero/tinybasic>).
This manual is derived from his `MANUAL.md` (GPL v3), reduced to the parts that exist in TinyELF Basic
and extended with everything that is specific to the Atari 2600 port.

> Please report anything that is wrong, missing or confusing in the TinyELF Basic thread on AtariAge.
> Every section carries a status tag so you know how much to trust it:
>
> | Tag | Meaning |
> |-----|---------|
> | **[HW]** | Verified on real hardware by the developers. |
> | **[UP]** | Inherited from Stefan's interpreter. Expected to behave like his manual describes; not individually re-tested on TinyELF yet. |
> | **[??]** | Described from reading the source code; not yet verified on hardware. |

---

## Contents

1. [Hardware and getting started](#1-hardware-and-getting-started)
2. [The language at a glance](#2-the-language-at-a-glance)
3. [Statement and function reference](#3-statement-and-function-reference)
4. [Variables, strings, arrays and special variables](#4-variables-strings-arrays-and-special-variables)
5. [The screen: text modes, GRAPHICS, PLOT, DRAWTO](#5-the-screen-text-modes-graphics-plot-drawto)
6. [Controllers: STICK, STRIG, PTRIG](#6-controllers-stick-strig-ptrig)
7. [Memory map for PEEK and POKE](#7-memory-map-for-peek-and-poke)
8. [Storage: drives, the TinyELF filesystem, LIST and ENTER](#8-storage-drives-the-tinyelf-filesystem-list-and-enter)
9. [Errors](#9-errors)
10. [Known limitations](#10-known-limitations)
11. [Appendix A – Differences to Stefan's manual](#appendix-a--differences-to-stefans-manual)
12. [Appendix B – Keyword list](#appendix-b--keyword-list)
13. [Credits and licence](#credits-and-licence)

---

## 1. Hardware and getting started

### 1.1 What you need

* An Atari 2600 (or compatible) with a **PlusCart** or **UnoCart** running **UCA firmware v2.3.32** or newer.
  TinyELF Basic uses the new vcsLib interface of the firmware and does not start on v2.3.28 or older.
  Chameleon Cart is untested.
* A **KeyPortari** keyboard adapter in both joystick ports with the **latest KeyPortari firmware**. TinyELF Basic uses its
  direct ASCII protocol (7 data bits over the joystick port) and its chip-enable line (bit 7). There is no other input method yet.
* For storage (optional): an I2C EEPROM device in **SaveKey** style, connected through the KeyPortari right joystick port
  (see [chapter 8](#8-storage-drives-the-tinyelf-filesystem-list-and-enter)). It shares the port lines with the KeyPortari.
  Before every storage access TinyELF Basic pulls the KeyPortari's chip-enable line low, which takes the KeyPortari off
  the lines at once, so storage commands start without a pause.

### 1.2 The screen

The display is a 36-column text kernel (ported from Omegamatrix's 36-character kernel):

* **36 columns × 14 rows** of text, each row 12 scanlines tall plus one blank transition line
  (13 scanlines per row, 182 in total).
* The character set lives in RAM and can be changed with `POKE` (see [chapter 7](#7-memory-map-for-peek-and-poke)).
* Text colour can be set per text row, background colour per graphics line ([chapter 7](#7-memory-map-for-peek-and-poke)).

### 1.3 Program memory

At start-up TinyELF Basic prints its banner, e.g.

```
TinyELF Basic v0.21 65535 Bytes Free
```

On a PlusCart or UnoCart 65535 bytes of RAM are available for the program and its variables. `SIZE` shows the remaining free memory.

### 1.4 A first session

Lines that start with a number are stored as program lines; everything else is executed immediately.

```
10 PRINT "Hello"
20 GOTO 10
RUN
```

* Typing a line number that already exists replaces that line.
* Typing **just a line number** deletes that line.
* There is no full-screen editor; retype a line to change it.
* Statements on one line are separated with `:`.
* **ESC** stops a running program, also while it waits in `INPUT` **[HW]**. At the prompt ESC throws away the line typed so far.

### 1.5 Emulators

TinyELF Basic runs in Gopher2600 (recent nightly, with `-keyportari ASCII` and `-right SAVEKEY`), with 49152 bytes
(48 KiB) of program memory.
TinyELF Basic does not run in Stella: Stella's ELF support emulates a Cortex-M0, while TinyELF Basic is built for the
Cortex-M4 of the PlusCart and UnoCart. Gopher2600's SaveKey emulation only knows the single I2C address 0x50
(drive **E1**, 32 KiB), so E2–E8 do not exist there. It does not emulate the KeyPortari chip-enable line, and it needs
a few idle frames between two storage accesses, which TinyELF Basic does not wait for: **storage commands can fail
in Gopher2600 with error 23 or 20.** Emulators also do not reproduce hardware timing problems, so always check
important things on real hardware.

---

## 2. The language at a glance

TinyELF Basic is an **integer-only BASIC**: numbers are 32-bit signed integers, there is no floating point.
Based on Stefan's *language sets*, TinyELF Basic is built with:

| Language set | In TinyELF Basic? |
|--------------|-------------------|
| Core (Palo Alto Tiny BASIC) | Yes |
| Apple 1 (variables, strings, arrays, `NOT`/`AND`/`OR`, `PEEK`, `POKE`, …) | Yes |
| Stefan's extensions (`ELSE`, `BREAK`, `CONT`, `SET`, `SQR`, `POW`, `MAP`, `GET`, `PUT`, …) | Yes |
| Error handling (`ERROR`) | Yes |
| Bitwise (`<<`, `>>`, `BIT`) | Yes |
| Dartmouth (`DATA`, `READ`, `RESTORE`, `DEF FN`, `ON … GOTO/GOSUB`) | Yes |
| Two-dimensional arrays and string arrays | Yes |
| MS-style strings (`LEFT$`, `RIGHT$`, `MID$`, `ASC`, `CHR$`, `SPC`, `STR$`, `VAL`, `INSTR`, `+` for strings) and the power operator `^` | Yes |
| MS-style variable names (up to 32 characters, the first two are significant) | Yes – see [section 4.1](#41-variables) |
| Timer (`MILLIS`, `DELAY`) | Yes |
| **TinyELF / Atari additions** (`GRAPHICS`, `PLOT`, `DRAWTO`, `STICK`, `STRIG`, `PTRIG`, `FORMAT`, `CATALOG`, `ENTER`, `DELETE`) | Yes – see chapters 5–8 |
| Float, Darkarts, IoT (`SENSOR`, `WIRE`, …), timers/events, structured (`WHILE`, `SWITCH`, `DO`), file I/O (`OPEN`, `CLOSE`, `FDISK`), Arduino pin I/O, `HELP`, `EDIT`, `USR`, `CALL` | **No** – see [Appendix A](#appendix-a--differences-to-stefans-manual) |

Other basics:

* Keywords are upper case. Variable names are converted to upper case on input.
* Spaces between keywords and names are optional: `FORI=1TO10` and `IFA=BTHENPRINT"EQUAL"` work.
* Expressions are evaluated left to right with `^` binding strongest, then `*`, `/`, `%`, then `+`, `-`.
  Comparisons bind weakest and yield **-1 for true and 0 for false**, so `NOT`, `AND` and `OR`
  work as bitwise operators (`NOT 0` is `-1`).
* `%` is the integer modulus.

---

## 3. Statement and function reference

### 3.1 Program control

#### `RUN`  **[UP]**
Starts the program. `RUN` clears all variables and stacks first. `GOTO line` restarts without clearing.

#### `NEW`  **[UP]**
Deletes the program and all variables. Settings made with `SET` are kept.

#### `STOP` / `END`  **[UP]**
`STOP` halts the program; `CONT` (typed at the prompt) resumes it. `END` ends the program.
The **ESC** key stops a running program from the keyboard **[HW]**.

#### `CONT`  **[UP]**
At the prompt: continue after `STOP`. Inside a `FOR` loop: skip the rest of the loop body and go to the next iteration.

#### `DELAY ms`  **[??]**
Pauses the program for `ms` milliseconds; the screen keeps running while it waits. TinyELF Basic always runs at
60 frames per second (also on PAL consoles), so the time is rounded up to whole frames of 1/60 s.

```
10 PRINT "Wait...": DELAY 2000: PRINT "Go!"
```

#### `REM`  **[UP]**
`REM` itself is recognized as a keyword; everything after it to the end of the line is stored as-is, exactly as typed, including case:

```
10 REM This is a comment
```

#### `LIST`  **[UP]** / `LIST "En:file"` **[HW]**
```
LIST            list the whole program on screen
LIST 10         list line 10
LIST 10, 100    list lines 10 to 100
LIST "E1:X.LST" write the whole program as plain text to a file
```
The file form is described in [section 8.6](#86-list-and-enter-text-files).

#### `ENTER "En:file"`  **[HW]**
Reads a text listing from a file and stores it as program lines. See [section 8.6](#86-list-and-enter-text-files).

#### `DELETE "En:file"`  **[HW]**
Deletes a file from a drive. See [section 8.4](#84-save-load-and-delete).

### 3.2 Branching and loops

#### `GOTO`, `GOSUB`, `RETURN`  **[UP]**
`GOTO` and `GOSUB` accept expressions (`GOTO 100+I`). `RETURN` ends a subroutine.

#### `IF … THEN … ELSE`  **[UP]**
```
IF A=0 THEN PRINT "Zero" ELSE PRINT "Not zero"
IF B=A PRINT "Equal"
IF X=1 GOTO 100
```
`THEN` may be omitted. `ELSE` may also start the following program line. Zero is false, everything else true.

#### `FOR … TO … STEP … NEXT`  **[UP]**
```
FOR I=1 TO 10 STEP 2: PRINT I: NEXT I
```
`STEP` defaults to 1 and the variable after `NEXT` is optional. Each loop needs its own `NEXT`
(no `NEXT I,J`). `FOR I: … NEXT` is an endless loop. `BREAK` leaves the innermost loop, `CONT` jumps to its next iteration.

#### `ON … GOTO`, `ON … GOSUB`  **[HW]**
```
ON X GOTO 100, 200, 300
ON K-1 GOSUB 500, 600
```
Jumps to the first line for 1, the second for 2 and so on. If the value is 0, negative or larger than the number
of lines, there is no jump and execution continues with the next statement.

### 3.3 Input and output

#### `PRINT`  **[UP]**
```
PRINT "Hello"
PRINT A, B          the comma inserts a space
PRINT A; B          the semicolon joins without space
PRINT #6, A, B      numbers right-aligned in a field of width 6; PRINT #0 resets
PRINT "SCORE "+S$   + joins strings
PRINT SPC(5); "X"   SPC(n) prints n spaces
```
`PRINT` ends the line with LF. A row of exactly 36 characters already moves the cursor to the next row, so the LF
then leaves an empty row; end such a `PRINT` with `;` to avoid that.

#### `INPUT`  **[UP]**
```
INPUT A
INPUT "Number: ", A
INPUT "First: ", A, "Second: ", B
```
The `?` prompt is only shown when no prompt string is given. Numeric input may be an expression
(`3+4*(4+5)` is accepted).

#### `GET` and `PUT`  **[HW]**
`GET A` reads one key from the keyboard **without waiting** and without RETURN: it returns the key code, or 0 if no key
is pressed. To wait for a key, loop:

```
10 GET K: IF K=0 THEN GOTO 10
```

`PUT 65` writes the character with that code.

#### `TAB`  **[UP]**
`TAB 20` outputs 20 spaces; `PRINT TAB(20); "x"` is also accepted.

#### `CLS`  **[HW]**
Clears the screen and homes the cursor. In `GRAPHICS 3` and `GRAPHICS 8` it also clears the graphics area.

#### `LOCATE x, y`  **[HW]**
Moves the cursor to column `x`, row `y`. Coordinates start at **1** (columns 1…36, or 1…18 in `GRAPHICS 1`; rows 1…14).
A position outside the screen is ignored and the cursor stays where it is.

### 3.4 Memory and system

#### `PEEK(addr)`, `POKE addr, value`  **[HW]**
On TinyELF these address the **kernel's hardware memory map** (screen RAM, colour RAM, character RAM, TIA/RIOT shadows),
not the BASIC program memory. See [chapter 7](#7-memory-map-for-peek-and-poke). `PEEK` returns 0…255.
`POKE` with a **negative** address writes to BASIC memory at address `-addr`; `PEEK` cannot read BASIC memory.

#### `DUMP`  **[UP]**
`DUMP 0, 100` prints a hex dump of BASIC memory.

#### `CLR`  **[UP]**
Removes all variables and clears the stacks.

#### `SET n, value`  **[UP]**
Changes interpreter properties. Settings survive `NEW` and `CLR`. The ones that are useful on TinyELF:

| Call | Effect |
|------|--------|
| `SET 0, 1` | Debug mode: shows the token stream. `SET 0, 0` switches it off. |
| `SET 12, 0` | Arrays start at index 0 instead of 1. Any positive lower bound is allowed. |
| `SET 16, n` | Default length of automatically created strings (default 32). |
| `SET 17, 1` | C-style booleans (true = 1). Default is -1 (MS style). |
| `SET 19, 1` | `RND(n)` returns 1…n instead of 0…n-1. |
| `SET 20, 0` | Switch off substring notation. |
| `SET 21, 1` | `DIM A(10)` creates elements 0…10 (MS style). |
| `SET 23, 1` | Allow lower-case names. |
| `SET 25, 1` | Numbers in Microsoft style: `PRINT` puts a space before a positive number and after every number, `STR$(5)` is `" 5"`. **[??]** |

> Do **not** use `SET 1` (EEPROM autorun flag of the Arduino versions) or `SET 2`…`SET 5` (output/input device switching);
> they are inherited from other platforms and can lock you out.

### 3.5 Functions

| Function | Description |
|----------|-------------|
| `ABS(x)` | Absolute value. **[UP]** |
| `INT(x)` | Integer part (identity on this integer-only BASIC). **[UP]** |
| `SGN(x)` | -1, 0 or 1. **[UP]** |
| `RND(n)` | Random number 0…n-1 (see `SET 19`). `RND(-n)` gives -n…-1. The seed is `@R`. **[UP]** |
| `SQR(x)` | Integer square root. **[UP]** |
| `POW(a, b)` | a to the power b (0 for a negative exponent). **[UP]** |
| `MAP(v, a, b, c, d)` | Maps `v` from range a…b to range c…d, like Arduino `map`. **[UP]** |
| `LEN(s$)` | Length of a string. **[UP]** |
| `BIT(v, n)` | Non-zero if bit `n` of `v` is set. **[UP]** |
| `SIZE` | Free memory between program and variables. **[UP]** |
| `HIMEM` | Topmost free memory cell. **[UP]** |
| `MILLIS(n)` | Milliseconds since power-on, divided by `n` (`MILLIS(1)` is plain milliseconds). Counts in steps of one frame (1/60 s). **[??]** |
| `PEEK(a)` | See above. **[HW]** |
| `STICK(n)`, `STRIG(n)`, `PTRIG(n)` | Controllers, see [chapter 6](#6-controllers-stick-strig-ptrig). **[HW]** |

String functions **[??]**. Every string argument can be any string expression, also another function:
`RIGHT$(STR$(X),2)`, `ASC(MID$(A$,I,1))`.

| Function | Description |
|----------|-------------|
| `LEFT$(s$, n)` | The first `n` characters. |
| `RIGHT$(s$, n)` | The last `n` characters. |
| `MID$(s$, p, n)` | `n` characters from position `p`; `MID$(s$, p)` is the rest from position `p` on. |
| `ASC(s$)` | Code of the first character, 0 for an empty string. |
| `CHR$(n)` | A string of one character with code `n`. |
| `STR$(n)` | The number as a string, without a leading space: `STR$(5)` is `"5"`. |
| `VAL(s$)` | The number at the start of the string: `VAL("12AB")` is 12, `VAL("X")` is 0. |
| `INSTR(s$, f$)` | Position of `f$` in `s$`, 0 if it is not found. `INSTR(s$, f$, p)` starts the search at position `p`. |
| `SPC(n)` | Only in `PRINT`: `n` spaces. |

Operators: `+ - * / %`, power `^`, comparisons `= < > <= >= <>`, logic `NOT AND OR` (bitwise), shifts `<<` and `>>`
(same precedence as `*`; `PRINT 1<<2` prints 4). **[UP]**
`^` is evaluated left to right like the other operators (`2^3^2` is 64); a negative exponent gives 0.

### 3.6 DATA, READ, RESTORE and DEF FN

#### `DATA`, `READ`, `RESTORE`  **[HW]**
```
10 DATA 3, 5, -2, "HI"
20 READ A, B, C, S$
30 RESTORE
40 READ D
```
* `DATA` lines hold numbers (also negative) and strings. They are never executed and can stand anywhere in the program.
* `READ` takes the next values, going through all `DATA` lines in line-number order.
* `RESTORE` starts again with the first value. `RESTORE n` continues with the n-th value. **[UP]**
* A `READ` after the last value gives **0 and no error** (Stefan's design, not Dartmouth style).

#### `DEF FN` and `FN`  **[HW]**
```
10 DEF FN F(X)=X*X+1
20 PRINT FN F(4)            prints 17
30 PRINT FN F(FN F(1))      functions can be nested
```
* A function is one expression with one numeric parameter, or none: `DEF FN P()=…`.
* The `DEF` line has to run before the function is used.
* `FN` only works inside a running program. Typed at the prompt it gives error 10.
* `LIST` shows `FN F(X)` without the space as `FNF(X)`; both forms are accepted.

---

## 4. Variables, strings, arrays and special variables

### 4.1 Variables  **[??]**
* A name starts with a letter, followed by letters, digits or `_`, up to 32 characters.
* Like in Commodore and other Microsoft BASICs, **only the first two characters count**: `HIGH`, `HITS` and `HI`
  are the same variable. `LIST` shows each name as it was typed.
* A keyword ends a name, which is why spaces are optional (`IFA=BTHEN…`, `FORI=ATOB`). For the same reason a name
  must not contain a keyword: `SCORE` is read as `SC OR E`, `POINTS` as `PO INT S`, `TOTAL` as `TO TAL`.
* The whole name is stored at every place it is used, so long names cost program memory.
* Variables live on a heap and are created on first use.

### 4.2 Strings  **[UP]**
* String variables end in `$`: `ME$="Hello"`.
* Strings are created with length 32 on first use (`SET 16` changes that) or dimensioned explicitly: `DIM A$(100)`.
* Substring notation: `A$(1,4)` is characters 1 to 4, `A$(6)` is from character 6 on.
  `A$(12)=" today"` appends/overwrites from position 12. `LEN(A$)` gives the length.
* A string used in a numeric expression evaluates to the code of its first character.
* Strings are compared with `=`, `<>`, `<`, `>`, `<=` and `>=` **[??]**: the first different character decides, a shorter
  string comes before a longer one that starts the same (`"AB"<"ABC"`).

**MS-style strings** **[??]**: besides the substring notation there are the functions `LEFT$`, `RIGHT$`, `MID$`, `ASC`,
`CHR$`, `STR$`, `VAL` and `INSTR` ([section 3.5](#35-functions)). `+` joins strings in an assignment and in `PRINT`:

```
10 A$="TINY": B$="ELF"
20 C$=A$+B$+"!"
30 PRINT "HELLO "+C$
```

Strings are changed in place, which makes a few things different from Microsoft BASIC:

* `A$=A$+B$` works, but the string that is assigned must not appear further right: `A$=B$+A$` and `A$="X"+A$`
  give wrong results. Use a second string: `T$=B$+A$: A$=T$`.
* `+` only works in an assignment and in `PRINT`, not in function arguments or comparisons:
  `LEN(A$+B$)` gives error 22.
* `STR$` and `PRINT` write numbers without the spaces of Microsoft BASIC; `SET 25, 1` switches them on.
* `SET 20, 0` switches to the Microsoft notation for string arrays: `DIM N$(5)` then creates the strings `N$(1)`…`N$(5)`
  and `N$(2)` is the second string. The substring notation is off then; use `MID$`.

### 4.3 Arrays  **[UP]**
* `DIM A(100)` creates `A(1)`…`A(100)`. Without `DIM`, an array has 10 elements. `SET 12, 0` makes indexing start at 0.
* **Two-dimensional arrays** **[HW]**: `DIM A(3,4)` creates `A(1,1)`…`A(3,4)`. An index outside the
  dimension gives error 16. Arrays without `DIM` stay one-dimensional.
* **String arrays** **[HW]**: `DIM C$(8,3)` creates **3 strings of up to 8 characters**
  (length first, then count). The element is the second bracket: `C$()(2)` is the whole second string,
  `C$(2,3)(2)` a substring of it.

```
10 DIM C$(8,3)
20 C$()(1)="AB": C$()(2)="CDE"
30 PRINT C$()(1); C$()(2); LEN(C$()(2))       prints ABCDE3
```

### 4.4 Special variables (`@`)  **[UP]**
The `@` character starts variables that give access to system properties:

| Name | Meaning |
|------|---------|
| `@()` | Array over the free memory (from the top of memory down to the program). `@` alone is the number of elements. |
| `@R` | Random number seed (readable and writable). |
| `@A` | Number of characters available on the input stream. |
| `@C` | Next character of the input stream (0 if none). |
| `@I`, `@O` | Current input/output stream number. |
| `@S` | I/O error status. |
| `@T` | Milliseconds since power-on, like `MILLIS(1)`. **[??]** |
| `@U` | User variable (reserved, no function on TinyELF yet). |

---

## 5. The screen: text modes, GRAPHICS, PLOT, DRAWTO

### 5.1 `GRAPHICS mode`  **[HW]**

`GRAPHICS n` switches the display mode and, unless told otherwise, clears the screen. The mode number
follows the Atari BASIC convention: the mode is in the low four bits, **+16** removes the text window,
**+32** does not clear the screen.

| Mode | Description | Coordinate range for `PLOT`/`DRAWTO` |
|------|-------------|----------------------------------------|
| `0` | Normal text, 36 columns × 14 rows. Power-on default. | – |
| `1` | Large text, 18 double-width columns. | – |
| `3` | Playfield (block) graphics, 40 columns. | `x` 0…39, `y` 0…10 (with text window) or 0…13 (with `+16`) |
| `8` | High resolution graphics using the player objects. | `x` 0…144, `y` 0…142 (with text window) or 0…180 (with `+16`) |

Any other mode number gives error 16.

* In modes 3 and 8 the bottom **3 text rows** stay available as a scrolling text window
  (`PRINT`, `INPUT`, and `LIST` still work there). `GRAPHICS 8+16` uses the full screen, with no text window.
* `GRAPHICS 8+32` keeps the current screen content (no clear).
* The graphics cursor is reset to 0,0 by `GRAPHICS`.

Mode 3 has one block per text row vertically, so its blocks are much taller than wide (like real Atari GRAPHICS 3).
Mode 8 is 145 pixels wide; a few `x` positions fall on physically dark gaps between player columns and are silently skipped.

### 5.2 `PLOT x, y`  **[HW]**
Lights one pixel and remembers the position as the start of the next `DRAWTO`.
Coordinates outside the range for the current mode (or a call in a mode without graphics) give error 16.
Pixels can only be set; clear with `CLS` or `GRAPHICS`. There is no `COLOR` command yet – see [chapter 7](#7-memory-map-for-peek-and-poke).

### 5.3 `DRAWTO x, y`  **[HW]**
Draws a straight line (Bresenham) from the last `PLOT`/`DRAWTO` position to `x, y`.

```
10 GRAPHICS 8
20 PLOT 0,0
30 DRAWTO 144,100
40 DRAWTO 0,100
50 DRAWTO 0,0
```

### 5.4 `CLS`, `LOCATE`
Described in [section 3.3](#33-input-and-output).

### 5.5 Pictures from image files
The **TinyELF Image Converter** (`tools/TinyELF-Image-Converter.html`, opens in any browser, works offline) turns a PNG or
JPEG into a TinyELF Basic program: `DATA` lines that a short loop copies into the `GRAPHICS 8` video RAM
(`POKE 4096+…`). It dithers the picture onto the real 145 × 181 `PLOT` grid and can use the colour features of
[chapter 7](#7-memory-map-for-peek-and-poke): a background colour per scanline and a pixel colour per text row,
picked from the picture. Load the result with `ENTER` and start it with `RUN`.

---

## 6. Controllers: STICK, STRIG, PTRIG  **[HW]**

| Function | Returns |
|----------|---------|
| `STICK(n)` | Joystick `n` (0 or 1) as a 4-bit value: bit 0 up, bit 1 down, bit 2 left, bit 3 right. A bit is **0 when the direction is pressed**, so 15 means centred. |
| `STRIG(n)` | Fire button of joystick `n`: **0 while pressed**, 1 while released. |
| `PTRIG(n)` | Paddle trigger `n` (0…3): 0 while pressed, 1 while released. Paddles 0/1 are the left/right lines of port 0, paddles 2/3 of port 1. |

**With a KeyPortari attached:**

* **No key pressed:** the KeyPortari passes the devices plugged into it (joysticks, paddles, SaveKey, …) straight through, so the functions above work as usual.
* **Key pressed:** as long as a key is held and the KeyPortari's chip-enable line is not pulled low, the KeyPortari puts the key's 7-bit ASCII value on the port. That value then also shows up in the bits read by `STICK` and `PTRIG`.
* **`STRIG`:** never affected by the KeyPortari.

How a SaveKey affects these functions is not documented yet.

---

## 7. Memory map for PEEK and POKE  **[HW]**

`PEEK` and `POKE` address the kernel's hardware map (the address is decoded by testing the highest set bit):

| Address (dec) | Hex | Region |
|---------------|-----|--------|
| 4096… | `$1000` | **Video RAM**: the screen pixels, 20 bytes per scanline (3360 bytes, see [section 7.4](#74-video-ram)). |
| 2048… | `$0800` | **Character RAM**: the redefinable character set, 12 bytes per character (see [section 7.3](#73-character-ram)). |
| 1024… | `$0400` | **Background colour RAM**: one colour byte per graphics line (up to 182). |
| 512… | `$0200` | **RIOT mirror RAM** (32 bytes), including the frame counter and the interpreter's token budget. |
| 256… | `$0100` | **Text colour RAM**: one colour byte per text row (rows 0…13). |
| 128… | `$0080` | **Playfield shadow RAM**: 8 bytes per text row (`COLUPF`, `CTRLPF`, `PF0`–`PF2` for the left half and the right half). |
| 0…26 | `$00`–`$1A` | **TIA register shadow**: the sound registers (see [section 7.2](#72-sound)) and a few kernel variables. |

Only use the addresses listed here. `PEEK` and `POKE` do not check them: a `POKE` to an "illegal" address outside these regions overwrites other memory, and weird things will happen, up to a crash.

Colour values are TIA colour bytes (hue in the high nibble, luminance in the low nibble), e.g.

```
POKE 256, 30      colour of text row 0
POKE 1024+10, 150 background colour of graphics line 10
```

The default background colour depends on the TV system: **148** (`$94`) on NTSC, **180** (`$B4`) on a PAL 2600 and
**164** (`$A4`) on a PAL 7800. To give all lines the PAL 2600 default again:

```
FOR I=0 TO 181: POKE 1024+I, 180: NEXT I
```

### 7.1 Interpreter values in the RIOT mirror RAM

| Address (dec) | Meaning |
|---------------|---------|
| 648…651 | **Frame counter**, 32 bit, low byte first. `PEEK(648)+256*PEEK(649)` gives the lower 16 bits. |
| 652, 653 | **Token budget**, 16 bit, low byte first (see below). |
| 654 | Height of the text window in `GRAPHICS 3` and `8`, in text rows (for example `POKE 654,7`). Default value is `3` |

The screen is redrawn after a number of interpreted tokens, the **token budget**. At start-up it is set from the CPU clock
(clock / 650000, that is **332** on a 216 MHz cartridge). A smaller budget gives a steadier picture but slower programs,
a larger one faster programs, but the picture can roll or go black while a long computation runs.

```
POKE 653,0: POKE 652,100     budget 100
POKE 653,1: POKE 652,76      budget 332 (1*256+76)
```

### 7.2 Sound  **[HW]**

There is no sound command. Sound is made by writing the TIA audio registers into the TIA register shadow with `POKE`.
The address is the TIA register number (`$15`…`$1A`), written in decimal:

| Address (dec) | Hex | TIA register | Values |
|---------------|-----|--------------|--------|
| 21, 22 | `$15`, `$16` | `AUDC0`, `AUDC1`: tone type of channel 0 and 1 | 0…15, for example 4 = pure tone, 8 = noise |
| 23, 24 | `$17`, `$18` | `AUDF0`, `AUDF1`: frequency divider | 0…31, a larger value gives a lower pitch |
| 25, 26 | `$19`, `$1A` | `AUDV0`, `AUDV1`: volume | 0 (off)…15 |

The kernel copies these six registers to the TIA once per frame. A new value is therefore heard from the next frame on,
and changes faster than one frame (about 17 ms) are not heard. Use `DELAY` between the steps of a sound effect.

```
POKE 21,4: POKE 23,10: POKE 25,12     pure tone on channel 0
POKE 25,0                             channel 0 off
```

A tone keeps playing until its volume is set to 0, also after `END` or **ESC**. A program should end with
`POKE 25,0: POKE 26,0`.

The other addresses of the block are used by the kernel: 0…3 hold the character mask, the cursor column and row and
the frame parity, 8…13 the last values read from `INPT0`…`INPT5`. Do not `POKE` addresses 0…3.

### 7.3 Character RAM

The character set lies in RAM and can be changed with `POKE`. It holds the ASCII codes 32 (space) to 127 in order, shifted by 32: the 12 bytes of character `code` start at `2048 + (code − 32) × 12`, one byte per scanline from top to bottom.

| Code | Characters | Addresses |
|------|------------|-----------|
| 32 | space | 2048…2059 |
| 48…57 | `0`…`9` | 2240…2359 |
| 65…90 | `A`…`Z` | 2444…2755 |
| 97…122 | `a`…`z` | 2828…3139 |
| 127 | Delete | 3188…3199 |

Codes 32…126 can be printed. Code 127 is Delete: printing it moves the cursor back, so its pattern is never shown. The 144 bytes after it (3200…3343) are unused.

Every byte holds one pixel row of the character, which is 3 pixels wide:

* The **low nibble** is the normal row: 4 = left, 2 = middle, 1 = right pixel. Bit 3 (value 8) is the gap column to the left of the character and is 0 in a normal character.
* The **high nibble** is the same row inverted (15 minus the low nibble, so the gap column is lit). TinyELF Basic draws the blinking cursor with it.

To redefine a row with the pattern `V` (0…7), poke `(15 − V) × 16 + V`. This makes `#` (code 35) a solid block:

```
FOR L=0 TO 11: POKE 2048+(35-32)*12+L, (15-7)*16+7: NEXT L
```

A change only affects characters printed afterwards: the screen holds pixels, not character codes (see [section 7.4](#74-video-ram)). `GRAPHICS 1` uses the same patterns with every pixel doubled in width.

### 7.4 Video RAM

The video RAM holds the pixels of the screen, not character codes: `PRINT` copies the character's pattern from the character RAM into it, and `PLOT` in `GRAPHICS 8` sets single bits in it. (`GRAPHICS 3` uses the playfield shadow RAM instead.) It is organised by text row and scanline:

```
address = 4096 + (row × 12 + line) × 20 + byte
```

* `row` is the text row 0…13, `line` the scanline within that row 0…11, from top to bottom. The 13th scanline of each row, the gap between two text rows, is not stored and always shows the background.
* Every scanline has **20 bytes**. Bytes 0…17 are pixels, bytes 18 and 19 are needed by the display kernel. The whole area is 14 × 12 × 20 = 3360 bytes (4096…7455).

**Bytes 0…17:** in `GRAPHICS 0`, byte `n` holds the two text columns `2n` and `2n+1`; in `GRAPHICS 1` it holds column `n` with every pixel doubled. Bit 7 is the leftmost pixel. In `GRAPHICS 0` every character cell is 4 pixels wide (gap column plus 3 pixels), but the TIA cannot place its objects exactly 8 pixels apart, so the two cells sit at slightly different bits from byte to byte:

| Bytes | Left column (`2n`) | Right column (`2n+1`) |
|-------|--------------------|-----------------------|
| 1, 3, 5, 13, 15, 17 | gap bit 7, pixels bits 6…4 | gap bit 3, pixels bits 2…0 |
| 2, 4, 6…12, 14, 16 | pixels bits 7…5 (no gap bit) | gap bit 4, pixels bits 3…1 |
| 0 | gap bit 6, pixels bits 5…3 | gap bit 2, left and middle pixel bits 1…0, right pixel in byte 19 |

Bit 0 of the bytes in the second line is only set by inverse characters (the cursor block). Bit 7 of byte 0 is hidden behind the left border.

**Byte 18** is a copy the kernel needs: `(byte 0 AND 7) OR (byte 2 AND 248)`. After a `POKE` into byte 0 or byte 2 of a scanline, write byte 18 of that scanline as well.

**Byte 19:** only bit 1 (value 2) is used. It switches a TIA missile that draws the right pixel of text column 1 (of column 0 in `GRAPHICS 1`). Write only 0 or 2 – the other bits end up in the TIA's horizontal motion register and shift the picture.

The kernel shows every other byte of a scanline in one frame and the rest in the next, so a change is complete after two frames. The 20 bytes after the last scanline (7456…7475) belong to the kernel; do not change them.

This draws a line under text row 0 (scanline 11) across bytes 0…18. A few single dark columns remain, where no TIA object can draw a pixel:

```
A=4096+11*20: FOR B=0 TO 18: POKE A+B, 255: NEXT B
```

---

## 8. Storage: drives, the TinyELF filesystem, LIST and ENTER

All of this chapter is **[HW]**: verified on real hardware, including exchanging files with the
SaveKey-Manager PC tool in both directions.

### 8.1 Drives E1–E8

Storage is on I2C EEPROM devices in a SaveKey-compatible joystick-port adapter. Storage devices are addressed
like the drives of an Atari DOS, but named **E1 to E8** instead of D1 to D8:

* The 8 possible I2C addresses **0x50…0x57** correspond to E1…E8.
* Every address covers a 64 KiB block. **A run of consecutive responding addresses is one drive** with
  `n × 64 KiB`, named after its *lowest* address. Example: an AT24CM02 (256 KiB) answering 0x54–0x57 is the single drive **E5**;
  E6, E7 and E8 are then *part of E5* and cannot be selected on their own.
* A lone address is one drive of unknown size – a single 0x50 could be a 4 KiB or a 64 KiB chip, the scan cannot tell.
  `FORMAT` therefore assumes **32 KiB** unless you give a size.
* The drives are **scanned every time a command uses one**; nothing is cached.

In a typical current setup: **E1** is a single 0x50 chip and **E5** is the 256 KiB device on 0x54–0x57 (SaveKey Plus).

Every storage command needs the **drive prefix `"En:"`**. There is no default drive. Without a prefix, or for a drive
that is not present, the command fails with an error (see [chapter 9](#9-errors)).

### 8.2 File names

`"E1:NAME.EXT"` – up to **8 characters** of name and **3 of extension** (DOS 8.3 style).
Characters beyond that are silently dropped. File names are **case-sensitive as typed** (`Test.lst` and `TEST.LST` are different files).
The filename has to be a literal string in quotes.

### 8.3 `FORMAT "En:" [, size]`

Prepares a drive. Without `size`, a multi-address drive uses its full capacity (`n × 64 KiB`) and a single address uses 32 KiB.
An explicit `size` (in bytes) is checked against a multi-address drive's capacity; the filesystem itself cannot address more
than **262144 bytes (256 KiB)**.

```
FORMAT "E1:"
FORMAT "E5:"
FORMAT "E1:", 4096
```

`FORMAT` only writes the header, the free-space map and the (empty) directory. **It does not erase the data area**
(to spare the EEPROM write cycles), so old data stays physically present but unreachable.

The sector size and the maximum number of files depend on the capacity:

| Drive capacity | Sector size | Directory | Max. files |
|----------------|-------------|-----------|------------|
| up to 8 KiB | 64 bytes | 1 sector | 4 |
| up to 16 KiB | 64 bytes | 2 sectors | 8 |
| up to 32 KiB | 64 bytes | 4 sectors | 16 |
| over 32 KiB, up to 128 KiB | 128 bytes | 8 sectors | 64 |
| over 128 KiB, up to 256 KiB | 256 bytes | 4 sectors | 64 |

Each sector holds `sector size − 3` bytes of file data. The maximum number of sectors is 1024. The maximum number of files is 64 (same as in Atari DOS 2.x)

### 8.4 SAVE, LOAD and DELETE

```
SAVE "E1:HELLO.BAS"
LOAD "E1:HELLO.BAS"
DELETE "E1:HELLO.BAS"
```

* `SAVE` stores the program in its **tokenized (binary) form**. It is fast and compact but not readable on a PC.
  A file with the same name is **overwritten** (its sectors are freed first), so a name never appears twice.
* `LOAD` **replaces** the program in memory and clears the variables.
* `DELETE` frees the file's sectors and removes it from the directory.

### 8.5 `CATALOG "En:"`

Lists the files of a drive:

```
CATALOG "E5:"
HELLO.BAS       3
TEST.LST       17
Size 262144 (Free 255752)
```
(numbers illustrative)

* The number next to a name is the file's length in **sectors** (not bytes).
* `Size` is the formatted capacity, `Free` the free data space in bytes (an upper bound: the last sector of every file is usually only partly used).
* Files written by other tools (for example uploaded with SaveKey-Manager) appear here too.

### 8.6 LIST and ENTER text files

`SAVE`/`LOAD` use the internal binary format. To exchange programs as **plain text** with a PC (edit them in any editor,
keep them in version control, post them on the forum) use the text pair:

```
LIST "E1:HELLO.LST"      write the program as text
NEW
ENTER "E1:HELLO.LST"     read the text back in
```

`LIST "En:name"`:
* Writes the whole program as detokenized text, one line per program line, each terminated by LF.
  Keywords are expanded, so the file reads like the listing on screen.
* Only a literal string as the target is supported, and there is no line range with a file target yet.
* An existing file of the same name is overwritten.

`ENTER "En:name"`:
* Reads a text file and stores every line that starts with a line number, exactly as if you had typed it.
  It **merges** with the program in memory (like Atari DOS `ENTER`): lines with the same number are replaced, all other lines stay.
  Use `NEW` first for a clean slate.
* Lines without a leading line number are ignored; lines starting with `#` are skipped.
* LF, CR or CRLF line endings all work. A line may be up to about 250 characters.

### 8.7 Autostart with `E1:AUTORUN.BAS`

A program saved as `AUTORUN.BAS` on drive E1 starts by itself when TinyELF Basic boots, like `AUTORUN.SYS` on an Atari DOS disk.

```
SAVE "E1:AUTORUN.BAS"
```

* At power-on the file is loaded and run instead of showing the start banner. When the program ends, `READY` appears and the program stays in memory.
* Only drive E1 is checked, and the file has to be a `SAVE` file; a text file written with `LIST` does not start.
* Without a SaveKey, on an unformatted drive or without the file, TinyELF Basic starts normally, without an error message.
* `ESC` stops the program like any other. To get rid of the autostart, stop it and type `DELETE "E1:AUTORUN.BAS"`, or delete or rename the file with the SaveKey-Manager.

### 8.8 Practical notes

* While a storage command runs, the screen is blanked and shows a solid **pink/magenta** colour.
* Formatted drives use the same on-disk format as the **SaveKey-Manager** PC tool. You can inspect, download and upload files with it.

### 8.9 Error behaviour of the storage commands

| Command | Error when it fails |
|---------|---------------------|
| `FORMAT` | 23 (bad/missing drive prefix, drive not present, or a size too large for the drive); 16 for a size above 262144 or below zero |
| `SAVE` | 23 (bad/missing drive prefix, drive not present or not formatted); 14 when the drive is full or a write fails |
| `LOAD` | 20 |
| `DELETE` | 23 (bad/missing drive prefix, drive not present or not formatted); 20 when the file does not exist |
| `CATALOG` | 23 |
| `LIST "file"` | 23 |
| `ENTER` | 20 (file not found, drive missing/not formatted) |

---

## 9. Errors

TinyELF Basic prints only the **error number**, for example:

```
Error 10
READY
```

(The text messages of Stefan's interpreter are not compiled in.)

| Code | Meaning |
|------|---------|
| 10 | Syntax error |
| 11 | Number error |
| 12 | Division by zero |
| 13 | Unknown line |
| 14 | Out of memory (also: `SAVE` to a full drive) |
| 15 | Stack error |
| 16 | Index or parameter out of range (also: `PLOT`/`DRAWTO` outside the range or in a mode without graphics, unsupported `GRAPHICS` mode) |
| 17 | String operation error |
| 18 | Error in variable handling |
| 19 | Error in loop handling |
| 20 | File error |
| 21 | Function error |
| 22 | Wrong number or type of arguments |
| 23 | EEPROM error |
| 24 | SD card error (unused on TinyELF) |

Some commands report a token value (a negative or otherwise unusual number) instead, for example `RETURN` without `GOSUB`.
After an error the program is stopped, variables and the program pointer are kept and `CONT` can continue.

### Error trapping with `ERROR`  **[UP]**

```
10 ERROR GOTO 1000     jump to line 1000 when an error occurs
ERROR STOP             back to normal behaviour
ERROR CONT             continue after errors
```
Inside the handler `ERROR` used as a value is the error code. As with all error handling here, the FOR and GOSUB stacks are gone after an error.

---

## 10. Known limitations

* **No `OPEN`, `CLOSE`, `FDISK`** – the general file I/O set is not part of the current language profile.
* **No `COLOR` command** – colours are set by `POKE`, see [chapter 7](#7-memory-map-for-peek-and-poke).
* **No pixel erase** in `PLOT`.
* **`GRAPHICS` 2, 4–7, 9–11** are not implemented (error 16).
* **Emulators:** Gopher2600 only emulates a SaveKey at address 0x50 (drive E1 only), and storage commands can fail
  there with error 23 or 20 (see [section 1.5](#15-emulators)).
* **Long computations blank the screen.** While a statement or a run of statements takes longer than a frame, no picture
  is generated; the TV may roll or show black until the next frame. This is harmless. The token budget ([section 7.1](#71-interpreter-values-in-the-riot-mirror-ram))
  trades picture stability against speed.
* **7800:** On a 7800 (PAL) the first two and last two characters of a line can show faint artefacts from neighbouring
  characters. The 2600 is unaffected.
* **Filesystem ceiling:** the on-disk format cannot address more than 256 KiB per drive.
* **A drive must be formatted by TinyELF (or SaveKey-Manager) before use** – SaveKey chips formatted by other software are not recognized.

---

## Appendix A – Differences to Stefan's manual

For readers who know Stefan's original `MANUAL.md`: the following parts **do not exist** in TinyELF Basic.

| Removed | Reason |
|---------|--------|
| Floating point (`SIN`, `COS`, `TAN`, `ATAN`, `LOG`, `EXP`, `SET 18`, `SET 24`, `@P`) | Integer-only build. |
| Darkarts: `MALLOC`, `FIND`, `EVAL` | Not compiled in. |
| IoT: `AVAIL`, `SENSOR`, `WIRE`, `SLEEP`, `NETSTAT` | Not compiled in. |
| Timers and events: `AFTER`, `EVERY`, `EVENT` | Not compiled in. |
| Structured language: `WHILE/WEND`, `REPEAT/UNTIL`, `SWITCH/CASE`, `DO/DEND`, multiline `IF` | Not compiled in. |
| BASIC 2 extensions: line editor (`EDIT`), `HELP` | Not compiled in. |
| File I/O set: `OPEN`, `CLOSE`, `FDISK`, `@S` file status, POSIX/`autorun.bas` semantics | Replaced by the TinyELF filesystem ([chapter 8](#8-storage-drives-the-tinyelf-filesystem-list-and-enter)); autostart with `E1:AUTORUN.BAS` ([section 8.7](#87-autostart-with-e1autorunbas)). |
| Arduino I/O: `PINM`, `DWRITE`, `DREAD`, `AWRITE`, `AREAD`, `AZERO`, `LED`, `PULSE` | No such pins on the cartridge. |
| `PLAY` (tone on a pin) | No pins; sound comes from the TIA audio registers, see [section 7.2](#72-sound). |
| `USR`, `CALL`, and the low-level extension mechanism | Not compiled in. |
| Generic `COLOR`, `LINE`, `CIRCLE`, `RECT`, `FCIRCLE`, `FRECT` | Replaced by the Atari-style `GRAPHICS`/`PLOT`/`DRAWTO`. |
| Hardware driver chapters (serial, second serial, wire, radio, MQTT, RTC, VGA, POSIX/Raspberry Pi/ESP special systems) | Not applicable. |
| `SET` codes 1–11, 13–15 and 24 (hardware that is not present, floating point output) | Only the language-behaviour codes are meaningful on TinyELF. |
| The error message texts | Only error numbers are printed. |

**Added by TinyELF Basic:** `GRAPHICS`, `PLOT`, `DRAWTO`, `STICK`, `STRIG`, `PTRIG`, `FORMAT`, `CATALOG`, `LIST "file"`, `ENTER`, `DELETE`,
the E1–E8 filesystem, the hardware memory map for `PEEK`/`POKE`.

**Different in TinyELF Basic:** long variable names follow the Microsoft rule, only the first two characters are significant
([section 4.1](#41-variables)). `MILLIS` and `DELAY` count in frames of 1/60 s. ESC instead of `#` stops a program.
`SET 25` (Microsoft style numbers) and the string comparisons with `<`, `>`, `<=`, `>=` are TinyELF additions.

---

## Appendix B – Keyword list

All keywords:

```
ABS       AND       ASC       BIT       BREAK     CATALOG   CHR$      CLR
CLS       CONT      DATA      DEF       DELAY     DELETE    DIM       DRAWTO
DUMP      ELSE      END       ENTER     ERROR     FN        FOR       FORMAT
GET       GOSUB     GOTO      GRAPHICS  HIMEM     IF        INPUT     INSTR
INT       LEFT$     LEN       LET       LIST      LOAD      LOCATE    MAP
MID$      MILLIS    NEW       NEXT      NOT       ON        OR        PEEK
PLOT      POKE      POW       PRINT     PTRIG     PUT       READ      REM
RESTORE   RETURN    RIGHT$    RND       RUN       SAVE      SET       SGN
SIZE      SPC       SQR       STEP      STICK     STOP      STR$      STRIG
TAB       THEN      TO        VAL
```

Operators: `+ - * / % ^  = < > <= >= <>  << >>`.

---

## Credits and licence

* **Stefan Lenz** – the *Basic2* / *IoT BASIC* interpreter this port is built on
  (<https://github.com/slviajero/tinybasic>, GPL v3) and the manual this document is derived from.
* **Li-Chen Wang** – Palo Alto Tiny BASIC, the historical ancestor of the core language.
* **Omegamatrix** – the 36-character display kernel
  (<https://forums.atariage.com/topic/317204-36-character-kernel-redux>).
* **KeyPortari** – keyboard adapter and its direct ASCII protocol.
* **SaveKey / AtariVox** I2C protocol – the bit-banging driver is a port of the 6507 driver by Alex Herbert
  (optimised by Thomas Jentzsch).
* **SaveKey-Manager** – PC tool sharing the TinyELF on-disk format.
* **Zachary Scolaro** – vcsLib and the ELF support in the UCA firmware.
* **Gopher2600** (JetSetIlly) – emulator used for testing.

This manual is derived from Stefan Lenz's `MANUAL.md` and, like the interpreter, is distributed under the
GNU General Public License, version 3.

TinyELF Basic was first discussed in the AtariAge VCS-ELF Club:
<https://forums.atariage.com/topic/385281-elf-basic-interpreter-for-the-2600>
