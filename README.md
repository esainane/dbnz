DBNZ
===

An emulator for a processor with only one instruction:
*Decrement and branch if non zero*

`dbnz decrement_target, jump_target`

At each step, the machine decrements the the value addressed by the value at its cursor. If the value is zero, the cursor moved to the next line (moving forward two cells). If the value is not zero, the cursor is moved to the address described by the value one step ahead of its cursor.

Gadgets
---

All gadgets are defined by application of DBNZ or reference to previously defined gadgets.

### zero(ptr)
Zeroes out the  value pointed to by `ptr`.

```
dbnz ptr, this - 1
```

### dec(ptr)
Simple decrement and move to next instruction.
```
dbnz ptr, this + 1
```

### inc(ptr, tmp)
Increments ptr by one, and moves to the next instruction.

This works by decrementing tmp MAX times, and decrementing ptr MAX-1 times.

Naturally, when you can only decrement addresses, incrementing takes a very long time. :)

```
zero(tmp)
dec(tmp)
:zerotmp          ; tmp is already decremented once; we decrement ptr MAX-1 times.
dec(ptr)
dbnz tmp, done
:done
```

### jmp(label, tmp)
Unconditionally jumps to the address pointed to by `label`.

```
dbnz tmp, label
dbnz tmp, label
```

A value cannot be zero for two decrements in a row. If the label is required to be dynamic, the second call can simply jump back to the first whenever it falls through:

```
:l
dbnz tmp, label
dbnz tmp, l
```


### halt(status)
Causing the interpreter to exit with the specified status code, by jumping to a specially interpreted unaligned address.

When jumping to an odd (unaligned) address, the interpreter exits with the floored value of half of the address.

```
add(status, status)
inc(status)
jmp(status)
```

### add(ptr, tgt, tmp)
Adds the value at `ptr` to `tgt`.

This introduces the MAX - register pattern, where a register storing value v is zeroed out, and MAX - v is stored in another register.

```
zero(tmp)
:zeroptr          ; zero out ptr, adding MAX - ptr to tmp
dec(tmp)
dbnz ptr, zeroptr
                  ;
:zerotmp          ; zero out tmp, adding MAX - tmp (ie. the original value of ptr) to ptr and tgt
dec(ptr)
dec(tgt)
dbnz tmp, zerotmp
```

### dup(ptr, tgt, tmp)

Duplicates the value at ptr to tgt
```
zero(tgt)
add(ptr, tgt, tmp)
```

### sub(ptr, tgt, tmp)
Subtracts the value at ptr from tgt

```
zero(tmp)
:zeroptr          ; zero out ptr, adding MAX - ptr to tmp
dec(tgt)
dec(tmp)
dbnz ptr, zeroptr
                  ;
:zerotmp          ; Start decrementing the wrapped value at ptr until its original value is restored
dec(ptr)
dbnz tmp, zerotmp
```
