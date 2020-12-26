// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Marco Zafra.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_emscripten.c
/// \brief Emscripten interface header.

#ifndef __I_EMSCRIPTEN__
#define __I_EMSCRIPTEN__

#include "../../doomtype.h"

////////////////////////////////////////////////////////////////////////
// Program API: Served by WASM program for access by JS.
// All APIs here are exported to JS on compile.
////////////////////////////////////////////////////////////////////////

// Main Loop
int Em_Program_Main(void);

#ifdef MAINLOOPBYFUNCTION
void Em_Program_Loop(void);
#endif

////////////////////////////////////////////////////////////////////////
// Shell API: Served by JS shell for access by WASM program
////////////////////////////////////////////////////////////////////////

// Main Loop
void Em_Shell_StartedMainLoopCallback(void);
boolean Em_Shell_GetTimingByRaf(void);

// Error Handling
void Em_Shell_InvalidateErrorChecks(void);

////////////////////////////////////////////////////////////////////////
// Internal API: Private methods for WASM logic
////////////////////////////////////////////////////////////////////////

// Main Loop
// \todo move this to a more generic header
#ifdef MAINLOOPBYFUNCTION
extern tic_t oldentertics;
extern tic_t rendertimeout;
void I_SetupMainLoop(void);
#endif

#endif // __I_EMSCRIPTEN__
