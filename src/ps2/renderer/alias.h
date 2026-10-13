#pragma once
/* ================================================================================================
 * File: alias.h
 * Brief: Alias models (.mdl): the monsters, the items, the view weapon. QuakeSpasm's r_alias.c on
 *        the VU1 path, with gl_mesh.c's load-time half (GL_MakeAliasModelDisplayLists) and the
 *        player skins' colours (R_TranslatePlayerSkin).
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/common.h"
#include <tamtypes.h>

namespace ps2::alias {

// Capacity of the pool of per-model draw data. A qmodel_t has one at most, and gl_model.c reuses
// none until Mod_ResetAll, which frees them all first: so its MAX_MOD_KNOWN.
constexpr u32 kMaxModels = 512;

// Registers the alias model cvars and builds the shading tables. R_Init calls it.
void Init();

// What every alias model drawn this frame shares: the dynamic lights that are still alive. The
// view calls it once per frame, before it draws any entity.
void BeginFrame();

// Draws one alias model entity (R_DrawAliasModel): culled, its pose lerped and lit, with its
// fullbright texels over it and, with r_shadows, its shadow. 'viewModel' is the view weapon, whose
// depth is squeezed to the near end of the z-buffer so it never pokes into the walls.
void DrawAliasModel(entity_t & e, bool viewModel);

// Forgets the player skin textures, which TexMgr_NewGame has just freed (R_NewGame).
void NewGame();

// The most alias models with draw data at once this run, of kMaxModels: every one loaded since the
// last Mod_ResetAll. The map cycle reports it.
u32 PeakModelCount();

} // namespace ps2::alias
