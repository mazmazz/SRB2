// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Marco Zafra.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_emscripten.c
/// \brief Emscripten interface code.

#include "i_emscripten.h"
#include "../../m_argv.h"
#include "../../doomdef.h"
#include "../../d_main.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

////////////////////////////////////////////////////////////////////////
// Error Handling
////////////////////////////////////////////////////////////////////////

void Em_Shell_InvalidateErrorChecks(void)
{
    EM_ASM({
        //Shell.InvalidateErrorChecks();
    });
}

////////////////////////////////////////////////////////////////////////
// Main Loop
////////////////////////////////////////////////////////////////////////

void Em_Shell_StartedMainLoopCallback(void)
{
    EM_ASM({
        //Shell.StartedMainLoopCallback();
    });
}

boolean Em_Shell_GetTimingByRaf(void) {
    boolean timingByRaf = EM_ASM_INT({
        //return TimingByRequestAnimationFrame;
        return false;
    });

    return timingByRaf;
}

#ifdef MAINLOOPBYFUNCTION
extern tic_t oldentertics;
extern tic_t rendertimeout;

void I_SetupMainLoop(void)
{
    boolean timingByRaf;
    
    // Setup persistent variables for D_SRB2LoopIter()
    oldentertics = 0;
    rendertimeout = INFTICS;

    timingByRaf = Em_Shell_GetTimingByRaf();

    Em_Shell_StartedMainLoopCallback();
    
    if (timingByRaf)
        // Timing done by requestAnimationFrame, which fires whenever possible.
        // we do a timing check in main loop to not exceed NEWTICRATE
        emscripten_set_main_loop(Em_Program_Loop, 0, 0);
    else
        // Timing done by setTimeout. Enforce NEWTICRATE to appease error handler.
        emscripten_set_main_loop(Em_Program_Loop, 1000/NEWTICRATE, 0);
}

void Em_Program_Loop(void)
{
    Em_Shell_InvalidateErrorChecks();
    D_SRB2LoopIter();
}
#endif

////////////////////////////////////////////////////////////////////////
// Entry Point
////////////////////////////////////////////////////////////////////////

static void Em_SyncFSOnEntry(void)
{
    EM_ASM(
        try
		{
			// Make a directory other than '/'
			// FS.mkdir('/user');
			// Then mount with IDBFS type
			FS.mount(IDBFS, {}, '/home/web_user');

			// Then sync
			FS.syncfs(true, function (err) {
				console.log("Intial syncFS done");
				console.log(err);
				Module.ccall("Em_Program_Main", 'number', [], []);
        	});
		} 
		catch (err)
		{
			// May have already mounted during preRun, so fail silently
			console.log(err);
			Module.ccall("Em_Program_Main", 'number', [], []);
		}
    );
}

int main(int argc, char **argv)
{
    myargc = argc;
	myargv = argv;

    Em_SyncFSOnEntry();

	return 0;
}
