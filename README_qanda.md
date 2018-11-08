## Why this tool if only FOSS games allow cheating?

For the challenge of cheating at them without looking at the source code or
even modifying it and also for education purposes. Advanced reverse engineering
methods are often used by computer criminals (Crackers) for cheating at games.
They do not tell you how they do it. The goal is to get to know the operating
system and the memory management better.

## Why a command line tool?

The command line interface is important e.g. if the game uses full-screen and
does not allow to switch back to another window. Some people start another
display server session for the game but that wastes memory. Command line tools
or libraries can be also used as a backend for a GUI. Command line tools can
endure decades while GUIs change more frequently.

## Where is the difference compared to other tools?

Ugtrain is the only game trainer for the command line on Linux. Other tools
plain never work twice and waste time with memory search and GUI development.
The goal of ugtrain is to reliably freeze all memory values which can be found
and modified by scanmem in the exe, in libraries, on the heap, and on the stack.
Ugtrain has ASLR/PIC/PIE, function hooking, and pointer following support for
this. Universal checks make this safe while other tools blindly modify
configured values. While other tools require root privileges for ptrace(),
ugtrain runs the game to avoid this.
The focus is on C/C++ games using default memory allocation methods.

Games using a runtime engine where memory allocations are done by the engine in
memory-mapped regions are future work.

## Static Memory

### How does Static Memory Cheating work with ugtrain?

The ugtrain freezes memory values but does not search for them to make it
simple. You have to know the memory addresses already and to put them into a
config file.
You can search for the memory values e.g. with the command line tool "scanmem".
Ugtrain integrates scanmem to avoid requiring root privileges (-S option).

See [Static Memory Cheating Documentation](doc/ugtrain-statmem.md) for details.

## Dynamic Memory

### How does Dynamic Memory Cheating work with ugtrain?

C++ games usually store their memory objects dynamically on the heap. Ugtrain
hooks memory allocation functions like e.g. malloc() with LD_PRELOAD to get
the relevant objects the game allocates. The advantage over pointer following
from static memory is that this is easy to discover, easy to adapt, provides
the object size, and works with all memory objects the same C++ constructor
allocates. But pointer following has its advantages if used from within a known
dynamic memory object. This way, ugtrain can keep track of the relation between
the memory objects.

See [Dynamic Memory Cheating Documentation](doc/ugtrain-dynmem.md) for details.
