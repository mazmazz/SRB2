// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  enumtables.h
/// \brief Enumeration string tables for dehacked.

#include "d_think.h"

extern const char *const MOBJFLAG_LIST[];

extern const char *const MOBJFLAG2_LIST[];

extern const char *const MOBJEFLAG_LIST[];

extern const char *const PLAYERFLAG_LIST[];

extern const char *const GAMETYPERULE_LIST[];

extern const char *const POWERS_LIST[];

extern const char *const HUDITEMS_LIST[];

extern const char *const MENUTYPES_LIST[];

extern const char *const STATE_LIST[];

extern const char *const MOBJTYPE_LIST[];

/** Action pointer for reading actions from Dehacked lumps.
  */
typedef struct
{
	actionf_t action; ///< Function pointer corresponding to the actual action.
	const char *name; ///< Name of the action in ALL CAPS.
} actionpointer_t;

extern actionpointer_t actionpointers[];
