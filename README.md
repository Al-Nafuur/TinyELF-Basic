TinyELF Basic is a new experimental BASIC interpreter environment running entirely on the microcontroller of modern Atari 2600 flash cartridges (UnoCart/PlusCart/Chameleon Cart). It brings a classic 1970s-era TinyBasic interpreter to the VCS while using a custom 36-character wide display kernel.


**Display Kernel**

The video system is powered by a version of @Omegamatrix's 36-Character kernel ported to ELF (https://forums.atariage.com/topic/317204-36-character-kernel-redux).

The kernel provides:
- 36-column text mode
- Background colors can be changed every graphics line
- Text colors can set for every text row
- Character set in RAM
- Dedicated screen RAM
 

**Input Device**

TinyELF Basic is using the KeyPortari as input device. A new direct ASCII protocol is implemented for TinyELF Basic. Maybe in the future an expansion board for the KeyPortari can be used to store the basic programs to an audio port.


**BASIC Interpreter**

The interpreter is derived from the historical 1970s TinyBasic (http://ittybittycomputers.com/IttyBitty/TinyBasic) rewritten in C.

It currently implements the original 70s command set.
 

**Possible future additions:**

- "POKE", "PEEK" access to kernel's screen RAM, color RAM, charset RAM, and TIA registers (audio)
- Graphics commands in basic to "draw" to the screen.
- Optional upgrade to the more capable "Basic2" interpreter (https://github.com/slviajero/tinybasic/tree/main/Basic2)


**Hardware & Compatibility**

TinyELF Basic runs as an ELF ROM on:
- PlusCart
- UnoCart
- Chameleon Cart (untested)
- Stella / Gopher2600 emulators (input limited: no KeyPortari protocol yet)


**Memory Model**

The interpreter and screen system run entirely inside the cartridge MCU RAM. Current layout:

- ~3 KB Screen RAM
- ~1 KB Character Set RAM
- ~170 B Color RAM
- 32 KB BASIC RAM (Chameleon will have less! maybe 16K or 24K)

Note: UnoCart/PlusCart will later gain *full 64 KB BASIC RAM* via CCRAM (Chameleon has no CCRAM).
 

**Execution Model**

The current BASIC interpreter "loop" runs about 10 times per frame (with plenty of headroom, up to ~220 tokens per frame should be possible depending on the parser used). After each slice, control returns to the ELF loop to wait for end-of-overblank and render the next frame.
 

**Origins & Discussion**

This project was first proposed by me in the VCS-ELF Club on AtariAge:
https://forums.atariage.com/topic/385281-elf-basic-interpreter-for-the-2600
 

**Summary**

TinyELF Basic is a new experiment in “cartridge-resident computing” for the Atari 2600:

- A real BASIC interpreter
- A wide-character text display
- Keyboard input via KeyPortari
- Running mostly on the cartridge MCU
- Programmable by the user in "real time"


**Call to Action - Ideas Wanted!**

This is an early experimental release, and I would love community input:
- Propose a better Basic interpreter (should be C-code based)
- New BASIC commands?
- Graphics mode ideas?
- Preferred syntax for POKE?
- Editor improvements?
- Charset tweaks?
- Suggestions for sound/music integration?
- Or simply: try the ROM and break things!

If anyone wants to fork, optimize, or experiment with the interpreter or display kernel, please do, this project thrives on creative input.
