
# Quake port for the PlayStation 2

## Overview

This is an unofficial fan-made port of id Software's Quake to the PlayStation 2 console. It derives
from [QuakeSpasm][link_quakespasm] 0.97.0, the modern, faithful Quake engine based on FitzQuake and
id's GPL source release: the engine, client, server and QuakeC virtual machine here are
QuakeSpasm's. id's original release notes for the Quake source code are in
[src/README.md](src/README.md).

Like the [Quake II port][link_q2_ps2] it builds on, it relies solely on the free
[PS2DEV SDK][link_ps2_dev], with no official Sony SDK and no proprietary libraries. The goal is a fully
functional and playable single-player Quake on the PS2, built entirely with freely available tools.

QuakeSpasm's C code under [src/quake/](src/quake/) is kept as close to untouched as possible; every change
is tagged `// [PS2_QUAKE]: <why>`. Everything specific to the console lives in [src/ps2/](src/ps2/) and is
written in modern C++ (C++20, no exceptions, no RTTI, warnings-as-errors). That backend comes from the
Quake II port: its renderer (GS front-end, VRAM texture heap, VU1 microprograms), input, audio, memory and
debugging code carry over, ported to QuakeSpasm's interfaces.

### Status

**Every bring-up phase is done.** QuakeSpasm runs on the PS2: in PCSX2 it finds the game data
on `host:`, draws its console, menus and status bar, and takes the DualShock pad (with rumble)
and a USB keyboard, through the attract-mode demos and every map of the registered game. The 3D view draws
all of it: the world - textured, lightmapped, with its fullbright texels, warping water and
scrolling sky - and the brush models, monsters, items, view weapon, sprites and particles. The
sound effects and the soundtrack play through the SPU2, and games save to the memory card. Its
PC-sized limits are cut down to fit the PS2's 32 MB. It hasn't been tried on real hardware yet.
The port was brought up in phases, each checked in PCSX2:

1. Compile QuakeSpasm with the EE toolchain. *Done.*
2. Link and boot, rendering nothing and logging to stdout. *Done.*
3. Game data and the game loop, headless. *Done.*
4. 2D: console, menus, HUD. *Done.*
5. Input: DualShock and USB keyboard. *Done.*
6. 3D: world, lightmaps, water, sky, models, sprites, particles. *Done.* GS fog was left out on
   purpose: none of id's 38 maps sets it, so it stays a future exercise for custom maps.
7. Sound, CD music, save games. *Done.*

What is left is refinement: performance work, and a first run on a real console.

---

## Getting started

### Prerequisites

1. **The ps2dev toolchain and PS2SDK.** Build/install from [ps2dev][link_ps2_dev]
   (`ps2toolchain` + `ps2sdk`). The build expects these on `PATH`:

   | Tool | Comes from | Typical location |
   | --- | --- | --- |
   | `mips64r5900el-ps2-elf-gcc` / `-g++` | ps2toolchain | `$PS2DEV/ee/bin` |
   | `openvcl` | openvcl | `$PS2DEV/bin` |
   | `dvp-as` | ps2toolchain (dvp) | `$PS2DEV/dvp/bin` |
   | `bin2c` | ps2sdk | `$PS2SDK/bin` |

   A `~/.zshenv` (or equivalent) along these lines is what the Makefile and the VSCode tasks expect:

   ```sh
   export PS2DEV=$HOME/ps2dev
   export PS2SDK=$PS2DEV/ps2sdk
   export PATH=$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH
   ```

2. **PCSX2**, to run it on the emulator, plus a PS2 BIOS image for it. The Makefile's `run` target
   defaults to `/Applications/PCSX2.app/Contents/MacOS/PCSX2`; override `PCSX2=` elsewhere.

3. **The Quake game data.** It is not included here. Place it in an `id1/` directory at the root of
   the repository (it is `.gitignore`d): either the shareware `pak0.pak` (episode 1), or the
   registered game's `pak0.pak` and `pak1.pak`.

   ```
   quake-ps2/
     id1/
       pak0.pak
       pak1.pak   (registered game only)
       music/     (optional, trackNN.adp or trackNN.wav soundtrack - see below)
   ```

   The soundtrack is optional. Rip the Quake CD's audio tracks 2-11 to 16-bit PCM WAVs named
   `id1/music/trackNN.wav` (NN = the CD track number; any case), then run `make music` to encode
   them into the `trackNN.adp` files the game streams: SPU2 ADPCM at 22050 Hz, about 1.5 MB a
   minute. Only the `.adp` files need to go onto a USB stick. Input must be at 22050 Hz or exactly
   twice that (a CD rip); convert anything else first, e.g. `afconvert -f WAVE -d LEI16@22050 -c 2
   in.flac out.wav` on macOS. Skipping `make music` works too: a track with no `.adp` plays from its
   `trackNN.wav`, at seven times the bytes read and more EE time to resample a 44.1 kHz rip.

### Building

