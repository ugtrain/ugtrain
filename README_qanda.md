## Why this tool if only FOSS games allow cheating?

For the challenge of cheating at them without looking at the source code or
even modifying it and also for education purposes. Advanced reverse
engineering methods are often used by computer criminals (Crackers) for
cheating at games. They don't tell you how they do it. The goal is to
get to know the operating system and the memory management better.

## Why a CLI tool?

CLI is important e.g. if you play full-screen on WINE. Some people
start another X session for the game but we don't like that. Moreover,
you will be able to write your own GUI frontend for it. CLI tools can
endure decades while GUIs change more frequently.

## Static Memory

### How does Static Memory Cheating work with ugtrain?

The ugtrain freezes memory values but doesn't search for them to make it
simple. You have to know the memory addresses already and to put them into
a config file.
You can search for the memory values e.g. with the CLI tool "scanmem".

See doc/ugtrain-statmem.txt for details!

**There are also GUI tools which can freeze found memory values.
Where is the difference?**

Well, this tool is for the CLI and has much more trainer features. It will
become multi-platform and the simple config files give you a lot of
flexibility.

There are universal checks implemented. These prevent changing values e.g.
while being in the game main menu or making an opponent immortal by accident
in an array. This makes static memory freezing a lot safer.

It is also planned to provide static memory adaption. This can be quite
difficult as memory is accessed directly and we have to use a debugger to
find the related code part in the game binary. The goal is that static memory
search has to be done only once.

Furthermore, a REAL game trainer doesn't require own static memory search. If
the memory location changes between game runs, then either the binary is a
position independent executable (PIE) or this memory value is not static at
all (e.g. stored on the heap) and dynamic memory cheating is required.

The ugtrain has PIE support since version 0.2.0. Its trick is to determine
the load address of the executable from /proc/$pid/maps and to add it to the
static memory addresses from the config. So your config must be aware of PIE.
The reason for the load address being different each execution is a security
measure called address space layout randomization (ASLR). Also libraries are
loaded at random location as these are position independent code (PIC).
There is a special scanmem version which supports PIC, PIE and ASLR as well
(merged upstream 2014-05-16).

## Dynamic Memory

### Why is dynamic memory so important and how to hack it?

Since leaving the good old DOS times, games aren't implemented with pure
static memory in C any more but instead object-oriented with e.g. C++.
Memory objects are stored dynamically on the heap. But modern operating
systems use heap and stack randomization for security (ASLR). The only thing
which remains constant is the jump-back code address of the malloc call and
its (reverse) offset on the stack.
With PIE even the code address is different each execution. The load address
of the binary has to be determined from /proc/$pid/maps and to be added to
the code address from within the binary to get the randomized code address
on the stack in this case. The ugtrain does that automatically.
Sure, there are also static access pointers holding the result of a memory
allocation. But these are difficult to adapt and also often only work with
a single object. Imagine a vector or list in between. This is why ugtrain
prefers API hooking.

With libmemdisc preloaded and static memory search in parallel you can
easily discover the code address and register with libmemhack as well as
ugtrain on it to freeze the memory values within the allocated object. The
preloaded game communicates via FIFOs with ugtrain and tells it exactly when
the relevant malloc/free call occurred. This is especially useful if your
distribution requires ptrace and therefore ugtrain to be run as root. Then,
the game process still can be called as regular user. The "HeroAircraft"
object of the game "Chromium B.S.U." is one of the included examples.

In the meanwhile there is a lot of automation and simplification implemented
so that the options -P and -D will help you with that. The option -P ensures
that ugtrain runs the game in the same process group. This way root is not
required any more. The libmemhack is automatically preloaded if -P doesn't
get an argument. With -D the libmemdisc is preloaded instead and after
recording the malloc calls it searches reverse for the found memory address
from static memory search.

See doc/ugtrain-dynmem.txt for details!

There is also automatic adaption to game version/compiler/distribution
differences. Please have a look at the -A option. The examples will ask
you for automatic adaption the first time you run them. You should do
so and a shell script based on "objdump -D" and "grep" is called to find
the correct code addresses in your game binary. With that the discovery is
run to find the reverse stack offset per memory class. With everything
adapted the config is written back to disk and the game starts automatically
with the new config and libmemhack.

You see, dynamic memory cheating really separates the noobs from the pros!
But as ugtrain is an education project, we show you in detail how it works.

### Why using the reverse stack offset and not GNU backtrace()?

Oh, you can use GNU backtrace() with the config option "use\_gbt" and with the
stage 3 discovery string "3;gbt;0x0;0x0;<mem_size>". Then, the configured
reverse stack offsets are ignored. But the game may crash with SIGSEGV if it
hits a NULL pointer while backtracing and it is slower as it has to iterate
through the stack frames. This is why we limit the depth to 4 code addresses.
You see, it makes sense to use the reverse stack offset.

Please read 'TODO' for further information.

## Why is contributing to ugtrain so important?

The more you help, the faster ugtrain development goes! The
more people help, the less likely it becomes that this project
is thrown to the graveyard of discontinued FOSS projects.

Homebrew FOSS is the most honest form of FOSS. It takes a lot of
passion and sacrifice to start a homebrew FOSS project alone and
to keep it running without commercial interest. We make our money
with something else. So we only have improving the software in mind.

We don't want your money, we want your help! Testing this tool
and reporting your experience back to the mailing list is already
very good contribution. We want to improve our software and we want to
see that our sacrifice is honored to motivate us continuing the work.
You get mentioned as a contributor in return. This makes you and your
skills better known. It can also help you with jobs in commercial
environments as you can prove your skills and interests.
