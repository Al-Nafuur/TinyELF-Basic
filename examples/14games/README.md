# Games

Text listings for TinyELF Basic. Copy a file to your SaveKey (for example with SaveKey-Manager) and load it with `ENTER`:

```
NEW
ENTER "E1:Hangman.lst"
RUN
```

File names on TinyELF are case-sensitive, so type them exactly as listed below.

Most of these games come from the classic *BASIC Computer Games* collection (David H. Ahl, Creative Computing) and were written for 64-column terminals. On the 36-column TinyELF screen long lines wrap; picture-style output such as `Diamond.lst` breaks up, while the games themselves play normally.

They need TinyELF Basic v0.16 or newer (`DATA`/`READ`, `MILLIS`, `DELAY`). All of them were tested in Gopher2600: each one loads, starts and runs with input without an error.

## The games

| File | Game |
|------|------|
| `23Match.lst` | 23 Matches – take 1, 2 or 3 matches from a pile of 23 |
| `AceyDucy.lst` | Acey Ducey – bet that a card falls between two others |
| `Amazing.lst` | Amazing – draw a maze of the size you choose |
| `Bagels.lst` | Bagels – guess a 3-digit number |
| `Batnum.lst` | Batnum – take objects from a pile, with your own rules for winning |
| `Battle.lst` | Battle – find and sink the hidden fleet |
| `Bomb.lst` | Bombardment – bomb the enemy's hidden outposts |
| `Bowling.lst` | Bowling – a game of bowling for up to four players |
| `Boxing.lst` | Boxing – a three-round boxing match |
| `Buzzword.lst` | Buzzword – generate buzzword-bingo phrases |
| `Calendar.lst` | Calendar – print a calendar for any year |
| `Change.lst` | Change – work out the change for a purchase |
| `Chemist.lst` | Chemist – dilute the acid in time |
| `Chomp.lst` | Chomp – don't eat the poisoned cookie (two players) |
| `Combat.lst` | Combat – army, navy and air force against the computer |
| `Craps.lst` | Craps – the casino dice game |
| `Cube.lst` | Cube – cross a cube full of hidden mines |
| `Diamond.lst` | Diamond – print a diamond pattern |
| `Dice.lst` | Dice – roll two dice many times and count the results |
| `Gomoko.lst` | Gomoko – get five in a row |
| `Hangman.lst` | Hangman – guess the word letter by letter |
| `Hello.lst` | Hello – a little chat with the computer |
| `HighIQ.lst` | High IQ – the peg solitaire board game |
| `HiLo.lst` | Hi-Lo – guess a number between 1 and 100 |
| `Hockey.lst` | Hockey – an ice hockey match |
| `Letter.lst` | Letter – guess the letter |
| `Life.lst` | Life – Conway's Game of Life |
| `Life4Two.lst` | Life for Two – Game of Life as a two-player game |
| `LitQuiz.lst` | Litquiz – a literature quiz for children |
| `Love.lst` | Love – print a page full of LOVE |
| `MastMind.lst` | Mastermind – crack the colour code |
| `MathDice.lst` | Mathdice – add up the dice as fast as you can |
| `Mugwump.lst` | Mugwump – find four mugwumps on a grid |
| `Nicomach.lst` | Nicomachus – the computer finds your number from its remainders |
| `Nim.lst` | Nim – take objects from several piles |
| `Number.lst` | Number – a simple number-guessing game |
| `OneCheck.lst` | One Check – checkers solitaire |
| `Poetry1.lst` | Poetry – the computer writes poems |
| `Poetry2.lst` | Poetry – a second poem generator |
| `Reverse.lst` | Reverse – sort a list of numbers by reversing parts of it |
| `Rocket.lst` | Rocket – the classic lunar lander |
| `RockScis.lst` | Rock, Scissors, Paper |
| `Russian.lst` | Russian Roulette |
| `Slots.lst` | Slots – a slot machine |
| `Stars.lst` | Stars – find the number using a star bar as a hint |
| `Synonym.lst` | Synonym – a vocabulary trainer |
| `TicTac.lst` | Tic-Tac-Toe – the computer moves first |
| `TicTac2.lst` | Tic-Tac-Toe with a board display |
| `TicTacF.lst` | Tic-Tac-Toe, full version: you or the computer starts |
| `Tower.lst` | Towers of Hanoi |
| `Trap.lst` | Trap – trap a number between two limits |
| `Trek.lst` | Trek – a short Star Trek game |
| `Word.lst` | Word – guess a five-letter word |

`TinyAdv1.lst` – *The Old Castle*, a small text adventure written for TinyELF Basic.

`Dungeon.lst` – *Dungeons of Ekileugor*, a small roguelike: explore the dungeon, fight snakes, collect gold and potions
and find the stairs down. Keys: `I` `J` `K` `L` or the cursor keys move, `R` rests, `Q` drinks a potion, `X` ends the game.
It uses the full 36-column screen and needs TinyELF Basic v0.20 or newer. Tested on real hardware.

## Credits and licence

The games listed in the table are the ports by Stefan Lenz from the examples of his BASIC interpreter
(<https://github.com/slviajero/tinybasic>, folder `examples/14games`), renamed to 8.3 file names for TinyELF.
They are distributed under his BSD 3-Clause licence, see [`LICENSE-tinybasic.txt`](../LICENSE-tinybasic.txt).
The original games were published by Creative Computing.

`Dungeon.lst` is a TinyELF Basic port of *Dungeons of Ekileugor* by Chris Pressey, Cat's Eye Technologies
(<https://git.catseye.tc/Dungeons-of-Ekileugor/>), originally written for the Commodore VIC-20.
It is distributed under the original BSD 3-Clause licence, see [`LICENSE-ekileugor.txt`](../LICENSE-ekileugor.txt).
