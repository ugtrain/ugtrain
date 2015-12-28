## Description

The ugtrain (say U-G-train) is an advanced free and universal game trainer for
the command line under GPLv3 license. The **dynamic memory support** sets it
apart. An integrated preloader, a memory discovery and a memory hacking library
are included for this.

The ugtrain uses one simple **config file per game** which can be exchanged
with others. **Example configs** for games which allow cheating are included.
These also come with **automatic adaption** for dynamic memory so that you can
use them right away on your system after executing it.

Furthermore, security measures like **ASLR and PIE are bypassed**. Together with
**universal checks**, reliable and stable static memory cheating is provided.
All this makes ugtrain now already the **best game trainer on Linux**. And
with **scanmem** it integrates the best memory search on Linux. There is even
**no need for root privileges**.

What a **game trainer** is: <br/>
http://en.wikipedia.org/wiki/Trainer_%28games%29

For **further documentation** see:

* [Questions and Answers]
(https://github.com/ugtrain/ugtrain/blob/master/README_qanda.md)
* [Static Memory Cheating Documentation]
(https://github.com/ugtrain/ugtrain/blob/master/doc/ugtrain-statmem.txt)
* [Dynamic Memory Cheating Documentation]
(https://github.com/ugtrain/ugtrain/blob/master/doc/ugtrain-dynmem.txt)
* [Config Syntax Description]
(https://github.com/ugtrain/ugtrain/blob/master/examples/README.md)
* [Development TODOs]
(https://github.com/ugtrain/ugtrain/blob/master/TODO)

## Logo and Screenshots
<table><tr><td>
<a href="img/ugtrain_logo_300px.png">
  <img src="img/ugtrain_logo_300px.png"
   alt="ugtrain logo" align="left" width="250" />
</a></td><td>
<a href="img/ugtrain_chromium-bsu64.png">
  <img src="img/ugtrain_chromium-bsu64.png"
   alt="ugtrain cheating at Chromium B.S.U. 64 bit" align="right" width="400" />
</a></td></tr></table>

## Videos of the Examples

**Chromium B.S.U.** <br/>
https://www.youtube.com/watch?v=mTpC30tSMqU

**Warzone 2100** <br/>
https://www.youtube.com/watch?v=1GcppQNpdTc

## Legal Warning

Don't use game trainers or any other cheating software for commercial
closed-source, multi-player or online games! Please don't use ugtrain
for that! You may violate copyright and other laws by doing so. Recording
videos of doing so makes the situation even worse in terms of law. Please
always read the end user license agreement (EULA) of the game to be very
sure if you are really allowed to do so or not! If in doubt, use games
with official FOSS licenses (like GPLv2) in single-player mode only.

Especially don't cheat at Steam! They send out crash dumps with delicate
information included. We can't accept that you impair our reputation by
sending dumps with ugtrain parts visible to them.

![Don't cheat at Steam!](img/dont_cheat_at_steam_300px.png)

## How to Build and Install

Make sure that the following packages are installed on your system: <br/>
autotools-dev, autoconf, automake, libtoolize and libtool

1. Generate the missing autotools files: <br/>
`./autogen.sh`

2. Configure the build to generate the Makefiles: <br/>
`./configure --prefix=/usr`

3. Build the ugtrain binaries: <br/>
`make`

4. install ugtrain, tools and the libs to /usr: <br/>
`sudo make install`

5. regenerate the ld.so cache (see "man ld.so"): <br/>
`sudo ldconfig -v`

`objdump` (package binutils) and `scanmem` should be installed as well.

Special configure options: <br/>
Compile hooking libs as 32 and 64 bit (EXPERIMENTAL): <br/>
`./configure --prefix=/usr --enable-multilib` <br/>
Compile hooking libs with GLIB function hooking (EXPERIMENTAL): <br/>
`./configure --prefix=/usr --enable-glib`

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
game with libmemhack64.so preloaded and starts locking the configured
values. In examples/README.md the config syntax is described.

If it asks you for automatic adaption, you should accept. The
adaption can be triggered explicitly as well: <br/>
`ugtrain -A examples/chromium-bsu64`

## Current Limitations

**CPU Architecture**

* PCs: tested on x86 and x86\_64 only
* smartphones: tested on ARMv7 (OMAP3630 in Nokia N9) only

**Operating System**

* Linux only

**Static Memory**

* no adaption yet
* no support for values within a library (PIC) yet

**Dynamic Memory**

* growing of objects/structures experimental, no documentation yet
* no adaption for allocations within a library (PIC) yet
* disassembly within discovery and adaption for x86 and x86\_64 only
* doesn't work with WINE yet

**Compilation**

* 32 bit and x86\_64 only
* tested with GCC g++ only

**GUI**

* can't serve as a backend yet

**Packaging**

* only Debian packaging
* packaging is limited to PCs and MeeGo 1.2 Harmattan (Nokia N9)

**Testing**

* testing is limited to manual tests so far
* tests are mostly based on Debian/Ubuntu distributions

# How to Contribute

Please **create issues on GitHub** in order to report bugs, send feature
requests, send improvement suggestions or request help. If you've developed
really good git commits after forking ugtrain on GitHub, then please
**create pull requests** in order to request a review and merging your commits
into the mainline.

https://github.com/ugtrain/ugtrain

Please **star ugtrain** on GitHub if you like it and **watch it** if you would
like to get informed about issues and pull requests.

In case you don't like doing that via GitHub, just send an email to: <br/>
ugtrain-devel@lists.sourceforge.net

The SourceForge project is only there for having a mailing list, a blog,
a code mirror as well as rating and further promotion/SEO possibilities.

Note: All this has public archives. So please **don't** send anything illegal
(like configs for commercial games) there.

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
