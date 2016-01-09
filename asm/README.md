DBNZ Assembler
===

This is a compiler for DBNZ, providing a relatively high level assembly helper for the project.

.dbnz files permit helper macros to be defined, allowing for rapid abstraction into rather interesting programs.


Machine format
---

The generated set of states is divided into four parts. In order:
 * Constant pool (Fixed size)
 * Program (Fixed size)
 * Heap (Growing upwards from the end of the program)
 * Stack (Fixed bound; growing downwards from the end of the state space)

No protections are made to any part of the program at any point, excepting that jumps must be to an aligned (even) slot address.
Macros are free to modify parts of the stack above them, below them, mess with the executable program, and mangle the constant pool.
There are no safety belts here, and you're welcome take advantage of possible Fun optimizations from this.

Language
---

### Overall grammar
```
program ::= macrolist statementlist
macrolist ::= macro [macrolist]
macro ::= "def" IDENTIFIER "(" paramlist ")" statementlist BLANKLINE
paramlist ::= IDENTIFIER [paramtail]
paramtail ::= "," IDENTIFIER [paratail]
statementlist ::= call | dbnz
call ::= IDENTIFIER "(" rvallist ")"
rvallist ::= rval [rvaltail]
rvaltail ::= "," rval [rvaltail]
dbnz ::= "dbnz" rval, rval
rval ::= CONSTANT | STACKVAL | CELLADDR | rval "+" rval | rval "-" rval

CONSTANT ::= "&" NUMBER
STACKVAL ::= "@" NONZERONUMBER
CELLADDR ::= NUMBER

IDENTIFIER ::= [a-z]+[a-z0-9]*
NUMBER ::= 0 | NONZERONUMBER
NONZERONUMBER ::= [1-9][0-9]*
```

Macros may call any macro non-recursively, including macros that are defined later in the program.

Single-line comments beginning with `;` or `//` are permitted. Multi-line comments between `/*` and `*/` are permitted.

Completely blank lines are only permitted at the beginning of a program, at the end of a program, and after macros. A macro definition is terminated by such a blank line.

By convention, visual space within a statement list should be provided by a single ; that is also aligned with other ; to the right of the statement list.

### Keywords
A few special keywords are also provided for convenience:
 * this - Refers to the current cell. If used in a macro, refers to the cell that immediately follows.
 * data - Resolves to the cell past the end of the compiled program.

### Labels
Labels are also provided. An identifier preceded by a colon appearing on its own line will not emit any instructions.
Any reference to the label identifier, before or after, will be replaced with the index of the cell that follows it.

A label within a macro is considered a *local* label. *local* labels can only be referenced from with the macro they are in. A local label can be passed as a parameter to macros calls, which can then refer to the address of this label with their identifier.

### Expressions
Basic "pointer arithmetic" expressions, resolvable at compile time, are permitted as values for the dbnz instruction. For instance, the following are valid:
 * `dbmz data + 2, label`
 * `dbmz data - this, this + 2`

Subtracting "this" or "data" is permitted. Doing so may lead to some rather interesting behavior. :>

### Constants
Specifying &value, such as &42, will create an entry in the "constant pool" which contains the literal value 42, and &value will resolve to the cell in the constant pool where 42 is stored. Multiple references to the same constant will always resolve to the same constant cell address.

By convention, it is assumed that this will only ever be used by macros such as the provided dup() that take care to restore their original value. However, there are no safety belts here if you want to try something Fun. :>

### Heap
The heap is an informal name for the state space between the end of the compiled program and the stack. A helper constant keyword "data" refers to the first cell past the end of the compiled program, ie. the start of the heap. No assertions or assumptions are made about usage or management.

You could add a convention that has "allocations" increment a counter on the heap, and compact (shifting downwards) the heap and reducing this counter when memory is freed. If you also keep track of the stack length, you could trigger some handler when you would want to grow the heap into used stack space. This would probably only be a panic halt, unless you feel like implementing your own oomkiller. :>

### Stack
Specifying @value, such as @42, resolves to a temporary stack cell. Stack slots are allocated from @1 onwards - @0 will produce a parse error.

Stacks cells should generally be considered uninitialized. It is possible to add a own convention that has your functions zero parameters when they're done, if you would like to assume stack cells come zeroed.

No protections are made for stopping the stack intruding on the heap or vice versa. You could add a convention that has functions decrement a stack length counter on the heap and increment it when they finish.

Each macro is provided its own, local, stack segment, which is has the length of its largest stack parameter. A macro can call a macro with a stack parameter, which can then be used as a normal identifier.

Compilation
---

Currently, definitions are simply macros, which means all operations are always inlined.
In particular, this means recursive macro calls are not allowed!
Putting definitions in a common area and allowing callers to reuse code would be an interesting experiment for the future, but copying and restoring registers/cells is quite expensive here. :)
