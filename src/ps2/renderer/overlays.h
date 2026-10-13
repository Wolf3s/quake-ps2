#pragma once
/* ================================================================================================
 * File: overlays.h
 * Brief: The backend's on-screen debug overlays: the FPS counter and the profile, memory, VRAM
 *        and draw statistics panels, each behind its own cvar.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/common.h"

namespace ps2::overlay {

// Registers the overlay cvars. Call once, during Host_Init.
void Init();

// Draws whichever overlays are on, over everything else in the frame, in screen pixels. Call
// last thing before the frame is submitted (GL_EndRendering).
void Draw();

// The FPS counter's switch (ps2_show_fps) and the panels' master switch (ps2_debug_overlays), for
// the Video Options page's rows (vid.cpp).
cvar_t & FpsCounterCvar();
cvar_t & DebugOverlaysCvar();

} // namespace ps2::overlay
