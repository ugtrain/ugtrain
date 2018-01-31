## Static Memory Cheating

Modifying static memory within a position-dependent executable is very easy.
Between two game runs the variable of interest stays at the same memory
location. E.g. on Linux x86_64 the executable is always loaded to virtual
memory address `0x400000`. Writable data is stored in the ELF sections `.data`
and `.bss`. These have own regions in memory.

### ASLR/PIC/PIE

Only security measures like address space layout randomization with
position-independent code or a position-independent executable (ASLR/PIC/PIE)
make this more complex. Ugtrain supports ASLR with PIE but PIC (static memory
within a library) is not supported, yet. But let us start simple.

### Value Entry in Config

First of all, ugtrain does not provide a config editor. Use your favorite
text editor to write configs for ugtrain.

All you need is the value name, absolute memory address, data type, wish
value, key bindings, the initial activation state, and optional checks.
The checks are used for stability as with a new game executable, the memory
locations of variables in static memory could have changed.

Example:
```
Power 0xc3cd44 i32 l 5000 1,0 a
check this i32 g 10
```

The 32 bit signed integer (`i32`) `Power` value at `0xc3cd44` is set to `5000`
every second if it is less than (`l`) `5000` and greater than (`g`) `10`. This
is activated (`a`) when starting ugtrain and the activation can be toggled with
the keys '1' and '0' on the ugtrain command line. With `d` it would be
deactivated initially and with `w` you could turn this into watching instead
of modifying. Furthermore, the check uses the keyword `this` for the address
to avoid duplication of `0xc3cd44`.

Get familiar with the config syntax by looking at the examples in the
[examples/](../examples) directory and the syntax description in
[examples/README.md](../examples/README.md).

### Running Ugtrain

Ugtrain always attaches to the most recently started instance of the game.
Place the process name of the game like e.g. `warzone2100` in the first line
of the config so that ugtrain can find its process ID (pid) by process name.
It is preferred to let ugtrain run the game with the `-P` option.

Looks like this: `ugtrain -P <config>`.

If the config cannot be found in the current directory, then ugtrain looks
for it in `~/.ugtrain/`. Also have a look at the config entry `game_call` if
the game cannot be started from the command line with the process name.

### Ptrace Limitations

Ugtrain uses the syscall `ptrace()` for attaching to the game process and
modifying its values.

**PLEASE NOTE**: `ptrace()` very likely requires root privileges if not called
from the parent process of the game. The reason for this limitation is the Yama
security module with its `ptrace_scope` parameter set to >= 1.

See: https://www.kernel.org/doc/Documentation/security/Yama.txt

This is why it is preferred to let ugtrain run the game. The same limitation
applies to the Linux memory scanner `scanmem`.


## Static Memory Discovery

The command line tool `scanmem` in version >= 0.16 is recommended for the
memory search. It provides the features described here. The discovery process
is described with the game "Warzone 2100" as an example here.

### Avoid Running Scanmem as Root

Create a config with just the process name of the game:
```
$ echo "warzone2100" > wz2100.conf
```

Add a `game_call` config entry if needed.

Run `ugtrain -P -S <config>` to let ugtrain run the game with scanmem as the
parent of the game.

**Implementation Details**

Running a process group consisting of scanmem and the game is some kind of a
chicken-and-egg problem here as scanmem requires the game pid.

This is how it works:
```
fork() -> [fork() -> exec() game] -> sleep(3) -> exec() scanmem
```

It is possible that 3 seconds are not long enough. So enter the `reset` command
on the scanmem command line as soon as you see the value you want to modify.
Scanmem reloads the known memory regions then.

### Number Search

If the value in the game is displayed the same way it is stored, then it is
easy to find it by entering its value as numbers into scanmem until there is
a single match. The default scan data type is `int`. Try with `float` or
`number` if nothing is found this way. Use the command `help option` for
details. It is also possible to search for a value range like e.g. `1..3` which
searches for everything between 1 and 3 inclusive.
This is especially needed for floats where it is hard to match the exact value.

### Search for Bools and Values of Display Bars

If you are guessing that there is a static bool, then try to search for `0` for
`false` and `1` for `true`. It may take quite some cycles to reduce the number
of matches. C++ and standard C likely use the `uint8` data type for bool but
there is no guarantee for that. If you only see a value bar, then try the commands
`snapshot`, `<`, `>` and `=`. If the bar decreases but you cannot find it with
`<`, then try the inverse logic.

