DBNZ Simulator
===

This is a library for DBNZ, forming the simulation core of the project.

The runtime is extremely simple, only needing to handle the dbnz instruction.

DBNZ
---
At each step:
 - If the cursor is out of bounds, halt and catch fire.
 - If the address designated by the cell under the cursor is out of bounds, halt and catch fire.
 - Decrement the number at the address designated by the cell under the cursor.
  - If the result is non zero, jump to the address designated by the cell after the cursor.
  - Otherwise, move the cursor forward two cells.
 - If the cursor is at an unaligned position, exit with the return code set to how many unaligned positions precede this one, ie. half the address, rounded down.

State file format
---
You probably want to be generating these with the [compiler, via the helper language](https://github.com/esainane/dbnz/tree/master/asm).

If you want to hand optimize your program, the input format is a list of numbers.
 - The first number indicates the amount of memory the program requires.
 - The second number indicates the cell index the cursor should start at.
 - The rest of the file is a list of numbers representing the state of the machine, starting from cell index zero. There must be at least cell one per line and at two most cells per line. Anything not a number, or after something that isn't a number, is ignored. If the state file has fewer numbers than cells in program memory, the remainder of the cells are initialized to zero.
