# Benchmarks

Classic BASIC benchmarks to measure how fast TinyELF Basic runs. Copy a file to your SaveKey (for example from the Public Library of SaveKey-Manager), then:

```
NEW
ENTER "E1:RFBench1.lst"
RUN
```

File names on TinyELF are case-sensitive, so type them exactly as listed below.

The programs measure with `MILLIS`, which on TinyELF counts in steps of one frame (1/60 s). The times include the time TinyELF spends drawing the screen while the program runs. They need TinyELF Basic v0.16 or newer and were tested in Gopher2600.

## The programs

| File | Measures |
|------|----------|
| `RFBench1.lst` … `RFBench7.lst` | The seven Rugg/Feldman benchmarks; each one prints the reference times of other BASICs for comparison |
| `Creative.lst` | The Creative Computing benchmark |
| `Bench.lst` | A quick test: 10,000 loops with an assignment |
| `BenchOps.lst` | Time for an empty loop, one token, an assignment and a multiplication |

## Credits and licence

These programs are from the examples of Stefan Lenz's BASIC interpreter
(<https://github.com/slviajero/tinybasic>, folders `examples/11stefans` and `test`),
renamed to 8.3 file names for TinyELF. They are distributed under his BSD 3-Clause licence,
see [`LICENSE-tinybasic.txt`](../LICENSE-tinybasic.txt).