Three dependencies come in as git submodules: [vclpp](https://github.com/glampert/vclpp), the
preprocessor the VU microprograms go through, at [src/tools/vclpp/](src/tools/vclpp/) (with a submodule
of its own), [vu-checker](https://github.com/glampert/vu-checker), the checks every VU build runs over
the toolchain's output, at [src/tools/vu-checker/](src/tools/vu-checker/), and
[miniz](https://github.com/richgel999/miniz), the deflate codec for save games, at
[src/tools/miniz/](src/tools/miniz/). Clone with `--recursive`, or run
`git submodule update --init --recursive` in an existing clone.

```sh
make            # debug build   -> build/debug/quake.elf (+ host tools)
make release    # optimized     -> build/release/quake.elf
make run        # build, then launch it in PCSX2
```

| | `make` (debug) | `make release` |
| --- | --- | --- |
| Optimization | `-O2` | `-O3` |
| Debug info | `-gdwarf-2 -gz` | none |
| `PS2_Assert` / `PS2_AssertMsg` | on | compiled out |
| `PS2_QUAKE_DEBUG` (tests, debug-only code) | in | compiled out |

Both configs strip the ELF that runs; the symbols stay next to it in `quake_unstripped.elf`, which is
the one to feed `addr2line` or `build/tools/symbolize`. `make compiledb` regenerates
`compile_commands.json` for IntelliSense/clangd, and `make clean` removes all build output.

To cut a release, [make_github_release.sh](src/tools/scripts/make_github_release.sh)
`[-f] [version]` runs `make clean`, `make` and `make release`, then packages both ELFs with an
`id1/` holding the committed `config.cfg` and an empty `music/` into `github_rel_<version>/`, one
folder and one zip per config (`q1ps2_<config>_<version>`). The player adds the paks. `version`
defaults to the one the game reports, QuakeSpasm's (`QUAKESPASM_VERSION`, `_VER_PATCH` and
`_VER_SUFFIX` in `src/quake/quakedef.h`).

### Running in PCSX2

`make run` symlinks `build/<config>/id1` to the repo's `id1/` and launches
`PCSX2 -batch -elf build/<config>/quake.elf`. PCSX2 needs a few settings that are **off by default**.
Edit `~/Library/Application Support/PCSX2/inis/PCSX2.ini` (macOS) **while PCSX2 is closed**, since it
rewrites the file on exit:

```ini
[EmuCore]
HostFs = true             ; host: maps to the ELF's directory; without it there is no game data

[Logging]
EnableIOPConsole = true   ; the game's stdout goes through the IOP
EnableFileLogging = true  ; ...and into logs/emulog.txt

[USB1]
Type = hidkbd             ; optional: a USB keyboard that passes host keystrokes through
```

The engine's console output is prefixed with `[Q1]` in the log:

```sh
tail -f ~/Library/Application\ Support/PCSX2/logs/emulog.txt | grep '\[Q1\]'
```

Keep the ELF's full path under 152 characters: PCSX2 hands it to the BIOS as `host:<path>`, and a
longer one crashes the BIOS's loader before the game starts, with no output but a run of
`TLB Miss` lines. A release unzipped deep in a folder tree can get there.

`make run RUN_ARGS="-heapsize 20480"` puts arguments on the game's command line (PCSX2's
`-gameargs`). `+commands` there do nothing with the shareware data: like id's Quake,
QuakeSpasm only runs them for the registered version. An `id1/autoexec.cfg` runs at boot
either way, as on the desktop, and is the way to script a session.

### Running on a console

It hasn't been tried on real hardware yet; this is what the boot code expects. Build the release ELF
(`make release`, then `build/release/quake.elf`) and copy it and `id1/` side by side into a folder on
a FAT32/exFAT USB drive or on a PFS partition of the internal HDD. Launch the ELF from a loader such
as wLaunchELF:

```
mass:/QUAKE/
  quake.elf
  id1/
    pak0.pak
    pak1.pak   (registered game only)
    music/     (optional: the trackNN.adp files `make music` writes; the .wav files can stay behind)
```

- **Where it looks.** Boot looks for `id1/pak0.pak` on `host:` first, which fails at once on a
  console. An ELF launched from USB (`mass:`) then looks over USB only, and leaves the HDD alone;
  launched from anywhere else, it looks on the HDD, then over USB. On both, the folder the ELF was
  launched from is tried before the drive's root. On the HDD the launch partition goes first, then the
  other partitions; over USB every FAT volume is tried (`mass0:` to `mass9:`, one per partition), after
  waiting up to 10 seconds for the drive to come up. The storage, keyboard and sound drivers are
  embedded in the ELF, so nothing else needs installing.
- **The IOP is taken as the launcher left it.** Each IOP driver the game needs (iomanX and fileXio,
  the HDD's and USB's, and later the pad, memory card, sound and keyboard drivers) is reused when the
  launcher left one doing that job running, and loaded otherwise, so a USB drive the launcher has
  already mounted is read at once. The IOP is reset first only when something running can't work
  alongside the game's drivers, and the boot falls back to that reset when a driver won't start, when
  no game data turns up through the launcher's drivers, or when too little IOP memory is left.
  `-DPS2_QUAKE_ALWAYS_RESET_IOP=1` in `CONFIG_DEFS` resets on every boot. The `IOP boot:` lines, in
  the log file and from `ps2_dump_iop_mods`, say what the launcher left running, what was reused,
  loaded or reset, and when each step finished.
- **If it finds nothing**, the error screen shows the path the loader passed, the folders it searched,
  the HDD's status and the USB volumes it mounted.
- **Load times.** The HDD loads a level about twice as fast as the best USB setup. The PS2's USB is
  1.1, at best about 0.9 MB/s, so a faster stick doesn't help; what helps is fewer USB commands.
  Format the stick FAT32 with **32 KB clusters or larger**: the FAT driver reads at most one
  cluster per command, and each command costs about 5 ms on top of its transfer. macOS picks 8 KB
  for a 16 GB stick, and Disk Utility can't change it; `sudo newfs_msdos -F 32 -c 64 -v NAME
  /dev/rdiskNsM` can (it erases the volume: check the device with `diskutil list` first). Once
  the drive is found, the boot raises fileXio's IOP read buffer from 16 to 64 KB
  (`PS2_QUAKE_FILEXIO_BUFFER_KB` in [iop_boot.cpp](src/ps2/system/iop_boot.cpp)): it reads one
  buffer at a time, so on the HDD it sets the transfer size. On the Quake II port's console that
  and the bigger clusters took a level's load from 58.6 to 40.4 s over USB; the HDD took 15.8 s.
- **A log on the drive.** A console run has no stdout anyone can read: `ps2_logfile 1` (in
  `id1/autoexec.cfg`, or at the console) writes `quake.log` next to `id1/`. See
  [Debugging commands](#debugging-commands).
- **Saves and settings go to the memory card** in slot 1 (see [Save games](#save-games)), never to
  the USB drive or the HDD.
- **NTSC or PAL.** The game runs in the console's own standard: 50 Hz and 512 lines on a PAL
  console, 60 Hz and 448 on an NTSC one. Options → Video Options → Video mode forces one, after a
  restart; a PAL console can drive NTSC if the TV takes 60 Hz.
- **The debug ELF** (`build/debug/quake.elf`) runs the same way, with assertions on and the debug
  overlays showing.

**Trying the console's storage in PCSX2.** With `HostFs = false` the boot takes the console's path.
For USB, give PCSX2 a writable raw disk image with an MBR and a FAT32 partition (not an ISO), with
`id1/` at its root:

```ini
[USB2]
Type = Msd
Msd_subtype = 0
Msd_ImagePathMsd = /absolute/path/to/quake-usb.img
```

On macOS, [make_usb_image.py](src/tools/scripts/make_usb_image.py) makes one from the current
builds and `id1/`: `make && make release && src/tools/scripts/make_usb_image.py`. It writes
`build/quake-usb.img` (`--output` for another path, `--force` to replace one, `--size-mib` for a
fixed size; at least 256 MiB, with room to spare), holding `quake_debug.elf` and
`quake_release.elf` beside a copy of `id1/`: the paks, `config.cfg` and any `autoexec.cfg`, and
the music's `.adp` tracks but not its `.wav` sources. It runs no build, and leaves the stripped
ELFs' symbols in `build/`. Close PCSX2 before replacing an image it has attached. `hdiutil attach
-imagekey diskimage-class=CRawDiskImage build/quake-usb.img` mounts one afterwards, to read the
`quake.log` a run left there. `[DEV9/Hdd]` `HddEnable` and `HddFile` attach an HDD image the same way. PCSX2 launches
the ELF with a `host:` path, so the boot searches the HDD before USB, as it would for an ELF
launched from the HDD. PCSX2 doesn't model either drive's speed, so its load times say nothing
about a console's.

### Debugging commands

| Command | What it does |
| --- | --- |
| `ps2_memstats` | Prints the backend's memory tags, and dlmalloc's arena, in-use, untagged-malloc and free totals. |
| `hunk_print` | QuakeSpasm's own: prints the hunk by block, with what remains. |
| `ps2_dump_iop_mods` | Prints the boot's `IOP boot:` report, lists the IOP modules loaded now, and says how much IOP memory is free in one block. |
| `screenshot` | QuakeSpasm's own, TGA only: reads the last frame back out of GS VRAM and writes `id1/spasmNNNN.tga`. |
| `ps2_saveinfo` | Says where saves are going, and lists the memory card's save directory. |
| `menu_debug` | Opens the debug page (below), as QuakeSpasm's `menu_options` opens its own. |

**The debug page**, Options → Debug Options, puts these on the gamepad, for a console with no
keyboard: the log file on or off, the IOP report, the save list, `condump` (the console's text to
`id1/condump.txt`), a screenshot of the next frame without the menu (`id1/spasmNNNN.tga`), and in
the builds that have them, `ps2_test_quit` and the perf test and map cycle below. A report row opens the console on what it printed; there L2 and R2 scroll it, as PgUp
and PgDn do on a keyboard ([debug_menu.cpp](src/ps2/debug/debug_menu.cpp)).

**The log file.** `ps2_logfile 1` copies everything the console prints, plus a `Sys_Error` and the
reports printed on the way to one (the debug build's EE exception report, render pipeline dump and
stack traces, the out-of-memory stats), to `quake.log` next to `id1/`: `build/<config>/quake.log` on
`host:`, the game data's folder on the HDD or the USB drive. It opens with the boot report. Each
line is stamped with the seconds since `main()` began. The configs that can turn the log on only run
once `Host_Init` is done, so until it is on, the first 16 KB of output waits in RAM and goes in
first: a log turned on from `autoexec.cfg` or the console still starts at boot. Every write opens,
appends to and closes the file. On a FAT drive the close is the only flush (bdmfs_fatfs has no
sync), so a run that hangs or crashes still leaves its log up to the last line. On the HDD each
close is followed by a `fileXioSync` of the mounted partition. On the Quake II port's console that
cost about 23 ms a line over USB, so a load timed with the log on mostly measures the log. A write
that fails turns logging off, with a warning on stdout.

**Loading screen.** While the loading plaque is up, the screen shows the plaque with a status bar
along the bottom naming the file being read, and the files, megabytes and seconds so far. It is
redrawn at most every 200 ms, and as soon as a file of 256 KB or more starts. Each frame of it is
shown before its draw returns
([loading_screen.cpp](src/ps2/renderer/loading_screen.cpp)): with `ps2_gs_latency` on, the plaque
`SCR_BeginLoadingPlaque` draws would otherwise wait for a next frame that nothing draws until the
level is up, so it never showed. QuakeSpasm raises the plaque, over a game or a demo, for a level
change, a game loaded from the menu and the demo loop's next demo. A `map`, typed at the console or
the menu's New Game, disconnects first, and loads with no plaque.

**Load trace.** Set `PS2_QUAKE_LOAD_TRACE` to 1 in [load_trace.h](src/ps2/debug/load_trace.h) (it
ships 0; make tracks the header, so no clean is needed). The log file then defaults to on, and the
level loading path prints `[load +<ms> ms]` lines: the plaque going up and down, the server's spawn
(the world loaded, the entities spawned with their models), the client's precache of models and
sounds, `R_NewMap`, each signon and the first 3D view after the load. Every file read is traced
with its size and read time. A heartbeat a second shows the signon while a level comes up. "+ms" is
the time since the previous trace line, so a step's duration is on the line that ends it.

**I/O log.** A build with `-DPS2_QUAKE_IO_LOG=1` in `CONFIG_DEFS` (it ships 0; clear
`build/<config>/src` first, since make doesn't track flags) records what each load under the
loading plaque read: a line per file with its time in the file calls and its bytes, and a summary
with the files, the megabytes, the time in file calls against the whole load, and the fileXio
buffer size ([io_log.h](src/ps2/debug/io_log.h)). The lines stay in RAM until the load ends and
then go to stdout and `id1/io_log.txt` in one write, so they don't slow the load they time, as
the load trace's lines do with the log file on. It is how a console's load times are compared:
build it as a release, and load from the menu or by a level change. The console says
`I/O log: load N (maps/...) written to <path>` as each load ends.

**Render pipeline hangs.** In debug builds every wait on the GS
([cmd_buffer.cpp](src/ps2/renderer/cmd_buffer.cpp)) gives up after about a second, dumps the
pipeline and stops with `Sys_Error("Render pipeline hang: ...")`. The dump
([pipeline_dump.cpp](src/ps2/debug/pipeline_dump.cpp)) has the VIF1 DMA channel, VIF1, VU1's run
state (VPU-STAT), the GIF and its tag registers, and the GS CSR, each raw and decoded. Then come:

- the raw qwords just before `D1_TADR`, where the DMAC stopped;
- the chain trail: every DMA tag from the start of the stuck chain up to `D1_TADR` is walked, and
  the last 32 are printed with their VIFcodes decoded (UNPACKs, `MSCAL` addresses, FLUSHes, DIRECT
  sizes). A DIRECT's GIF packet is followed tag by tag, so a packet that asks for more data than
  its block holds, which holds PATH2 open forever, is called out;
- which microprogram ran last (the last `MSCAL` on the way), and its GS packet in VU1 memory,
  checked the same way in that program's layout. A particle batch header holding a count the EE
  can't send is flagged, and the packet is read where that count made the program kick;
- all 16 KB of VU1 data memory;
- the call stack of the wait that gave up.

All of it goes to stdout and to the log file, about 1,100 lines, so a hang on a console leaves
the whole report in `quake.log`. Before the error screen draws, it stops DMA channels 1 and 2 and
resets VIF1, VU1 and the GIF, in libgs's `GsResetPath` order
([scr_print.cpp](src/ps2/debug/scr_print.cpp)): a GIF still holding a path open would otherwise
keep the error screen's own transfers out. Running out of memory prints dlmalloc's state and the
call stack the same way before its error screen.

Unattended tests are armed by cvars (see [CVARS.md](CVARS.md)). `ps2_testmaps 1` (debug builds)
loads every level in the paks in turn (the campaign in play order, then the deathmatch maps) and
prints what each costs in memory. `ps2_perftest 1` (profile builds) plays the three demos with the
frame log on, and `ps2_testmaps 2` does the same over every level: it turns a full circle at a few
viewpoints spread over each. Each test quits when it ends; `ps2_test_quit 0` leaves the game up
for the next one instead, which is how the debug page runs several tests in a session on a
console. Both captures are for the scripts in
[src/tools/scripts/frame_log/](src/tools/scripts/frame_log/) (`frame_budget.py --pal` for a PAL
console's). A console has no stdout anyone reads: a build with `PS2_QUAKE_FRAME_LOG_FILE=1`
appends the capture to `id1/frame_log.txt` as well
([performance.md](.claude/rules/performance.md) has the recipe).

---

## Rendering

QuakeSpasm draws through the public functions of its OpenGL renderer, and the backend
implements those directly, with no GL underneath. All of it draws but fog, which no id map sets
(`gl_fog.c` still parses it, so a custom map that sets it plays, unfogged).

- **Video and the frame** ([vid.cpp](src/ps2/renderer/vid.cpp)). `VID_Init` brings the GS up at
  640x448 on NTSC and 640x512 on PAL, with two framebuffers, 16-bit by default
  (`ps2_fb_16bit`), and a 16-bit z-buffer. `ps2_video_mode` picks the standard: `auto` takes the
  console's own (from its BIOS ROM name), and `ntsc` or `pal` force one, so a PAL console can run
  at 60 Hz on a TV that takes it ([video_mode.h](src/ps2/system/video_mode.h), which the error
  screen shares). Both are read at boot, from the config ahead of the configs, as QuakeSpasm reads
  its video mode. 512 lines leave 240 KB less VRAM for textures than 448 (2168 KB). Options' Video
  Options page, which QuakeSpasm leaves to the video module, holds them with `ps2_fb_dither` and
  `ps2_gs_latency`. `SCR_UpdateScreen` draws each frame between `GL_BeginRendering`, which clears
  it to `r_clearcolor`, and `GL_EndRendering`, which submits it. By default the GS draws a
  finished frame while the EE builds the next one (`ps2_gs_latency`).
- **Boot splash** ([splash_screen.h](src/ps2/renderer/splash_screen.h)). First thing in
  `main()`, before any IOP driver comes up, the GS is set up the way `gs::Init` sets it up with
  the default video settings, and the QUAKE plaque from the menus (`gfx/qplaque.lmp`) is drawn at
  twice its size, centred on black and brightened by `kColorIntensity`, into what becomes the
  first framebuffer. So it stays up through the IOP boot, the search for the game data and
  `gs::Init`, until the game presents its first frame. When `ps2_video_mode` or `ps2_fb_16bit`
  lay that framebuffer out differently, `gs::Init` starts it black instead, since it would read
  the plaque's texels the wrong way. A splash that can't be drawn is skipped. The plaque and the
  palette it draws through are compiled into the ELF ([src/ps2/builtin/](src/ps2/builtin/), made
  by [imgdump](src/tools/host/imgdump.cpp)), and the menus draw that same copy: the file is never
  read, a mod's own included.
- **Palette.** Quake's textures stay 8-bit in VRAM (PSMT8) and sample `gfx/palette.lmp`
  through a CLUT, built once as the GS comes up.
- **Command buffer.** A frame is one DMA chain to VIF1, built in a 1.5 MB block of two 768 KB
  halves, so one can fill while the other draws. 3D batches go to the VU1 microprograms, and 2D
  goes straight to the GS. The sky and the world are kicked as soon as they are built, so the GS
  draws them while the EE builds the entities, the water, the particles and the 2D. A frame that
  outgrows its half kicks what it has built and carries on in the other half, which that kick has
  already waited out, so the GS draws the first part while the EE builds the rest.
- **2D** ([draw.cpp](src/ps2/renderer/draw.cpp)), in place of QuakeSpasm's `gl_draw.c`: console
  text, pics, fills, the tiled border, and QuakeSpasm's canvases, each of whose
  `glOrtho`/`glViewport` pairs becomes a scale and an offset. WAD pics under 64x64 share two
  256x256 scrap atlases, as in QuakeSpasm; every other pic gets its own texture. The menus
  default to twice their size (`scr_menuscale` 2), which fills the 640x448 screen the way the
  original filled 320x200. At that size QuakeSpasm's slider values, which sit past the menu's
  right edge, are cut off. The status bar stays at its own size (`scr_sbarscale` 1), centred
  and 48 lines tall, as id's Quake drew it at 640x480. At 2 it spanned the screen and, with
  the 3D view running under the translucent bar (`scr_sbaralpha` below 1), covered most of the
  view weapon. The 2D samples nearest, so only whole scales keep its pixels even.
- **The world** ([view.cpp](src/ps2/renderer/view.cpp)), in place of QuakeSpasm's
  `gl_rmain.c`, `r_world.c` and `r_brush.c`: its visibility code as it is (`R_MarkSurfaces`
  walking the PVS onto the textures' chains), and its multipass drawing on VU1 - each chain's
  textures, then the lightmaps multiplying the framebuffer by the light (up to nearly twice,
  QuakeSpasm's `gl_overbright`), then the fullbright texels added over that. Water, slime and
  lava ripple on VU1 with QuakeSpasm's warp, an 8-texel sine every 128 texels, bent at the
  corners of a 32-unit grid their surfaces are cut on (so `gl_subdivide_size` does nothing).
  Brush entities run the same passes
  under their own transform. With `gl_flashblend` the dynamic lights glow as additive fans
  instead of lighting the walls.
- **Sky** ([sky.cpp](src/ps2/renderer/sky.cpp)), in place of `gl_sky.c`: QuakeSpasm's two
  scrolling layers on a box around the camera, tessellated by `r_sky_quality` (12, at most 16
  here) and drawn only over what the sky surfaces in view cover; `r_fastsky` draws the flat
  colour. The GS's z-test can't pass "farther", which QuakeSpasm's drawing order needs, so the
  order turns round: the layers go down first, then the sky surfaces' depth alone, then the
  world. Skyboxes (`sky`, worldspawn's `sky`) need external images, which the PS2 doesn't load.
- **Alias models** ([alias.cpp](src/ps2/renderer/alias.cpp)), in place of `r_alias.c` and
  `gl_mesh.c`: the monsters, items and view weapon, lerped between two poses on VU1. The EE
  copies each corner's two pose words into the frame as they are, and VU1 converts, lerps and
  transforms them. What else a corner needs - which pose vertex it is, its skin coordinates - is
  built once, when the model first loads, and read where it lies. They are lit as QuakeSpasm
  lights them (`gl_overbright_models` included), with the skin's fullbright texels drawn over
  again from the same vertices. The view weapon's depth is squeezed into the near end of the
  z-buffer. `r_shadows` draws QuakeSpasm's flattened shadows. Players wear their colours.
- **Sprites and particles** ([sprite.cpp](src/ps2/renderer/sprite.cpp),
  [particles.cpp](src/ps2/renderer/particles.cpp)): sprites as quads in all five of
  QuakeSpasm's orientations, unlit, their holes cut by the alpha test; particles as QuakeSpasm's
  discs (or squares, `r_particles 2`), each a GS sprite VU1 expands, growing with distance.
- **Draw data** ([brush.cpp](src/ps2/renderer/brush.cpp)): every brush surface's vertices,
  baked as the VU1 path takes them when a map loads, each wall's texture coordinates moved by
  whole repeats to start near zero (a console's GS loses precision on large ones;
  `ps2_st_rebase`), and the lightmaps
  ([lightmap.cpp](src/ps2/renderer/lightmap.cpp)): 256x256 atlases of one byte per luxel,
  rebuilt when a light style moves or a dynamic light reaches them.
- **Textures** ([texmgr.cpp](src/ps2/renderer/texmgr.cpp)): the BSP's textures stay on the
  hunk, all four of id's mip levels, and upload from there. Skins and sprite frames are copied,
  their rows padded to 16 texels, since the model file they come from is gone once it has
  loaded; a skin's glow texture draws from the skin's copy. Each samples through the CLUT for
  the palette QuakeSpasm would pick: whole, with the fullbright range black for the lit pass,
  or that range alone for the glow pass. A texture the engine frees while a frame is being
  recorded - a model the cache evicts mid-frame - is released once that frame has drawn.
- **Debug overlays** ([overlays.cpp](src/ps2/renderer/overlays.cpp)): an FPS counter and
  panels for frame times, memory, VRAM and draw statistics, on by default in debug builds
  (see [CVARS.md](CVARS.md)).
- **Screenshots.** `screenshot` reads the last finished frame back out of VRAM, turning the GS
  bus around for it, and writes `id1/spasmNNNN.tga`.

---

## Input

[input.cpp](src/ps2/input/input.cpp) implements QuakeSpasm's `IN_*` seam, in place of its SDL
`in_sdl.c`, over the DualShock pad ([pad.cpp](src/ps2/input/pad.cpp): libpad on the ROM's PADMAN,
locked into analog mode) and a USB keyboard ([keyboard.cpp](src/ps2/input/keyboard.cpp): the
ps2kbd driver, read raw).

**The pad** follows QuakeSpasm's game controller model. Each button sends one of the controller
keys QuakeSpasm already has, so the menus take it (Cross is Enter, Circle is Back) and it binds
like any other key (`bind RTRIGGER +attack`). id's `default.cfg` binds none of them, so the PS2
adds the binds below right after it runs (`PS2_DefaultConfig`, called from `cmd.c`), which puts
them back on the options menu's "Reset to defaults" too. Most are QuakeSpasm's own pad binds.

| Button | Key | Default bind |
| --- | --- | --- |
| Cross / Circle | `ABUTTON` / `BBUTTON` | `+jump` / `+movedown` (swim down) |
| Square / Triangle | `XBUTTON` / `YBUTTON` | |
| L1 / R1 | `LSHOULDER` / `RSHOULDER` | `impulse 12` / `impulse 10` (previous / next weapon) |
| L2 / R2 | `LTRIGGER` / `RTRIGGER` | `+jump` / `+attack` |
| L3 / R3 | `LTHUMB` / `RTHUMB` | |
| D-pad | the arrow keys | id's `+forward`, `+back`, `+left`, `+right` |
| Start / Select | `ESCAPE` / `TAB` | the menu / id's `+showscores` |

The left stick moves and strafes and the right one turns and looks, through QuakeSpasm's `joy_*`
cvars (dead zones, response exponents, sensitivities, `joy_invert`, `joy_swapmovelook`) at its
defaults, but for wider dead zones: 0.25 of the stick's travel instead of 0.175, since a worn
DualShock 2's stick can rest far enough off centre to turn the camera on its own. Past the dead
zone the response still ramps up from zero. The defaults also turn `cl_alwaysrun` on, so the
stick's deflection sets the pace all the way up to a run. Outside a game the left stick works as the arrow keys, and held buttons
repeat, so menus scroll. While the console is down, L2 and R2 scroll it, as PgUp and PgDn: a pad
has no way to reach those keys. Unlike QuakeSpasm's, `joy_enable 0` switches off only the sticks:
the pad is all the input most PS2s have.

**Rumble** ([rumble.cpp](src/ps2/input/rumble.cpp)) answers what happens to the player with short
bursts on the pad's two motors, which overlap when they run at the same time: each weapon's shots
(by the muzzle flash the server flags on the player), damage taken (from `V_ParseDamage`, stronger
the harder the hit), pickups (the bonus flash every pickup sends) and powerups coming on. It is
off in menus, the console, demos and while paused; `in_rumble 0` turns it off.

**A USB keyboard** is optional (`in_keyboard`, on by default in debug builds only: it is a
development tool, and a build that never brings its driver up loads none of its IOP modules). It
sends keys by position, as on a US layout, and types through QuakeSpasm's text input
(`Char_Event`) with Shift applied; the last key pressed repeats while held. Every poll is a file
read on the IOP, so the keyboard is read every frame only while in use (a key in the last 10
seconds, or one held) and four times a second otherwise, and never while the music stream has a
read out, which the poll would wait for. `in_debugkeys 1` prints every pad button and keyboard usage as it arrives,
mapped or not. `in_keyboardmap <usage> <key>` points a USB usage at another key: PCSX2's
passthrough keyboard sends usage 0x34, the apostrophe, for the host's `` ` ``, so 0x34 opens the
console by default, and `in_keyboardmap 0x34 '` gives the apostrophe back on real hardware.

---

## Sound

QuakeSpasm's mixer (`snd_dma.c`, `snd_mix.c`, `snd_mem.c`) runs unchanged. The backend
([audio/](src/ps2/audio/)) implements its `SNDDMA_*` seam in place of `snd_sdl.c`:

- [`AudsrvDevice`](src/ps2/audio/audsrv_device.h) brings up `libsd.irx` and `audsrv.irx` (both
  embedded in the ELF) and opens a 16-bit stereo stream. The SPU2 is only reachable from the IOP,
  so every mixed byte crosses SIF through audsrv's RPCs, and audsrv upsamples it to the SPU2's
  48 kHz.
- [`MixRing`](src/ps2/audio/mix_ring.h) is the 64 KB buffer the mixer paints into. The play
  position reported back to the engine is how far the backend has submitted, not where the SPU2
  is, as WinQuake's waveOut backend did: it advances at the playback rate and never goes back,
  which is what QuakeSpasm's `GetSoundtime` needs.
- **The feeder** ([snd.cpp](src/ps2/audio/snd.cpp)) plays the part a sound card's DMA plays on a
  PC. It is a thread one priority level above the main thread, woken every 5 ms by a timer
  alarm, that keeps audsrv's queue 43 ms deep from what the mixer has painted. audsrv's IOP
  thread never checks for an empty queue: fed once a frame, any stall longer than its 106 ms
  queue (a level load, a save) would replay the last 106 ms in a loop, a loud buzz. The feeder
  carries on through a stall, padding the queue with silence once the painted audio runs out,
  without moving the position the engine reads back, so the mix picks up where it stopped. The
  IOP round trips happen on its thread too, off the main thread's frame.

**The mixing rate** is 22050 Hz: QuakeSpasm's `snd_mixspeed`, which defaults to 22050 on the PS2
instead of 44100. Quake's sound effects are 11025 Hz, and each is kept resampled to the mixing
rate in the hunk's cache, so 44100 would quadruple that (3.3 MB at 22050 in demo1) and the
mixing, and bring in QuakeSpasm's lowpass filter on top. 22050 is also the CD music's rate.
`-mixspeed <rate>` on the command line picks another rate audsrv takes: 11025, 12000, 22050,
24000, 32000, 44100 or 48000. Sound starts before `config.cfg` runs, so the cvar itself can't
change it.

In PCSX2 (debug build, demo1) the mixing costs about 0.42 ms of the main thread's frame. The
feeder is awake about 0.45 ms a frame, most of it waiting on the IOP while the main thread
runs. A new sound reaches the speakers after `_snd_mixahead` (0.1 s) plus the 43 ms queue.
`-nosound` turns sound off, as on QuakeSpasm, and an IOP driver that doesn't come up only costs
the sound.

**CD music.** QuakeSpasm plays a map's track off the Quake CD, or else from a loose
`music/trackNN` file through its codec libraries. This port reads no audio CD and has no codec
layer, so [cd_audio.cpp](src/ps2/audio/cd_audio.cpp) implements QuakeSpasm's `BGM_*` music over
loose `id1/music/trackNN.adp` files, SPU2 ADPCM: the PS2's native sample format, 3.5 times
smaller than 16-bit PCM, encoded by `make music` ([musenc](src/tools/host/musenc.cpp)). A track
with no `.adp` plays from its `trackNN.wav`. [`MusicStream`](src/ps2/audio/music_stream.h) keeps
two read buffers in flight (8 KB each for ADPCM, up to 64 KB each for a WAV), allocated only
while a track plays, and decodes on demand; the file reads come from a reader thread at the
main thread's priority. The music goes into the mixer's raw-sample channel, as `bgmusic.c`'s
did, so it reaches the SPU2 inside the audsrv stream; a 44.1 kHz WAV goes through a 23-tap
half-band decimator ([half_band.h](src/ps2/audio/half_band.h)) on the way in, and other rates
through a linear resampler. The file format is the Quake II port's, so its tracks play here too.

It behaves as `bgmusic.c` does: `bgm_extmusic` (the options menu's "External Music") turns it on
and off, `bgmvolume` ("CD Music Volume") sets the level, `music_loop` decides whether a track
starts over when it ends, and `music <name>`, `music_stop`, `music_pause` and `music_resume`
drive it from the console. Turning `bgm_extmusic` off stops the music and turning it on starts
the map's track again, right away rather than at the next map. In PCSX2 the ADPCM decode costs
about 0.16 ms of EE time per frame, and mixing the music in another 0.08 ms.

---

## Save games

QuakeSpasm saves a game as one text file, `<gamedir>/<name>.sav`, written through stdio, and its
load and save menus read each slot's comment line back the same way. On the PS2 those three
spots go through the backend ([save/](src/ps2/save/)) instead, and the saves live on the memory
card in MEMORY CARD slot 1:

- **A slot** (`s0` to `s19` from the menus, any name from the console's `save`) is one archive
  file on the card: a header carrying the menu's comment line, then the `.sav` text deflated
  ([slot_archive.cpp](src/ps2/save/slot_archive.cpp)), CRC-checked throughout. A save is written
  to the slot's *other* file (`<slot>_a.q1s` / `<slot>_b.q1s`) before the previous copy is
  deleted, so a failed or interrupted save never loses the one before it. The text is deflated
  as `Host_Savegame_f` writes it ([packed_blob.cpp](src/ps2/save/packed_blob.cpp)): an 83 KB
  e1m1 save takes 11 KB of the card, and about 0.7 s to write.
- **The card** is reached through libmc and the ROM's MCMAN/MCSERV
  ([memcard.cpp](src/ps2/save/memcard.cpp)), in a `Q1PS2` directory with the `icon.sys` and 3D
  icon the PS2 browser shows: the quad damage, built from the game's own `progs/quaddama.mdl`
  ([mc_icon.cpp](src/ps2/save/mc_icon.cpp)).
- **Running from `host:`**, the archived `ps2_savedevice` cvar keeps saves as QuakeSpasm does,
  plain `.sav` files in `id1/` (`host`, the default there), or sends them to the card (`mc`).
  `ps2_saveinfo` says where they go and lists the card's files.

`config.cfg` - the binds and archived cvars - is kept where each platform keeps settings
([save_api.cpp](src/ps2/save/save_api.cpp)):

| | Game data on `host:` | Game data on HDD (`pfs0:`) or USB (`massN:`) |
|---|---|---|
| **Saving** | `id1/config.cfg` on the host, as always; plus the card's `Q1PS2/config.cfg` when saves go to the card (`ps2_savedevice mc`) | the card's `Q1PS2/config.cfg` only. Never the game-data drive |
| **Loading** | the host's `id1/config.cfg` first - the file to edit by hand while developing; the card's if there is none | the card's first - the player's own settings; the drive's `id1/config.cfg` only if the card has none |

The cvars read ahead of `config.cfg` running (`ps2_fb_16bit`, `in_keyboard`) are read from the same
file. QuakeSpasm writes the config on quit, which a console rarely sees, so leaving the options
menu writes it too; the card copy is only rewritten when it changed. A card that is missing or
full just skips it, with a line on the console.

The repository tracks `id1/config.cfg`, the settings a run under the emulator starts from. The text
the game writes is deterministic: the binds in key order, the archived cvars sorted by name, each
as it was set. So a run that changes no setting writes the file back byte for byte, and `git status`
shows when one did.

---

## Source layout

```
src/
  quake/                QuakeSpasm's C: engine, client, server, QuakeC VM
  ps2/                  the PS2 backend - all new C++ code
    system/             main() entry point, Sys_* seam, IOP boot, dlmalloc heap
    renderer/           GS front-end, VRAM heap, textures, models, VU1 path
      vu1progs/         VU1 microprograms (.vcl)
    audio/              SNDDMA_* seam, audsrv device, mix ring, CD music streaming
    input/              IN_* seam, DualShock pad and rumble, USB keyboard
    math/               vector/matrix math for the renderer
    net/                the network driver table (loopback only)
    save/               save games on the memory card
    debug/              on-screen error printing, the log file, stack traces, EE exception handling
    tests/              standalone bring-up scenes and tests
    builtin/            images compiled into the ELF (the QUAKE plaque, the palette), from imgdump
  tools/
    host/               host-side command line tools (unpak, imgdump, musenc)
    scripts/            Python helpers (symbolize, compile_commands.json, frame logs, the USB
                        image) and make_github_release.sh
    vclpp/              VCL preprocessor for the VU microprograms (git submodule)
    vu-checker/         openvcl/dvp-as output checks run by every VU build (git submodule)
    miniz/              deflate codec for the save games (git submodule)
    vscode_extensions/  VCL/VU assembly syntax highlighting for VSCode
```

`src/ps2` started out as the Quake II port's backend; all of it is ported to QuakeSpasm now.

---

## Credits

- [QuakeSpasm][link_quakespasm], which this port derives from: everything under [src/quake/](src/quake/)
  is QuakeSpasm 0.97.0's code, by the QuakeSpasm developers, changed only where tagged `[PS2_QUAKE]`.
  QuakeSpasm is itself based on John Fitzgibbons' FitzQuake.
- id Software's Quake, released under the GPL in 1999 ([source][link_id_repo]; the original release
  notes are in [src/README.md](src/README.md)).
- The PS2 backend under [src/ps2/](src/ps2/) comes from the [Quake II port][link_q2_ps2].

---

## License

Quake was released by id Software under the GNU General Public License, and QuakeSpasm is
distributed under the same terms. New code written for the PS2 port and any changes made to the
original source code are also released under the GNU General Public License version 2. See the
accompanying LICENSE file for details.

[link_quakespasm]: https://github.com/sezero/quakespasm
[link_id_repo]: https://github.com/id-Software/Quake
[link_q2_ps2]: https://github.com/glampert/quake2-ps2
[link_ps2_dev]: https://github.com/ps2dev
