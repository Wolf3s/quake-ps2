/* ================================================================================================
 * File: perf_run.cpp
 * Brief: Unattended performance run. See perf_run.h.
 *
 *  Drives the demos through the console command buffer rather than calling into the client
 *  directly, as map_cycle drives maps: the sequence exercised is then byte for byte the one a
 *  player produces.
 *
 *  Ending the run is the part that needs care. The attract loop never ends: when a demo runs
 *  out, CL_NextDemo plays the next one and wraps from demo3 back to demo1. The run takes the
 *  loop out of the picture (cls.demonum = -1, as "playdemo" from the console does on a bad
 *  file) and plays each demo itself, so a demo running out - CL_StopPlayback clearing
 *  cls.demoplayback - is that demo finishing, which is what the state machine watches.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/common.h"
#include "ps2/tests/perf_run.h"
#if PS2_QUAKE_PROFILE
#include "ps2/renderer/profile.h"
#include "ps2/system/sys.h"
#include "ps2/tests/test_quit.h"
#endif // PS2_QUAKE_PROFILE

namespace ps2::test {
namespace {

// Archived, because a console has no command line: the way to arm a run before the first frame
// is a line in config.cfg, and the game rewrites that file from the archived cvars on the way
// out. Without the flag the run's own quit would erase the line that started it.
static cvar_t s_enabled = ps2::MakeCvar("ps2_perftest", "0", CVAR_ARCHIVE);

#if PS2_QUAKE_PROFILE
// The demos the attract loop cycles through (quake.rc's startdemos), in its order.
constexpr const char * kDemos[] = { "demo1", "demo2", "demo3" };

// A demo that never comes up, or never ends, is a failed run rather than a reason to sit there
// forever. Both are far past anything the stock demos take. The load allowance is a console's:
// over USB a level load took from 78 s to over two minutes on the Quake II port.
constexpr int kLoadTimeoutMs = 240 * 1000;
constexpr int kPlayTimeoutMs = 10 * 60 * 1000;

enum class State
{
    Idle,     // Cvars not applied yet; set them and stop what is playing.
    Stopping, // Waiting for the attract loop's demo to go away.
    Loading,  // playdemo issued, waiting for the demo to start.
    Playing   // Demo running - the part being measured.
};

// Frames between disconnects while waiting for the attract loop to stop.
constexpr int kStopRetryFrames = 30;

static State s_state       = State::Idle;
static int   s_stopFrames  = 0;
static bool  s_done        = false;
static int   s_nextDemo    = 0;
static int   s_issuedAtMs  = 0;
static int   s_startedAtMs = 0;
static int   s_failed      = 0;

// What a capture turns off, and the values they had before it, put back when it ends, ahead of any
// quit: the on-screen debug panels are archived, so a run leaves config.cfg as it found it, and
// the game stays as it was if the run leaves it up (ps2_test_quit 0). developer 0 keeps
// Con_DPrintf out of the run: each line is a round trip to the IOP, a spike in whatever frame it
// lands in.
constexpr const char * kCaptureCvars[] = {
    "developer", "ps2_show_fps", "ps2_show_memstats", "ps2_show_vramstats", "ps2_show_drawstats", "ps2_show_profile"
};
static char s_savedValues[ps2::ArrayLength(kCaptureCvars)][MAX_QPATH] = {};

void StartDemo()
{
    Con_Printf("PerfRun: [%d/%d] playing %s\n", s_nextDemo + 1, ps2::ArrayLength(kDemos), kDemos[s_nextDemo]);

    Cbuf_AddText(va("playdemo %s\n", kDemos[s_nextDemo]));

    s_issuedAtMs = ps2::sys::Milliseconds();
    s_state      = State::Loading;
}

// Moves past the demo just finished, or ends the run if that was the last one.
void NextDemoOrFinish()
{
    ++s_nextDemo;
    if (s_nextDemo < ps2::ArrayLength(kDemos))
    {
        StartDemo();
        return;
    }

    Con_Printf("PerfRun: complete - %d of %d demos played, %d timed out.\n",
               ps2::ArrayLength(kDemos) - s_failed, ps2::ArrayLength(kDemos), s_failed);

    EndPerfCapture();

    // EndTest disarms it before quitting, so the config the quit writes has it back at 0. A run
    // that never reaches here (a crash, or the emulator being closed) deliberately stays armed.
    if (EndTest("PerfRun", s_enabled.name))
    {
        s_done = true;
        return;
    }

    // Staying up for another test.
    s_state      = State::Idle;
    s_stopFrames = 0;
    s_nextDemo   = 0;
    s_failed     = 0;
}
#endif // PS2_QUAKE_PROFILE

} // namespace

void RegisterPerfTestCvar()
{
    Cvar_RegisterVariable(&s_enabled);
}

#if PS2_QUAKE_PROFILE
void BeginPerfCapture()
{
    // Set directly rather than through the command buffer: they have to be in effect before
    // the first measured frame. developer goes to 0 and every on-screen debug panel off (see
    // kCaptureCvars): the panels cost EE time of their own, and the frame log carries every
    // number they show. EndPerfCapture puts them back.
    for (int i = 0; i < ps2::ArrayLength(kCaptureCvars); ++i)
    {
        q_strlcpy(s_savedValues[i], Cvar_VariableString(kCaptureCvars[i]), sizeof(s_savedValues[i]));
        Cvar_Set(kCaptureCvars[i], "0");
    }
    Cvar_Set("ps2_frame_log", "1");

    // Every file the run opens gets a line in the log, which is how a load mid-level names
    // itself. Only these runs ask for them: a map load opens hundreds of files, and each note
    // is a line the next dump sends through the IOP.
    ps2::debug::FrameLogNoteOpens(true);
}

void EndPerfCapture()
{
    // The frame log writes in batches, so the tail of the run is still buffered. This also marks
    // the end, which is what tells a completed capture apart from one the emulator cut short.
    ps2::debug::FrameLogFinish();

    // Nothing is measured from here on. The frame log goes off whatever it was before the run: a
    // second capture in the session turns it on again, and appends to the same log.
    ps2::debug::FrameLogNoteOpens(false);
    Cvar_Set("ps2_frame_log", "0");
    for (int i = 0; i < ps2::ArrayLength(kCaptureCvars); ++i)
    {
        Cvar_Set(kCaptureCvars[i], s_savedValues[i]);
    }
}

void RunPerfTest()
{
    if (s_enabled.value == 0.0f || s_done)
    {
        return;
    }

    switch (s_state)
    {
    case State::Idle:
        BeginPerfCapture();
        Con_Printf("PerfRun: starting - %d demos, developer 0, overlays off, frame log and file-open notes on.\n",
                   ps2::ArrayLength(kDemos));

        // Out of the attract loop, and whatever it is playing stopped, so the first demo starting
        // is an unambiguous signal rather than something already true.
        cls.demonum = -1;
        Cbuf_AddText("disconnect\n");
        s_issuedAtMs = ps2::sys::Milliseconds();
        s_state      = State::Stopping;
        break;

    case State::Stopping:
        if (!cls.demoplayback && !sv.active)
        {
            StartDemo();
        }
        else if ((ps2::sys::Milliseconds() - s_issuedAtMs) > kLoadTimeoutMs)
        {
            Con_Printf("PerfRun: the attract loop never stopped - starting anyway.\n");
            StartDemo();
        }
        else if (++s_stopFrames % kStopRetryFrames == 0)
        {
            // Armed from config.cfg, the run gets here before quake.rc's startdemos has played
            // anything, and the playdemo it queued starts a demo after the disconnect: stop that
            // one too.
            Cbuf_AddText("disconnect\n");
        }
        break;

    case State::Loading:
        if (cls.demoplayback)
        {
            s_startedAtMs = ps2::sys::Milliseconds();
            s_state       = State::Playing;
            break;
        }
        if ((ps2::sys::Milliseconds() - s_issuedAtMs) > kLoadTimeoutMs)
        {
            Con_Printf("PerfRun: '%s' never started after %d seconds - moving on.\n",
                       kDemos[s_nextDemo], kLoadTimeoutMs / 1000);
            ++s_failed;
            NextDemoOrFinish();
        }
        break;

    case State::Playing:
        // With the attract loop out of the way, the demo running out stops playback for good.
        if (!cls.demoplayback)
        {
            NextDemoOrFinish();
            break;
        }
        if ((ps2::sys::Milliseconds() - s_startedAtMs) > kPlayTimeoutMs)
        {
            Con_Printf("PerfRun: '%s' still running after %d minutes - moving on.\n",
                       kDemos[s_nextDemo], kPlayTimeoutMs / (60 * 1000));
            ++s_failed;
            NextDemoOrFinish();
        }
        break;
    }
}
#endif // PS2_QUAKE_PROFILE

} // namespace ps2::test
