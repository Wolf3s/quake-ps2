#pragma once
/* ================================================================================================
 * File: perf_run.h
 * Brief: Unattended performance run: plays the attract loop's demos with the profiling cvars
 *        set, then quits, so a capture needs nobody watching it.
 *
 *        The value of a performance number is entirely in being able to compare it to the
 *        last one, and that needs the run to be identical: same demos, same order, same cvars,
 *        same start and same end. A human driving the game reproduces none of those.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

namespace ps2::test {

// Registers the "ps2_perftest" cvar. VID_Init calls it, in every build: builds without the
// profiler can't run the test, but they register the cvar anyway. It is archived, and an
// archived cvar a build never registers is dropped from config.cfg on quit, so switching
// between debug and release builds would otherwise add and remove its line each time.
void RegisterPerfTestCvar();

#if PS2_QUAKE_PROFILE
// Advances the performance run by one frame. Call every frame from the main loop, outside
// Host_Frame. Gated by the "ps2_perftest" cvar; a no-op when it is 0 and once the run is over.
//
// To arm a run, add this to id1/config.cfg with the game closed (or to autoexec.cfg):
//
//     ps2_perftest "1"
//
// From there it takes itself: forces "developer 0" and "ps2_frame_log 1", turns off the
// on-screen debug panels (the frame log records everything they show), stops the attract
// loop, plays demo1, demo2 and demo3 once each, ends the frame log cleanly, puts developer
// and the panels back (the frame log goes off) and quits. The emulator log then holds one
// complete capture, terminated by an "FLOG#end" row so a truncated one is recognisable. With
// "ps2_test_quit 0" it stays up instead, and can be armed again (test_quit.h).
//
// One shot: the cvar is archived and set back to 0 before quitting, so the config written on
// the way out disarms the next launch. A run that does not finish stays armed, which is what
// you want when the emulator was closed mid-capture.
//
// Do not enable alongside "ps2_testmaps" - both drive the game through the command buffer
// and would fight over it.
void RunPerfTest();

// What every unattended capture does before its first measured frame and after its last, shared
// by RunPerfTest's demos and the map cycle's perf pass ("ps2_testmaps 2"). BeginPerfCapture forces
// "developer 0" and "ps2_frame_log 1", turns the file-open notes on and the on-screen debug
// panels off. EndPerfCapture writes the frame log's last batch and its FLOG#end row, turns the
// frame log and its notes off, and puts developer and the panels back as they were (the panels
// are archived). Whether the game quits after is the test's call (test_quit.h).
void BeginPerfCapture();
void EndPerfCapture();
#endif // PS2_QUAKE_PROFILE

} // namespace ps2::test
