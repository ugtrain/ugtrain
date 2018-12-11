## Static Memory Cheating

### Table of Contents

   * [Static Memory Cheating](#static-memory-cheating)
      * [Basics](#basics)
      * [ASLR/PIC/PIE](#aslrpicpie)
      * [Value Entry in Config](#value-entry-in-config)
      * [Running Ugtrain](#running-ugtrain)
      * [Ptrace Limitations](#ptrace-limitations)
   * [Static Memory Discovery](#static-memory-discovery)
      * [Avoid Running Scanmem as Root](#avoid-running-scanmem-as-root)
      * [Number Search](#number-search)
      * [Search for Bools and Values of Display Bars](#search-for-bools-and-values-of-display-bars)
      * [Warzone 2100 Power Value](#warzone-2100-power-value)
      * [Matches in other Regions](#matches-in-other-regions)
      * [No Matches](#no-matches)
   * [Static Memory Adaptation](#static-memory-adaptation)
   * [Special Cases](#special-cases)
      * [Stack Values](#stack-values)
      * [Library Values](#library-values)
      * [Pointer Following](#pointer-following)

### Basics

Modifying static memory within a position-dependent executable is very easy.
Between two game runs the variable of interest stays at the same memory
location. E.g. on Linux x86_64 the executable is always loaded to virtual
memory address `0x400000`. Writable data is stored in the ELF sections `.data`
and `.bss`. These have own regions in memory.

### ASLR/PIC/PIE

Only security measures like address space layout randomization with
position-independent code or a position-independent executable (ASLR/PIC/PIE)
make this more complex. Ugtrain supports ASLR with PIE and PIC (static memory
within a library).

Although code load addresses differ, there is a static offset of `0x200000`
between the code and data start on Linux x86_64. So the assembly code usually
accesses static memory relative to the instruction pointer.
But let us start simple.

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
the keys `1` and `0` on the ugtrain command line. With `d` it would be
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


-----

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
Let us say 10. You want to toggle it with the key `1` or `0` and it should be
activated upon game start.

The new config looks like this:
```
warzone2100
Power 0xc3cd44 i32 l 5000 1,0 a
check this i32 g 10
```

This is all you need!
See [examples/README.md](../examples/README.md) for the complete config syntax.

```
$ ugtrain -P wz2100.conf
...
exe_offs: 0x7fcddc0e6000
PIE (position-independent executable) detected!
Power at 0x7fcddcd22d44, Data: 0x0 (0)
Power at 0x7fcddcd22d44, Data: 0x514 (1300)
Power at 0x7fcddcd22d44, Data: 0x1388 (5000)
...
```

### Matches in other Regions

**heap**

See: [Dynamic Memory Cheating Documentation](./ugtrain-dynmem.md)

An unpreferred alternative is [Pointer Following](#pointer-following).

**stack**

See: [Stack Values](#stack-values)

**code**

See: [Library Values](#library-values)

**misc**

If you find matches in a misc region, then this means that it has been created
by `mmap()` directly. This is often the case if a runtime engine like those
for Java or JavaScript is used. Runtime engine specific hacks are required
here. The memory allocation functions are located inside the runtime engine.
So the engine has to be analyzed or reverse-engineered.

There is no support in ugtrain for this so far.

### No Matches

Are you trying to cheat at **Steam/multi-player/online/streamed games**?

Then the answer is that the values you are interested in are very likely stored
on the game server. Look at the source code or you could use reverse engineering
methods (`gdb`, `ltrace`, `strace`, `tcpdump`, `wireshark`, etc) to find out.
Only input and graphics cheating remains.

**Cheating at such games is strongly discouraged.**

The game works **offline**?

FOSS games do not attempt to hide memory values as the source code is open
anyway. Commercial games in contrast might use addition or multiplication of a
random value. Even more effective are reversing endianness, XOR, shifting, or
other bitwise operations. Sometimes only the displayed value can be found in
memory. A `gdb` watchpoint can be set to it and it can be tried to find its
calculation from the stored value. If also the displayed value cannot be found,
then full reverse engineering is required.

Sometimes FOSS games like e.g. `endless-sky` use unusual double precision
floats. If a dynamic memory object has a user-defined name like the ships in
this game, then the string search might be able to find the address of that
string and you can search for a pointer pointing to it afterward.


-----

## Static Memory Adaptation

Adapting static memory is quite difficult as the game code accesses it directly
without a library call. A memory breakpoint (a watchpoint) has to be set with
a debugger like `gdb` which is triggered at each write to the found memory
address. The moment the debugger halts the game process, the related assembly
code should be dumped. The difficulty is then to find a unique similar code
part in another executable of the game. The executable differs with different
compiler options/versions, distribution, or another game version. The accessed
static memory address has to be extracted from the found code part.
This address has to be put into the config afterward. But this is not supported,
yet.

In most cases it is easier to do the discovery with scanmem again.


-----

## Special Cases

### Stack Values

Stack values use a mix between static and dynamic memory cheating. It is
required to get the stack end (`__libc_stack_end`) from within the game
process as its offsets to the stack region limits are randomized. Ugtrain gets
the stack end with the dynamic memory cheating and discovery methods. During
discovery, the stack address from scanmem is matched against the stack region
and subtracted from the recorded stack end. So we get a forward stack offset
for the config.

See also: [Dynamic Memory Cheating: Stack Values](ugtrain-dynmem.md#stack-values)

An example is the `Credits` value in the game `endless-sky`:
```
Credits stack 0x4a0 i64 l 500000000 A,X a
check this i64 g 1000
```

The keyword `stack` indicates that this is a stack value. During stack value
cheating, ugtrain subtracts the configured forward stack offset from the stack
end and gets the current stack address of that value. Then it has to wait for
one second so that the game has a chance to fill the stack first. And afterward
a stack value behaves like all other static memory values. But be careful not
to corrupt the stack and not to crash the game this way. Start with watching
such values first.

### Library Values

Static memory values within a library (PIC) are very similar to PIE values.
But to find them in memory, the scanmem commands `option region_scan_level 3`
and `reset` have to be entered first. Scanmem ignores the library memory regions
by default as interesting static memory values within them are really rare.

There are two methods how libraries are loaded: The first one is to link against
the library and the dynamic loader loads it early into game memory (early PIC
handling). The second one is to load it later on with `dlopen()` (late PIC
handling). Ugtrain keeps reading the memory regions in every cycle until all
libraries are loaded. It is assumed that those do not get reloaded during
runtime for now.

A real-world example could not be found. So let us assume this:
```
Money lib libmygame.so 0x201020 i64 l 999999 A,X a
check this i64 g 100
```

The keyword `lib` indicates that this is a library value and that the library
name will follow. The library name should be unique so that it can be easily
found as a sub-string in the file name of its memory regions. The rest is
identical to static memory values within the PIE executable.

### Pointer Following

Pointer following from static memory to a single dynamic memory object is
possible with ugtrain but unpreferred. It is the method of `Cheat Engine` on
Windows but on Linux there is a huge variety of game binaries. That makes
adapting configs much more important. A pointer in static memory is plain
too hard to discover and to adapt. But this is how it works:

A `gdb` watchpoint is required to find the code modifying the found object
member. By disassembly the offset within the object and with that, the object
address is discovered. Look for the object address as `int64` in the x86_64
`exe` regions and with a bit of luck there is a single match for this. But the
size of the memory object remains unknown. You could guess and dump (key `>`)
as well as compare it in two states to find further values within the object.

Ugtrain has an example for `chromium-bsu` for this to be able to compare the
method with the preferred dynamic memory cheating method by API hooking. The
object size is known from the preferred method.

**Pointer following**: [chromium-bsu64_pointer.conf](../examples/incomplete/chromium-bsu64_pointer.conf)
```
ptrmemstart HeroAircraft 288
Lives 0xbc i32 l 9 1,0 a
ptrmemend

HeroAircraftPtr 0x653260 p HeroAircraft always
check this p e heap
```

The `HeroAircraftPtr` in static memory points to the `HeroAircraft` object
which is `288` bytes in size. Ugtrain follows the pointer with type `p` in
every cycle (`always`). If a constant value would be needed from it, then
following it `once` would be enough. A `heap` check is required to make this
safe. Ugtrain reads the game memory regions every cycle to get the current
heap limits for this. The `Lives` are stored at offset `0xbc` within the
object. Details are described in the example.

**API hooking**: [chromium-bsu64.conf](../examples/chromium-bsu64.conf)

Compare this to the dynamic memory cheating example.
