## Dynamic Memory Cheating

You should have read the
[Static Memory Cheating Documentation](ugtrain-statmem.md) first.

### Table of Contents

   * [Dynamic Memory Cheating](#dynamic-memory-cheating)
      * [Basics](#basics)
      * [Preloading Details](#preloading-details)
      * [Ugtrain Details](#ugtrain-details)
      * [Hardening](#hardening)
   * [Dynamic Memory Discovery](#dynamic-memory-discovery)
      * [Discovery Basics](#discovery-basics)
      * [Discovery Example](#discovery-example)
      * [Discovery Details](#discovery-details)
      * [Stack Values](#stack-values)
      * [Finding more Values in Object](#finding-more-values-in-object)
      * [Multiple Objects](#multiple-objects)
   * [Dynamic Memory Adaptation](#dynamic-memory-adaptation)
   * [Known Issues](#known-issues)

### Basics

Dynamic memory cheating with ugtrain usually requires a preloader which loads a
memory hacking library into the game process. Ugtrain has a preloader feature
(`-P`) and `libmemhack` for that. The library gets the code addresses of unique
`malloc()` calls and their reverse stack offsets as a config through a
FIFO.<br/>
The lib reports back through another FIFO which objects have been allocated to
which memory addresses. It also keeps track of these memory addresses and
sends the information about the freeing of these memory objects through the
output FIFO. This is required for stability.<br/>
You can call this "dynamic memory spying" if you will. This is completely
different from the default method of Cheat Engine. They follow access pointers
to one in static memory. But static memory is hard to adapt and that is only
reliable for a single memory object of the same class.

### Preloading Details

The preloader feature (`-P`) only appends libmemhack to the environment variable
`LD_PRELOAD` and runs the game. The dynamic loader `ld.so` does the rest. You
can choose further preloaders if you want. It is only important that this
preloader also appends to LD\_PRELOAD. The ugtrain sets an environment
variable with the game process name. This way libmemhack knows when to
activate itself (not in another preloader, not in a shell, etc.). There is no
need for root privileges and this also works for pure static memory.<br/>
See the `--pre-cmd` option of ugtrain.

The lib has a constructor function as well as `malloc()` and `free()` hooks.
It wraps these functions. In the constructor, the lib opens the input and the
output FIFO and tries to read the config for 1 second. If there is no config
or there is an error in the config, then it does not get active. This means
that it operates like libc malloc() and free(). But activated, it compares for
each malloc first the malloc size and then the code address at the reverse stack
offset with the config. If it matches, it sends the code and the memory address
(returned by libc malloc) through the FIFO. The memory address is put into the
in-memory config to be used for the free() hook. Only memory addresses are
compared there in order to find a free() which is relevant to be reported.

The in-memory config is stored as an array of config entries on the heap. An
array of memory addresses of the currently allocated objects is part of a
config entry. It grows dynamically with a size-doubling allocator with a mutex
held. The memory addresses are filled with an invalid pointer value and the last
value is always that invalid pointer. This way free() only iterates until it
hits an invalid pointer. Upon a free(), the found memory address is set to NULL
and malloc() can re-use it. This way the first invalid pointer represents the
number of objects which had to be kept track of at once. The more objects there
are, the slower it gets.

### Ugtrain Details

FIFOs: `/tmp/memhack_in`, `/tmp/memhack_out`<br/>
Required config options: `dynmemstart`, `dynmemend`, >= 1 value inside<br/>
Special config option: `checko`

Value locations are used as offsets within the objects. They become only active
if at least one object has been allocated. The library sends the code and the
memory address through the FIFO to ugtrain. Ugtrain appends this to a vector of
memory addresses in the related memory class then. It also grows the old value
vectors per configured value in that class. This way ugtrain always knows which
value was in memory before changing it and can show it to the user.<br/>
Displaying all values at once would be too much. This is why ugtrain uses a
**print index**. With the keys `+` and `-`, the print index for all memory
classes is increased or decreased. In case you need a snapshot/dump of all
objects, use the `>` key.

You can use a special check (`checko`) if the allocated object belongs to you or
to an opponent otherwise. If it fails, it removes the object from the active
ugtrain in-memory config by setting its memory address to NULL. Following
values in that object are not processed anymore. After leaving the critical
section, the object is removed from the vectors and freed. Afterward, the user
is informed. This behaves very similar to having received a free() from
libmemhack.

But keep in mind: Libmemhack does not know about that. It still tracks
kicked-out objects and will send a free() for these.

The rest works the same way as with static memory.

### Hardening

With a **PIE** (position independent executable) and **ASLR** (address space
layout randomization), the code addresses on the stack are randomized. Ugtrain
determines the load address of the executable from `/proc/$pid/maps` and
sends it to libmemhack. Libmemhack adds it to the configured code addresses
afterward. E.g. Ubuntu uses PIE on everything since version 13.04.

Another trick is to put the allocation into a library with **PIC** (position
independent code). Also here the load address of the configured library is
determined and added to configured code addresses. For late library load with
`dlopen()`, libmemhack hooks that function as well to inform ugtrain about the
library load. An example for this is the battle tanks game `btanks` with its
`libbt_objects.so`.


-----

## Dynamic Memory Discovery

### Discovery Basics

For discovery, another library (`libmemdisc`) is required. This one is
preloaded as well. The ugtrain option `-D` expands automatically to
`-D -P libmemdisc64.so`. It is recommended to combine with `scanmem` as options
`-S -D` or `-SD` to avoid the need for root privileges for scanmem.

This records all malloc()s with **object address** and **size** to the file
`/tmp/memdisc_file` by default. This file can be huge. So libmemdisc uses
an additional FIFO `/tmp/memdisc_out` with caching and ugtrain uses a
separate thread to write the data from the FIFO to the file. It is important
to **terminate the game regularly** while the interesting memory object is still
allocated (`Alt+F4` usually). This flushes the cache to the FIFO. **Do not
return to a main menu** as new objects could be allocated to the previous
location of the object you are interested in.

Only then, it is guaranteed that all malloc()s have been recorded and that
the allocation for the interesting memory object can be found easily.

```
[ugt] Memory address (e.g. 0xdeadbeef):
```
Now ugtrain will ask you for the heap memory address of the variable you are
interested in found by scanmem. This can be e.g. the address of the `Lives`
value in the `HeroAircraft` object of the game `Chromium B.S.U.`. Ugtrain parses
the memory discovery file for matching allocations reverse and (if done
properly) the first line will show:
* the memory address of the object,
* the size of the object, and
* the offset of the value within the object

In the next step, use the found memory object size as a filter to do the
backtracing in order to find the allocator for this object in the code. The size
must be **at least 8 bytes**. Let us assume that the size 288 bytes has been
found. Then repeat the discovery with the options `-SD 288`.

**Note:** Not all allocations can be backtraced as backtracing costs a lot of
time and the game might notice being too slow and terminate itself.

An example allocation in the memory discovery file is:
```
m0x556b1a2289d0;s288
```

`m` means `malloc() return address` and `s` means `size of the object` here.

### Discovery Example

Let us discover the `HeroAircraft` object with the `Lives` variable of the
game `Chromium B.S.U.` 64 bit PIE here.

**Preparation**

Write the process name of the game into a config file:
```
$ echo "chromium-bsu" > chromium-bsu.conf
```

Adding a `game_call` or `game_path` config entry is not needed here as the
shell command `chromium-bsu` also starts the process `chromium-bsu`.

**Discovery of the Object Size**

Run the game with discovery and scanmem. Find the 32 bit integer lives value
(search for `4`, then `3`, then `2`) first. Pause the game (`p` key), do the
search, resume the game (`p` key), lose a live (double right-click for
self-destruction), and repeat for this. Afterward, test if setting the value
to `9` works.<br/>
Then terminate the game with `Alt+F4` and terminate scanmem with its `exit`
command. Pass the found heap address of the lives to ugtrain and let it do the
matching. Copy&Paste is recommended to transfer the memory address from scanmem
to ugtrain.

```
$ ugtrain -SD chromium-bsu.conf
...
1> list
[ 0] 556b1a228a8c,  2 +       3bba8c,  heap, 3, [I64 I32 I16 I8 ]
1> set 9
info: setting *0x556b1a228a8c to 0x9...
1> exit
[ugt] Memory address (e.g. 0xdeadbeef): 0x556b1a228a8c
[ugt] Searching reverse for 0x556b1a228a8c in discovery output..
m0x556b1a2289d0;s288 contains 0x556b1a228a8c, offs: 0xbc
m0x556b1a2286e0;s1036 contains 0x556b1a228a8c, offs: 0x3ac
m0x556b1a224890;s17024 contains 0x556b1a228a8c, offs: 0x41fc
```

Only the first matching allocation line is interesting if the discovery has been
done properly. This shows you that the malloc size is `288` bytes and the offset
within the object is `0xbc`.

**Discovery of the Allocator**

Now provide the discovered size `288` as a parameter of the `-D` option to
enable the backtracing. A guess unwinder is used within ugtrain. Libmemdisc
does not know about valid code addresses here and just records the first 11
values from the stack of the current memory allocation.

In the output, the `c` stands for `code address` and the `o` for the `reverse
stack offset` where the code address was located.

```
$ ugtrain -SD "288" chromium-bsu.conf
...
[ugt] Memory address (e.g. 0xdeadbeef): 0x5589d5317cbc
[ugt] Searching reverse for 0x5589d5317cbc in discovery output..
m0x5589d5317c00;s288 contains 0x5589d5317cbc, offs: 0xbc
  c0x10ab7;o0x8 chromium-bsu <_Znwm@plt>
  c0x195ec;o0x28 chromium-bsu
  c0x25770;o0x38 chromium-bsu
------------------------------
dynmemstart ClassName 288 0x10ab7 0x8
ValueName 0xbc i32 watch
dynmemend
------------------------------
```

The names of libraries and the executable are only shown if their code addresses
are found on the stack. Their load address is subtracted automatically from
their found code addresses then.<br/>
Only `<*@plt>` direct library calls are displayed and `_Znwm()` on the first
position is exactly what we are looking for. It is a GNU C++ internal function
(part of the `new` operator) which calls `malloc()` internally. On 32 bit
systems it is called `_Znwj()`.<br/>
You get to know the jump-back code address `0x10ab7` and the reverse stack
offset `0x8` here. The stack offset is low and always the same as _Znwm()
and _Znwj() are hooked additionally to malloc(). The allocation is done
from within the executable here.

That is it! You can write a basic config for the lives in `chromium-bsu`.
The discovery even already shows a configuration template for this. The real
names of the class or the value do not really matter. Use your imagination.
Start with a watcher in order to verify that your findings are correct. It
is often better to use the smallest possible data type. Later it can be
possibly discovered that a larger type is used. E.g. here when letting
too many enemies through, the lives are set to `-1` (`0xffffffff`). When hex
dumping this (pressing the `>` key), then you would see that there is
actually a 32 bit signed integer here.

The new config is:
```
chromium-bsu
dynmemstart HeroAircraft 288 0x10ab7 0x8
Lives 0xbc i32 watch
dynmemend
```

See [examples/README.md](../examples/README.md) for the complete config syntax.

**Running the new Config**

```
$ ugtrain -P chromium-bsu.conf
```

Now run your discovered config with memory hacking and watch how the lives
value changes. When being sure about it, you can add the checks and the
wish value, assign one or more comma-separated activation keys, and set
the initial activation state. The lives are frozen to the wish value then.
```
...
Lives 0xbc i32 l 9 1,0 a
check this i32 g 0
...
```

Here you would want the lives to be updated to `9` only if they are less than
`9` and also greater than `0`. Otherwise, unexpected behavior could occur. This
also ensures being on the correct memory address in case of adaptation.<br/>
With the keys `1` and `0` you can toggle the activation of this value. Ugtrain
uses non-blocking `getchar()` just like the Linux command `top`. In the
last column the `a` means that this is activated at trainer start.

```
$ ugtrain -P chromium-bsu.conf
...
[ugt] Starting game with libmemhack64.so preloaded..
...
[memhack] config[0]: mem_size: 288; code_addr: 0x10ab7; stack_offs: 0x8;
[ugt] exe_offs: 0x558fbcaf5000
[ugt] PIE (position independent executable) detected!
[memhack] Config after early PIC/PIE handling:
[memhack] config[0]: mem_size: 288; code_addr: 0x558fbcb05ab7; stack_offs: 0x8;
[ugt] stack_end: 0x7fffdb83c770
[ugt] ===> Obj. HeroAircraft created 1 time(s); now: 1
[ugt] *HeroAircraft[0] = 0x558fbdafe440, 1 obj.
[ugt] Lives at 0x558fbdafe4fc, Data: 0x4 (4)
[ugt] *HeroAircraft[0] = 0x558fbdafe440, 1 obj.
[ugt] Lives at 0x558fbdafe4fc, Data: 0x9 (9)
...
[ugt] ===> Obj. HeroAircraft freed 1 time(s); remaining: 0
```

The lives are frozen to 9. So freezing your first value in dynamic memory works.
This is with PIE as the load address of the executable has to be added back
to the configured code address. Without PIE, the load address of the executable
would be always `0x400000` on x86\_64 and the code address would be
configured/used as is.


### Discovery Details

Actually, libmemdisc takes a stage number and stage parameters separated by
semicolons as a config. The original idea was that four stages are required:

1. Find the malloc size           (all mallocs and frees are recorded)
2. Verify the malloc size         (frees are ignored)
3. Find the code address
4. Find the reverse stack offset

But in praxis the discovery defaults to stage 2 and then stage 4 with the malloc
size given. Problematic is only the unlikely case of small allocations of less
than 8 bytes. E.g. for backtracing a 32 bit integer allocation, the ugtrain
option `-D "4;4"` is required.

The four stages are still useful for testing, debugging, and research.

Possible stage parameters for the `-D` option:

1. `StageNr`
2. `StageNr[;MemSize]`
3. `StageNr[;gbt][;CodeFilter];MemSize[;CodeAddr]`
4. `StageNr[;CodeFilter];MemSize[;CodeAddr]`

Examples:

1. `"1"`
2. `"2"`, `"2;0"`, `"2;288"`
3. `"3;288"`, `"3;exe;288"`, `"3;gbt;exe;288"`
4. `"4;288"`, `"4;exe;288"`, `"4;exe;288;0x40f4e7"`

Size 0 means that all sizes are recorded in stage 2. The ugtrain expands the
first example string to the second in stage 2. In stages 3 and 4 ugtrain finds
out the code start and end for you for the code filter you have specified. It
reads from `/proc/$pid/maps` to find them. Use `exe` to filter the backtrace
output to the executable. A library name like e.g. `libbt_objects.so` filters
the backtrace to a library. Without a code filter set, ugtrain shows all code
addresses it finds in the backtrace. Only the malloc size is important in
these stages. Using GNU `backtrace()` with the `gbt` option in stage 3 is
unpreferred, rather unstable, and should be reserved to testing only.

Returns:

1. `m0x87acd0;s288`, `f0x87acd0`<br/>
--> `m` for "malloc" and `f` for "free", the address is the memory address of
the object, `s` is the size in bytes
2. `m0x87acd0;s288`
3. `m0x87acd0;s288;c0x40f4e7;c0x417801`<br/>
--> `c` is the code address in the backtrace
4. `m0x87acd0;s288;c0x40f4e7;o0x8;c0x417801;o0x28`<br/>
--> `o` is the reverse stack offset of the code address

The symbol lookup during stages 3 and 4 is based on `objdump -D`. The memory
regions are read once during game start and after `dlopen()` calls for late
PIC handling.


### Stack Values

Values on the stack like e.g. the `Credits` in the game `endless-sky` behave
like static memory after initialization. Libmemdisc sends the stack end
(`__libc_stack_end`) through the FIFO first and shows it as well. So the first
entry in the discovery file is something like:

* `S0x7ffc83da2420`

The `S` stands for stack end. Together with the stack region start determined
by ugtrain, the given memory address is matched against the stack region during
discovery. To avoid that too many mallocs are recorded as well, use an unlikely
object size. The stack region is matched first. So if there is a match, then
discovery can be interrupted.

Example:
```
$ ugtrain -SD 999 endless-sky64.conf
...
> 892090882
...
38/38 searching 0x7ffc83d83000 - 0x7ffc83da4000..........ok
...
1> list
[ 0] 7ffc83da1f80, 37 +        1ef80, stack, 892090882, [I64 I32 ]
1> exit
[ugt] Memory address (e.g. 0xdeadbeef): 7ffc83da1f80
[ugt] Searching reverse for 0x7ffc83da1f80 in discovery output..
stack contains 0x7ffc83da1f80, offs: 0x4a0
```

`0x7ffc83da1f80` is located between stack start `0x7ffc83d83000` and stack end
`0x7ffc83da2420`. Stack end - stack address = forward stack offset = `0x4a0`.
Ugtrain does this calculation for convenience. So `0x4a0` can be put into the
config. Then libmemhack will provide the stack end and ugtrain will subtract
the forward stack offset from the current stack end again to get the current
address of the configured stack value.

See also: [Static Memory Cheating: Stack Values](ugtrain-statmem.md#stack-values)


### Finding more Values in Object

When running ugtrain with memory hacking, you can use the object dump now by
pressing the `>` key. For the example there is the HeroAircraft object dumped
as `0_000.dump`. In front of the `_` there is the class index and behind it the
object index. The next dump renames the existing dump file to `0_000~.dump` and
overwrites in case such file already exists. This way always upto two states for
a memory object are stored.

Do the first dump when you are low on everything (ammo, shields, etc.) and the
second one when you are high on everything. Then just compare both dumps with a
binary diff tool like e.g. `vbindiff`. This way you can easily find other values
like ammo and shields. Zero is also with a float zero in hex.
```
$ vbindiff 0_000~.dump 0_000.dump
```

If a text diff tool like e.g. `meld` fits you better, then you will need to
convert the files into hex text first. This is done e.g. with the tool `xxd`
which is part of the `vim` package.
```
$ xxd 0_000~.dump > 0_000~.dump.txt
$ xxd 0_000.dump > 0_000.dump.txt
$ meld 0_000~.dump.txt 0_000.dump.txt
```

Here in the `doc` directory you can find these files for the HeroAircraft
example. In the first state (`0_000~.dump`) there are no shields, no extra
weapons and the aircraft is very close to being destroyed. In the other state
there are extra shields, no damage and all extra weapons with full ammo. Compare
the files. Search mainly for transitions from zero to a quite high number and
patterns of three times the same value (for weapons or ammo).

**Remember:** Everything is stored little-endian and very likely memory aligned.

Have you found the other values?

At offset `0x58` it looks like having a bool array related to weapons. Bools are
often stored as `u8`, esp. in standard C99/C++. There is three times transition
from false (`0x0`) to true (`0x1`) and the following byte seems to be unused for
padding/alignment and remains `0x0`.

Look for low precision values with some zeros still in them - either integers
or floats. In front of the lives only 0xb8 looks interesting like a negative
float but -16.875 does not sound reasonable. Instead the location `0xd0` looks
very good: There is three times `0x43160000` (150.0). Yes, looks like having
found the ammo array as floats! Hmmm, another 3 values float array follows
with transition from -1.0 to -0.02. That is the gun pause and not interesting
as it is not possible to make the guns shoot faster.

Proceed to `0xec`. You see a float transition from -21.6 to -500.0. Looks like
they store the armor points as a negative damage value. You should be very close
to the shields now as this would be typical for SW developers. Indeed, at `0xf0`
there is a transition to 992.3. It is very difficult to retain full shields for
very long time but from here you can guess that 1000.0 is the maximum. You can
also work with the dynamic wish value `max` in the config to verify.

The next value at `0xf4` seems to be a float as well. It is a transition from
5150.0 to 7250.0. This must be the score. It took a little longer to acquire the
weapons and shields - so there is higher score. Strange that they store it as
float! Modifying it does not make much sense, so ignore or just watch it.

Set up all values where you are guessing interesting values with watching first
to verify that they are the values you are looking for.


### Multiple Objects

If you have got a lot of objects and do not know exactly which ones are yours
and which ones are enemies, use the binary diff tool `ugsimfind`. It compares
upto 100 object files byte by byte and tells you for each byte its value as well
as how many and which files have the same value. This is very very useful if you
are searching the `u8` player ID in Warzone 2100 Droids. Use it as follows:

```
$ ugsimfind 0_*.dump | less
...

fidx: 0x22
0x00 '.' : 7    0_000.dump, 0_001.dump, 0_042.dump, 0_043.dump, 0_044.dump,
0_045.dump, 0_046.dump,
0x07 '.' : 26   0_002.dump, 0_003.dump, 0_004.dump, 0_005.dump, 0_006.dump,
0_007.dump, 0_008.dump, ...,
0x06 '.' : 14   0_010.dump, 0_011.dump, 0_012.dump, 0_019.dump, 0_020.dump,
0_021.dump, 0_022.dump, ...,

...
```

The campaign starts with 7 own Droids and 40 enemies in Warzone 2100. From the
type string inside, you can see that these files are exactly these 7 own Droids.
You have player ID `0` and the AI has ID `6` and ID `7`. So your player ID is
located at offset `0x22`.


-----

## Dynamic Memory Adaptation

Example configs will ask you if you want to do adaptation when using them for the
first time. You should accept. This looks e.g. like this:
```
$ ugtrain -P examples/chromium-bsu64
...
[ugt] Adaptation to your compiler/game version is required.
[ugt] Adaptation script: examples/adapt/chromium-bsu_adapt64.sh
[ugt] Run the adaptation script, now (y/n)? : y
```

If you are not sure here, just take a look at the adaptation shell script. It
searches for a unique call to `_Znwm()`, `_Znwj()`, or `malloc()` in the
disassembly (`objdump -D`) of the game binary. If it fails, it does not return
anything. Then it runs in debug mode automatically. But you can also run it
manually with the game binary path and `DEBUG` as parameters:
```
$ examples/adapt/chromium-bsu_adapt64.sh `which chromium-bsu` DEBUG
```

Please open a GitHub issue with that output to get help how to fix the
adaptation. If this fails, then you have to do full discovery.

In this case adaptation succeeds:
```
...
[ugt] Adaptation return:
1;HeroAircraft;0x120;0x10ab7
[ugt] Class HeroAircraft, old_size: 288, new_size: 288
[ugt] Class HeroAircraft, old_code: 0x14b57, new_code: 0x10ab7
[ugt] Writing back config..
```

The object size and the code address can be adapted. Here only the code address
changed. The reverse stack offset is not adapted for simplicity and as it is
usually always the same. If it should really change, then doing a new discovery
is the best choice.

If the malloc size changed as well, then there is likely another config for
that specific game version - esp. if the offsets of the object members changed.
In examples it is noted for which game versions they work adapted.

After writing back the config, the config command `adapt_required` is set to
`0` so that you do not get bothered with the adaptation wish again. After
successful adaptation the game is also started automatically with libmemhack
preloaded and you can see if the new config works.

You can also trigger this procedure explicitly with the `-A` option. E.g.:
```
$ ugtrain -A examples/chromium-bsu64
```

-----

## Known Issues

**Issue:**<br/>
Memory allocations of GNOME games cannot be discovered.

These use functions like `g_malloc()` and `g_slice_alloc()` which are only
available in GLib/GTK+ environments.

**Solution:**<br/>
Configure ugtrain build with `./configure --prefix=/usr --enable-glib`.

**Note:** The relevant `g_object_new()` is far away from `g_slice_alloc()` on
the stack. So this feature is still limited and experimental.

-----

**Issue:**<br/>
`LD_PRELOAD` is ignored if the set-group-ID bit is set.
```
$ ls -l /usr/games
-rwxr-xr-x 1 root root      567 Jan 15  2013 gnome-sudoku
-rwxr-sr-x 1 root games  132624 Jan 15  2013 gnomine
-rwxr-sr-x 1 root games  149016 Jan 15  2013 mahjongg
-rwxr-xr-x 1 root root   224328 Apr 16  2012 sol
```

In this case you would have no chance to hook functions of `gnomine` or
`mahjongg`.

**Solution:**<br/>
Remove the set-group-ID bit for them and add yourself into the related group
(`games` here) if required.
```
$ sudo chmod g-s /usr/games/mahjongg /usr/games/gnomine
```

-----

**Issue:**<br/>
`LD_PRELOAD` not effective (not allowed for regular user)

There are high security environments like MeeGo 1.2 Harmattan on the Nokia
N9/N900 smart phone with a security kernel module. On these, a regular user
is not allowed to use LD\_PRELOAD.

**Solution:**<br/>
In this case make a backup of the original ELF binary and use
`patchelf --add-needed <lib> <filename>` to add libmemhack/disc directly into
the .dynsym and .dynamic sections in front of all other libraries.
Then run ugtrain with `-P -` to tell it that LD\_PRELOAD is not available and
that the library will be loaded anyways.

The other functionality like with LD\_PRELOAD is exactly the same.
