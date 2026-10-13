/* ================================================================================================
 * File: debug_menu.cpp
 * Brief: The Options menu's "Debug Options" page: the backend's debugging aids on the gamepad, for
 *        a console with no keyboard to type them at the Quake console. The log file, the IOP and
 *        save reports, a dump of the console to a file, a screenshot, and the perf test and the
 *        map cycle in the builds that have them. menu.c opens it from its Options list and hands it its frames
 *        and keys (engine_hooks.h); it draws the way QuakeSpasm's own pages do.
 *
 *  Each row does what a console command or cvar does, so the page adds nothing a keyboard
 *  couldn't reach: it only puts it under the gamepad. A row that prints a report opens the
 *  console on it as well, where L2 and R2 scroll (input.cpp).
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/common.h"
#include "ps2/engine_hooks.h"
#include "ps2/system/iop_boot.h"

#include <cstdio>
#include <cstring>

namespace {

// A row: an off/on checkbox showing a cvar's value, a command to run, or a gap between groups.
struct Row
{
    const char * label;
    const char * help;         // under the rows while the cursor is on this one; 40 columns at most
    const char * cvar;         // a checkbox's cvar, toggled by Enter or left/right
    void (*action)();          // what Enter does on any other row
};

// Puts the console up, for a report just printed there, as Options' "Goto console" does. It goes
// over a demo or a game; in single player the game stands still while it is up.
void ShowConsole()
{
    m_state = m_none;
    Con_ToggleConsole_f();
}

void DumpIopModules()
{
    // Printed first, so it is all on the console when it opens.
    Cmd_ExecuteString("ps2_dump_iop_mods", src_command);
    ShowConsole();
}

void ListSaves()
{
    Cmd_ExecuteString("ps2_saveinfo", src_command);
    ShowConsole();
}

// What the last row picked did, under the rows, for the rows that stay on the page. Empty for none.
// Room for "couldn't write " and the path DumpConsole writes.
static char s_feedback[sizeof("couldn't write ") + sizeof(com_gamedir) + sizeof("/condump.txt")];

// QuakeSpasm's condump, then the HDD's metadata flush every file the game writes there needs (see
// SyncGameDataDevice). condump reports a failed open only on the console, so the file is looked
// for afterwards to say here which it was.
void DumpConsole()
{
    Cmd_ExecuteString("condump", src_command);
    ps2::sys::SyncGameDataDevice();

    char path[sizeof(com_gamedir) + sizeof("/condump.txt")];
    std::snprintf(path, sizeof(path), "%s/condump.txt", com_gamedir);

    std::FILE * const file = std::fopen(path, "rb");
    if (file != nullptr)
    {
        std::fclose(file);
    }
    std::snprintf(s_feedback, sizeof(s_feedback), "%s %s", (file != nullptr) ? "wrote" : "couldn't write", path);
}

// Leaves the menu and takes a screenshot a frame later, of what the menu covered: "wait" holds the
// command until the next frame, and the frame drawn in between has no menu on it.
void TakeScreenshot()
{
    IN_Activate();
    key_dest = key_game;
    m_state  = m_none;
    Cbuf_AddText("wait\nscreenshot\n");
}

#if PS2_QUAKE_PROFILE || PS2_QUAKE_DEBUG
// Starts a test that drives the game through the command buffer by its cvar, and leaves the menu
// to let it. The two would fight over the server (perf_run.h), so neither starts while the other
// runs.
void StartTest(const char * cvarName)
{
    if (Cvar_VariableValue("ps2_perftest") != 0.0f || Cvar_VariableValue("ps2_testmaps") != 0.0f)
    {
        std::snprintf(s_feedback, sizeof(s_feedback), "a test is running already");
        return;
    }

    Cvar_Set(cvarName, "1");
    IN_Activate();
    key_dest = key_game;
    m_state  = m_none;
}
#endif // PS2_QUAKE_PROFILE || PS2_QUAKE_DEBUG

#if PS2_QUAKE_PROFILE
void StartPerfTest()
{
    StartTest("ps2_perftest");
}
#endif // PS2_QUAKE_PROFILE

#if PS2_QUAKE_DEBUG
void StartMapCycle()
{
    StartTest("ps2_testmaps");
}
#endif // PS2_QUAKE_DEBUG

const Row kRows[] = {
    { "Log to file",          "the console to quake.log; slow on USB", "ps2_logfile", nullptr        },
    { "Dump IOP modules",     "boot report, modules, IOP memory",      nullptr,       DumpIopModules },
    { "List saves",           "where saves go, and the card's saves",  nullptr,       ListSaves      },
    { "Save console to file", "the console's text to condump.txt",     nullptr,       DumpConsole    },
    { "Take screenshot",      "the screen without this menu, in id1/", nullptr,       TakeScreenshot },
#if PS2_QUAKE_PROFILE || PS2_QUAKE_DEBUG
    { nullptr,                nullptr,                                 nullptr,       nullptr        },
    { "Quit after a test",    "off: stay up for the next test",        "ps2_test_quit", nullptr      },
#endif // PS2_QUAKE_PROFILE || PS2_QUAKE_DEBUG
#if PS2_QUAKE_PROFILE
    { "Start perf test",      "the demos, with the frame log on",      nullptr,       StartPerfTest  },
#endif // PS2_QUAKE_PROFILE
#if PS2_QUAKE_DEBUG
    { "Start map cycle",      "every level, and what each one takes",  nullptr,       StartMapCycle  },
#endif // PS2_QUAKE_DEBUG
};

constexpr int kNumRows = ps2::ArrayLength(kRows);

// The rows sit where Options puts its own: 8 units apart from y 32, labels ending at column 22,
// the cursor at x 200 and the checkboxes at 220.
constexpr int kRowsY        = 32;
constexpr int kLabelX       = 16;
constexpr int kLabelColumns = 22;
constexpr int kCursorX      = 200;
constexpr int kCheckboxX    = 220;

static int s_cursor = 0;

bool IsGap(const int row)
{
    return kRows[row].label == nullptr;
}

// Moves the cursor a row up or down, wrapping, past the gaps.
void MoveCursor(const int step)
{
    do
    {
        s_cursor = (s_cursor + step + kNumRows) % kNumRows;
    } while (IsGap(s_cursor));
}

void ToggleCvar(const char * cvarName)
{
    Cvar_SetValue(cvarName, (Cvar_VariableValue(cvarName) != 0.0f) ? 0.0f : 1.0f);
    s_feedback[0] = '\0';
}

} // namespace

// ------------------------------------------------------------------------------------------------
// engine_hooks.h
// ------------------------------------------------------------------------------------------------

void PS2_DebugMenuOpen()
{
    key_dest      = key_menu;
    m_state       = m_ps2debug;
    m_entersound  = true;
    s_feedback[0] = '\0';
}

void PS2_DebugMenuDraw()
{
    // There is no debug banner among the game's pictures: the page sits under the Options one.
    M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
    qpic_t * const banner = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - banner->width) / 2, 4, banner);

    for (int i = 0; i < kNumRows; ++i)
    {
        const Row & row = kRows[i];
        if (IsGap(i))
        {
            continue;
        }

        const int y = kRowsY + (8 * i);
        M_Print(kLabelX + ((kLabelColumns - static_cast<int>(std::strlen(row.label))) * 8), y, row.label);
        if (row.cvar != nullptr)
        {
            M_DrawCheckbox(kCheckboxX, y, Cvar_VariableValue(row.cvar) != 0.0f);
        }
    }

    M_DrawCharacter(kCursorX, kRowsY + (8 * s_cursor), 12 + (static_cast<int>(realtime * 4.0) & 1));

    // What the row under the cursor does, centred under the rows, then what the last one did.
    const char * const help = kRows[s_cursor].help;
    const int helpY = kRowsY + (8 * (kNumRows + 1));
    M_Print((320 - (static_cast<int>(std::strlen(help)) * 8)) / 2, helpY, help);
    if (s_feedback[0] != '\0')
    {
        M_PrintWhite((320 - (static_cast<int>(std::strlen(s_feedback)) * 8)) / 2, helpY + 16, s_feedback);
    }
}

void PS2_DebugMenuKey(const int key)
{
    switch (key)
    {
    case K_ESCAPE:
    case K_BBUTTON:
        M_Menu_Options_f();
        break;

    case K_UPARROW:
        S_LocalSound("misc/menu1.wav");
        MoveCursor(-1);
        break;

    case K_DOWNARROW:
        S_LocalSound("misc/menu1.wav");
        MoveCursor(1);
        break;

    case K_LEFTARROW:
    case K_RIGHTARROW:
        if (kRows[s_cursor].cvar != nullptr)
        {
            S_LocalSound("misc/menu3.wav");
            ToggleCvar(kRows[s_cursor].cvar);
        }
        break;

    case K_ENTER:
    case K_KP_ENTER:
    case K_ABUTTON:
        m_entersound = true;
        if (kRows[s_cursor].cvar != nullptr)
        {
            ToggleCvar(kRows[s_cursor].cvar);
        }
        else if (kRows[s_cursor].action != nullptr)
        {
            kRows[s_cursor].action();
        }
        break;

    default:
        break;
    }
}
