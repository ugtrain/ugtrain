## Description

The ugtrain is an advanced free and universal game trainer for the CLI under
GPLv2 license on GitHub and Sourceforge.net. It is mostly written in C with
C++ parts for convenience and some shell scripts. The dynamic memory support
sets it apart. An integrated preloader, a memory discovery and a memory
hacking library are included for this.
The ugtrain uses one simple config file per game which can be exchanged
with others. Examples for games which allow cheating are included. These
also come with automatic adaption for dynamic memory so that you can use
them right away on your system after adaption.

The ugtrain is powered by the Open Game Cheating Association.

What a **game trainer** is: <br/>
http://en.wikipedia.org/wiki/Trainer_%28games%29

For **further documentation** see:

* README_qanda.md (Questions and Answers)
* doc/ugtrain-statmem.txt (Static Memory Cheating Documentation)
* doc/ugtrain-dynmem.txt (Dynamic Memory Cheating Documentation)
* examples/README (Config Syntax Description)

## Videos of the Examples

**Chromium B.S.U.** <br/>
https://www.youtube.com/watch?v=mTpC30tSMqU

**Warzone 2100** <br/>
https://www.youtube.com/watch?v=1GcppQNpdTc

## Legal Warning

Don't use game trainers or any other cheating software for commercial
closed-source, multi-player or online games! Please don't use "ugtrain"
for that! You may violate copyright and other laws by doing so. Recording
videos of doing so makes the situation even worse in terms of law. Please
always read the end user license agreement (EULA) of the game to be very
sure if you are really allowed to do so or not! If in doubt, use games
with official FOSS licenses (like GPLv2) only.

## How to Build and Install

Make sure that the following packages are installed on your system: <br/>
autotools-dev, autoconf, automake, libtoolize and libtool

1. Generate the missing autotools files: <br/>
`./autogen.sh`

2. Configure the build to generate the Makefiles: <br/>
`./configure`

3. Build the ugtrain binaries: <br/>
`make`

4. install ugtrain, tools and the libs to /usr/local: <br/>
`sudo make install`

5. (optionally) ensure $PATH includes /usr/local/bin: <br/>
`echo "PATH=$PATH:/usr/local/bin" >> ~/.profile` <br/>
`source ~/.profile`

6. (optionally) ensure that the ld.so config includes /usr/local/lib\*: <br/>
`echo -e "/usr/local/lib\n/usr/local/lib64" >> /etc/ld.so.conf.d/local.conf` <br/>
`ldconfig -v  # regenerate the ld.so cache`

These paths simplify file usage. Where to set these environment
variables is distribution-specific. See "man ld.so".

"objdump" and "scanmem" should be installed as well.

Special configure options: <br/>
Compile hooking libs as 32 and 64 bit (EXPERIMENTAL): <br/>
`./configure --enable-multilib` <br/>
Compile hooking libs with GLIB function hooking (EXPERIMENTAL): <br/>
`./configure --enable-glib`

## How to Use

Use the compiled tool as follows: <br/>
`ugtrain [options] <config>[.conf]`

This searches in the current working directory for the <config>.conf.
If it can't find it there, it tries ~/.ugtrain/<config>.conf.

For details use: <br/>
`ugtrain --help`

Do e.g. the following: <br/>
`ugtrain -P examples/chromium-bsu64`

This parses the chromium-bsu64.conf from the examples, runs the
game with libmemhack64.so preloaded and starts freezing the
configured values. In examples/README the config syntax is described.

If it asks you for automatic adaption, you should accept. The
adaption can be triggered explicitly as well: <br/>
`ugtrain -A examples/chromium-bsu64`

## Current Limitations

**CPU Architecture**

* tested on x86 and x86\_64 only

**Operating System**

* Linux only

**Static Memory**

* no adaption yet
* no support for values in a library (PIC) yet

**Dynamic Memory**

* constructors-only (no growing objects/structures)
* no support for allocations in a library (PIC) yet
* doesn't work with WINE yet

**Compilation**

* 32 bit and x86\_64 only
* tested with GCC g++ only

**GUI**

* can't serve as a backend yet

# How to Contribute

Please send your bug reports, patches and improvement suggestions to: <br/>
ugtrain-devel@lists.sourceforge.net

Note: This list has public archives. So please don't send
anything illegal (like commercial game configs) there.

# How to Help the Open Game Cheating Association

If you like Open Game Cheating and want to show it by becoming
a member of the Open Game Cheating Association, just write an
email to the founder Sebastian Parschauer with your request: <br/>
s.parschauer@gmx.de

https://github.com/OpenGameCheatingAssociation/og-cheating

# Help Wanted

This project needs to fill the following positions with
one or more hobbyists (as we are strictly non-commercial):

* Linux C/C++ Game Cheating Developer
* Windows C/C++ Game Cheating Developer
* Autotools Specialist
* Promoter
* Tester
* Example Config Maintainer (writes configs and creates cheating videos)
* Web Designer
* Linux Distribution Packager

Your help is very much appreciated!