### Warzone 2100 Power Value

Let us do an example with the `Power` value in 64 bit Warzone 2100. In the game
versions 2.3.x .. 3.2.x it can be found as `i32` like it is displayed during
the game.

Our `wz2100.conf` looks like this for now:
```
warzone2100
```

Run the game and scanmem:
```
$ ugtrain -P -S wz2100.conf
```

Start a new single-player alpha campaign and look at the power bar.
Switch to scanmem and enter `reset`:
```
> reset
```

Power bar within the game:

![](../img/wz2100power.png)

Enter the value of the power bar:
```
> 1300
```

Now in this case 784 matches have been found. Build the command center and
enter the new power value:
```
784> 1200
```

A single match? Lucky you! List the match, set it to 5000 and build another
building to verify that you have not just changed a display value.
```
1> list
[ 0] 7f9d7e623d44, 35 +       c3cd44,   exe, 1200, [I32 I16 ]
1> set 0=5000
info: setting *0x7f9d7e623d44 to 0x1388...
```

It is visible that this is part of the executable and this is a PIE here.
So what you put into the config is the match offset `0xc3cd44`. Ugtrain
adds the load address again when running the game with a PIE. Without a
PIE, you would have to put the memory address directly into the config.

After building the research facility the value is at 4900. So you have
found the correct memory location.

With the scanmem `lregions` command you can see in which memory region
you are (region ID `35`). This is the .bss section as it has no file name
(`unassociated`) but it is consecutive to the .data section. The address
to which the dynamic loader has loaded the ELF binary is `0x7f9d7d9e7000`
here. Without a PIE on x86_64 this would always be `0x400000` or on 32 bit
x86 it would always be `0x8048000`.
```
1> lregions
...
[34] 7f9d7df11000,   102400 bytes,   exe, 7f9d7d9e7000, rw-, /usr/games/warzone2100
[35] 7f9d7df2a000, 11665408 bytes,   exe, 7f9d7d9e7000, rw-, unassociated
[36] 7f9d803f9000, 35975168 bytes,  heap, 7f9d803f9000, rw-, [heap]
[37] 7fffc8c00000,   139264 bytes, stack, 7fffc8c00000, rw-, [stack]
```

Congratulations! You have found a static memory value. Close the game and type
`exit` on the scanmem command line.

Start watching the power value. Scanmem showed that `i32` and `i16` are
possible. In general, use the lowest possible data type. But when setting it
to > 65535 (0xffff), you will see that `i32` is possible.

The new config looks like this:
```
warzone2100
Power 0xc3cd44 i32 watch
```

Run it without `-S`:
```
$ ugtrain -P wz2100.conf
```

Now you can see that the power value is 0 in main menu and 1300 upon
campaign start. This confirms that ugtrain is able to keep track of the power
value at every game start even with a PIE.

It is okay to modify it now. Freeze it to at least 5000. It should only be set
to that value if it is lower than that and greater than 0 or a bit greater.
Let us say 10. You want to toggle it with the key '1' or '0' and it should be
activated upon game start.

The new config looks like this:
```
warzone2100
Power 0xc3cd44 i32 l 5000 1,0 a
check 0xc3cd44 i32 g 10
```

This is all you need!
See [examples/README.md](../examples/README.md) for the complete config syntax.

```
$ ugtrain -P wz2100.conf
...
exe_offs: 0x7fcddc0e6000
PIE (position independent executable) detected!
Power at 0x7fcddcd22d44, Data: 0x0 (0)
Power at 0x7fcddcd22d44, Data: 0x514 (1300)
Power at 0x7fcddcd22d44, Data: 0x1388 (5000)
...
```

## Static Memory Adaption

Adapting static memory is quite difficult as the game code accesses it directly
without a library call. A memory breakpoint (a watch point) has to be set with
a debugger like `gdb` which is triggered at each write to the found memory
address. The moment the debugger halts the game process, the related assembly
code should be dumped. The difficulty is then to find a unique similar code
part in another executable of the game. The executable differs with different
compiler options/versions, distribution, or another game version. The accessed
static memory address has to be extracted from the found code part.
This address has to be put into the config afterward. But this is not supported,
yet.

In most cases it is easier to do the discovery with scanmem again.
