/* ================================================================================================
 * File: test_quit.cpp
 * Brief: How the unattended tests end. See test_quit.h.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/common.h"
#include "ps2/tests/test_quit.h"

#if PS2_QUAKE_DEBUG || PS2_QUAKE_PROFILE
namespace ps2::test {
namespace {

// Not archived: each boot starts with the tests quitting, as a run armed from config.cfg expects.
static cvar_t s_quitWhenDone = ps2::MakeCvar("ps2_test_quit", "1", CVAR_NONE);

} // namespace

void RegisterTestQuitCvar()
{
    Cvar_RegisterVariable(&s_quitWhenDone);
}

bool EndTest(const char * testName, const char * testCvarName)
{
    Cvar_Set(testCvarName, "0");

    if (s_quitWhenDone.value == 0.0f)
    {
        Con_Printf("%s: done, and ps2_test_quit is 0: the game stays up for the next test.\n", testName);
        return false;
    }

    // "quit" through the command buffer, so it runs at the top of the next frame, from where
    // Host_Shutdown takes everything down in the usual order.
    Con_Printf("%s: done - quitting now (ps2_test_quit 1).\n", testName);
    Cbuf_AddText("quit\n");
    return true;
}

} // namespace ps2::test
#endif // PS2_QUAKE_DEBUG || PS2_QUAKE_PROFILE
