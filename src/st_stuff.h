// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  st_stuff.h
/// \brief Status bar header

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

#include "doomtype.h"
#include "d_event.h"
#include "d_player.h"
#include "r_defs.h"
#include "g_input.h"

//
// STATUS BAR
//

// Called by main loop.
void ST_Ticker(boolean run);

// Called by main loop.
void ST_Drawer(void);

// Called when the console player is spawned on each level.
void ST_Start(void);

// Called by startup code.
void ST_Init(void);

// Called by G_Responder() when pressing F12 while viewing a demo.
void ST_changeDemoView(void);

void ST_UnloadGraphics(void);
void ST_LoadGraphics(void);

// face load graphics, called when skin changes
void ST_LoadFaceGraphics(INT32 playernum);
void ST_ReloadSkinFaceGraphics(void);

void ST_doPaletteStuff(void);

// title card
void ST_startTitleCard(void);
void ST_runTitleCard(void);
void ST_drawTitleCard(void);
void ST_preDrawTitleCard(void);
void ST_preLevelTitleCardDrawer(void);
void ST_drawWipeTitleCard(void);

// Touch screen inputs
#ifdef TOUCHINPUTS
void ST_drawTouchGameInput(touchconfig_t *config, boolean drawgamecontrols, INT32 alphalevel);
void ST_drawTouchMenuInput(void);
void ST_drawTouchDPad(
	fixed_t dpadx, fixed_t dpady, fixed_t dpadw, fixed_t dpadh,
	touchconfig_t *tleft, boolean moveleft,
	touchconfig_t *tright, boolean moveright,
	touchconfig_t *tup, boolean moveup,
	touchconfig_t *tdown, boolean movedown,
	boolean backing, INT32 flags, INT32 accent);
void ST_drawJoystickBacking(fixed_t padx, fixed_t pady, fixed_t padw, fixed_t padh, fixed_t scale, UINT8 color, INT32 flags);
void ST_drawTouchJoystick(fixed_t dpadx, fixed_t dpady, fixed_t dpadw, fixed_t dpadh, UINT8 color, INT32 flags);
#endif

extern tic_t lt_ticker, lt_lasttic;
extern tic_t lt_exitticker, lt_endtime;

// return if player a is in the same team as player b
boolean ST_SameTeam(player_t *a, player_t *b);

//--------------------
// status bar overlay
//--------------------

extern boolean st_overlay; // sb overlay on or off when fullscreen
extern INT32 st_palette; // 0 is default, any others are special palettes.
extern INT32 st_translucency;

extern lumpnum_t st_borderpatchnum;
// patches, also used in intermission
extern patch_t *tallnum[10];
extern patch_t *sboscore;
extern patch_t *sbotime;
extern patch_t *sbocolon;
extern patch_t *sboperiod;
extern patch_t *faceprefix[MAXSKINS]; // face status patches
extern patch_t *superprefix[MAXSKINS]; // super face status patches
extern patch_t *livesback;
extern patch_t *stlivex;
extern patch_t *ngradeletters[7];

/** HUD location information (don't move this comment)
  */
typedef struct
{
	INT32 x, y, f;
} hudinfo_t;

typedef enum
{
	HUD_LIVES,

	HUD_RINGS,
	HUD_RINGSNUM,
	HUD_RINGSNUMTICS,

	HUD_SCORE,
	HUD_SCORENUM,

	HUD_TIME,
	HUD_MINUTES,
	HUD_TIMECOLON,
	HUD_SECONDS,
	HUD_TIMETICCOLON,
	HUD_TICS,

	HUD_SS_TOTALRINGS,

	HUD_GETRINGS,
	HUD_GETRINGSNUM,
	HUD_TIMELEFT,
	HUD_TIMELEFTNUM,
	HUD_TIMEUP,
	HUD_HUNTPICS,
	HUD_POWERUPS,

	NUMHUDITEMS
} hudnum_t;
extern hudinfo_t hudinfo[NUMHUDITEMS];

#define ST_WEAPONS_X ((BASEVIDWIDTH / 2) - (NUM_WEAPONS * 10) - 6)
#define ST_WEAPONS_Y 176 // HUD_LIVES
#define ST_WEAPONS_W 20
#define ST_WEAPONS_H 20

extern UINT16 objectsdrawn;

#endif
