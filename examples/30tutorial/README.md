# Tutorials

Short programs that show one feature of TinyELF Basic each. Copy a file to your SaveKey (for example from the Public Library of SaveKey-Manager) and load it with `ENTER`:

```
NEW
ENTER "E1:Loops.lst"
LIST
RUN
```

File names on TinyELF are case-sensitive, so type them exactly as listed below. Some programs were written for wider screens; long lines wrap on the 36-column TinyELF screen.

All of them were tested in Gopher2600 with TinyELF Basic v0.16: each one loads and runs without an error.

## The programs

| File | Shows |
|------|-------|
| `HelloWld.lst` | The inevitable Hello World |
| `Cond.lst` | `IF … THEN … ELSE` |
| `Logic.lst` | Logical expressions with `AND`, `OR`, `NOT` |
| `Loops.lst` | Loops in BASIC, also the advanced forms |
| `NestFor.lst` | Four nested `FOR` loops |
| `OnGoto.lst` | `ON … GOTO` and `ON … GOSUB` |
| `Func.lst` | Function definitions with `DEF FN` |
| `ReadData.lst` | `READ` and `DATA` |
| `PutGet.lst` | `PUT` and `GET` |
| `StopCont.lst` | `STOP` and `CONT` |
| `Debug.lst` | The debug mode settings |
| `Dump.lst` | `DUMP` of the BASIC memory |
| `Table.lst` | Print a table of values |
| `Math.lst` | The extended math functions |
| `Root.lst` | Find the closest integer square root |
| `Random.lst` | Analyse the distribution of random numbers |
| `Sort.lst` | Sort a set of numbers |
| `Array2D.lst` | Two-dimensional arrays |
| `String1.lst` | Strings (1): dimensioning and comparing |
| `String2.lst` | Strings (2): substrings, splitting and joining |
| `String3.lst` | Strings (3): characters and strings |
| `String4.lst` | Strings (4): string arrays |
| `StrTest.lst` | A string test program |
| `StrCase.lst` | Change the case of a string |
| `StrChar.lst` | Work with single characters of a string |
| `Primes.lst` | The first N prime numbers |
| `Primes2.lst` | The same with `@()` and `BREAK` |
| `Fibo.lst` | Fibonacci numbers and the golden ratio in integers |
| `Euler9.lst` | Project Euler problem 9 |
| `Parabola.lst` | Draw a parabola with text |
| `Mandel.lst` | The Mandelbrot set in integer arithmetic |
| `Eliza.lst` | ELIZA, the computer psychotherapist from the 70s |

## Credits and licence

These programs are from the examples of Stefan Lenz's BASIC interpreter
(<https://github.com/slviajero/tinybasic>, folders `examples/00tutorial`, `examples/08strings` and `examples/11stefans`),
renamed to 8.3 file names for TinyELF. They are distributed under his BSD 3-Clause licence,
see [`LICENSE-tinybasic.txt`](../LICENSE-tinybasic.txt).
