// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  dehacked.c
/// \brief Load dehacked file and change tables and text

#include "doomdef.h"
#include "d_main.h" // for srb2home
#include "g_game.h"
#include "sounds.h"
#include "info.h"
#include "d_think.h"
#include "m_argv.h"
#include "z_zone.h"
#include "w_wad.h"
#include "m_menu.h"
#include "m_misc.h"
#include "f_finale.h"
#include "y_inter.h"
#include "dehacked.h"
#include "st_stuff.h"
#include "i_system.h"
#include "p_local.h" // for var1 and var2, and some constants
#include "p_setup.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_patch.h"
#include "r_things.h" // R_Char2Frame
#include "r_sky.h"
#include "fastcmp.h"
#include "lua_script.h"
#include "lua_hook.h"
#include "d_clisrv.h"

#include "m_cond.h"

#include "v_video.h" // video flags (for lua)

#include "enumtables.h"

#ifdef HWRENDER
#include "hardware/hw_light.h"
#endif

#ifdef PC_DOS
#include <stdio.h> // for snprintf
//int	snprintf(char *str, size_t n, const char *fmt, ...);
int	vsnprintf(char *str, size_t n, const char *fmt, va_list ap);
#endif

// Free slot names
// The crazy word-reading stuff uses these.
static char *FREE_STATES[NUMSTATEFREESLOTS];
static char *FREE_MOBJS[NUMMOBJFREESLOTS];
static UINT8 used_spr[(NUMSPRITEFREESLOTS / 8) + 1]; // Bitwise flag for sprite freeslot in use! I would use ceil() here if I could, but it only saves 1 byte of memory anyway.
#define initfreeslots() {\
memset(FREE_STATES,0,sizeof(char *) * NUMSTATEFREESLOTS);\
memset(FREE_MOBJS,0,sizeof(char *) * NUMMOBJFREESLOTS);\
memset(used_spr,0,sizeof(UINT8) * ((NUMSPRITEFREESLOTS / 8) + 1));\
}

// Crazy word-reading stuff
/// \todo Put these in a seperate file or something.
static mobjtype_t get_mobjtype(const char *word);
static statenum_t get_state(const char *word);
static spritenum_t get_sprite(const char *word);
static playersprite_t get_sprite2(const char *word);
static sfxenum_t get_sfx(const char *word);
#ifdef MUSICSLOT_COMPATIBILITY
static UINT16 get_mus(const char *word, UINT8 dehacked_mode);
#endif
static hudnum_t get_huditem(const char *word);
static menutype_t get_menutype(const char *word);
//static INT16 get_gametype(const char *word);
//static powertype_t get_power(const char *word);

boolean deh_loaded = false;
static int dbg_line;

static boolean gamedataadded = false;
static boolean titlechanged = false;
static boolean introchanged = false;

ATTRINLINE static FUNCINLINE char myfget_color(MYFILE *f)
{
	char c = *f->curpos++;
	if (c == '^') // oh, nevermind then.
		return '^';

	if (c >= '0' && c <= '9')
		return 0x80+(c-'0');
	return 0x80; // Unhandled -- default to no color
}

ATTRINLINE static FUNCINLINE char myfget_hex(MYFILE *f)
{
	char c = *f->curpos++, endchr = 0;
	if (c == '\\') // oh, nevermind then.
		return '\\';

	if (c >= '0' && c <= '9')
		endchr += (c-'0') << 4;
	else if (c >= 'A' && c <= 'F')
		endchr += ((c-'A') + 10) << 4;
	else if (c >= 'a' && c <= 'f')
		endchr += ((c-'a') + 10) << 4;
	else // invalid. stop and return a question mark.
		return '?';

	c = *f->curpos++;
	if (c >= '0' && c <= '9')
		endchr += (c-'0');
	else if (c >= 'A' && c <= 'F')
		endchr += ((c-'A') + 10);
	else if (c >= 'a' && c <= 'f')
		endchr += ((c-'a') + 10);
	else // invalid. stop and return a question mark.
		return '?';

	return endchr;
}

char *myfgets(char *buf, size_t bufsize, MYFILE *f)
{
	size_t i = 0;
	if (myfeof(f))
		return NULL;
	// we need one byte for a null terminated string
	bufsize--;
	while (i < bufsize && !myfeof(f))
	{
		char c = *f->curpos++;
		if (c == '^')
			buf[i++] = myfget_color(f);
		else if (c == '\\')
			buf[i++] = myfget_hex(f);
		else if (c != '\r')
			buf[i++] = c;
		if (c == '\n')
			break;
	}
	buf[i] = '\0';

	dbg_line++;
	return buf;
}

static char *myhashfgets(char *buf, size_t bufsize, MYFILE *f)
{
	size_t i = 0;
	if (myfeof(f))
		return NULL;
	// we need one byte for a null terminated string
	bufsize--;
	while (i < bufsize && !myfeof(f))
	{
		char c = *f->curpos++;
		if (c == '^')
			buf[i++] = myfget_color(f);
		else if (c == '\\')
			buf[i++] = myfget_hex(f);
		else if (c != '\r')
			buf[i++] = c;
		if (c == '\n') // Ensure debug line is right...
			dbg_line++;
		if (c == '#')
		{
			if (i > 0) // don't let i wrap past 0
				i--; // don't include hash char in string
			break;
		}
	}
	if (buf[i] != '#') // don't include hash char in string
		i++;
	buf[i] = '\0';

	return buf;
}

static INT32 deh_num_warning = 0;

FUNCPRINTF static void deh_warning(const char *first, ...)
{
	va_list argptr;
	char *buf = Z_Malloc(1000, PU_STATIC, NULL);

	va_start(argptr, first);
	vsnprintf(buf, 1000, first, argptr); // sizeof only returned 4 here. it didn't like that pointer.
	va_end(argptr);

	if(dbg_line == -1) // Not in a SOC, line number unknown.
		CONS_Alert(CONS_WARNING, "%s\n", buf);
	else
		CONS_Alert(CONS_WARNING, "Line %u: %s\n", dbg_line, buf);

	deh_num_warning++;

	Z_Free(buf);
}

static void deh_strlcpy(char *dst, const char *src, size_t size, const char *warntext)
{
	size_t len = strlen(src)+1; // Used to determine if truncation has been done
	if (len > size)
		deh_warning("%s exceeds max length of %s", warntext, sizeu1(size-1));
	strlcpy(dst, src, size);
}

/* ======================================================================== */
// Load a dehacked file format
/* ======================================================================== */
/* a sample to see
                   Thing 1 (Player)       {           // MT_PLAYER
INT32 doomednum;     ID # = 3232              -1,             // doomednum
INT32 spawnstate;    Initial frame = 32       "PLAY",         // spawnstate
INT32 spawnhealth;   Hit points = 3232        100,            // spawnhealth
INT32 seestate;      First moving frame = 32  "PLAY_RUN1",    // seestate
INT32 seesound;      Alert sound = 32         sfx_None,       // seesound
INT32 reactiontime;  Reaction time = 3232     0,              // reactiontime
INT32 attacksound;   Attack sound = 32        sfx_None,       // attacksound
INT32 painstate;     Injury frame = 32        "PLAY_PAIN",    // painstate
INT32 painchance;    Pain chance = 3232       255,            // painchance
INT32 painsound;     Pain sound = 32          sfx_plpain,     // painsound
INT32 meleestate;    Close attack frame = 32  "NULL",         // meleestate
INT32 missilestate;  Far attack frame = 32    "PLAY_ATK1",    // missilestate
INT32 deathstate;    Death frame = 32         "PLAY_DIE1",    // deathstate
INT32 xdeathstate;   Exploding frame = 32     "PLAY_XDIE1",   // xdeathstate
INT32 deathsound;    Death sound = 32         sfx_pldeth,     // deathsound
INT32 speed;         Speed = 3232             0,              // speed
INT32 radius;        Width = 211812352        16*FRACUNIT,    // radius
INT32 height;        Height = 211812352       56*FRACUNIT,    // height
INT32 dispoffset;    DispOffset = 0           0,              // dispoffset
INT32 mass;          Mass = 3232              100,            // mass
INT32 damage;        Missile damage = 3232    0,              // damage
INT32 activesound;   Action sound = 32        sfx_None,       // activesound
INT32 flags;         Bits = 3232              MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH,
INT32 raisestate;    Respawn frame = 32       S_NULL          // raisestate
                                         }, */

#ifdef HWRENDER
static INT32 searchvalue(const char *s)
{
	while (s[0] != '=' && s[0])
		s++;
	if (s[0] == '=')
		return atoi(&s[1]);
	else
	{
		deh_warning("No value found");
		return 0;
	}
}

static float searchfvalue(const char *s)
{
	while (s[0] != '=' && s[0])
		s++;
	if (s[0] == '=')
		return (float)atof(&s[1]);
	else
	{
		deh_warning("No value found");
		return 0;
	}
}
#endif

// These are for clearing all of various things
static void clear_conditionsets(void)
{
	UINT8 i;
	for (i = 0; i < MAXCONDITIONSETS; ++i)
		M_ClearConditionSet(i+1);
}

static void clear_levels(void)
{
	INT16 i;

	// This is potentially dangerous but if we're resetting these headers,
	// we may as well try to save some memory, right?
	for (i = 0; i < NUMMAPS; ++i)
	{
		if (!mapheaderinfo[i] || i == (tutorialmap-1))
			continue;

		// Custom map header info
		// (no need to set num to 0, we're freeing the entire header shortly)
		Z_Free(mapheaderinfo[i]->customopts);

		P_DeleteFlickies(i);
		P_DeleteGrades(i);

		Z_Free(mapheaderinfo[i]);
		mapheaderinfo[i] = NULL;
	}

	// Realloc the one for the current gamemap as a safeguard
	P_AllocMapHeader(gamemap-1);
}

static boolean findFreeSlot(INT32 *num)
{
	// Send the character select entry to a free slot.
	while (*num < MAXSKINS && (description[*num].used))
		*num = *num+1;

	// No more free slots. :(
	if (*num >= MAXSKINS)
		return false;

	// Redesign your logo. (See M_DrawSetupChoosePlayerMenu in m_menu.c...)
	description[*num].picname[0] = '\0';
	description[*num].nametag[0] = '\0';
	description[*num].displayname[0] = '\0';
	description[*num].oppositecolor = SKINCOLOR_NONE;
	description[*num].tagtextcolor = SKINCOLOR_NONE;
	description[*num].tagoutlinecolor = SKINCOLOR_NONE;

	// Found one! ^_^
	return (description[*num].used = true);
}

// Reads a player.
// For modifying the character select screen
static void readPlayer(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *displayname = ZZ_Alloc(MAXLINELEN+1);
	INT32 i;
	boolean slotfound = false;

	#define SLOTFOUND \
		if (!slotfound && (slotfound = findFreeSlot(&num)) == false) \
			goto done;

	displayname[MAXLINELEN] = '\0';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			for (i = 0; i < MAXLINELEN-3; i++)
			{
				char *tmp;
				if (s[i] == '=')
				{
					tmp = &s[i+2];
					strncpy(displayname, tmp, SKINNAMESIZE);
					break;
				}
			}

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "PLAYERTEXT"))
			{
				char *playertext = NULL;

				SLOTFOUND

				for (i = 0; i < MAXLINELEN-3; i++)
				{
					if (s[i] == '=')
					{
						playertext = &s[i+2];
						break;
					}
				}
				if (playertext)
				{
					strcpy(description[num].notes, playertext);
					strcat(description[num].notes, myhashfgets(playertext, sizeof (description[num].notes), f));
				}
				else
					strcpy(description[num].notes, "");

				// For some reason, cutting the string did not work above. Most likely due to strcpy or strcat...
				// It works down here, though.
				{
					INT32 numline = 0;
					for (i = 0; i < MAXLINELEN-1; i++)
					{
						if (numline < 20 && description[num].notes[i] == '\n')
							numline++;

						if (numline >= 20 || description[num].notes[i] == '\0' || description[num].notes[i] == '#')
							break;
					}
				}
				description[num].notes[strlen(description[num].notes)-1] = '\0';
				description[num].notes[i] = '\0';
				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);

			if (fastcmp(word, "PICNAME"))
			{
				SLOTFOUND
				strncpy(description[num].picname, word2, 8);
			}
			// new character select
			else if (fastcmp(word, "DISPLAYNAME"))
			{
				SLOTFOUND
				// replace '#' with line breaks
				// (also remove any '\n')
				{
					char *cur = NULL;

					// remove '\n'
					cur = strchr(displayname, '\n');
					if (cur)
						*cur = '\0';

					// turn '#' into '\n'
					cur = strchr(displayname, '#');
					while (cur)
					{
						*cur = '\n';
						cur = strchr(cur, '#');
					}
				}
				// copy final string
				strncpy(description[num].displayname, displayname, SKINNAMESIZE);
			}
			else if (fastcmp(word, "OPPOSITECOLOR") || fastcmp(word, "OPPOSITECOLOUR"))
			{
				SLOTFOUND
				description[num].oppositecolor = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "NAMETAG") || fastcmp(word, "TAGNAME"))
			{
				SLOTFOUND
				strncpy(description[num].nametag, word2, 8);
			}
			else if (fastcmp(word, "TAGTEXTCOLOR") || fastcmp(word, "TAGTEXTCOLOUR"))
			{
				SLOTFOUND
				description[num].tagtextcolor = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "TAGOUTLINECOLOR") || fastcmp(word, "TAGOUTLINECOLOUR"))
			{
				SLOTFOUND
				description[num].tagoutlinecolor = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "STATUS"))
			{
				/*
					You MAY disable previous entries if you so desire...
					But try to enable something that's already enabled and you will be sent to a free slot.

					Because of this, you are allowed to edit any previous entries you like, but only if you
					signal that you are purposely doing so by disabling and then reenabling the slot.
				*/
				if (i && !slotfound && (slotfound = findFreeSlot(&num)) == false)
					goto done;

				description[num].used = (!!i);
			}
			else if (fastcmp(word, "SKINNAME"))
			{
				// Send to free slot.
				SLOTFOUND
				strlcpy(description[num].skinname, word2, sizeof description[num].skinname);
				strlwr(description[num].skinname);
			}
			else
				deh_warning("readPlayer %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty
	#undef SLOTFOUND
done:
	Z_Free(displayname);
	Z_Free(s);
}

// TODO: Figure out how to do undolines for this....
// TODO: Warnings for running out of freeslots
static void readfreeslots(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word,*type;
	char *tmp;
	int i;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			type = strtok(s, "_");
			if (type)
				strupr(type);
			else
				break;

			word = strtok(NULL, "\n");
			if (word)
				strupr(word);
			else
				break;

			// TODO: Check for existing freeslot mobjs/states/etc. and make errors.
			// TODO: Out-of-slots warnings/errors.
			// TODO: Name too long (truncated) warnings.
			if (fastcmp(type, "SFX"))
				S_AddSoundFx(word, false, 0, false);
			else if (fastcmp(type, "SPR"))
			{
				for (i = SPR_FIRSTFREESLOT; i <= SPR_LASTFREESLOT; i++)
				{
					if (used_spr[(i-SPR_FIRSTFREESLOT)/8] & (1<<(i%8)))
					{
						if (!sprnames[i][4] && memcmp(sprnames[i],word,4)==0)
							sprnames[i][4] = (char)f->wad;
						continue; // Already allocated, next.
					}
					// Found a free slot!
					strncpy(sprnames[i],word,4);
					//sprnames[i][4] = 0;
					used_spr[(i-SPR_FIRSTFREESLOT)/8] |= 1<<(i%8); // Okay, this sprite slot has been named now.
					break;
				}
			}
			else if (fastcmp(type, "S"))
			{
				for (i = 0; i < NUMSTATEFREESLOTS; i++)
					if (!FREE_STATES[i]) {
						FREE_STATES[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
						strcpy(FREE_STATES[i],word);
						break;
					}
			}
			else if (fastcmp(type, "MT"))
			{
				for (i = 0; i < NUMMOBJFREESLOTS; i++)
					if (!FREE_MOBJS[i]) {
						FREE_MOBJS[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
						strcpy(FREE_MOBJS[i],word);
						break;
					}
			}
			else if (fastcmp(type, "SPR2"))
			{
				// Search if we already have an SPR2 by that name...
				for (i = SPR2_FIRSTFREESLOT; i < (int)free_spr2; i++)
					if (memcmp(spr2names[i],word,4) == 0)
						break;
				// We found it? (Two mods using the same SPR2 name?) Then don't allocate another one.
				if (i < (int)free_spr2)
					continue;
				// Copy in the spr2 name and increment free_spr2.
				if (free_spr2 < NUMPLAYERSPRITES) {
					strncpy(spr2names[free_spr2],word,4);
					spr2defaults[free_spr2] = 0;
					spr2names[free_spr2++][4] = 0;
				} else
					deh_warning("Ran out of free SPR2 slots!\n");
			}
			else if (fastcmp(type, "TOL"))
			{
				// Search if we already have a typeoflevel by that name...
				for (i = 0; TYPEOFLEVEL[i].name; i++)
					if (fastcmp(word, TYPEOFLEVEL[i].name))
						break;

				// We found it? Then don't allocate another one.
				if (TYPEOFLEVEL[i].name)
					continue;

				// We don't, so freeslot it.
				if (lastcustomtol == (UINT32)MAXTOL) // Unless you have way too many, since they're flags.
					deh_warning("Ran out of free typeoflevel slots!\n");
				else
				{
					G_AddTOL(lastcustomtol, word);
					lastcustomtol <<= 1;
				}
			}
			else
				deh_warning("Freeslots: unknown enum class '%s' for '%s_%s'", type, type, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readthing(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word, "MAPTHINGNUM") || fastcmp(word, "DOOMEDNUM"))
			{
				mobjinfo[num].doomednum = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "SPAWNSTATE"))
			{
				mobjinfo[num].spawnstate = get_number(word2);
			}
			else if (fastcmp(word, "SPAWNHEALTH"))
			{
				mobjinfo[num].spawnhealth = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "SEESTATE"))
			{
				mobjinfo[num].seestate = get_number(word2);
			}
			else if (fastcmp(word, "SEESOUND"))
			{
				mobjinfo[num].seesound = get_number(word2);
			}
			else if (fastcmp(word, "REACTIONTIME"))
			{
				mobjinfo[num].reactiontime = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "ATTACKSOUND"))
			{
				mobjinfo[num].attacksound = get_number(word2);
			}
			else if (fastcmp(word, "PAINSTATE"))
			{
				mobjinfo[num].painstate = get_number(word2);
			}
			else if (fastcmp(word, "PAINCHANCE"))
			{
				mobjinfo[num].painchance = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "PAINSOUND"))
			{
				mobjinfo[num].painsound = get_number(word2);
			}
			else if (fastcmp(word, "MELEESTATE"))
			{
				mobjinfo[num].meleestate = get_number(word2);
			}
			else if (fastcmp(word, "MISSILESTATE"))
			{
				mobjinfo[num].missilestate = get_number(word2);
			}
			else if (fastcmp(word, "DEATHSTATE"))
			{
				mobjinfo[num].deathstate = get_number(word2);
			}
			else if (fastcmp(word, "DEATHSOUND"))
			{
				mobjinfo[num].deathsound = get_number(word2);
			}
			else if (fastcmp(word, "XDEATHSTATE"))
			{
				mobjinfo[num].xdeathstate = get_number(word2);
			}
			else if (fastcmp(word, "SPEED"))
			{
				mobjinfo[num].speed = get_number(word2);
			}
			else if (fastcmp(word, "RADIUS"))
			{
				mobjinfo[num].radius = get_number(word2);
			}
			else if (fastcmp(word, "HEIGHT"))
			{
				mobjinfo[num].height = get_number(word2);
			}
			else if (fastcmp(word, "DISPOFFSET"))
			{
				mobjinfo[num].dispoffset = get_number(word2);
			}
			else if (fastcmp(word, "MASS"))
			{
				mobjinfo[num].mass = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "DAMAGE"))
			{
				mobjinfo[num].damage = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "ACTIVESOUND"))
			{
				mobjinfo[num].activesound = get_number(word2);
			}
			else if (fastcmp(word, "FLAGS"))
			{
				mobjinfo[num].flags = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "RAISESTATE"))
			{
				mobjinfo[num].raisestate = get_number(word2);
			}
			else
				deh_warning("Thing %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

#ifdef HWRENDER
static void readlight(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *tmp;
	INT32 value;
	float fvalue;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			fvalue = searchfvalue(s);
			value = searchvalue(s);

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "TYPE"))
			{
				lspr[num].type = (UINT16)value;
			}
			else if (fastcmp(word, "OFFSETX"))
			{
				lspr[num].light_xoffset = fvalue;
			}
			else if (fastcmp(word, "OFFSETY"))
			{
				lspr[num].light_yoffset = fvalue;
			}
			else if (fastcmp(word, "CORONACOLOR"))
			{
				lspr[num].corona_color = value;
			}
			else if (fastcmp(word, "CORONARADIUS"))
			{
				lspr[num].corona_radius = fvalue;
			}
			else if (fastcmp(word, "DYNAMICCOLOR"))
			{
				lspr[num].dynamic_color = value;
			}
			else if (fastcmp(word, "DYNAMICRADIUS"))
			{
				lspr[num].dynamic_radius = fvalue;

				/// \note Update the sqrradius! unnecessary?
				lspr[num].dynamic_sqrradius = fvalue * fvalue;
			}
			else
				deh_warning("Light %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}
#endif // HWRENDER

static void readspriteframe(MYFILE *f, spriteinfo_t *sprinfo, UINT8 frame)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;
	INT32 value;
	char *lastline;

	do
	{
		lastline = f->curpos;
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word
			word = s;
			while ((*word == '\t') || (*word == ' '))
				word++;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
			{
				*(tmp-1) = '\0';
				// Now get the part after
				word2 = tmp += 2;
			}
			else
			{
				// Get the part before the " "
				tmp = strchr(s, ' ');
				if (tmp)
				{
					*tmp = '\0';
					// Now get the part after
					tmp++;
					word2 = tmp;
				}
				else
					break;
			}
			strupr(word);
			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "XPIVOT"))
				sprinfo->pivot[frame].x = value;
			else if (fastcmp(word, "YPIVOT"))
				sprinfo->pivot[frame].y = value;
			else if (fastcmp(word, "ROTAXIS"))
				sprinfo->pivot[frame].rotaxis = value;
			else
			{
				f->curpos = lastline;
				break;
			}
		}
	} while (!myfeof(f)); // finish when the line is empty
	Z_Free(s);
}

static void readspriteinfo(MYFILE *f, INT32 num, boolean sprite2)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;
#ifdef HWRENDER
	INT32 value;
#endif
	char *lastline;
	INT32 skinnumbers[MAXSKINS];
	INT32 foundskins = 0;

	// allocate a spriteinfo
	spriteinfo_t *info = Z_Calloc(sizeof(spriteinfo_t), PU_STATIC, NULL);
	info->available = true;

#ifdef ROTSPRITE
	if ((sprites != NULL) && (!sprite2))
		R_FreeSingleRotSprite(&sprites[num]);
#endif

	do
	{
		lastline = f->curpos;
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word
			word = s;
			while ((*word == '\t') || (*word == ' '))
				word++;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
			{
				*(tmp-1) = '\0';
				// Now get the part after
				word2 = tmp += 2;
			}
			else
			{
				// Get the part before the " "
				tmp = strchr(s, ' ');
				if (tmp)
				{
					*tmp = '\0';
					// Now get the part after
					tmp++;
					word2 = tmp;
				}
				else
					break;
			}
			strupr(word);
#ifdef HWRENDER
			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "LIGHTTYPE"))
			{
				if (sprite2)
					deh_warning("Sprite2 %s: invalid word '%s'", spr2names[num], word);
				else
				{
					INT32 oldvar;
					for (oldvar = 0; t_lspr[num] != &lspr[oldvar]; oldvar++)
						;
					t_lspr[num] = &lspr[value];
				}
			}
			else
#endif
			if (fastcmp(word, "SKIN"))
			{
				INT32 skinnum = -1;
				if (!sprite2)
				{
					deh_warning("Sprite %s: %s keyword found outside of SPRITE2INFO block, ignoring", spr2names[num], word);
					continue;
				}

				// make lowercase
				strlwr(word2);
				skinnum = R_SkinAvailable(word2);
				if (skinnum == -1)
				{
					deh_warning("Sprite2 %s: unknown skin %s", spr2names[num], word2);
					break;
				}

				skinnumbers[foundskins] = skinnum;
				foundskins++;
			}
			else if (fastcmp(word, "DEFAULT"))
			{
				if (!sprite2)
				{
					deh_warning("Sprite %s: %s keyword found outside of SPRITE2INFO block, ignoring", spr2names[num], word);
					continue;
				}
				if (num < (INT32)free_spr2 && num >= (INT32)SPR2_FIRSTFREESLOT)
					spr2defaults[num] = get_number(word2);
				else
				{
					deh_warning("Sprite2 %s: out of range (%d - %d), ignoring", spr2names[num], SPR2_FIRSTFREESLOT, free_spr2-1);
					continue;
				}
			}
			else if (fastcmp(word, "FRAME"))
			{
				UINT8 frame = R_Char2Frame(word2[0]);
				// frame number too high
				if (frame >= 64)
				{
					if (sprite2)
						deh_warning("Sprite2 %s: invalid frame %s", spr2names[num], word2);
					else
						deh_warning("Sprite %s: invalid frame %s", sprnames[num], word2);
					break;
				}

				// read sprite frame and store it in the spriteinfo_t struct
				readspriteframe(f, info, frame);
				if (sprite2)
				{
					INT32 i;
					if (!foundskins)
					{
						deh_warning("Sprite2 %s: no skins specified", spr2names[num]);
						break;
					}
					for (i = 0; i < foundskins; i++)
					{
						size_t skinnum = skinnumbers[i];
						skin_t *skin = &skins[skinnum];
						spriteinfo_t *sprinfo = skin->sprinfo;
#ifdef ROTSPRITE
						R_FreeSkinRotSprite(skinnum);
#endif
						M_Memcpy(&sprinfo[num], info, sizeof(spriteinfo_t));
					}
				}
				else
					M_Memcpy(&spriteinfo[num], info, sizeof(spriteinfo_t));
			}
			else
			{
				//deh_warning("Sprite %s: unknown word '%s'", sprnames[num], word);
				f->curpos = lastline;
				break;
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
	Z_Free(info);
}

static void readsprite2(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word, "DEFAULT"))
				spr2defaults[num] = get_number(word2);
			else
				deh_warning("Sprite2 %s: unknown word '%s'", spr2names[num], word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

// copypasted from readPlayer :]
static const char *const GAMETYPERULE_LIST[];
static void readgametype(MYFILE *f, char *gtname)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2, *word2lwr = NULL;
	char *tmp;
	INT32 i, j;

	INT16 newgtidx = 0;
	UINT32 newgtrules = 0;
	UINT32 newgttol = 0;
	INT32 newgtpointlimit = 0;
	INT32 newgttimelimit = 0;
	UINT8 newgtleftcolor = 0;
	UINT8 newgtrightcolor = 0;
	INT16 newgtrankingstype = -1;
	int newgtinttype = 0;
	char gtdescription[441];
	char gtconst[MAXLINELEN];

	// Empty strings.
	gtdescription[0] = '\0';
	gtconst[0] = '\0';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "DESCRIPTION"))
			{
				char *descr = NULL;

				for (i = 0; i < MAXLINELEN-3; i++)
				{
					if (s[i] == '=')
					{
						descr = &s[i+2];
						break;
					}
				}
				if (descr)
				{
					strcpy(gtdescription, descr);
					strcat(gtdescription, myhashfgets(descr, sizeof (gtdescription), f));
				}
				else
					strcpy(gtdescription, "");

				// For some reason, cutting the string did not work above. Most likely due to strcpy or strcat...
				// It works down here, though.
				{
					INT32 numline = 0;
					for (i = 0; i < MAXLINELEN-1; i++)
					{
						if (numline < 20 && gtdescription[i] == '\n')
							numline++;

						if (numline >= 20 || gtdescription[i] == '\0' || gtdescription[i] == '#')
							break;
					}
				}
				gtdescription[strlen(gtdescription)-1] = '\0';
				gtdescription[i] = '\0';
				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
			{
				if (!word2lwr)
					word2lwr = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
				strcpy(word2lwr, word2);
				strupr(word2);
			}
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);

			// Game type rules
			if (fastcmp(word, "RULES"))
			{
				// GTR_
				newgtrules = (UINT32)get_number(word2);
			}
			// Identifier
			else if (fastcmp(word, "IDENTIFIER"))
			{
				// GT_
				strncpy(gtconst, word2, MAXLINELEN);
			}
			// Point and time limits
			else if (fastcmp(word, "DEFAULTPOINTLIMIT"))
				newgtpointlimit = (INT32)i;
			else if (fastcmp(word, "DEFAULTTIMELIMIT"))
				newgttimelimit = (INT32)i;
			// Level platter
			else if (fastcmp(word, "HEADERCOLOR") || fastcmp(word, "HEADERCOLOUR"))
				newgtleftcolor = newgtrightcolor = (UINT8)get_number(word2);
			else if (fastcmp(word, "HEADERLEFTCOLOR") || fastcmp(word, "HEADERLEFTCOLOUR"))
				newgtleftcolor = (UINT8)get_number(word2);
			else if (fastcmp(word, "HEADERRIGHTCOLOR") || fastcmp(word, "HEADERRIGHTCOLOUR"))
				newgtrightcolor = (UINT8)get_number(word2);
			// Rankings type
			else if (fastcmp(word, "RANKINGTYPE"))
			{
				// Case insensitive
				newgtrankingstype = (int)get_number(word2);
			}
			// Intermission type
			else if (fastcmp(word, "INTERMISSIONTYPE"))
			{
				// Case sensitive
				newgtinttype = (int)get_number(word2lwr);
			}
			// Type of level
			else if (fastcmp(word, "TYPEOFLEVEL"))
			{
				if (i) // it's just a number
					newgttol = (UINT32)i;
				else
				{
					UINT32 tol = 0;
					tmp = strtok(word2,",");
					do {
						for (i = 0; TYPEOFLEVEL[i].name; i++)
							if (fasticmp(tmp, TYPEOFLEVEL[i].name))
								break;
						if (!TYPEOFLEVEL[i].name)
							deh_warning("readgametype %s: unknown typeoflevel flag %s\n", gtname, tmp);
						tol |= TYPEOFLEVEL[i].flag;
					} while((tmp = strtok(NULL,",")) != NULL);
					newgttol = tol;
				}
			}
			// The SOC probably provided gametype rules as words,
			// instead of using the RULES keyword.
			// Like for example "NOSPECTATORSPAWN = TRUE".
			// This is completely valid, and looks better anyway.
			else
			{
				UINT32 wordgt = 0;
				for (j = 0; GAMETYPERULE_LIST[j]; j++)
					if (fastcmp(word, GAMETYPERULE_LIST[j])) {
						wordgt |= (1<<j);
						if (i || word2[0] == 'T' || word2[0] == 'Y')
							newgtrules |= wordgt;
						break;
					}
				if (!wordgt)
					deh_warning("readgametype %s: unknown word '%s'", gtname, word);
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	// Free strings.
	Z_Free(s);
	if (word2lwr)
		Z_Free(word2lwr);

	// Ran out of gametype slots
	if (gametypecount == NUMGAMETYPEFREESLOTS)
	{
		CONS_Alert(CONS_WARNING, "Ran out of free gametype slots!\n");
		return;
	}

	// Add the new gametype
	newgtidx = G_AddGametype(newgtrules);
	G_AddGametypeTOL(newgtidx, newgttol);
	G_SetGametypeDescription(newgtidx, gtdescription, newgtleftcolor, newgtrightcolor);

	// Not covered by G_AddGametype alone.
	if (newgtrankingstype == -1)
		newgtrankingstype = newgtidx;
	gametyperankings[newgtidx] = newgtrankingstype;
	intermissiontypes[newgtidx] = newgtinttype;
	pointlimits[newgtidx] = newgtpointlimit;
	timelimits[newgtidx] = newgttimelimit;

	// Write the new gametype name.
	Gametype_Names[newgtidx] = Z_StrDup((const char *)gtname);

	// Write the constant name.
	if (gtconst[0] == '\0')
		strncpy(gtconst, gtname, MAXLINELEN);
	G_AddGametypeConstant(newgtidx, (const char *)gtconst);

	// Update gametype_cons_t accordingly.
	G_UpdateGametypeSelections();

	CONS_Printf("Added gametype %s\n", Gametype_Names[newgtidx]);
}

static const struct {
	const char *name;
	const mobjtype_t type;
} FLICKYTYPES[] = {
	{"BLUEBIRD", MT_FLICKY_01}, // Flicky (Flicky)
	{"RABBIT",   MT_FLICKY_02}, // Pocky (1)
	{"CHICKEN",  MT_FLICKY_03}, // Cucky (1)
	{"SEAL",     MT_FLICKY_04}, // Rocky (1)
	{"PIG",      MT_FLICKY_05}, // Picky (1)
	{"CHIPMUNK", MT_FLICKY_06}, // Ricky (1)
	{"PENGUIN",  MT_FLICKY_07}, // Pecky (1)
	{"FISH",     MT_FLICKY_08}, // Nicky (CD)
	{"RAM",      MT_FLICKY_09}, // Flocky (CD)
	{"PUFFIN",   MT_FLICKY_10}, // Wicky (CD)
	{"COW",      MT_FLICKY_11}, // Macky (SRB2)
	{"RAT",      MT_FLICKY_12}, // Micky (2)
	{"BEAR",     MT_FLICKY_13}, // Becky (2)
	{"DOVE",     MT_FLICKY_14}, // Docky (CD)
	{"CAT",      MT_FLICKY_15}, // Nyannyan (Flicky)
	{"CANARY",   MT_FLICKY_16}, // Lucky (CD)
	{"a", 0}, // End of normal flickies - a lower case character so will never fastcmp valid with uppercase tmp
	//{"FLICKER",          MT_FLICKER}, // Flacky (SRB2)
	{"SPIDER",   MT_SECRETFLICKY_01}, // Sticky (SRB2)
	{"BAT",      MT_SECRETFLICKY_02}, // Backy (SRB2)
	{"SEED",                MT_SEED}, // Seed (CD)
	{NULL, 0}
};

#define MAXFLICKIES 64

static void readlevelheader(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	//char *word3; // Non-uppercase version of word2
	char *tmp;
	INT32 i;

	// Reset all previous map header information
	P_AllocMapHeader((INT16)(num-1));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word, because some things (Lua.) move it
			word = s;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			i = atoi(word2); // used for numerical settings


			if (fastcmp(word, "LEVELNAME"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->lvlttl, word2,
					sizeof(mapheaderinfo[num-1]->lvlttl), va("Level header %d: levelname", num));
				strlcpy(mapheaderinfo[num-1]->selectheading, word2, sizeof(mapheaderinfo[num-1]->selectheading)); // not deh_ so only complains once
				continue;
			}
			// CHEAP HACK: move this over here for lowercase subtitles
			if (fastcmp(word, "SUBTITLE"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->subttl, word2,
					sizeof(mapheaderinfo[num-1]->subttl), va("Level header %d: subtitle", num));
				continue;
			}

			// Lua custom options also go above, contents may be case sensitive.
			if (fastncmp(word, "LUA.", 4))
			{
				UINT8 j;
				customoption_t *modoption;

				// Note: we actualy strlwr word here, so things are made a little easier for Lua
				strlwr(word);
				word += 4; // move past "lua."

				// ... and do a simple name sanity check; the name must start with a letter
				if (*word < 'a' || *word > 'z')
				{
					deh_warning("Level header %d: invalid custom option name \"%s\"", num, word);
					continue;
				}

				// Sanity limit of 128 params
				if (mapheaderinfo[num-1]->numCustomOptions == 128)
				{
					deh_warning("Level header %d: too many custom parameters", num);
					continue;
				}
				j = mapheaderinfo[num-1]->numCustomOptions++;

				mapheaderinfo[num-1]->customopts =
					Z_Realloc(mapheaderinfo[num-1]->customopts,
						sizeof(customoption_t) * mapheaderinfo[num-1]->numCustomOptions, PU_STATIC, NULL);

				// Newly allocated
				modoption = &mapheaderinfo[num-1]->customopts[j];

				strncpy(modoption->option, word,  31);
				modoption->option[31] = '\0';
				strncpy(modoption->value,  word2, 255);
				modoption->value[255] = '\0';
				continue;
			}

			// Now go to uppercase
			strupr(word2);

			// List of flickies that are be freed in this map
			if (fastcmp(word, "FLICKYLIST") || fastcmp(word, "ANIMALLIST"))
			{
				if (fastcmp(word2, "NONE"))
					P_DeleteFlickies(num-1);
				else if (fastcmp(word2, "DEMO"))
					P_SetDemoFlickies(num-1);
				else if (fastcmp(word2, "ALL"))
				{
					mobjtype_t tmpflickies[MAXFLICKIES];

					for (mapheaderinfo[num-1]->numFlickies = 0;
					((mapheaderinfo[num-1]->numFlickies < MAXFLICKIES) && FLICKYTYPES[mapheaderinfo[num-1]->numFlickies].type);
					mapheaderinfo[num-1]->numFlickies++)
						tmpflickies[mapheaderinfo[num-1]->numFlickies] = FLICKYTYPES[mapheaderinfo[num-1]->numFlickies].type;

					if (mapheaderinfo[num-1]->numFlickies) // just in case...
					{
						size_t newsize = sizeof(mobjtype_t) * mapheaderinfo[num-1]->numFlickies;
						mapheaderinfo[num-1]->flickies = Z_Realloc(mapheaderinfo[num-1]->flickies, newsize, PU_STATIC, NULL);
						M_Memcpy(mapheaderinfo[num-1]->flickies, tmpflickies, newsize);
					}
				}
				else
				{
					mobjtype_t tmpflickies[MAXFLICKIES];
					mapheaderinfo[num-1]->numFlickies = 0;
					tmp = strtok(word2,",");
					// get up to the first MAXFLICKIES flickies
					do {
						if (mapheaderinfo[num-1]->numFlickies == MAXFLICKIES) // never going to get above that number
						{
							deh_warning("Level header %d: too many flickies\n", num);
							break;
						}

						if (fastncmp(tmp, "MT_", 3)) // support for specified mobjtypes...
						{
							i = get_mobjtype(tmp);
							if (!i)
							{
								//deh_warning("Level header %d: unknown flicky mobj type %s\n", num, tmp); -- no need for this line as get_mobjtype complains too
								continue;
							}
							tmpflickies[mapheaderinfo[num-1]->numFlickies] = i;
						}
						else // ...or a quick, limited selection of default flickies!
						{
							for (i = 0; FLICKYTYPES[i].name; i++)
								if (fastcmp(tmp, FLICKYTYPES[i].name))
									break;

							if (!FLICKYTYPES[i].name)
							{
								deh_warning("Level header %d: unknown flicky selection %s\n", num, tmp);
								continue;
							}
							tmpflickies[mapheaderinfo[num-1]->numFlickies] = FLICKYTYPES[i].type;
						}
						mapheaderinfo[num-1]->numFlickies++;
					} while ((tmp = strtok(NULL,",")) != NULL);

					if (mapheaderinfo[num-1]->numFlickies)
					{
						size_t newsize = sizeof(mobjtype_t) * mapheaderinfo[num-1]->numFlickies;
						mapheaderinfo[num-1]->flickies = Z_Realloc(mapheaderinfo[num-1]->flickies, newsize, PU_STATIC, NULL);
						// now we add them to the list!
						M_Memcpy(mapheaderinfo[num-1]->flickies, tmpflickies, newsize);
					}
					else
						deh_warning("Level header %d: no valid flicky types found\n", num);
				}
			}

			// NiGHTS grades
			else if (fastncmp(word, "GRADES", 6))
			{
				UINT8 mare = (UINT8)atoi(word + 6);

				if (mare <= 0 || mare > 8)
				{
					deh_warning("Level header %d: unknown word '%s'", num, word);
					continue;
				}

				P_AddGradesForMare((INT16)(num-1), mare-1, word2);
			}

			// Strings that can be truncated
			else if (fastcmp(word, "SELECTHEADING"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->selectheading, word2,
					sizeof(mapheaderinfo[num-1]->selectheading), va("Level header %d: selectheading", num));
			}
			else if (fastcmp(word, "SCRIPTNAME"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->scriptname, word2,
					sizeof(mapheaderinfo[num-1]->scriptname), va("Level header %d: scriptname", num));
			}
			else if (fastcmp(word, "RUNSOC"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->runsoc, word2,
					sizeof(mapheaderinfo[num-1]->runsoc), va("Level header %d: runsoc", num));
			}
			else if (fastcmp(word, "ACT"))
			{
				if (i >= 0 && i < 20) // 0 for no act number, TTL1 through TTL19
					mapheaderinfo[num-1]->actnum = (UINT8)i;
				else
					deh_warning("Level header %d: invalid act number %d", num, i);
			}
			else if (fastcmp(word, "NEXTLEVEL"))
			{
				if      (fastcmp(word2, "TITLE"))      i = 1100;
				else if (fastcmp(word2, "EVALUATION")) i = 1101;
				else if (fastcmp(word2, "CREDITS"))    i = 1102;
				else if (fastcmp(word2, "ENDING"))     i = 1103;
				else
				// Support using the actual map name,
				// i.e., Nextlevel = AB, Nextlevel = FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);

				mapheaderinfo[num-1]->nextlevel = (INT16)i;
			}
			else if (fastcmp(word, "TYPEOFLEVEL"))
			{
				if (i) // it's just a number
					mapheaderinfo[num-1]->typeoflevel = (UINT32)i;
				else
				{
					UINT32 tol = 0;
					tmp = strtok(word2,",");
					do {
						for (i = 0; TYPEOFLEVEL[i].name; i++)
							if (fastcmp(tmp, TYPEOFLEVEL[i].name))
								break;
						if (!TYPEOFLEVEL[i].name)
							deh_warning("Level header %d: unknown typeoflevel flag %s\n", num, tmp);
						tol |= TYPEOFLEVEL[i].flag;
					} while((tmp = strtok(NULL,",")) != NULL);
					mapheaderinfo[num-1]->typeoflevel = tol;
				}
			}
			else if (fastcmp(word, "KEYWORDS"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->keywords, word2,
						sizeof(mapheaderinfo[num-1]->keywords), va("Level header %d: keywords", num));
			}
			else if (fastcmp(word, "MUSIC"))
			{
				if (fastcmp(word2, "NONE"))
					mapheaderinfo[num-1]->musname[0] = 0; // becomes empty string
				else
				{
					deh_strlcpy(mapheaderinfo[num-1]->musname, word2,
						sizeof(mapheaderinfo[num-1]->musname), va("Level header %d: music", num));
				}
			}
#ifdef MUSICSLOT_COMPATIBILITY
			else if (fastcmp(word, "MUSICSLOT"))
			{
				i = get_mus(word2, true);
				if (i && i <= 1035)
					snprintf(mapheaderinfo[num-1]->musname, 7, "%sM", G_BuildMapName(i));
				else if (i && i <= 1050)
					strncpy(mapheaderinfo[num-1]->musname, compat_special_music_slots[i - 1036], 7);
				else
					mapheaderinfo[num-1]->musname[0] = 0; // becomes empty string
				mapheaderinfo[num-1]->musname[6] = 0;
			}
#endif
			else if (fastcmp(word, "MUSICTRACK"))
				mapheaderinfo[num-1]->mustrack = ((UINT16)i - 1);
			else if (fastcmp(word, "MUSICPOS"))
				mapheaderinfo[num-1]->muspos = (UINT32)get_number(word2);
			else if (fastcmp(word, "MUSICINTERFADEOUT"))
				mapheaderinfo[num-1]->musinterfadeout = (UINT32)get_number(word2);
			else if (fastcmp(word, "MUSICINTER"))
				deh_strlcpy(mapheaderinfo[num-1]->musintername, word2,
					sizeof(mapheaderinfo[num-1]->musintername), va("Level header %d: intermission music", num));
			else if (fastcmp(word, "MUSICPOSTBOSS"))
				deh_strlcpy(mapheaderinfo[num-1]->muspostbossname, word2,
					sizeof(mapheaderinfo[num-1]->muspostbossname), va("Level header %d: post-boss music", num));
			else if (fastcmp(word, "MUSICPOSTBOSSTRACK"))
				mapheaderinfo[num-1]->muspostbosstrack = ((UINT16)i - 1);
			else if (fastcmp(word, "MUSICPOSTBOSSPOS"))
				mapheaderinfo[num-1]->muspostbosspos = (UINT32)get_number(word2);
			else if (fastcmp(word, "MUSICPOSTBOSSFADEIN"))
				mapheaderinfo[num-1]->muspostbossfadein = (UINT32)get_number(word2);
			else if (fastcmp(word, "FORCERESETMUSIC"))
			{
				// This is a weird one because "FALSE"/"NO" could either apply to "leave to default preference" (cv_resetmusic)
				// or "force off". Let's assume it means "force off", and let an unspecified value mean "default preference"
				if      (fastcmp(word2, "OFF") || word2[0] == 'F' || word2[0] == 'N')  i = 0;
				else if (fastcmp(word2, "ON") || word2[0] == 'T' || word2[0] == 'Y')   i = 1;
				else i = -1; // (fastcmp(word2, "DEFAULT"))

				if (i >= -1 && i <= 1) // -1 to force off, 1 to force on, 0 to honor default.
					// This behavior can be disabled with cv_resetmusicbyheader
					mapheaderinfo[num-1]->musforcereset = (SINT8)i;
				else
					deh_warning("Level header %d: invalid forceresetmusic option %d", num, i);
			}
			else if (fastcmp(word, "FORCECHARACTER"))
			{
				strlcpy(mapheaderinfo[num-1]->forcecharacter, word2, SKINNAMESIZE+1);
				strlwr(mapheaderinfo[num-1]->forcecharacter); // skin names are lowercase
			}
			else if (fastcmp(word, "WEATHER"))
				mapheaderinfo[num-1]->weather = (UINT8)get_number(word2);
			else if (fastcmp(word, "SKYNUM"))
				mapheaderinfo[num-1]->skynum = (INT16)i;
			else if (fastcmp(word, "INTERSCREEN"))
				strncpy(mapheaderinfo[num-1]->interscreen, word2, 8);
			else if (fastcmp(word, "PRECUTSCENENUM"))
				mapheaderinfo[num-1]->precutscenenum = (UINT8)i;
			else if (fastcmp(word, "CUTSCENENUM"))
				mapheaderinfo[num-1]->cutscenenum = (UINT8)i;
			else if (fastcmp(word, "COUNTDOWN"))
				mapheaderinfo[num-1]->countdown = (INT16)i;
			else if (fastcmp(word, "PALETTE"))
				mapheaderinfo[num-1]->palette = (UINT16)i;
			else if (fastcmp(word, "NUMLAPS"))
				mapheaderinfo[num-1]->numlaps = (UINT8)i;
			else if (fastcmp(word, "UNLOCKABLE"))
			{
				if (i >= 0 && i <= MAXUNLOCKABLES) // 0 for no unlock required, anything else requires something
					mapheaderinfo[num-1]->unlockrequired = (SINT8)i - 1;
				else
					deh_warning("Level header %d: invalid unlockable number %d", num, i);
			}
			else if (fastcmp(word, "LEVELSELECT"))
				mapheaderinfo[num-1]->levelselect = (UINT8)i;
			else if (fastcmp(word, "SKYBOXSCALE"))
				mapheaderinfo[num-1]->skybox_scalex = mapheaderinfo[num-1]->skybox_scaley = mapheaderinfo[num-1]->skybox_scalez = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEX"))
				mapheaderinfo[num-1]->skybox_scalex = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEY"))
				mapheaderinfo[num-1]->skybox_scaley = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEZ"))
				mapheaderinfo[num-1]->skybox_scalez = (INT16)i;

			else if (fastcmp(word, "BONUSTYPE"))
			{
				if      (fastcmp(word2, "NONE"))   i = -1;
				else if (fastcmp(word2, "NORMAL")) i =  0;
				else if (fastcmp(word2, "BOSS"))   i =  1;
				else if (fastcmp(word2, "ERZ3"))   i =  2;
				else if (fastcmp(word2, "NIGHTS")) i =  3;
				else if (fastcmp(word2, "NIGHTSLINK")) i = 4;

				if (i >= -1 && i <= 4) // -1 for no bonus. Max is 4.
					mapheaderinfo[num-1]->bonustype = (SINT8)i;
				else
					deh_warning("Level header %d: invalid bonus type number %d", num, i);
			}

			// Title card
			else if (fastcmp(word, "TITLECARDZIGZAG"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->ltzzpatch, word2,
					sizeof(mapheaderinfo[num-1]->ltzzpatch), va("Level header %d: title card zigzag patch name", num));
			}
			else if (fastcmp(word, "TITLECARDZIGZAGTEXT"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->ltzztext, word2,
					sizeof(mapheaderinfo[num-1]->ltzztext), va("Level header %d: title card zigzag text patch name", num));
			}
			else if (fastcmp(word, "TITLECARDACTDIAMOND"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->ltactdiamond, word2,
					sizeof(mapheaderinfo[num-1]->ltactdiamond), va("Level header %d: title card act diamond patch name", num));
			}

			else if (fastcmp(word, "MAXBONUSLIVES"))
				mapheaderinfo[num-1]->maxbonuslives = (SINT8)i;
			else if (fastcmp(word, "LEVELFLAGS"))
				mapheaderinfo[num-1]->levelflags = (UINT16)i;
			else if (fastcmp(word, "MENUFLAGS"))
				mapheaderinfo[num-1]->menuflags = (UINT8)i;

			// Individual triggers for level flags, for ease of use (and 2.0 compatibility)
			else if (fastcmp(word, "SCRIPTISFILE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_SCRIPTISFILE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_SCRIPTISFILE;
			}
			else if (fastcmp(word, "SPEEDMUSIC"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_SPEEDMUSIC;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_SPEEDMUSIC;
			}
			else if (fastcmp(word, "NOSSMUSIC"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_NOSSMUSIC;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_NOSSMUSIC;
			}
			else if (fastcmp(word, "NORELOAD"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_NORELOAD;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_NORELOAD;
			}
			else if (fastcmp(word, "NOZONE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_NOZONE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_NOZONE;
			}
			else if (fastcmp(word, "SAVEGAME"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_SAVEGAME;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_SAVEGAME;
			}
			else if (fastcmp(word, "MIXNIGHTSCOUNTDOWN"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_MIXNIGHTSCOUNTDOWN;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_MIXNIGHTSCOUNTDOWN;
			}
			else if (fastcmp(word, "WARNINGTITLE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_WARNINGTITLE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_WARNINGTITLE;
			}
			else if (fastcmp(word, "NOTITLECARD"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_NOTITLECARD;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_NOTITLECARD;
			}
			else if (fastcmp(word, "SHOWTITLECARDFOR"))
			{
				mapheaderinfo[num-1]->levelflags |= LF_NOTITLECARD;
				tmp = strtok(word2,",");
				do {
					if (fastcmp(tmp, "FIRST"))
						mapheaderinfo[num-1]->levelflags &= ~LF_NOTITLECARDFIRST;
					else if (fastcmp(tmp, "RESPAWN"))
						mapheaderinfo[num-1]->levelflags &= ~LF_NOTITLECARDRESPAWN;
					else if (fastcmp(tmp, "RECORDATTACK"))
						mapheaderinfo[num-1]->levelflags &= ~LF_NOTITLECARDRECORDATTACK;
					else if (fastcmp(tmp, "ALL"))
						mapheaderinfo[num-1]->levelflags &= ~LF_NOTITLECARD;
					else if (!fastcmp(tmp, "NONE"))
						deh_warning("Level header %d: unknown titlecard show option %s\n", num, tmp);

				} while((tmp = strtok(NULL,",")) != NULL);
			}

			// Individual triggers for menu flags
			else if (fastcmp(word, "HIDDEN"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_HIDEINMENU;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_HIDEINMENU;
			}
			else if (fastcmp(word, "HIDEINSTATS"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_HIDEINSTATS;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_HIDEINSTATS;
			}
			else if (fastcmp(word, "RECORDATTACK") || fastcmp(word, "TIMEATTACK"))
			{ // TIMEATTACK is an accepted alias
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_RECORDATTACK;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_RECORDATTACK;
			}
			else if (fastcmp(word, "NIGHTSATTACK"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_NIGHTSATTACK;
				else
					mapheaderinfo[num-1]->menuflags &= LF2_NIGHTSATTACK;
			}
			else if (fastcmp(word, "NOVISITNEEDED"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_NOVISITNEEDED;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_NOVISITNEEDED;
			}
			else if (fastcmp(word, "WIDEICON"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_WIDEICON;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_WIDEICON;
			}
			else if (fastcmp(word, "STARTRINGS"))
				mapheaderinfo[num-1]->startrings = (UINT16)i;
			else if (fastcmp(word, "SPECIALSTAGETIME"))
				mapheaderinfo[num-1]->sstimer = i;
			else if (fastcmp(word, "SPECIALSTAGESPHERES"))
				mapheaderinfo[num-1]->ssspheres = i;
			else if (fastcmp(word, "GRAVITY"))
				mapheaderinfo[num-1]->gravity = FLOAT_TO_FIXED(atof(word2));
			else
				deh_warning("Level header %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

#undef MAXFLICKIES

static void readcutscenescene(MYFILE *f, INT32 num, INT32 scenenum)
{
	char *s = Z_Calloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	INT32 i;
	UINT16 usi;
	UINT8 picid;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "SCENETEXT"))
			{
				char *scenetext = NULL;
				char *buffer;
				const int bufferlen = 4096;

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '=')
					{
						scenetext = &s[i+2];
						break;
					}
				}

				if (!scenetext)
				{
					Z_Free(cutscenes[num]->scene[scenenum].text);
					cutscenes[num]->scene[scenenum].text = NULL;
					continue;
				}

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '\0')
					{
						s[i] = '\n';
						s[i+1] = '\0';
						break;
					}
				}

				buffer = Z_Malloc(4096, PU_STATIC, NULL);
				strcpy(buffer, scenetext);

				strcat(buffer,
					myhashfgets(scenetext, bufferlen
					- strlen(buffer) - 1, f));

				// A cutscene overwriting another one...
				Z_Free(cutscenes[num]->scene[scenenum].text);

				cutscenes[num]->scene[scenenum].text = Z_StrDup(buffer);

				Z_Free(buffer);

				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);
			usi = (UINT16)i;


			if (fastcmp(word, "NUMBEROFPICS"))
			{
				cutscenes[num]->scene[scenenum].numpics = (UINT8)i;
			}
			else if (fastncmp(word, "PIC", 3))
			{
				picid = (UINT8)atoi(word + 3);
				if (picid > 8 || picid == 0)
				{
					deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
					continue;
				}
				--picid;

				if (fastcmp(word+4, "NAME"))
				{
					strncpy(cutscenes[num]->scene[scenenum].picname[picid], word2, 8);
				}
				else if (fastcmp(word+4, "HIRES"))
				{
					cutscenes[num]->scene[scenenum].pichires[picid] = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				}
				else if (fastcmp(word+4, "DURATION"))
				{
					cutscenes[num]->scene[scenenum].picduration[picid] = usi;
				}
				else if (fastcmp(word+4, "XCOORD"))
				{
					cutscenes[num]->scene[scenenum].xcoord[picid] = usi;
				}
				else if (fastcmp(word+4, "YCOORD"))
				{
					cutscenes[num]->scene[scenenum].ycoord[picid] = usi;
				}
				else
					deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(cutscenes[num]->scene[scenenum].musswitch, word2, 7);
				cutscenes[num]->scene[scenenum].musswitch[6] = 0;
			}
#ifdef MUSICSLOT_COMPATIBILITY
			else if (fastcmp(word, "MUSICSLOT"))
			{
				i = get_mus(word2, true);
				if (i && i <= 1035)
					snprintf(cutscenes[num]->scene[scenenum].musswitch, 7, "%sM", G_BuildMapName(i));
				else if (i && i <= 1050)
					strncpy(cutscenes[num]->scene[scenenum].musswitch, compat_special_music_slots[i - 1036], 7);
				else
					cutscenes[num]->scene[scenenum].musswitch[0] = 0; // becomes empty string
				cutscenes[num]->scene[scenenum].musswitch[6] = 0;
			}
#endif
			else if (fastcmp(word, "MUSICTRACK"))
			{
				cutscenes[num]->scene[scenenum].musswitchflags = ((UINT16)i) & MUSIC_TRACKMASK;
			}
			else if (fastcmp(word, "MUSICPOS"))
			{
				cutscenes[num]->scene[scenenum].musswitchposition = (UINT32)get_number(word2);
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				cutscenes[num]->scene[scenenum].musicloop = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "TEXTXPOS"))
			{
				cutscenes[num]->scene[scenenum].textxpos = usi;
			}
			else if (fastcmp(word, "TEXTYPOS"))
			{
				cutscenes[num]->scene[scenenum].textypos = usi;
			}
			else if (fastcmp(word, "FADEINID"))
			{
				cutscenes[num]->scene[scenenum].fadeinid = (UINT8)i;
			}
			else if (fastcmp(word, "FADEOUTID"))
			{
				cutscenes[num]->scene[scenenum].fadeoutid = (UINT8)i;
			}
			else if (fastcmp(word, "FADECOLOR"))
			{
				cutscenes[num]->scene[scenenum].fadecolor = (UINT8)i;
			}
			else
				deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readcutscene(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	// Allocate memory for this cutscene if we don't yet have any
	if (!cutscenes[num])
		cutscenes[num] = Z_Calloc(sizeof (cutscene_t), PU_STATIC, NULL);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " ");
			if (word2)
				value = atoi(word2);
			else
			{
				deh_warning("No value for token %s", word);
				continue;
			}

			if (fastcmp(word, "NUMSCENES"))
			{
				cutscenes[num]->numscenes = value;
			}
			else if (fastcmp(word, "SCENE"))
			{
				if (1 <= value && value <= 128)
				{
					readcutscenescene(f, num, value - 1);
				}
				else
					deh_warning("Scene number %d out of range (1 - 128)", value);

			}
			else
				deh_warning("Cutscene %d: unknown word '%s', Scene <num> expected.", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readtextpromptpage(MYFILE *f, INT32 num, INT32 pagenum)
{
	char *s = Z_Calloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	INT32 i;
	UINT16 usi;
	UINT8 picid;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "PAGETEXT"))
			{
				char *pagetext = NULL;
				char *buffer;
				const int bufferlen = 4096;

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '=')
					{
						pagetext = &s[i+2];
						break;
					}
				}

				if (!pagetext)
				{
					Z_Free(textprompts[num]->page[pagenum].text);
					textprompts[num]->page[pagenum].text = NULL;
					continue;
				}

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '\0')
					{
						s[i] = '\n';
						s[i+1] = '\0';
						break;
					}
				}

				buffer = Z_Malloc(4096, PU_STATIC, NULL);
				strcpy(buffer, pagetext);

				// \todo trim trailing whitespace before the #
				// and also support # at the end of a PAGETEXT with no line break

				strcat(buffer,
					myhashfgets(pagetext, bufferlen
					- strlen(buffer) - 1, f));

				// A text prompt overwriting another one...
				Z_Free(textprompts[num]->page[pagenum].text);

				textprompts[num]->page[pagenum].text = Z_StrDup(buffer);

				Z_Free(buffer);

				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);
			usi = (UINT16)i;

			// copypasta from readcutscenescene
			if (fastcmp(word, "NUMBEROFPICS"))
			{
				textprompts[num]->page[pagenum].numpics = (UINT8)i;
			}
			else if (fastcmp(word, "PICMODE"))
			{
				UINT8 picmode = 0; // PROMPT_PIC_PERSIST
				if (usi == 1 || word2[0] == 'L') picmode = PROMPT_PIC_LOOP;
				else if (usi == 2 || word2[0] == 'D' || word2[0] == 'H') picmode = PROMPT_PIC_DESTROY;
				textprompts[num]->page[pagenum].picmode = picmode;
			}
			else if (fastcmp(word, "PICTOLOOP"))
				textprompts[num]->page[pagenum].pictoloop = (UINT8)i;
			else if (fastcmp(word, "PICTOSTART"))
				textprompts[num]->page[pagenum].pictostart = (UINT8)i;
			else if (fastcmp(word, "PICSMETAPAGE"))
			{
				if (usi && usi <= textprompts[num]->numpages)
				{
					UINT8 metapagenum = usi - 1;

					textprompts[num]->page[pagenum].numpics = textprompts[num]->page[metapagenum].numpics;
					textprompts[num]->page[pagenum].picmode = textprompts[num]->page[metapagenum].picmode;
					textprompts[num]->page[pagenum].pictoloop = textprompts[num]->page[metapagenum].pictoloop;
					textprompts[num]->page[pagenum].pictostart = textprompts[num]->page[metapagenum].pictostart;

					for (picid = 0; picid < MAX_PROMPT_PICS; picid++)
					{
						strncpy(textprompts[num]->page[pagenum].picname[picid], textprompts[num]->page[metapagenum].picname[picid], 8);
						textprompts[num]->page[pagenum].pichires[picid] = textprompts[num]->page[metapagenum].pichires[picid];
						textprompts[num]->page[pagenum].picduration[picid] = textprompts[num]->page[metapagenum].picduration[picid];
						textprompts[num]->page[pagenum].xcoord[picid] = textprompts[num]->page[metapagenum].xcoord[picid];
						textprompts[num]->page[pagenum].ycoord[picid] = textprompts[num]->page[metapagenum].ycoord[picid];
					}
				}
			}
			else if (fastncmp(word, "PIC", 3))
			{
				picid = (UINT8)atoi(word + 3);
				if (picid > MAX_PROMPT_PICS || picid == 0)
				{
					deh_warning("textpromptscene %d: unknown word '%s'", num, word);
					continue;
				}
				--picid;

				if (fastcmp(word+4, "NAME"))
				{
					strncpy(textprompts[num]->page[pagenum].picname[picid], word2, 8);
				}
				else if (fastcmp(word+4, "HIRES"))
				{
					textprompts[num]->page[pagenum].pichires[picid] = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				}
				else if (fastcmp(word+4, "DURATION"))
				{
					textprompts[num]->page[pagenum].picduration[picid] = usi;
				}
				else if (fastcmp(word+4, "XCOORD"))
				{
					textprompts[num]->page[pagenum].xcoord[picid] = usi;
				}
				else if (fastcmp(word+4, "YCOORD"))
				{
					textprompts[num]->page[pagenum].ycoord[picid] = usi;
				}
				else
					deh_warning("textpromptscene %d: unknown word '%s'", num, word);
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(textprompts[num]->page[pagenum].musswitch, word2, 7);
				textprompts[num]->page[pagenum].musswitch[6] = 0;
			}
#ifdef MUSICSLOT_COMPATIBILITY
			else if (fastcmp(word, "MUSICSLOT"))
			{
				i = get_mus(word2, true);
				if (i && i <= 1035)
					snprintf(textprompts[num]->page[pagenum].musswitch, 7, "%sM", G_BuildMapName(i));
				else if (i && i <= 1050)
					strncpy(textprompts[num]->page[pagenum].musswitch, compat_special_music_slots[i - 1036], 7);
				else
					textprompts[num]->page[pagenum].musswitch[0] = 0; // becomes empty string
				textprompts[num]->page[pagenum].musswitch[6] = 0;
			}
#endif
			else if (fastcmp(word, "MUSICTRACK"))
			{
				textprompts[num]->page[pagenum].musswitchflags = ((UINT16)i) & MUSIC_TRACKMASK;
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				textprompts[num]->page[pagenum].musicloop = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
			}
			// end copypasta from readcutscenescene
			else if (fastcmp(word, "NAME"))
			{
				if (*word2 != '\0')
				{
					INT32 j;

					// HACK: Add yellow control char now
					// so the drawing function doesn't call it repeatedly
					char name[34];
					name[0] = '\x82'; // color yellow
					name[1] = 0;
					strncat(name, word2, 33);
					name[33] = 0;

					// Replace _ with ' '
					for (j = 0; j < 32 && name[j]; j++)
					{
						if (name[j] == '_')
							name[j] = ' ';
					}

					strncpy(textprompts[num]->page[pagenum].name, name, 32);
				}
				else
					*textprompts[num]->page[pagenum].name = '\0';
			}
			else if (fastcmp(word, "ICON"))
				strncpy(textprompts[num]->page[pagenum].iconname, word2, 8);
			else if (fastcmp(word, "ICONALIGN"))
				textprompts[num]->page[pagenum].rightside = (i || word2[0] == 'R');
			else if (fastcmp(word, "ICONFLIP"))
				textprompts[num]->page[pagenum].iconflip = (i || word2[0] == 'T' || word2[0] == 'Y');
			else if (fastcmp(word, "LINES"))
				textprompts[num]->page[pagenum].lines = usi;
			else if (fastcmp(word, "BACKCOLOR"))
			{
				INT32 backcolor;
				if      (i == 0 || fastcmp(word2, "WHITE")) backcolor = 0;
				else if (i == 1 || fastcmp(word2, "GRAY") || fastcmp(word2, "GREY") ||
					fastcmp(word2, "BLACK")) backcolor = 1;
				else if (i == 2 || fastcmp(word2, "SEPIA")) backcolor = 2;
				else if (i == 3 || fastcmp(word2, "BROWN")) backcolor = 3;
				else if (i == 4 || fastcmp(word2, "PINK")) backcolor = 4;
				else if (i == 5 || fastcmp(word2, "RASPBERRY")) backcolor = 5;
				else if (i == 6 || fastcmp(word2, "RED")) backcolor = 6;
				else if (i == 7 || fastcmp(word2, "CREAMSICLE")) backcolor = 7;
				else if (i == 8 || fastcmp(word2, "ORANGE")) backcolor = 8;
				else if (i == 9 || fastcmp(word2, "GOLD")) backcolor = 9;
				else if (i == 10 || fastcmp(word2, "YELLOW")) backcolor = 10;
				else if (i == 11 || fastcmp(word2, "EMERALD")) backcolor = 11;
				else if (i == 12 || fastcmp(word2, "GREEN")) backcolor = 12;
				else if (i == 13 || fastcmp(word2, "CYAN") || fastcmp(word2, "AQUA")) backcolor = 13;
				else if (i == 14 || fastcmp(word2, "STEEL")) backcolor = 14;
				else if (i == 15 || fastcmp(word2, "PERIWINKLE")) backcolor = 15;
				else if (i == 16 || fastcmp(word2, "BLUE")) backcolor = 16;
				else if (i == 17 || fastcmp(word2, "PURPLE")) backcolor = 17;
				else if (i == 18 || fastcmp(word2, "LAVENDER")) backcolor = 18;
				else if (i >= 256 && i < 512) backcolor = i; // non-transparent palette index
				else if (i < 0) backcolor = INT32_MAX; // CONS_BACKCOLOR user-configured
				else backcolor = 1; // default gray
				textprompts[num]->page[pagenum].backcolor = backcolor;
			}
			else if (fastcmp(word, "ALIGN"))
			{
				UINT8 align = 0; // left
				if (usi == 1 || word2[0] == 'R') align = 1;
				else if (usi == 2 || word2[0] == 'C' || word2[0] == 'M') align = 2;
				textprompts[num]->page[pagenum].align = align;
			}
			else if (fastcmp(word, "VERTICALALIGN"))
			{
				UINT8 align = 0; // top
				if (usi == 1 || word2[0] == 'B') align = 1;
				else if (usi == 2 || word2[0] == 'C' || word2[0] == 'M') align = 2;
				textprompts[num]->page[pagenum].verticalalign = align;
			}
			else if (fastcmp(word, "TEXTSPEED"))
				textprompts[num]->page[pagenum].textspeed = get_number(word2);
			else if (fastcmp(word, "TEXTSFX"))
				textprompts[num]->page[pagenum].textsfx = get_number(word2);
			else if (fastcmp(word, "HIDEHUD"))
			{
				UINT8 hidehud = 0;
				if ((word2[0] == 'F' && (word2[1] == 'A' || !word2[1])) || word2[0] == 'N') hidehud = 0; // false
				else if (usi == 1 || word2[0] == 'T' || word2[0] == 'Y') hidehud = 1; // true (hide appropriate HUD elements)
				else if (usi == 2 || word2[0] == 'A' || (word2[0] == 'F' && word2[1] == 'O')) hidehud = 2; // force (hide all HUD elements)
				textprompts[num]->page[pagenum].hidehud = hidehud;
			}
			else if (fastcmp(word, "METAPAGE"))
			{
				if (usi && usi <= textprompts[num]->numpages)
				{
					UINT8 metapagenum = usi - 1;

					strncpy(textprompts[num]->page[pagenum].name, textprompts[num]->page[metapagenum].name, 32);
					strncpy(textprompts[num]->page[pagenum].iconname, textprompts[num]->page[metapagenum].iconname, 8);
					textprompts[num]->page[pagenum].rightside = textprompts[num]->page[metapagenum].rightside;
					textprompts[num]->page[pagenum].iconflip = textprompts[num]->page[metapagenum].iconflip;
					textprompts[num]->page[pagenum].lines = textprompts[num]->page[metapagenum].lines;
					textprompts[num]->page[pagenum].backcolor = textprompts[num]->page[metapagenum].backcolor;
					textprompts[num]->page[pagenum].align = textprompts[num]->page[metapagenum].align;
					textprompts[num]->page[pagenum].verticalalign = textprompts[num]->page[metapagenum].verticalalign;
					textprompts[num]->page[pagenum].textspeed = textprompts[num]->page[metapagenum].textspeed;
					textprompts[num]->page[pagenum].textsfx = textprompts[num]->page[metapagenum].textsfx;
					textprompts[num]->page[pagenum].hidehud = textprompts[num]->page[metapagenum].hidehud;

					// music: don't copy, else each page change may reset the music
				}
			}
			else if (fastcmp(word, "TAG"))
				strncpy(textprompts[num]->page[pagenum].tag, word2, 33);
			else if (fastcmp(word, "NEXTPROMPT"))
				textprompts[num]->page[pagenum].nextprompt = usi;
			else if (fastcmp(word, "NEXTPAGE"))
				textprompts[num]->page[pagenum].nextpage = usi;
			else if (fastcmp(word, "NEXTTAG"))
				strncpy(textprompts[num]->page[pagenum].nexttag, word2, 33);
			else if (fastcmp(word, "TIMETONEXT"))
				textprompts[num]->page[pagenum].timetonext = get_number(word2);
			else
				deh_warning("PromptPage %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readtextprompt(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	// Allocate memory for this prompt if we don't yet have any
	if (!textprompts[num])
		textprompts[num] = Z_Calloc(sizeof (textprompt_t), PU_STATIC, NULL);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " ");
			if (word2)
				value = atoi(word2);
			else
			{
				deh_warning("No value for token %s", word);
				continue;
			}

			if (fastcmp(word, "NUMPAGES"))
			{
				textprompts[num]->numpages = min(max(value, 0), MAX_PAGES);
			}
			else if (fastcmp(word, "PAGE"))
			{
				if (1 <= value && value <= MAX_PAGES)
				{
					textprompts[num]->page[value - 1].backcolor = 1; // default to gray
					textprompts[num]->page[value - 1].hidehud = 1; // hide appropriate HUD elements
					readtextpromptpage(f, num, value - 1);
				}
				else
					deh_warning("Page number %d out of range (1 - %d)", value, MAX_PAGES);

			}
			else
				deh_warning("Prompt %d: unknown word '%s', Page <num> expected.", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readmenu(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = (tmp += 2);
			strupr(word2);

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "BACKGROUNDNAME"))
			{
				strncpy(menupres[num].bgname, word2, 8);
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDEBACKGROUND"))
			{
				menupres[num].bghide = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "BACKGROUNDCOLOR"))
			{
				menupres[num].bgcolor = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDETITLEPICS") || fastcmp(word, "HIDEPICS") || fastcmp(word, "TITLEPICSHIDE"))
			{
				// true by default, except MM_MAIN
				menupres[num].hidetitlepics = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSMODE"))
			{
				if (fastcmp(word2, "USER"))
					menupres[num].ttmode = TTMODE_USER;
				else if (fastcmp(word2, "ALACROIX"))
					menupres[num].ttmode = TTMODE_ALACROIX;
				else if (fastcmp(word2, "HIDE") || fastcmp(word2, "HIDDEN") || fastcmp(word2, "NONE"))
				{
					menupres[num].ttmode = TTMODE_USER;
					menupres[num].ttname[0] = 0;
					menupres[num].hidetitlepics = true;
				}
				else // if (fastcmp(word2, "OLD") || fastcmp(word2, "SSNTAILS"))
					menupres[num].ttmode = TTMODE_OLD;
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSSCALE"))
			{
				// Don't handle Alacroix special case here; see Maincfg section.
				menupres[num].ttscale = max(1, min(8, (UINT8)get_number(word2)));
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSNAME"))
			{
				strncpy(menupres[num].ttname, word2, 9);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSX"))
			{
				menupres[num].ttx = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSY"))
			{
				menupres[num].tty = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSLOOP"))
			{
				menupres[num].ttloop = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSTICS"))
			{
				menupres[num].tttics = (UINT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLSPEED") || fastcmp(word, "TITLESCROLLXSPEED")
				|| fastcmp(word, "SCROLLSPEED") || fastcmp(word, "SCROLLXSPEED"))
			{
				menupres[num].titlescrollxspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLYSPEED") || fastcmp(word, "SCROLLYSPEED"))
			{
				menupres[num].titlescrollyspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(menupres[num].musname, word2, 7);
				menupres[num].musname[6] = 0;
				titlechanged = true;
			}
#ifdef MUSICSLOT_COMPATIBILITY
			else if (fastcmp(word, "MUSICSLOT"))
			{
				value = get_mus(word2, true);
				if (value && value <= 1035)
					snprintf(menupres[num].musname, 7, "%sM", G_BuildMapName(value));
				else if (value && value <= 1050)
					strncpy(menupres[num].musname, compat_special_music_slots[value - 1036], 7);
				else
					menupres[num].musname[0] = 0; // becomes empty string
				menupres[num].musname[6] = 0;
				titlechanged = true;
			}
#endif
			else if (fastcmp(word, "MUSICTRACK"))
			{
				menupres[num].mustrack = ((UINT16)value - 1);
				titlechanged = true;
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				// true by default except MM_MAIN
				menupres[num].muslooping = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "NOMUSIC"))
			{
				menupres[num].musstop = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "IGNOREMUSIC"))
			{
				menupres[num].musignore = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "FADESTRENGTH"))
			{
				// one-based, <= 0 means use default value. 1-32
				menupres[num].fadestrength = get_number(word2)-1;
				titlechanged = true;
			}
			else if (fastcmp(word, "NOENTERBUBBLE"))
			{
				menupres[num].enterbubble = !(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "NOEXITBUBBLE"))
			{
				menupres[num].exitbubble = !(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "ENTERTAG"))
			{
				menupres[num].entertag = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "EXITTAG"))
			{
				menupres[num].exittag = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "ENTERWIPE"))
			{
				menupres[num].enterwipe = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "EXITWIPE"))
			{
				menupres[num].exitwipe = get_number(word2);
				titlechanged = true;
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readhuditem(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 i;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			strupr(word2);

			i = atoi(word2); // used for numerical settings

			if (fastcmp(word, "X"))
			{
				hudinfo[num].x = i;
			}
			else if (fastcmp(word, "Y"))
			{
				hudinfo[num].y = i;
			}
			else if (fastcmp(word, "F"))
			{
				hudinfo[num].f = i;
			}
			else
				deh_warning("Level header %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

/*
Sprite number = 10
Sprite subnumber = 32968
Duration = 200
Next frame = 200
*/

static void readframe(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word1;
	char *word2 = NULL;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word1 = strtok(s, " ");
			if (word1)
				strupr(word1);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word1, "SPRITENUMBER") || fastcmp(word1, "SPRITENAME"))
			{
				states[num].sprite = get_sprite(word2);
			}
			else if (fastcmp(word1, "SPRITESUBNUMBER") || fastcmp(word1, "SPRITEFRAME"))
			{
				states[num].frame = (INT32)get_number(word2); // So the FF_ flags get calculated
			}
			else if (fastcmp(word1, "DURATION"))
			{
				states[num].tics = (INT32)get_number(word2); // So TICRATE can be used
			}
			else if (fastcmp(word1, "NEXT"))
			{
				states[num].nextstate = get_state(word2);
			}
			else if (fastcmp(word1, "VAR1"))
			{
				states[num].var1 = (INT32)get_number(word2);
			}
			else if (fastcmp(word1, "VAR2"))
			{
				states[num].var2 = (INT32)get_number(word2);
			}
			else if (fastcmp(word1, "ACTION"))
			{
				size_t z;
				boolean found = false;
				char actiontocompare[32];

				memset(actiontocompare, 0x00, sizeof(actiontocompare));
				strlcpy(actiontocompare, word2, sizeof (actiontocompare));
				strupr(actiontocompare);

				for (z = 0; z < 32; z++)
				{
					if (actiontocompare[z] == '\n' || actiontocompare[z] == '\r')
					{
						actiontocompare[z] = '\0';
						break;
					}
				}

				for (z = 0; actionpointers[z].name; z++)
				{
					if (actionpointers[z].action.acv == states[num].action.acv)
						break;
				}

				z = 0;
				found = LUA_SetLuaAction(&states[num], actiontocompare);
				if (!found)
					while (actionpointers[z].name)
					{
						if (fastcmp(actiontocompare, actionpointers[z].name))
						{
							states[num].action = actionpointers[z].action;
							states[num].action.acv = actionpointers[z].action.acv; // assign
							states[num].action.acp1 = actionpointers[z].action.acp1;
							found = true;
							break;
						}
						z++;
					}

				if (!found)
					deh_warning("Unknown action %s", actiontocompare);
			}
			else
				deh_warning("Frame %d: unknown word '%s'", num, word1);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

static void readsound(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " ");
			if (word2)
				value = atoi(word2);
			else
			{
				deh_warning("No value for token %s", word);
				continue;
			}

			if (fastcmp(word, "SINGULAR"))
			{
				S_sfx[num].singularity = value;
			}
			else if (fastcmp(word, "PRIORITY"))
			{
				S_sfx[num].priority = value;
			}
			else if (fastcmp(word, "FLAGS"))
			{
				S_sfx[num].pitch = value;
			}
			else if (fastcmp(word, "CAPTION") || fastcmp(word, "DESCRIPTION"))
			{
				deh_strlcpy(S_sfx[num].caption, word2,
					sizeof(S_sfx[num].caption), va("Sound effect %d: caption", num));
			}
			else
				deh_warning("Sound %d : unknown word '%s'",num,word);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

/** Checks if a game data file name for a mod is good.
 * "Good" means that it contains only alphanumerics, _, and -;
 * ends in ".dat"; has at least one character before the ".dat";
 * and is not "gamedata.dat" (tested case-insensitively).
 *
 * Assumption: that gamedata.dat is the only .dat file that will
 * ever be treated specially by the game.
 *
 * Note: Check for the tail ".dat" case-insensitively since at
 * present, we get passed the filename in all uppercase.
 *
 * \param s Filename string to check.
 * \return True if the filename is good.
 * \sa readmaincfg()
 * \author Graue <graue@oceanbase.org>
 */
static boolean GoodDataFileName(const char *s)
{
	const char *p;
	const char *tail = ".dat";

	for (p = s; *p != '\0'; p++)
		if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.')
			return false;

	p = s + strlen(s) - strlen(tail);
	if (p <= s) return false; // too short
	if (!fasticmp(p, tail)) return false; // doesn't end in .dat
	if (fasticmp(s, "gamedata.dat")) return false;

	return true;
}

static void reademblemdata(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	memset(&emblemlocations[num-1], 0, sizeof(emblem_t));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			value = atoi(word2); // used for numerical settings

			// Up here to allow lowercase in hints
			if (fastcmp(word, "HINT"))
			{
				while ((tmp = strchr(word2, '\\')))
					*tmp = '\n';
				deh_strlcpy(emblemlocations[num-1].hint, word2, sizeof (emblemlocations[num-1].hint), va("Emblem %d: hint", num));
				continue;
			}
			strupr(word2);

			if (fastcmp(word, "TYPE"))
			{
				if (fastcmp(word2, "GLOBAL"))
					emblemlocations[num-1].type = ET_GLOBAL;
				else if (fastcmp(word2, "SKIN"))
					emblemlocations[num-1].type = ET_SKIN;
				else if (fastcmp(word2, "SCORE"))
					emblemlocations[num-1].type = ET_SCORE;
				else if (fastcmp(word2, "TIME"))
					emblemlocations[num-1].type = ET_TIME;
				else if (fastcmp(word2, "RINGS"))
					emblemlocations[num-1].type = ET_RINGS;
				else if (fastcmp(word2, "MAP"))
					emblemlocations[num-1].type = ET_MAP;
				else if (fastcmp(word2, "NGRADE"))
					emblemlocations[num-1].type = ET_NGRADE;
				else if (fastcmp(word2, "NTIME"))
					emblemlocations[num-1].type = ET_NTIME;
				else
					emblemlocations[num-1].type = (UINT8)value;
			}
			else if (fastcmp(word, "TAG"))
				emblemlocations[num-1].tag = (INT16)value;
			else if (fastcmp(word, "MAPNUM"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);

				emblemlocations[num-1].level = (INT16)value;
			}
			else if (fastcmp(word, "SPRITE"))
			{
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = word2[0];
				else
					value += 'A'-1;

				if (value < 'A' || value > 'Z')
					deh_warning("Emblem %d: sprite must be from A - Z (1 - 26)", num);
				else
					emblemlocations[num-1].sprite = (UINT8)value;
			}
			else if (fastcmp(word, "COLOR"))
				emblemlocations[num-1].color = get_number(word2);
			else if (fastcmp(word, "VAR"))
				emblemlocations[num-1].var = get_number(word2);
			else
				deh_warning("Emblem %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f));

	// Default sprite and color definitions for lazy people like me
	if (!emblemlocations[num-1].sprite) switch (emblemlocations[num-1].type)
	{
		case ET_RINGS:
			emblemlocations[num-1].sprite = 'R'; break;
		case ET_SCORE: case ET_NGRADE:
			emblemlocations[num-1].sprite = 'S'; break;
		case ET_TIME: case ET_NTIME:
			emblemlocations[num-1].sprite = 'T'; break;
		default:
			emblemlocations[num-1].sprite = 'A'; break;
	}
	if (!emblemlocations[num-1].color) switch (emblemlocations[num-1].type)
	{
		case ET_RINGS:
			emblemlocations[num-1].color = SKINCOLOR_GOLD; break;
		case ET_SCORE:
			emblemlocations[num-1].color = SKINCOLOR_BROWN; break;
		case ET_NGRADE:
			emblemlocations[num-1].color = SKINCOLOR_TEAL; break;
		case ET_TIME: case ET_NTIME:
			emblemlocations[num-1].color = SKINCOLOR_GREY; break;
		default:
			emblemlocations[num-1].color = SKINCOLOR_BLUE; break;
	}

	Z_Free(s);
}

static void readextraemblemdata(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	memset(&extraemblems[num-1], 0, sizeof(extraemblem_t));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "NAME"))
				deh_strlcpy(extraemblems[num-1].name, word2,
					sizeof (extraemblems[num-1].name), va("Extra emblem %d: name", num));
			else if (fastcmp(word, "OBJECTIVE"))
				deh_strlcpy(extraemblems[num-1].description, word2,
					sizeof (extraemblems[num-1].description), va("Extra emblem %d: objective", num));
			else if (fastcmp(word, "CONDITIONSET"))
				extraemblems[num-1].conditionset = (UINT8)value;
			else if (fastcmp(word, "SHOWCONDITIONSET"))
				extraemblems[num-1].showconditionset = (UINT8)value;
			else
			{
				strupr(word2);
				if (fastcmp(word, "SPRITE"))
				{
					if (word2[0] >= 'A' && word2[0] <= 'Z')
						value = word2[0];
					else
						value += 'A'-1;

					if (value < 'A' || value > 'Z')
						deh_warning("Emblem %d: sprite must be from A - Z (1 - 26)", num);
					else
						extraemblems[num-1].sprite = (UINT8)value;
				}
				else if (fastcmp(word, "COLOR"))
					extraemblems[num-1].color = get_number(word2);
				else
					deh_warning("Extra emblem %d: unknown word '%s'", num, word);
			}
		}
	} while (!myfeof(f));

	if (!extraemblems[num-1].sprite)
		extraemblems[num-1].sprite = 'X';
	if (!extraemblems[num-1].color)
		extraemblems[num-1].color = SKINCOLOR_BLUE;

	Z_Free(s);
}

static void readunlockable(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 i;

	memset(&unlockables[num], 0, sizeof(unlockable_t));
	unlockables[num].objective[0] = '/';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			i = atoi(word2); // used for numerical settings

			if (fastcmp(word, "NAME"))
				deh_strlcpy(unlockables[num].name, word2,
					sizeof (unlockables[num].name), va("Unlockable %d: name", num));
			else if (fastcmp(word, "OBJECTIVE"))
				deh_strlcpy(unlockables[num].objective, word2,
					sizeof (unlockables[num].objective), va("Unlockable %d: objective", num));
			else
			{
				strupr(word2);
				if (fastcmp(word, "HEIGHT"))
					unlockables[num].height = (UINT16)i;
				else if (fastcmp(word, "CONDITIONSET"))
					unlockables[num].conditionset = (UINT8)i;
				else if (fastcmp(word, "SHOWCONDITIONSET"))
					unlockables[num].showconditionset = (UINT8)i;
				else if (fastcmp(word, "NOCECHO"))
					unlockables[num].nocecho = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				else if (fastcmp(word, "NOCHECKLIST"))
					unlockables[num].nochecklist = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				else if (fastcmp(word, "TYPE"))
				{
					if (fastcmp(word2, "NONE"))
						unlockables[num].type = SECRET_NONE;
					else if (fastcmp(word2, "ITEMFINDER"))
						unlockables[num].type = SECRET_ITEMFINDER;
					else if (fastcmp(word2, "EMBLEMHINTS"))
						unlockables[num].type = SECRET_EMBLEMHINTS;
					else if (fastcmp(word2, "PANDORA"))
						unlockables[num].type = SECRET_PANDORA;
					else if (fastcmp(word2, "CREDITS"))
						unlockables[num].type = SECRET_CREDITS;
					else if (fastcmp(word2, "RECORDATTACK"))
						unlockables[num].type = SECRET_RECORDATTACK;
					else if (fastcmp(word2, "NIGHTSMODE"))
						unlockables[num].type = SECRET_NIGHTSMODE;
					else if (fastcmp(word2, "HEADER"))
						unlockables[num].type = SECRET_HEADER;
					else if (fastcmp(word2, "LEVELSELECT"))
						unlockables[num].type = SECRET_LEVELSELECT;
					else if (fastcmp(word2, "WARP"))
						unlockables[num].type = SECRET_WARP;
					else if (fastcmp(word2, "SOUNDTEST"))
						unlockables[num].type = SECRET_SOUNDTEST;
					else
						unlockables[num].type = (INT16)i;
				}
				else if (fastcmp(word, "VAR"))
				{
					// Support using the actual map name,
					// i.e., Level AB, Level FZ, etc.

					// Convert to map number
					if (word2[0] >= 'A' && word2[0] <= 'Z')
						i = M_MapNumber(word2[0], word2[1]);

					unlockables[num].variable = (INT16)i;
				}
				else
					deh_warning("Unlockable %d: unknown word '%s'", num+1, word);
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static const char NIGHTSGRADE_LIST[] = {
	'F', // GRADE_F
	'E', // GRADE_E
	'D', // GRADE_D
	'C', // GRADE_C
	'B', // GRADE_B
	'A', // GRADE_A
	'S', // GRADE_S
	'\0'
};

#define PARAMCHECK(n) do { if (!params[n]) { deh_warning("Too few parameters, need %d", n); return; }} while (0)
static void readcondition(UINT8 set, UINT32 id, char *word2)
{
	INT32 i;
	char *params[4]; // condition, requirement, extra info, extra info
	char *spos;

	conditiontype_t ty;
	INT32 re;
	INT16 x1 = 0, x2 = 0;

	INT32 offset = 0;

	spos = strtok(word2, " ");

	for (i = 0; i < 4; ++i)
	{
		if (spos != NULL)
		{
			params[i] = spos;
			spos = strtok(NULL, " ");
		}
		else
			params[i] = NULL;
	}

	if (!params[0])
	{
		deh_warning("condition line is empty");
		return;
	}

	if (fastcmp(params[0], "PLAYTIME"))
	{
		PARAMCHECK(1);
		ty = UC_PLAYTIME;
		re = atoi(params[1]);
	}
	else if        (fastcmp(params[0], "GAMECLEAR")
	|| (++offset && fastcmp(params[0], "ALLEMERALDS"))
	|| (++offset && fastcmp(params[0], "ULTIMATECLEAR")))
	{
		ty = UC_GAMECLEAR + offset;
		re = (params[1]) ? atoi(params[1]) : 1;
	}
	else if ((offset=0) || fastcmp(params[0], "OVERALLSCORE")
	||        (++offset && fastcmp(params[0], "OVERALLTIME"))
	||        (++offset && fastcmp(params[0], "OVERALLRINGS")))
	{
		PARAMCHECK(1);
		ty = UC_OVERALLSCORE + offset;
		re = atoi(params[1]);
	}
	else if ((offset=0) || fastcmp(params[0], "MAPVISITED")
	||        (++offset && fastcmp(params[0], "MAPBEATEN"))
	||        (++offset && fastcmp(params[0], "MAPALLEMERALDS"))
	||        (++offset && fastcmp(params[0], "MAPULTIMATE"))
	||        (++offset && fastcmp(params[0], "MAPPERFECT")))
	{
		PARAMCHECK(1);
		ty = UC_MAPVISITED + offset;

		// Convert to map number if it appears to be one
		if (params[1][0] >= 'A' && params[1][0] <= 'Z')
			re = M_MapNumber(params[1][0], params[1][1]);
		else
			re = atoi(params[1]);

		if (re < 0 || re >= NUMMAPS)
		{
			deh_warning("Level number %d out of range (1 - %d)", re, NUMMAPS);
			return;
		}
	}
	else if ((offset=0) || fastcmp(params[0], "MAPSCORE")
	||        (++offset && fastcmp(params[0], "MAPTIME"))
	||        (++offset && fastcmp(params[0], "MAPRINGS")))
	{
		PARAMCHECK(2);
		ty = UC_MAPSCORE + offset;
		re = atoi(params[2]);

		// Convert to map number if it appears to be one
		if (params[1][0] >= 'A' && params[1][0] <= 'Z')
			x1 = (INT16)M_MapNumber(params[1][0], params[1][1]);
		else
			x1 = (INT16)atoi(params[1]);

		if (x1 < 0 || x1 >= NUMMAPS)
		{
			deh_warning("Level number %d out of range (1 - %d)", re, NUMMAPS);
			return;
		}
	}
	else if ((offset=0) || fastcmp(params[0], "NIGHTSSCORE")
	||        (++offset && fastcmp(params[0], "NIGHTSTIME"))
	||        (++offset && fastcmp(params[0], "NIGHTSGRADE")))
	{
		PARAMCHECK(2); // one optional one

		ty = UC_NIGHTSSCORE + offset;
		i = (params[3] ? 3 : 2);
		if (fastncmp("GRADE_",params[i],6))
		{
			char *p = params[i]+6;
			for (re = 0; NIGHTSGRADE_LIST[re]; re++)
				if (*p == NIGHTSGRADE_LIST[re])
					break;
			if (!NIGHTSGRADE_LIST[re])
			{
				deh_warning("Invalid NiGHTS grade %s\n", params[i]);
				return;
			}
		}
		else
			re = atoi(params[i]);

		// Convert to map number if it appears to be one
		if (params[1][0] >= 'A' && params[1][0] <= 'Z')
			x1 = (INT16)M_MapNumber(params[1][0], params[1][1]);
		else
			x1 = (INT16)atoi(params[1]);

		if (x1 < 0 || x1 >= NUMMAPS)
		{
			deh_warning("Level number %d out of range (1 - %d)", re, NUMMAPS);
			return;
		}

		// Mare number (0 for overall)
		if (params[3]) // Only if we actually got 3 params (so the second one == mare and not requirement)
			x2 = (INT16)atoi(params[2]);
		else
			x2 = 0;

	}
	else if (fastcmp(params[0], "TRIGGER"))
	{
		PARAMCHECK(1);
		ty = UC_TRIGGER;
		re = atoi(params[1]);

		// constrained by 32 bits
		if (re < 0 || re > 31)
		{
			deh_warning("Trigger ID %d out of range (0 - 31)", re);
			return;
		}
	}
	else if (fastcmp(params[0], "TOTALEMBLEMS"))
	{
		PARAMCHECK(1);
		ty = UC_TOTALEMBLEMS;
		re = atoi(params[1]);
	}
	else if (fastcmp(params[0], "EMBLEM"))
	{
		PARAMCHECK(1);
		ty = UC_EMBLEM;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXEMBLEMS)
		{
			deh_warning("Emblem %d out of range (1 - %d)", re, MAXEMBLEMS);
			return;
		}
	}
	else if (fastcmp(params[0], "EXTRAEMBLEM"))
	{
		PARAMCHECK(1);
		ty = UC_EXTRAEMBLEM;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXEXTRAEMBLEMS)
		{
			deh_warning("Extra emblem %d out of range (1 - %d)", re, MAXEXTRAEMBLEMS);
			return;
		}
	}
	else if (fastcmp(params[0], "CONDITIONSET"))
	{
		PARAMCHECK(1);
		ty = UC_CONDITIONSET;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXCONDITIONSETS)
		{
			deh_warning("Condition set %d out of range (1 - %d)", re, MAXCONDITIONSETS);
			return;
		}
	}
	else
	{
		deh_warning("Invalid condition name %s", params[0]);
		return;
	}

	M_AddRawCondition(set, (UINT8)id, ty, re, x1, x2);
}
#undef PARAMCHECK

static void readconditionset(MYFILE *f, UINT8 setnum)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	UINT8 id;
	UINT8 previd = 0;

	M_ClearConditionSet(setnum);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			strupr(word2);

			if (fastncmp(word, "CONDITION", 9))
			{
				id = (UINT8)atoi(word + 9);
				if (id == 0)
				{
					deh_warning("Condition set %d: unknown word '%s'", setnum, word);
					continue;
				}
				else if (previd > id)
				{
					// out of order conditions can cause problems, so enforce proper order
					deh_warning("Condition set %d: conditions are out of order, ignoring this line", setnum);
					continue;
				}
				previd = id;

				readcondition(setnum, id, word2);
			}
			else
				deh_warning("Condition set %d: unknown word '%s'", setnum, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readmaincfg(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			strupr(word2);

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "EXECCFG"))
			{
				if (strchr(word2, '.'))
					COM_BufAddText(va("exec %s\n", word2));
				else
				{
					lumpnum_t lumpnum;
					char newname[9];

					strncpy(newname, word2, 8);

					newname[8] = '\0';

					lumpnum = W_CheckNumForName(newname);

					if (lumpnum == LUMPERROR || W_LumpLength(lumpnum) == 0)
						CONS_Debug(DBG_SETUP, "SOC Error: script lump %s not found/not valid.\n", newname);
					else
						COM_BufInsertText(W_CacheLumpNum(lumpnum, PU_CACHE));
				}
			}

			else if (fastcmp(word, "SPSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				spstage_start = (INT16)value;
			}
			else if (fastcmp(word, "SSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				sstage_start = (INT16)value;
				sstage_end = (INT16)(sstage_start+7); // 7 special stages total plus one weirdo
			}
			else if (fastcmp(word, "SMPSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				smpstage_start = (INT16)value;
				smpstage_end = (INT16)(smpstage_start+6); // 7 special stages total
			}
			else if (fastcmp(word, "REDTEAM"))
			{
				skincolor_redteam = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "BLUETEAM"))
			{
				skincolor_blueteam = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "REDRING"))
			{
				skincolor_redring = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "BLUERING"))
			{
				skincolor_bluering = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "INVULNTICS"))
			{
				invulntics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "SNEAKERTICS"))
			{
				sneakertics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "FLASHINGTICS"))
			{
				flashingtics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "TAILSFLYTICS"))
			{
				tailsflytics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "UNDERWATERTICS"))
			{
				underwatertics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "SPACETIMETICS"))
			{
				spacetimetics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "EXTRALIFETICS"))
			{
				extralifetics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "NIGHTSLINKTICS"))
			{
				nightslinktics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "GAMEOVERTICS"))
			{
				gameovertics = get_number(word2);
			}
			else if (fastcmp(word, "AMMOREMOVALTICS"))
			{
				ammoremovaltics = get_number(word2);
			}
			else if (fastcmp(word, "INTROTOPLAY"))
			{
				introtoplay = (UINT8)get_number(word2);
				// range check, you morons.
				if (introtoplay > 128)
					introtoplay = 128;
				introchanged = true;
			}
			else if (fastcmp(word, "LOOPTITLE"))
			{
				looptitle = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				titlemap = (INT16)value;
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDETITLEPICS") || fastcmp(word, "TITLEPICSHIDE"))
			{
				hidetitlepics = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSMODE"))
			{
				if (fastcmp(word2, "USER"))
					ttmode = TTMODE_USER;
				else if (fastcmp(word2, "ALACROIX"))
					ttmode = TTMODE_ALACROIX;
				else if (fastcmp(word2, "HIDE") || fastcmp(word2, "HIDDEN") || fastcmp(word2, "NONE"))
				{
					ttmode = TTMODE_USER;
					ttname[0] = 0;
					hidetitlepics = true;
				}
				else // if (fastcmp(word2, "OLD") || fastcmp(word2, "SSNTAILS"))
					ttmode = TTMODE_OLD;
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSSCALE"))
			{
				ttscale = max(1, min(8, (UINT8)get_number(word2)));
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSSCALESAVAILABLE"))
			{
				// SPECIAL CASE for Alacroix: Comma-separated list of resolutions that are available
				// for gfx loading.
				ttavailable[0] = ttavailable[1] = ttavailable[2] = ttavailable[3] =\
					ttavailable[4] = ttavailable[5] = false;

				if (strstr(word2, "1") != NULL)
					ttavailable[0] = true;
				if (strstr(word2, "2") != NULL)
					ttavailable[1] = true;
				if (strstr(word2, "3") != NULL)
					ttavailable[2] = true;
				if (strstr(word2, "4") != NULL)
					ttavailable[3] = true;
				if (strstr(word2, "5") != NULL)
					ttavailable[4] = true;
				if (strstr(word2, "6") != NULL)
					ttavailable[5] = true;
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSNAME"))
			{
				strncpy(ttname, word2, 9);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSX"))
			{
				ttx = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSY"))
			{
				tty = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSLOOP"))
			{
				ttloop = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSTICS"))
			{
				tttics = (UINT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLSPEED") || fastcmp(word, "TITLESCROLLXSPEED"))
			{
				titlescrollxspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLYSPEED"))
			{
				titlescrollyspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "CREDITSCUTSCENE"))
			{
				creditscutscene = (UINT8)get_number(word2);
				// range check, you morons.
				if (creditscutscene > 128)
					creditscutscene = 128;
			}
			else if (fastcmp(word, "DISABLESPEEDADJUST"))
			{
				disableSpeedAdjust = (value || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "NUMDEMOS"))
			{
				numDemos = (UINT8)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "DEMODELAYTIME"))
			{
				demoDelayTime = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "DEMOIDLETIME"))
			{
				demoIdleTime = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "USE1UPSOUND"))
			{
				use1upSound = (UINT8)(value || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "MAXXTRALIFE"))
			{
				maxXtraLife = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "USECONTINUES"))
			{
				useContinues = (UINT8)(value || word2[0] == 'T' || word2[0] == 'Y');
			}

			else if (fastcmp(word, "GAMEDATA"))
			{
				size_t filenamelen;

				// Check the data filename so that mods
				// can't write arbitrary files.
				if (!GoodDataFileName(word2))
					I_Error("Maincfg: bad data file name '%s'\n", word2);

				G_SaveGameData();
				strlcpy(gamedatafilename, word2, sizeof (gamedatafilename));
				strlwr(gamedatafilename);
				savemoddata = true;

				// Also save a time attack folder
				filenamelen = strlen(gamedatafilename)-4;  // Strip off the extension
				strncpy(timeattackfolder, gamedatafilename, min(filenamelen, sizeof (timeattackfolder)));
				timeattackfolder[min(filenamelen, sizeof (timeattackfolder) - 1)] = '\0';

				strcpy(savegamename, timeattackfolder);
				strlcat(savegamename, "%u.ssg", sizeof(savegamename));
				// can't use sprintf since there is %u in savegamename
				strcatbf(savegamename, srb2home, PATHSEP);

				gamedataadded = true;
				titlechanged = true;
			}
			else if (fastcmp(word, "RESETDATA"))
			{
				P_ResetData(value);
				titlechanged = true;
			}
			else if (fastcmp(word, "CUSTOMVERSION"))
			{
				strlcpy(customversionstring, word2, sizeof (customversionstring));
				//titlechanged = true;
			}
			else if (fastcmp(word, "BOOTMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				bootmap = (INT16)value;
				//titlechanged = true;
			}
			else if (fastcmp(word, "STARTCHAR"))
			{
				startchar = (INT16)value;
				char_on = -1;
			}
			else if (fastcmp(word, "TUTORIALMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				tutorialmap = (INT16)value;
			}
			else
				deh_warning("Maincfg: unknown word '%s'", word);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

static void readwipes(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *pword = word;
	char *word2;
	char *tmp;
	INT32 value;
	INT32 wipeoffset;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			value = atoi(word2); // used for numerical settings

			if (value < -1 || value > 99)
			{
				deh_warning("Wipes: bad value '%s'", word2);
				continue;
			}
			else if (value == -1)
				value = UINT8_MAX;

			// error catching
			wipeoffset = -1;

			if (fastncmp(word, "LEVEL_", 6))
			{
				pword = word + 6;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_level_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_level_final;
			}
			else if (fastncmp(word, "INTERMISSION_", 13))
			{
				pword = word + 13;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_intermission_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_intermission_final;
			}
			else if (fastncmp(word, "SPECINTER_", 10))
			{
				pword = word + 10;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_specinter_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_specinter_final;
			}
			else if (fastncmp(word, "MULTINTER_", 10))
			{
				pword = word + 10;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_multinter_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_multinter_final;
			}
			else if (fastncmp(word, "CONTINUING_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_continuing_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_continuing_final;
			}
			else if (fastncmp(word, "TITLESCREEN_", 12))
			{
				pword = word + 12;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_titlescreen_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_titlescreen_final;
			}
			else if (fastncmp(word, "TIMEATTACK_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_timeattack_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_timeattack_final;
			}
			else if (fastncmp(word, "CREDITS_", 8))
			{
				pword = word + 8;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_credits_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_credits_final;
				else if (fastcmp(pword, "INTERMEDIATE"))
					wipeoffset = wipe_credits_intermediate;
			}
			else if (fastncmp(word, "EVALUATION_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_evaluation_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_evaluation_final;
			}
			else if (fastncmp(word, "GAMEEND_", 8))
			{
				pword = word + 8;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_gameend_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_gameend_final;
			}
			else if (fastncmp(word, "SPECLEVEL_", 10))
			{
				pword = word + 10;
				if (fastcmp(pword, "TOWHITE"))
					wipeoffset = wipe_speclevel_towhite;
			}

			if (wipeoffset < 0)
			{
				deh_warning("Wipes: unknown word '%s'", word);
				continue;
			}

			if (value == UINT8_MAX
			 && (wipeoffset <= wipe_level_toblack || wipeoffset >= wipe_speclevel_towhite))
			{
				 // Cannot disable non-toblack wipes
				 // (or the level toblack wipe, or the special towhite wipe)
				deh_warning("Wipes: can't disable wipe of type '%s'", word);
				continue;
			}

			wipedefs[wipeoffset] = (UINT8)value;
		}
	} while (!myfeof(f));

	Z_Free(s);
}

// Used when you do something invalid like read a bad item number
// to prevent extra unnecessary errors
static void ignorelines(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;
		}
	} while (!myfeof(f));
	Z_Free(s);
}

static void DEH_LoadDehackedFile(MYFILE *f, boolean mainfile)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char textline[MAXLINELEN];
	char *word;
	char *word2;
	INT32 i;

	if (!deh_loaded)
		initfreeslots();

	deh_num_warning = 0;

	gamedataadded = titlechanged = introchanged = false;

	// it doesn't test the version of SRB2 and version of dehacked file
	dbg_line = -1; // start at -1 so the first line is 0.
	while (!myfeof(f))
	{
		char origpos[128];
		INT32 size = 0;
		char *traverse;

		myfgets(s, MAXLINELEN, f);
		memcpy(textline, s, MAXLINELEN);
		if (s[0] == '\n' || s[0] == '#')
			continue;

		traverse = s;

		while (traverse[0] != '\n')
		{
			traverse++;
			size++;
		}

		strncpy(origpos, s, size);
		origpos[size] = '\0';

		if (NULL != (word = strtok(s, " "))) {
			strupr(word);
			if (word[strlen(word)-1] == '\n')
				word[strlen(word)-1] = '\0';
		}
		if (word)
		{
			if (fastcmp(word, "FREESLOT"))
			{
				readfreeslots(f);
				continue;
			}
			else if (fastcmp(word, "MAINCFG"))
			{
				readmaincfg(f);
				continue;
			}
			else if (fastcmp(word, "WIPES"))
			{
				readwipes(f);
				continue;
			}
			word2 = strtok(NULL, " ");
			if (word2) {
				strupr(word2);
				if (word2[strlen(word2) - 1] == '\n')
					word2[strlen(word2) - 1] = '\0';
				i = atoi(word2);
			}
			else
				i = 0;
			if (fastcmp(word, "CHARACTER"))
			{
				if (i >= 0 && i < 32)
					readPlayer(f, i);
				else
				{
					deh_warning("Character %d out of range (0 - 31)", i);
					ignorelines(f);
				}
				continue;
			}
			else if (fastcmp(word, "EMBLEM"))
			{
				if (!mainfile && !gamedataadded)
				{
					deh_warning("You must define a custom gamedata to use \"%s\"", word);
					ignorelines(f);
				}
				else
				{
					if (!word2)
						i = numemblems + 1;

					if (i > 0 && i <= MAXEMBLEMS)
					{
						if (numemblems < i)
							numemblems = i;
						reademblemdata(f, i);
					}
					else
					{
						deh_warning("Emblem number %d out of range (1 - %d)", i, MAXEMBLEMS);
						ignorelines(f);
					}
				}
				continue;
			}
			else if (fastcmp(word, "EXTRAEMBLEM"))
			{
				if (!mainfile && !gamedataadded)
				{
					deh_warning("You must define a custom gamedata to use \"%s\"", word);
					ignorelines(f);
				}
				else
				{
					if (!word2)
						i = numextraemblems + 1;

					if (i > 0 && i <= MAXEXTRAEMBLEMS)
					{
						if (numextraemblems < i)
							numextraemblems = i;
						readextraemblemdata(f, i);
					}
					else
					{
						deh_warning("Extra emblem number %d out of range (1 - %d)", i, MAXEXTRAEMBLEMS);
						ignorelines(f);
					}
				}
				continue;
			}
			if (word2)
			{
				if (fastcmp(word, "THING") || fastcmp(word, "MOBJ") || fastcmp(word, "OBJECT"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_mobjtype(word2); // find a thing by name
					if (i < NUMMOBJTYPES && i > 0)
						readthing(f, i);
					else
					{
						deh_warning("Thing %d out of range (1 - %d)", i, NUMMOBJTYPES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SPRITE2"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_sprite2(word2); // find a sprite by name
					if (i < (INT32)free_spr2 && i >= (INT32)SPR2_FIRSTFREESLOT)
						readsprite2(f, i);
					else
					{
						deh_warning("Sprite2 number %d out of range (%d - %d)", i, SPR2_FIRSTFREESLOT, free_spr2-1);
						ignorelines(f);
					}
				}
#ifdef HWRENDER
				else if (fastcmp(word, "LIGHT"))
				{
					// TODO: Read lights by name
					if (i > 0 && i < NUMLIGHTS)
						readlight(f, i);
					else
					{
						deh_warning("Light number %d out of range (1 - %d)", i, NUMLIGHTS-1);
						ignorelines(f);
					}
				}
#endif
				else if (fastcmp(word, "SPRITE") || fastcmp(word, "SPRITEINFO"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_sprite(word2); // find a sprite by name
					if (i < NUMSPRITES && i > 0)
						readspriteinfo(f, i, false);
					else
					{
						deh_warning("Sprite number %d out of range (0 - %d)", i, NUMSPRITES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SPRITE2INFO"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_sprite2(word2); // find a sprite by name
					if (i < NUMPLAYERSPRITES && i >= 0)
						readspriteinfo(f, i, true);
					else
					{
						deh_warning("Sprite2 number %d out of range (0 - %d)", i, NUMPLAYERSPRITES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "LEVEL"))
				{
					// Support using the actual map name,
					// i.e., Level AB, Level FZ, etc.

					// Convert to map number
					if (word2[0] >= 'A' && word2[0] <= 'Z')
						i = M_MapNumber(word2[0], word2[1]);

					if (i > 0 && i <= NUMMAPS)
						readlevelheader(f, i);
					else
					{
						deh_warning("Level number %d out of range (1 - %d)", i, NUMMAPS);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "GAMETYPE"))
				{
					// Get the gametype name from textline
					// instead of word2, so that gametype names
					// aren't allcaps
					INT32 c;
					for (c = 0; c < MAXLINELEN; c++)
					{
						if (textline[c] == '\0')
							break;
						if (textline[c] == ' ')
						{
							char *gtname = (textline+c+1);
							if (gtname)
							{
								// remove funny characters
								INT32 j;
								for (j = 0; j < (MAXLINELEN - c); j++)
								{
									if (gtname[j] == '\0')
										break;
									if (gtname[j] < 32)
										gtname[j] = '\0';
								}
								readgametype(f, gtname);
							}
							break;
						}
					}
				}
				else if (fastcmp(word, "CUTSCENE"))
				{
					if (i > 0 && i < 129)
						readcutscene(f, i - 1);
					else
					{
						deh_warning("Cutscene number %d out of range (1 - 128)", i);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "PROMPT"))
				{
					if (i > 0 && i < MAX_PROMPTS)
						readtextprompt(f, i - 1);
					else
					{
						deh_warning("Prompt number %d out of range (1 - %d)", i, MAX_PROMPTS);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "FRAME") || fastcmp(word, "STATE"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_state(word2); // find a state by name
					if (i < NUMSTATES && i >= 0)
						readframe(f, i);
					else
					{
						deh_warning("Frame %d out of range (0 - %d)", i, NUMSTATES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SOUND"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_sfx(word2); // find a sound by name
					if (i < NUMSFX && i > 0)
						readsound(f, i);
					else
					{
						deh_warning("Sound %d out of range (1 - %d)", i, NUMSFX-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "HUDITEM"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_huditem(word2); // find a huditem by name
					if (i >= 0 && i < NUMHUDITEMS)
						readhuditem(f, i);
					else
					{
						deh_warning("HUD item number %d out of range (0 - %d)", i, NUMHUDITEMS-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "MENU"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_menutype(word2); // find a huditem by name
					if (i >= 1 && i < NUMMENUTYPES)
						readmenu(f, i);
					else
					{
						// zero-based, but let's start at 1
						deh_warning("Menu number %d out of range (1 - %d)", i, NUMMENUTYPES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "UNLOCKABLE"))
				{
					if (!mainfile && !gamedataadded)
					{
						deh_warning("You must define a custom gamedata to use \"%s\"", word);
						ignorelines(f);
					}
					else if (i > 0 && i <= MAXUNLOCKABLES)
						readunlockable(f, i - 1);
					else
					{
						deh_warning("Unlockable number %d out of range (1 - %d)", i, MAXUNLOCKABLES);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "CONDITIONSET"))
				{
					if (!mainfile && !gamedataadded)
					{
						deh_warning("You must define a custom gamedata to use \"%s\"", word);
						ignorelines(f);
					}
					else if (i > 0 && i <= MAXCONDITIONSETS)
						readconditionset(f, (UINT8)i);
					else
					{
						deh_warning("Condition set number %d out of range (1 - %d)", i, MAXCONDITIONSETS);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SRB2"))
				{
					if (isdigit(word2[0]))
					{
						i = atoi(word2);
						if (i != PATCHVERSION)
						{
							deh_warning(
									"Patch is for SRB2 version %d, "
									"only version %d is supported",
									i,
									PATCHVERSION
							);
						}
					}
					else
					{
						deh_warning(
								"SRB2 version definition has incorrect format, "
								"use \"SRB2 %d\"",
								PATCHVERSION
						);
					}
				}
				// Clear all data in certain locations (mostly for unlocks)
				// Unless you REALLY want to piss people off,
				// define a custom gamedata /before/ doing this!!
				// (then again, modifiedgame will prevent game data saving anyway)
				else if (fastcmp(word, "CLEAR"))
				{
					boolean clearall = (fastcmp(word2, "ALL"));

					if (!mainfile && !gamedataadded)
					{
						deh_warning("You must define a custom gamedata to use \"%s\"", word);
						continue;
					}

					if (clearall || fastcmp(word2, "UNLOCKABLES"))
						memset(&unlockables, 0, sizeof(unlockables));

					if (clearall || fastcmp(word2, "EMBLEMS"))
					{
						memset(&emblemlocations, 0, sizeof(emblemlocations));
						numemblems = 0;
					}

					if (clearall || fastcmp(word2, "EXTRAEMBLEMS"))
					{
						memset(&extraemblems, 0, sizeof(extraemblems));
						numextraemblems = 0;
					}

					if (clearall || fastcmp(word2, "CONDITIONSETS"))
						clear_conditionsets();

					if (clearall || fastcmp(word2, "LEVELS"))
						clear_levels();
				}
				else
					deh_warning("Unknown word: %s", word);
			}
			else
				deh_warning("missing argument for '%s'", word);
		}
		else
			deh_warning("No word in this line: %s", s);
	} // end while

	if (gamedataadded)
		G_LoadGameData();

	if (gamestate == GS_TITLESCREEN)
	{
		if (introchanged)
		{
			menuactive = false;
			I_UpdateMouseGrab();
			COM_BufAddText("playintro");
		}
		else if (titlechanged)
		{
			menuactive = false;
			I_UpdateMouseGrab();
			COM_BufAddText("exitgame"); // Command_ExitGame_f() but delayed
		}
	}

	dbg_line = -1;
	if (deh_num_warning)
	{
		CONS_Printf(M_GetText("%d warning%s in the SOC lump\n"), deh_num_warning, deh_num_warning == 1 ? "" : "s");
		if (devparm) {
			I_Error("%s%s",va(M_GetText("%d warning%s in the SOC lump\n"), deh_num_warning, deh_num_warning == 1 ? "" : "s"), M_GetText("See log.txt for details.\n"));
			//while (!I_GetKey())
				//I_OsPolling();
		}
	}

	deh_loaded = true;
	Z_Free(s);
}

// read dehacked lump in a wad (there is special trick for for deh
// file that are converted to wad in w_wad.c)
void DEH_LoadDehackedLumpPwad(UINT16 wad, UINT16 lump, boolean mainfile)
{
	MYFILE f;
	f.wad = wad;
	f.size = W_LumpLengthPwad(wad, lump);
	f.data = Z_Malloc(f.size + 1, PU_STATIC, NULL);
	W_ReadLumpPwad(wad, lump, f.data);
	f.curpos = f.data;
	f.data[f.size] = 0;
	DEH_LoadDehackedFile(&f, mainfile);
	Z_Free(f.data);
}

void DEH_LoadDehackedLump(lumpnum_t lumpnum)
{
	DEH_LoadDehackedLumpPwad(WADFILENUM(lumpnum),LUMPNUM(lumpnum), false);
}

static const char *const MOBJFLAG_LIST[] = {
	"SPECIAL",
	"SOLID",
	"SHOOTABLE",
	"NOSECTOR",
	"NOBLOCKMAP",
	"PAPERCOLLISION",
	"PUSHABLE",
	"BOSS",
	"SPAWNCEILING",
	"NOGRAVITY",
	"AMBIENT",
	"SLIDEME",
	"NOCLIP",
	"FLOAT",
	"BOXICON",
	"MISSILE",
	"SPRING",
	"BOUNCE",
	"MONITOR",
	"NOTHINK",
	"FIRE",
	"NOCLIPHEIGHT",
	"ENEMY",
	"SCENERY",
	"PAIN",
	"STICKY",
	"NIGHTSITEM",
	"NOCLIPTHING",
	"GRENADEBOUNCE",
	"RUNSPAWNFUNC",
	NULL
};

// \tMF2_(\S+).*// (.+) --> \t"\1", // \2
static const char *const MOBJFLAG2_LIST[] = {
	"AXIS",			  // It's a NiGHTS axis! (For faster checking)
	"TWOD",			  // Moves like it's in a 2D level
	"DONTRESPAWN",	  // Don't respawn this object!
	"DONTDRAW",		  // Don't generate a vissprite
	"AUTOMATIC",	  // Thrown ring has automatic properties
	"RAILRING",		  // Thrown ring has rail properties
	"BOUNCERING",	  // Thrown ring has bounce properties
	"EXPLOSION",	  // Thrown ring has explosive properties
	"SCATTER",		  // Thrown ring has scatter properties
	"BEYONDTHEGRAVE", // Source of this missile has died and has since respawned.
	"SLIDEPUSH",	  // MF_PUSHABLE that pushes continuously.
	"CLASSICPUSH",    // Drops straight down when object has negative momz.
	"INVERTAIMABLE",  // Flips whether it's targetable by A_LookForEnemies (enemies no, decoys yes)
	"INFLOAT",		  // Floating to a height for a move, don't auto float to target's height.
	"DEBRIS",		  // Splash ring from explosion ring
	"NIGHTSPULL",	  // Attracted from a paraloop
	"JUSTATTACKED",	  // can be pushed by other moving mobjs
	"FIRING",		  // turret fire
	"SUPERFIRE",	  // Firing something with Super Sonic-stopping properties. Or, if mobj has MF_MISSILE, this is the actual fire from it.
	"SHADOW",		  // Fuzzy draw, makes targeting harder.
	"STRONGBOX",	  // Flag used for "strong" random monitors.
	"OBJECTFLIP",	  // Flag for objects that always have flipped gravity.
	"SKULLFLY",		  // Special handling: skull in flight.
	"FRET",			  // Flashing from a previous hit
	"BOSSNOTRAP",	  // No Egg Trap after boss
	"BOSSFLEE",		  // Boss is fleeing!
	"BOSSDEAD",		  // Boss is dead! (Not necessarily fleeing, if a fleeing point doesn't exist.)
	"AMBUSH",         // Alternate behaviour typically set by MTF_AMBUSH
	"LINKDRAW",       // Draw vissprite of mobj immediately before/after tracer's vissprite (dependent on dispoffset and position)
	"SHIELD",         // Thinker calls P_AddShield/P_ShieldLook (must be partnered with MF_SCENERY to use)
	NULL
};

static const char *const MOBJEFLAG_LIST[] = {
	"ONGROUND", // The mobj stands on solid floor (not on another mobj or in air)
	"JUSTHITFLOOR", // The mobj just hit the floor while falling, this is cleared on next frame
	"TOUCHWATER", // The mobj stands in a sector with water, and touches the surface
	"UNDERWATER", // The mobj stands in a sector with water, and his waist is BELOW the water surface
	"JUSTSTEPPEDDOWN", // used for ramp sectors
	"VERTICALFLIP", // Vertically flip sprite/allow upside-down physics
	"GOOWATER", // Goo water
	"TOUCHLAVA", // The mobj is touching a lava block
	"PUSHED", // Mobj was already pushed this tic
	"SPRUNG", // Mobj was already sprung this tic
	"APPLYPMOMZ", // Platform movement
	"TRACERANGLE", // Compute and trigger on mobj angle relative to tracer
	NULL
};

static const char *const MAPTHINGFLAG_LIST[4] = {
	"EXTRA", // Extra flag for objects.
	"OBJECTFLIP", // Reverse gravity flag for objects.
	"OBJECTSPECIAL", // Special flag used with certain objects.
	"AMBUSH" // Deaf monsters/do not react to sound.
};

static const char *const PLAYERFLAG_LIST[] = {

	// Cvars
	"FLIPCAM", // Flip camera angle with gravity flip prefrence.
	"ANALOGMODE", // Analog mode?
	"DIRECTIONCHAR", // Directional character sprites?
	"AUTOBRAKE", // Autobrake?

	// Cheats
	"GODMODE",
	"NOCLIP",
	"INVIS",

	// True if button down last tic.
	"ATTACKDOWN",
	"USEDOWN",
	"JUMPDOWN",
	"WPNDOWN",

	// Unmoving states
	"STASIS", // Player is not allowed to move
	"JUMPSTASIS", // and that includes jumping.
	// (we don't include FULLSTASIS here I guess because it's just those two together...?)

	// Applying autobrake?
	"APPLYAUTOBRAKE",

	// Character action status
	"STARTJUMP",
	"JUMPED",
	"NOJUMPDAMAGE",

	"SPINNING",
	"STARTDASH",

	"THOKKED",
	"SHIELDABILITY",
	"GLIDING",
	"BOUNCING",

	// Sliding (usually in water) like Labyrinth/Oil Ocean
	"SLIDING",

	// NiGHTS stuff
	"TRANSFERTOCLOSEST",
	"DRILLING",

	// Gametype-specific stuff
	"GAMETYPEOVER", // Race time over, or H&S out-of-game
	"TAGIT", // The player is it! For Tag Mode

	/*** misc ***/
	"FORCESTRAFE", // Translate turn inputs into strafe inputs
	"CANCARRY", // Can carry?
	"FINISHED",

	NULL // stop loop here.
};

static const char *const GAMETYPERULE_LIST[] = {
	"CAMPAIGN",
	"RINGSLINGER",
	"SPECTATORS",
	"LIVES",
	"TEAMS",
	"FIRSTPERSON",
	"POWERSTONES",
	"TEAMFLAGS",
	"FRIENDLY",
	"SPECIALSTAGES",
	"EMERALDTOKENS",
	"EMERALDHUNT",
	"RACE",
	"TAG",
	"POINTLIMIT",
	"TIMELIMIT",
	"OVERTIME",
	"HURTMESSAGES",
	"FRIENDLYFIRE",
	"STARTCOUNTDOWN",
	"HIDEFROZEN",
	"BLINDFOLDED",
	"RESPAWNDELAY",
	"PITYSHIELD",
	"DEATHPENALTY",
	"NOSPECTATORSPAWN",
	"DEATHMATCHSTARTS",
	"SPAWNINVUL",
	"SPAWNENEMIES",
	"ALLOWEXIT",
	"NOTITLECARD",
	"CUTSCENES",
	NULL
};

// Linedef flags
static const char *const ML_LIST[16] = {
	"IMPASSIBLE",
	"BLOCKMONSTERS",
	"TWOSIDED",
	"DONTPEGTOP",
	"DONTPEGBOTTOM",
	"EFFECT1",
	"NOCLIMB",
	"EFFECT2",
	"EFFECT3",
	"EFFECT4",
	"EFFECT5",
	"NOSONIC",
	"NOTAILS",
	"NOKNUX",
	"BOUNCY",
	"TFERLINE"
};

// This DOES differ from r_draw's Color_Names, unfortunately.
// Also includes Super colors
static const char *COLOR_ENUMS[] = {
	"NONE",			// SKINCOLOR_NONE,

	// Greyscale ranges
	"WHITE",     	// SKINCOLOR_WHITE,
	"BONE",     	// SKINCOLOR_BONE,
	"CLOUDY",     	// SKINCOLOR_CLOUDY,
	"GREY",     	// SKINCOLOR_GREY,
	"SILVER",     	// SKINCOLOR_SILVER,
	"CARBON",     	// SKINCOLOR_CARBON,
	"JET",     		// SKINCOLOR_JET,
	"BLACK",     	// SKINCOLOR_BLACK,

	// Desaturated
	"AETHER",     	// SKINCOLOR_AETHER,
	"SLATE",     	// SKINCOLOR_SLATE,
	"BLUEBELL",   	// SKINCOLOR_BLUEBELL,
	"PINK",     	// SKINCOLOR_PINK,
	"YOGURT",     	// SKINCOLOR_YOGURT,
	"BROWN",     	// SKINCOLOR_BROWN,
	"BRONZE",     	// SKINCOLOR_BRONZE,
	"TAN",     		// SKINCOLOR_TAN,
	"BEIGE",     	// SKINCOLOR_BEIGE,
	"MOSS",     	// SKINCOLOR_MOSS,
	"AZURE",     	// SKINCOLOR_AZURE,
	"LAVENDER",     // SKINCOLOR_LAVENDER,

	// Viv's vivid colours (toast 21/07/17)
	"RUBY",     	// SKINCOLOR_RUBY,
	"SALMON",     	// SKINCOLOR_SALMON,
	"RED",     		// SKINCOLOR_RED,
	"CRIMSON",     	// SKINCOLOR_CRIMSON,
	"FLAME",     	// SKINCOLOR_FLAME,
	"KETCHUP",     	// SKINCOLOR_KETCHUP,
	"PEACHY",     	// SKINCOLOR_PEACHY,
	"QUAIL",     	// SKINCOLOR_QUAIL,
	"SUNSET",     	// SKINCOLOR_SUNSET,
	"COPPER",     	// SKINCOLOR_COPPER,
	"APRICOT",     	// SKINCOLOR_APRICOT,
	"ORANGE",     	// SKINCOLOR_ORANGE,
	"RUST",     	// SKINCOLOR_RUST,
	"GOLD",     	// SKINCOLOR_GOLD,
	"SANDY",     	// SKINCOLOR_SANDY,
	"YELLOW",     	// SKINCOLOR_YELLOW,
	"OLIVE",     	// SKINCOLOR_OLIVE,
	"LIME",     	// SKINCOLOR_LIME,
	"PERIDOT",     	// SKINCOLOR_PERIDOT,
	"APPLE",     	// SKINCOLOR_APPLE,
	"GREEN",     	// SKINCOLOR_GREEN,
	"FOREST",     	// SKINCOLOR_FOREST,
	"EMERALD",     	// SKINCOLOR_EMERALD,
	"MINT",     	// SKINCOLOR_MINT,
	"SEAFOAM",     	// SKINCOLOR_SEAFOAM,
	"AQUA",     	// SKINCOLOR_AQUA,
	"TEAL",     	// SKINCOLOR_TEAL,
	"WAVE",     	// SKINCOLOR_WAVE,
	"CYAN",     	// SKINCOLOR_CYAN,
	"SKY",     		// SKINCOLOR_SKY,
	"CERULEAN",     // SKINCOLOR_CERULEAN,
	"ICY",     		// SKINCOLOR_ICY,
	"SAPPHIRE",     // SKINCOLOR_SAPPHIRE,
	"CORNFLOWER",   // SKINCOLOR_CORNFLOWER,
	"BLUE",     	// SKINCOLOR_BLUE,
	"COBALT",     	// SKINCOLOR_COBALT,
	"VAPOR",     	// SKINCOLOR_VAPOR,
	"DUSK",     	// SKINCOLOR_DUSK,
	"PASTEL",     	// SKINCOLOR_PASTEL,
	"PURPLE",     	// SKINCOLOR_PURPLE,
	"BUBBLEGUM",    // SKINCOLOR_BUBBLEGUM,
	"MAGENTA",     	// SKINCOLOR_MAGENTA,
	"NEON",     	// SKINCOLOR_NEON,
	"VIOLET",     	// SKINCOLOR_VIOLET,
	"LILAC",     	// SKINCOLOR_LILAC,
	"PLUM",     	// SKINCOLOR_PLUM,
	"RASPBERRY",  	// SKINCOLOR_RASPBERRY,
	"ROSY",     	// SKINCOLOR_ROSY,

	// Super special awesome Super flashing colors!
	"SUPERSILVER1",	// SKINCOLOR_SUPERSILVER1
	"SUPERSILVER2",	// SKINCOLOR_SUPERSILVER2,
	"SUPERSILVER3",	// SKINCOLOR_SUPERSILVER3,
	"SUPERSILVER4",	// SKINCOLOR_SUPERSILVER4,
	"SUPERSILVER5",	// SKINCOLOR_SUPERSILVER5,

	"SUPERRED1",	// SKINCOLOR_SUPERRED1
	"SUPERRED2",	// SKINCOLOR_SUPERRED2,
	"SUPERRED3",	// SKINCOLOR_SUPERRED3,
	"SUPERRED4",	// SKINCOLOR_SUPERRED4,
	"SUPERRED5",	// SKINCOLOR_SUPERRED5,

	"SUPERORANGE1",	// SKINCOLOR_SUPERORANGE1
	"SUPERORANGE2",	// SKINCOLOR_SUPERORANGE2,
	"SUPERORANGE3",	// SKINCOLOR_SUPERORANGE3,
	"SUPERORANGE4",	// SKINCOLOR_SUPERORANGE4,
	"SUPERORANGE5",	// SKINCOLOR_SUPERORANGE5,

	"SUPERGOLD1",	// SKINCOLOR_SUPERGOLD1
	"SUPERGOLD2",	// SKINCOLOR_SUPERGOLD2,
	"SUPERGOLD3",	// SKINCOLOR_SUPERGOLD3,
	"SUPERGOLD4",	// SKINCOLOR_SUPERGOLD4,
	"SUPERGOLD5",	// SKINCOLOR_SUPERGOLD5,

	"SUPERPERIDOT1",	// SKINCOLOR_SUPERPERIDOT1
	"SUPERPERIDOT2",	// SKINCOLOR_SUPERPERIDOT2,
	"SUPERPERIDOT3",	// SKINCOLOR_SUPERPERIDOT3,
	"SUPERPERIDOT4",	// SKINCOLOR_SUPERPERIDOT4,
	"SUPERPERIDOT5",	// SKINCOLOR_SUPERPERIDOT5,

	"SUPERSKY1",	// SKINCOLOR_SUPERSKY1
	"SUPERSKY2",	// SKINCOLOR_SUPERSKY2,
	"SUPERSKY3",	// SKINCOLOR_SUPERSKY3,
	"SUPERSKY4",	// SKINCOLOR_SUPERSKY4,
	"SUPERSKY5",	// SKINCOLOR_SUPERSKY5,

	"SUPERPURPLE1",	// SKINCOLOR_SUPERPURPLE1,
	"SUPERPURPLE2",	// SKINCOLOR_SUPERPURPLE2,
	"SUPERPURPLE3",	// SKINCOLOR_SUPERPURPLE3,
	"SUPERPURPLE4",	// SKINCOLOR_SUPERPURPLE4,
	"SUPERPURPLE5",	// SKINCOLOR_SUPERPURPLE5,

	"SUPERRUST1",	// SKINCOLOR_SUPERRUST1
	"SUPERRUST2",	// SKINCOLOR_SUPERRUST2,
	"SUPERRUST3",	// SKINCOLOR_SUPERRUST3,
	"SUPERRUST4",	// SKINCOLOR_SUPERRUST4,
	"SUPERRUST5",	// SKINCOLOR_SUPERRUST5,

	"SUPERTAN1",	// SKINCOLOR_SUPERTAN1
	"SUPERTAN2",	// SKINCOLOR_SUPERTAN2,
	"SUPERTAN3",	// SKINCOLOR_SUPERTAN3,
	"SUPERTAN4",	// SKINCOLOR_SUPERTAN4,
	"SUPERTAN5"		// SKINCOLOR_SUPERTAN5,
};

static const char *const POWERS_LIST[] = {
	"INVULNERABILITY",
	"SNEAKERS",
	"FLASHING",
	"SHIELD",
	"CARRY",
	"TAILSFLY", // tails flying
	"UNDERWATER", // underwater timer
	"SPACETIME", // In space, no one can hear you spin!
	"EXTRALIFE", // Extra Life timer
	"PUSHING",
	"JUSTSPRUNG",
	"NOAUTOBRAKE",

	"SUPER", // Are you super?
	"GRAVITYBOOTS", // gravity boots

	// Weapon ammunition
	"INFINITYRING",
	"AUTOMATICRING",
	"BOUNCERING",
	"SCATTERRING",
	"GRENADERING",
	"EXPLOSIONRING",
	"RAILRING",

	// Power Stones
	"EMERALDS", // stored like global 'emeralds' variable

	// NiGHTS powerups
	"NIGHTS_SUPERLOOP",
	"NIGHTS_HELPER",
	"NIGHTS_LINKFREEZE",

	//for linedef exec 427
	"NOCONTROL",

	//for dyes
	"DYE",

	"JUSTLAUNCHED"
};

static const char *const HUDITEMS_LIST[] = {
	"LIVES",

	"RINGS",
	"RINGSNUM",
	"RINGSNUMTICS",

	"SCORE",
	"SCORENUM",

	"TIME",
	"MINUTES",
	"TIMECOLON",
	"SECONDS",
	"TIMETICCOLON",
	"TICS",

	"SS_TOTALRINGS",

	"GETRINGS",
	"GETRINGSNUM",
	"TIMELEFT",
	"TIMELEFTNUM",
	"TIMEUP",
	"HUNTPICS",
	"POWERUPS"
};

static const char *const MENUTYPES_LIST[] = {
	"NONE",

	"MAIN",

	// Single Player
	"SP_MAIN",

	"SP_LOAD",
	"SP_PLAYER",

	"SP_LEVELSELECT",
	"SP_LEVELSTATS",

	"SP_TIMEATTACK",
	"SP_TIMEATTACK_LEVELSELECT",
	"SP_GUESTREPLAY",
	"SP_REPLAY",
	"SP_GHOST",

	"SP_NIGHTSATTACK",
	"SP_NIGHTS_LEVELSELECT",
	"SP_NIGHTS_GUESTREPLAY",
	"SP_NIGHTS_REPLAY",
	"SP_NIGHTS_GHOST",

	// Multiplayer
	"MP_MAIN",
	"MP_SPLITSCREEN", // SplitServer
	"MP_SERVER",
	"MP_CONNECT",
	"MP_ROOM",
	"MP_PLAYERSETUP", // MP_PlayerSetupDef shared with SPLITSCREEN if #defined NONET
	"MP_SERVER_OPTIONS",

	// Options
	"OP_MAIN",

	"OP_P1CONTROLS",
	"OP_CHANGECONTROLS", // OP_ChangeControlsDef shared with P2
	"OP_P1MOUSE",
	"OP_P1JOYSTICK",
	"OP_JOYSTICKSET", // OP_JoystickSetDef shared with P2
	"OP_P1CAMERA",

	"OP_P2CONTROLS",
	"OP_P2MOUSE",
	"OP_P2JOYSTICK",
	"OP_P2CAMERA",

	"OP_PLAYSTYLE",

	"OP_VIDEO",
	"OP_VIDEOMODE",
	"OP_COLOR",
	"OP_OPENGL",
	"OP_OPENGL_LIGHTING",
	"OP_OPENGL_FOG",
	"OP_OPENGL_COLOR",

	"OP_SOUND",

	"OP_SERVER",
	"OP_MONITORTOGGLE",

	"OP_DATA",
	"OP_ADDONS",
	"OP_SCREENSHOTS",
	"OP_ERASEDATA",

	// Extras
	"SR_MAIN",
	"SR_PANDORA",
	"SR_LEVELSELECT",
	"SR_UNLOCKCHECKLIST",
	"SR_EMBLEMHINT",
	"SR_PLAYER",
	"SR_SOUNDTEST",

	// Addons (Part of MISC, but let's make it our own)
	"AD_MAIN",

	// MISC
	// "MESSAGE",
	// "SPAUSE",

	// "MPAUSE",
	// "SCRAMBLETEAM",
	// "CHANGETEAM",
	// "CHANGELEVEL",

	// "MAPAUSE",
	// "HELP",

	"SPECIAL"
};

struct {
	const char *n;
	// has to be able to hold both fixed_t and angle_t, so drastic measure!!
	lua_Integer v;
} const INT_CONST[] = {
	// If a mod removes some variables here,
	// please leave the names in-tact and just set
	// the value to 0 or something.

	// integer type limits, from doomtype.h
	// INT64 and UINT64 limits not included, they're too big for most purposes anyway
	// signed
	{"INT8_MIN",INT8_MIN},
	{"INT16_MIN",INT16_MIN},
	{"INT32_MIN",INT32_MIN},
	{"INT8_MAX",INT8_MAX},
	{"INT16_MAX",INT16_MAX},
	{"INT32_MAX",INT32_MAX},
	// unsigned
	{"UINT8_MAX",UINT8_MAX},
	{"UINT16_MAX",UINT16_MAX},
	{"UINT32_MAX",UINT32_MAX},

	// fixed_t constants, from m_fixed.h
	{"FRACUNIT",FRACUNIT},
	{"FRACBITS",FRACBITS},

	// doomdef.h constants
	{"TICRATE",TICRATE},
	{"MUSICRATE",MUSICRATE},
	{"RING_DIST",RING_DIST},
	{"PUSHACCEL",PUSHACCEL},
	{"MODID",MODID}, // I don't know, I just thought it would be cool for a wad to potentially know what mod it was loaded into.
	{"CODEBASE",CODEBASE}, // or what release of SRB2 this is.
	{"VERSION",VERSION}, // Grab the game's version!
	{"SUBVERSION",SUBVERSION}, // more precise version number
	{"NEWTICRATE",NEWTICRATE}, // TICRATE*NEWTICRATERATIO
	{"NEWTICRATERATIO",NEWTICRATERATIO},

	// Special linedef executor tag numbers!
	{"LE_PINCHPHASE",LE_PINCHPHASE}, // A boss entered pinch phase (and, in most cases, is preparing their pinch phase attack!)
	{"LE_ALLBOSSESDEAD",LE_ALLBOSSESDEAD}, // All bosses in the map are dead (Egg capsule raise)
	{"LE_BOSSDEAD",LE_BOSSDEAD}, // A boss in the map died (Chaos mode boss tally)
	{"LE_BOSS4DROP",LE_BOSS4DROP}, // CEZ boss dropped its cage
	{"LE_BRAKVILEATACK",LE_BRAKVILEATACK}, // Brak's doing his LOS attack, oh noes
	{"LE_TURRET",LE_TURRET}, // THZ turret
	{"LE_BRAKPLATFORM",LE_BRAKPLATFORM}, // v2.0 Black Eggman destroys platform
	{"LE_CAPSULE2",LE_CAPSULE2}, // Egg Capsule
	{"LE_CAPSULE1",LE_CAPSULE1}, // Egg Capsule
	{"LE_CAPSULE0",LE_CAPSULE0}, // Egg Capsule
	{"LE_KOOPA",LE_KOOPA}, // Distant cousin to Gay Bowser
	{"LE_AXE",LE_AXE}, // MKB Axe object
	{"LE_PARAMWIDTH",LE_PARAMWIDTH},  // If an object that calls LinedefExecute has a nonzero parameter value, this times the parameter will be subtracted. (Mostly for the purpose of coexisting bosses...)

	/// \todo Get all this stuff into its own sections, maybe. Maybe.

	// Frame settings
	{"FF_FRAMEMASK",FF_FRAMEMASK},
	{"FF_SPR2SUPER",FF_SPR2SUPER},
	{"FF_SPR2ENDSTATE",FF_SPR2ENDSTATE},
	{"FF_SPR2MIDSTART",FF_SPR2MIDSTART},
	{"FF_ANIMATE",FF_ANIMATE},
	{"FF_RANDOMANIM",FF_RANDOMANIM},
	{"FF_GLOBALANIM",FF_GLOBALANIM},
	{"FF_FULLBRIGHT",FF_FULLBRIGHT},
	{"FF_VERTICALFLIP",FF_VERTICALFLIP},
	{"FF_PAPERSPRITE",FF_PAPERSPRITE},
	{"FF_TRANSMASK",FF_TRANSMASK},
	{"FF_TRANSSHIFT",FF_TRANSSHIFT},
	// new preshifted translucency (used in source)
	{"FF_TRANS10",FF_TRANS10},
	{"FF_TRANS20",FF_TRANS20},
	{"FF_TRANS30",FF_TRANS30},
	{"FF_TRANS40",FF_TRANS40},
	{"FF_TRANS50",FF_TRANS50},
	{"FF_TRANS60",FF_TRANS60},
	{"FF_TRANS70",FF_TRANS70},
	{"FF_TRANS80",FF_TRANS80},
	{"FF_TRANS90",FF_TRANS90},
	// compatibility
	// Transparency for SOCs is pre-shifted
	{"TR_TRANS10",tr_trans10<<FF_TRANSSHIFT},
	{"TR_TRANS20",tr_trans20<<FF_TRANSSHIFT},
	{"TR_TRANS30",tr_trans30<<FF_TRANSSHIFT},
	{"TR_TRANS40",tr_trans40<<FF_TRANSSHIFT},
	{"TR_TRANS50",tr_trans50<<FF_TRANSSHIFT},
	{"TR_TRANS60",tr_trans60<<FF_TRANSSHIFT},
	{"TR_TRANS70",tr_trans70<<FF_TRANSSHIFT},
	{"TR_TRANS80",tr_trans80<<FF_TRANSSHIFT},
	{"TR_TRANS90",tr_trans90<<FF_TRANSSHIFT},
	// Transparency for Lua is not, unless capitalized as above.
	{"tr_trans10",tr_trans10},
	{"tr_trans20",tr_trans20},
	{"tr_trans30",tr_trans30},
	{"tr_trans40",tr_trans40},
	{"tr_trans50",tr_trans50},
	{"tr_trans60",tr_trans60},
	{"tr_trans70",tr_trans70},
	{"tr_trans80",tr_trans80},
	{"tr_trans90",tr_trans90},
	{"NUMTRANSMAPS",NUMTRANSMAPS},

	// Level flags
	{"LF_SCRIPTISFILE",LF_SCRIPTISFILE},
	{"LF_SPEEDMUSIC",LF_SPEEDMUSIC},
	{"LF_NOSSMUSIC",LF_NOSSMUSIC},
	{"LF_NORELOAD",LF_NORELOAD},
	{"LF_NOZONE",LF_NOZONE},
	{"LF_SAVEGAME",LF_SAVEGAME},
	{"LF_MIXNIGHTSCOUNTDOWN",LF_MIXNIGHTSCOUNTDOWN},
	{"LF_NOTITLECARDFIRST",LF_NOTITLECARDFIRST},
	{"LF_NOTITLECARDRESPAWN",LF_NOTITLECARDRESPAWN},
	{"LF_NOTITLECARDRECORDATTACK",LF_NOTITLECARDRECORDATTACK},
	{"LF_NOTITLECARD",LF_NOTITLECARD},
	{"LF_WARNINGTITLE",LF_WARNINGTITLE},
	// And map flags
	{"LF2_HIDEINMENU",LF2_HIDEINMENU},
	{"LF2_HIDEINSTATS",LF2_HIDEINSTATS},
	{"LF2_RECORDATTACK",LF2_RECORDATTACK},
	{"LF2_NIGHTSATTACK",LF2_NIGHTSATTACK},
	{"LF2_NOVISITNEEDED",LF2_NOVISITNEEDED},
	{"LF2_WIDEICON",LF2_WIDEICON},

	// Emeralds
	{"EMERALD1",EMERALD1},
	{"EMERALD2",EMERALD2},
	{"EMERALD3",EMERALD3},
	{"EMERALD4",EMERALD4},
	{"EMERALD5",EMERALD5},
	{"EMERALD6",EMERALD6},
	{"EMERALD7",EMERALD7},

	// SKINCOLOR_ doesn't include these..!
	{"MAXSKINCOLORS",MAXSKINCOLORS},
	{"MAXTRANSLATIONS",MAXTRANSLATIONS},

	// Precipitation
	{"PRECIP_NONE",PRECIP_NONE},
	{"PRECIP_STORM",PRECIP_STORM},
	{"PRECIP_SNOW",PRECIP_SNOW},
	{"PRECIP_RAIN",PRECIP_RAIN},
	{"PRECIP_BLANK",PRECIP_BLANK},
	{"PRECIP_STORM_NORAIN",PRECIP_STORM_NORAIN},
	{"PRECIP_STORM_NOSTRIKES",PRECIP_STORM_NOSTRIKES},

	// Shields
	{"SH_NONE",SH_NONE},
	// Shield flags
	{"SH_PROTECTFIRE",SH_PROTECTFIRE},
	{"SH_PROTECTWATER",SH_PROTECTWATER},
	{"SH_PROTECTELECTRIC",SH_PROTECTELECTRIC},
	{"SH_PROTECTSPIKE",SH_PROTECTSPIKE},
	// Indivisible shields
	{"SH_PITY",SH_PITY},
	{"SH_WHIRLWIND",SH_WHIRLWIND},
	{"SH_ARMAGEDDON",SH_ARMAGEDDON},
	{"SH_PINK",SH_PINK},
	// normal shields that use flags
	{"SH_ATTRACT",SH_ATTRACT},
	{"SH_ELEMENTAL",SH_ELEMENTAL},
	// Sonic 3 shields
	{"SH_FLAMEAURA",SH_FLAMEAURA},
	{"SH_BUBBLEWRAP",SH_BUBBLEWRAP},
	{"SH_THUNDERCOIN",SH_THUNDERCOIN},
	// The force shield uses the lower 8 bits to count how many extra hits are left.
	{"SH_FORCE",SH_FORCE},
	{"SH_FORCEHP",SH_FORCEHP}, // to be used as a bitmask only
	// Mostly for use with Mario mode.
	{"SH_FIREFLOWER",SH_FIREFLOWER},
	{"SH_STACK",SH_STACK},
	{"SH_NOSTACK",SH_NOSTACK},

	// Carrying
	{"CR_NONE",CR_NONE},
	{"CR_GENERIC",CR_GENERIC},
	{"CR_PLAYER",CR_PLAYER},
	{"CR_NIGHTSMODE",CR_NIGHTSMODE},
	{"CR_NIGHTSFALL",CR_NIGHTSFALL},
	{"CR_BRAKGOOP",CR_BRAKGOOP},
	{"CR_ZOOMTUBE",CR_ZOOMTUBE},
	{"CR_ROPEHANG",CR_ROPEHANG},
	{"CR_MACESPIN",CR_MACESPIN},
	{"CR_MINECART",CR_MINECART},
	{"CR_ROLLOUT",CR_ROLLOUT},
	{"CR_PTERABYTE",CR_PTERABYTE},

	// Ring weapons (ringweapons_t)
	// Useful for A_GiveWeapon
	{"RW_AUTO",RW_AUTO},
	{"RW_BOUNCE",RW_BOUNCE},
	{"RW_SCATTER",RW_SCATTER},
	{"RW_GRENADE",RW_GRENADE},
	{"RW_EXPLODE",RW_EXPLODE},
	{"RW_RAIL",RW_RAIL},

	// Character flags (skinflags_t)
	{"SF_SUPER",SF_SUPER},
	{"SF_NOSUPERSPIN",SF_NOSUPERSPIN},
	{"SF_NOSPINDASHDUST",SF_NOSPINDASHDUST},
	{"SF_HIRES",SF_HIRES},
	{"SF_NOSKID",SF_NOSKID},
	{"SF_NOSPEEDADJUST",SF_NOSPEEDADJUST},
	{"SF_RUNONWATER",SF_RUNONWATER},
	{"SF_NOJUMPSPIN",SF_NOJUMPSPIN},
	{"SF_NOJUMPDAMAGE",SF_NOJUMPDAMAGE},
	{"SF_STOMPDAMAGE",SF_STOMPDAMAGE},
	{"SF_MARIODAMAGE",SF_MARIODAMAGE},
	{"SF_MACHINE",SF_MACHINE},
	{"SF_DASHMODE",SF_DASHMODE},
	{"SF_FASTEDGE",SF_FASTEDGE},
	{"SF_MULTIABILITY",SF_MULTIABILITY},
	{"SF_NONIGHTSROTATION",SF_NONIGHTSROTATION},
	{"SF_NONIGHTSSUPER",SF_NONIGHTSSUPER},

	// Dashmode constants
	{"DASHMODE_THRESHOLD",DASHMODE_THRESHOLD},
	{"DASHMODE_MAX",DASHMODE_MAX},

	// Character abilities!
	// Primary
	{"CA_NONE",CA_NONE}, // now slot 0!
	{"CA_THOK",CA_THOK},
	{"CA_FLY",CA_FLY},
	{"CA_GLIDEANDCLIMB",CA_GLIDEANDCLIMB},
	{"CA_HOMINGTHOK",CA_HOMINGTHOK},
	{"CA_DOUBLEJUMP",CA_DOUBLEJUMP},
	{"CA_FLOAT",CA_FLOAT},
	{"CA_SLOWFALL",CA_SLOWFALL},
	{"CA_SWIM",CA_SWIM},
	{"CA_TELEKINESIS",CA_TELEKINESIS},
	{"CA_FALLSWITCH",CA_FALLSWITCH},
	{"CA_JUMPBOOST",CA_JUMPBOOST},
	{"CA_AIRDRILL",CA_AIRDRILL},
	{"CA_JUMPTHOK",CA_JUMPTHOK},
	{"CA_BOUNCE",CA_BOUNCE},
	{"CA_TWINSPIN",CA_TWINSPIN},
	// Secondary
	{"CA2_NONE",CA2_NONE}, // now slot 0!
	{"CA2_SPINDASH",CA2_SPINDASH},
	{"CA2_GUNSLINGER",CA2_GUNSLINGER},
	{"CA2_MELEE",CA2_MELEE},

	// Sound flags
	{"SF_TOTALLYSINGLE",SF_TOTALLYSINGLE},
	{"SF_NOMULTIPLESOUND",SF_NOMULTIPLESOUND},
	{"SF_OUTSIDESOUND",SF_OUTSIDESOUND},
	{"SF_X4AWAYSOUND",SF_X4AWAYSOUND},
	{"SF_X8AWAYSOUND",SF_X8AWAYSOUND},
	{"SF_NOINTERRUPT",SF_NOINTERRUPT},
	{"SF_X2AWAYSOUND",SF_X2AWAYSOUND},

	// Global emblem var flags
	{"GE_NIGHTSPULL",GE_NIGHTSPULL},
	{"GE_NIGHTSITEM",GE_NIGHTSITEM},

	// Map emblem var flags
	{"ME_ALLEMERALDS",ME_ALLEMERALDS},
	{"ME_ULTIMATE",ME_ULTIMATE},
	{"ME_PERFECT",ME_PERFECT},

	// p_local.h constants
	{"FLOATSPEED",FLOATSPEED},
	{"MAXSTEPMOVE",MAXSTEPMOVE},
	{"USERANGE",USERANGE},
	{"MELEERANGE",MELEERANGE},
	{"MISSILERANGE",MISSILERANGE},
	{"ONFLOORZ",ONFLOORZ}, // INT32_MIN
	{"ONCEILINGZ",ONCEILINGZ}, //INT32_MAX
	// for P_FlashPal
	{"PAL_WHITE",PAL_WHITE},
	{"PAL_MIXUP",PAL_MIXUP},
	{"PAL_RECYCLE",PAL_RECYCLE},
	{"PAL_NUKE",PAL_NUKE},
	// for P_DamageMobj
	//// Damage types
	{"DMG_WATER",DMG_WATER},
	{"DMG_FIRE",DMG_FIRE},
	{"DMG_ELECTRIC",DMG_ELECTRIC},
	{"DMG_SPIKE",DMG_SPIKE},
	{"DMG_NUKE",DMG_NUKE},
	//// Death types
	{"DMG_INSTAKILL",DMG_INSTAKILL},
	{"DMG_DROWNED",DMG_DROWNED},
	{"DMG_SPACEDROWN",DMG_SPACEDROWN},
	{"DMG_DEATHPIT",DMG_DEATHPIT},
	{"DMG_CRUSHED",DMG_CRUSHED},
	{"DMG_SPECTATOR",DMG_SPECTATOR},
	//// Masks
	{"DMG_CANHURTSELF",DMG_CANHURTSELF},
	{"DMG_DEATHMASK",DMG_DEATHMASK},

	// Intermission types
	{"int_none",int_none},
	{"int_coop",int_coop},
	{"int_match",int_match},
	{"int_teammatch",int_teammatch},
	//{"int_tag",int_tag},
	{"int_ctf",int_ctf},
	{"int_spec",int_spec},
	{"int_race",int_race},
	{"int_comp",int_comp},

	// Jingles (jingletype_t)
	{"JT_NONE",JT_NONE},
	{"JT_OTHER",JT_OTHER},
	{"JT_MASTER",JT_MASTER},
	{"JT_1UP",JT_1UP},
	{"JT_SHOES",JT_SHOES},
	{"JT_INV",JT_INV},
	{"JT_MINV",JT_MINV},
	{"JT_DROWN",JT_DROWN},
	{"JT_SUPER",JT_SUPER},
	{"JT_GOVER",JT_GOVER},
	{"JT_NIGHTSTIMEOUT",JT_NIGHTSTIMEOUT},
	{"JT_SSTIMEOUT",JT_SSTIMEOUT},
	// {"JT_LCLEAR",JT_LCLEAR},
	// {"JT_RACENT",JT_RACENT},
	// {"JT_CONTSC",JT_CONTSC},

	// Player state (playerstate_t)
	{"PST_LIVE",PST_LIVE}, // Playing or camping.
	{"PST_DEAD",PST_DEAD}, // Dead on the ground, view follows killer.
	{"PST_REBORN",PST_REBORN}, // Ready to restart/respawn???

	// Player animation (panim_t)
	{"PA_ETC",PA_ETC},
	{"PA_IDLE",PA_IDLE},
	{"PA_EDGE",PA_EDGE},
	{"PA_WALK",PA_WALK},
	{"PA_RUN",PA_RUN},
	{"PA_DASH",PA_DASH},
	{"PA_PAIN",PA_PAIN},
	{"PA_ROLL",PA_ROLL},
	{"PA_JUMP",PA_JUMP},
	{"PA_SPRING",PA_SPRING},
	{"PA_FALL",PA_FALL},
	{"PA_ABILITY",PA_ABILITY},
	{"PA_ABILITY2",PA_ABILITY2},
	{"PA_RIDE",PA_RIDE},

	// Current weapon
	{"WEP_AUTO",WEP_AUTO},
	{"WEP_BOUNCE",WEP_BOUNCE},
	{"WEP_SCATTER",WEP_SCATTER},
	{"WEP_GRENADE",WEP_GRENADE},
	{"WEP_EXPLODE",WEP_EXPLODE},
	{"WEP_RAIL",WEP_RAIL},
	{"NUM_WEAPONS",NUM_WEAPONS},

	// Value for infinite lives
	{"INFLIVES",INFLIVES},

	// Got Flags, for player->gotflag!
	// Used to be MF_ for some stupid reason, now they're GF_ to stop them looking like mobjflags
	{"GF_REDFLAG",GF_REDFLAG},
	{"GF_BLUEFLAG",GF_BLUEFLAG},

	// Customisable sounds for Skins, from sounds.h
	{"SKSSPIN",SKSSPIN},
	{"SKSPUTPUT",SKSPUTPUT},
	{"SKSPUDPUD",SKSPUDPUD},
	{"SKSPLPAN1",SKSPLPAN1}, // Ouchies
	{"SKSPLPAN2",SKSPLPAN2},
	{"SKSPLPAN3",SKSPLPAN3},
	{"SKSPLPAN4",SKSPLPAN4},
	{"SKSPLDET1",SKSPLDET1}, // Deaths
	{"SKSPLDET2",SKSPLDET2},
	{"SKSPLDET3",SKSPLDET3},
	{"SKSPLDET4",SKSPLDET4},
	{"SKSPLVCT1",SKSPLVCT1}, // Victories
	{"SKSPLVCT2",SKSPLVCT2},
	{"SKSPLVCT3",SKSPLVCT3},
	{"SKSPLVCT4",SKSPLVCT4},
	{"SKSTHOK",SKSTHOK},
	{"SKSSPNDSH",SKSSPNDSH},
	{"SKSZOOM",SKSZOOM},
	{"SKSSKID",SKSSKID},
	{"SKSGASP",SKSGASP},
	{"SKSJUMP",SKSJUMP},

	// 3D Floor/Fake Floor/FOF/whatever flags
	{"FF_EXISTS",FF_EXISTS},                   ///< Always set, to check for validity.
	{"FF_BLOCKPLAYER",FF_BLOCKPLAYER},         ///< Solid to player, but nothing else
	{"FF_BLOCKOTHERS",FF_BLOCKOTHERS},         ///< Solid to everything but player
	{"FF_SOLID",FF_SOLID},                     ///< Clips things.
	{"FF_RENDERSIDES",FF_RENDERSIDES},         ///< Renders the sides.
	{"FF_RENDERPLANES",FF_RENDERPLANES},       ///< Renders the floor/ceiling.
	{"FF_RENDERALL",FF_RENDERALL},             ///< Renders everything.
	{"FF_SWIMMABLE",FF_SWIMMABLE},             ///< Is a water block.
	{"FF_NOSHADE",FF_NOSHADE},                 ///< Messes with the lighting?
	{"FF_CUTSOLIDS",FF_CUTSOLIDS},             ///< Cuts out hidden solid pixels.
	{"FF_CUTEXTRA",FF_CUTEXTRA},               ///< Cuts out hidden translucent pixels.
	{"FF_CUTLEVEL",FF_CUTLEVEL},               ///< Cuts out all hidden pixels.
	{"FF_CUTSPRITES",FF_CUTSPRITES},           ///< Final step in making 3D water.
	{"FF_BOTHPLANES",FF_BOTHPLANES},           ///< Render inside and outside planes.
	{"FF_EXTRA",FF_EXTRA},                     ///< Gets cut by ::FF_CUTEXTRA.
	{"FF_TRANSLUCENT",FF_TRANSLUCENT},         ///< See through!
	{"FF_FOG",FF_FOG},                         ///< Fog "brush."
	{"FF_INVERTPLANES",FF_INVERTPLANES},       ///< Only render inside planes.
	{"FF_ALLSIDES",FF_ALLSIDES},               ///< Render inside and outside sides.
	{"FF_INVERTSIDES",FF_INVERTSIDES},         ///< Only render inside sides.
	{"FF_DOUBLESHADOW",FF_DOUBLESHADOW},       ///< Make two lightlist entries to reset light?
	{"FF_FLOATBOB",FF_FLOATBOB},               ///< Floats on water and bobs if you step on it.
	{"FF_NORETURN",FF_NORETURN},               ///< Used with ::FF_CRUMBLE. Will not return to its original position after falling.
	{"FF_CRUMBLE",FF_CRUMBLE},                 ///< Falls 2 seconds after being stepped on, and randomly brings all touching crumbling 3dfloors down with it, providing their master sectors share the same tag (allows crumble platforms above or below, to also exist).
	{"FF_SHATTERBOTTOM",FF_SHATTERBOTTOM},     ///< Used with ::FF_BUSTUP. Like FF_SHATTER, but only breaks from the bottom. Good for springing up through rubble.
	{"FF_MARIO",FF_MARIO},                     ///< Acts like a question block when hit from underneath. Goodie spawned at top is determined by master sector.
	{"FF_BUSTUP",FF_BUSTUP},                   ///< You can spin through/punch this block and it will crumble!
	{"FF_QUICKSAND",FF_QUICKSAND},             ///< Quicksand!
	{"FF_PLATFORM",FF_PLATFORM},               ///< You can jump up through this to the top.
	{"FF_REVERSEPLATFORM",FF_REVERSEPLATFORM}, ///< A fall-through floor in normal gravity, a platform in reverse gravity.
	{"FF_INTANGIBLEFLATS",FF_INTANGIBLEFLATS}, ///< Both flats are intangible, but the sides are still solid.
	{"FF_INTANGABLEFLATS",FF_INTANGIBLEFLATS}, ///< Both flats are intangable, but the sides are still solid.
	{"FF_SHATTER",FF_SHATTER},                 ///< Used with ::FF_BUSTUP. Bustable on mere touch.
	{"FF_SPINBUST",FF_SPINBUST},               ///< Used with ::FF_BUSTUP. Also bustable if you're in your spinning frames.
	{"FF_STRONGBUST",FF_STRONGBUST},           ///< Used with ::FF_BUSTUP. Only bustable by "strong" characters (Knuckles) and abilities (bouncing, twinspin, melee).
	{"FF_RIPPLE",FF_RIPPLE},                   ///< Ripple the flats
	{"FF_COLORMAPONLY",FF_COLORMAPONLY},       ///< Only copy the colormap, not the lightlevel
	{"FF_GOOWATER",FF_GOOWATER},               ///< Used with ::FF_SWIMMABLE. Makes thick bouncey goop.

#ifdef HAVE_LUA_SEGS
	// Node flags
	{"NF_SUBSECTOR",NF_SUBSECTOR}, // Indicate a leaf.
#endif

	// Slope flags
	{"SL_NOPHYSICS",SL_NOPHYSICS},
	{"SL_DYNAMIC",SL_DYNAMIC},

	// Angles
	{"ANG1",ANG1},
	{"ANG2",ANG2},
	{"ANG10",ANG10},
	{"ANG15",ANG15},
	{"ANG20",ANG20},
	{"ANG30",ANG30},
	{"ANG60",ANG60},
	{"ANG64h",ANG64h},
	{"ANG105",ANG105},
	{"ANG210",ANG210},
	{"ANG255",ANG255},
	{"ANG340",ANG340},
	{"ANG350",ANG350},
	{"ANGLE_11hh",ANGLE_11hh},
	{"ANGLE_22h",ANGLE_22h},
	{"ANGLE_45",ANGLE_45},
	{"ANGLE_67h",ANGLE_67h},
	{"ANGLE_90",ANGLE_90},
	{"ANGLE_112h",ANGLE_112h},
	{"ANGLE_135",ANGLE_135},
	{"ANGLE_157h",ANGLE_157h},
	{"ANGLE_180",ANGLE_180},
	{"ANGLE_202h",ANGLE_202h},
	{"ANGLE_225",ANGLE_225},
	{"ANGLE_247h",ANGLE_247h},
	{"ANGLE_270",ANGLE_270},
	{"ANGLE_292h",ANGLE_292h},
	{"ANGLE_315",ANGLE_315},
	{"ANGLE_337h",ANGLE_337h},
	{"ANGLE_MAX",ANGLE_MAX},

	// P_Chase directions (dirtype_t)
	{"DI_NODIR",DI_NODIR},
	{"DI_EAST",DI_EAST},
	{"DI_NORTHEAST",DI_NORTHEAST},
	{"DI_NORTH",DI_NORTH},
	{"DI_NORTHWEST",DI_NORTHWEST},
	{"DI_WEST",DI_WEST},
	{"DI_SOUTHWEST",DI_SOUTHWEST},
	{"DI_SOUTH",DI_SOUTH},
	{"DI_SOUTHEAST",DI_SOUTHEAST},
	{"NUMDIRS",NUMDIRS},

	// Sprite rotation axis (rotaxis_t)
	{"ROTAXIS_X",ROTAXIS_X},
	{"ROTAXIS_Y",ROTAXIS_Y},
	{"ROTAXIS_Z",ROTAXIS_Z},

	// Buttons (ticcmd_t)
	{"BT_WEAPONMASK",BT_WEAPONMASK}, //our first four bits.
	{"BT_WEAPONNEXT",BT_WEAPONNEXT},
	{"BT_WEAPONPREV",BT_WEAPONPREV},
	{"BT_ATTACK",BT_ATTACK}, // shoot rings
	{"BT_USE",BT_USE}, // spin
	{"BT_CAMLEFT",BT_CAMLEFT}, // turn camera left
	{"BT_CAMRIGHT",BT_CAMRIGHT}, // turn camera right
	{"BT_TOSSFLAG",BT_TOSSFLAG},
	{"BT_JUMP",BT_JUMP},
	{"BT_FIRENORMAL",BT_FIRENORMAL}, // Fire a normal ring no matter what
	{"BT_CUSTOM1",BT_CUSTOM1}, // Lua customizable
	{"BT_CUSTOM2",BT_CUSTOM2}, // Lua customizable
	{"BT_CUSTOM3",BT_CUSTOM3}, // Lua customizable

	// Lua command registration flags
	{"COM_ADMIN",COM_ADMIN},
	{"COM_SPLITSCREEN",COM_SPLITSCREEN},
	{"COM_LOCAL",COM_LOCAL},

	// cvflags_t
	{"CV_SAVE",CV_SAVE},
	{"CV_CALL",CV_CALL},
	{"CV_NETVAR",CV_NETVAR},
	{"CV_NOINIT",CV_NOINIT},
	{"CV_FLOAT",CV_FLOAT},
	{"CV_NOTINNET",CV_NOTINNET},
	{"CV_MODIFIED",CV_MODIFIED},
	{"CV_SHOWMODIF",CV_SHOWMODIF},
	{"CV_SHOWMODIFONETIME",CV_SHOWMODIFONETIME},
	{"CV_NOSHOWHELP",CV_NOSHOWHELP},
	{"CV_HIDEN",CV_HIDEN},
	{"CV_HIDDEN",CV_HIDEN},
	{"CV_CHEAT",CV_CHEAT},
	{"CV_NOLUA",CV_NOLUA},

	// v_video flags
	{"V_NOSCALEPATCH",V_NOSCALEPATCH},
	{"V_SMALLSCALEPATCH",V_SMALLSCALEPATCH},
	{"V_MEDSCALEPATCH",V_MEDSCALEPATCH},
	{"V_6WIDTHSPACE",V_6WIDTHSPACE},
	{"V_OLDSPACING",V_OLDSPACING},
	{"V_MONOSPACE",V_MONOSPACE},

	{"V_MAGENTAMAP",V_MAGENTAMAP},
	{"V_YELLOWMAP",V_YELLOWMAP},
	{"V_GREENMAP",V_GREENMAP},
	{"V_BLUEMAP",V_BLUEMAP},
	{"V_REDMAP",V_REDMAP},
	{"V_GRAYMAP",V_GRAYMAP},
	{"V_ORANGEMAP",V_ORANGEMAP},
	{"V_SKYMAP",V_SKYMAP},
	{"V_PURPLEMAP",V_PURPLEMAP},
	{"V_AQUAMAP",V_AQUAMAP},
	{"V_PERIDOTMAP",V_PERIDOTMAP},
	{"V_AZUREMAP",V_AZUREMAP},
	{"V_BROWNMAP",V_BROWNMAP},
	{"V_ROSYMAP",V_ROSYMAP},
	{"V_INVERTMAP",V_INVERTMAP},

	{"V_TRANSLUCENT",V_TRANSLUCENT},
	{"V_10TRANS",V_10TRANS},
	{"V_20TRANS",V_20TRANS},
	{"V_30TRANS",V_30TRANS},
	{"V_40TRANS",V_40TRANS},
	{"V_50TRANS",V_TRANSLUCENT}, // alias
	{"V_60TRANS",V_60TRANS},
	{"V_70TRANS",V_70TRANS},
	{"V_80TRANS",V_80TRANS},
	{"V_90TRANS",V_90TRANS},
	{"V_HUDTRANSHALF",V_HUDTRANSHALF},
	{"V_HUDTRANS",V_HUDTRANS},
	{"V_HUDTRANSDOUBLE",V_HUDTRANSDOUBLE},
	{"V_AUTOFADEOUT",V_AUTOFADEOUT},
	{"V_RETURN8",V_RETURN8},
	{"V_OFFSET",V_OFFSET},
	{"V_ALLOWLOWERCASE",V_ALLOWLOWERCASE},
	{"V_FLIP",V_FLIP},
	{"V_CENTERNAMETAG",V_CENTERNAMETAG},
	{"V_SNAPTOTOP",V_SNAPTOTOP},
	{"V_SNAPTOBOTTOM",V_SNAPTOBOTTOM},
	{"V_SNAPTOLEFT",V_SNAPTOLEFT},
	{"V_SNAPTORIGHT",V_SNAPTORIGHT},
	{"V_WRAPX",V_WRAPX},
	{"V_WRAPY",V_WRAPY},
	{"V_NOSCALESTART",V_NOSCALESTART},
	{"V_PERPLAYER",V_PERPLAYER},

	{"V_PARAMMASK",V_PARAMMASK},
	{"V_SCALEPATCHMASK",V_SCALEPATCHMASK},
	{"V_SPACINGMASK",V_SPACINGMASK},
	{"V_CHARCOLORMASK",V_CHARCOLORMASK},
	{"V_ALPHAMASK",V_ALPHAMASK},

	{"V_CHARCOLORSHIFT",V_CHARCOLORSHIFT},
	{"V_ALPHASHIFT",V_ALPHASHIFT},

	//Kick Reasons
	{"KR_KICK",KR_KICK},
	{"KR_PINGLIMIT",KR_PINGLIMIT},
	{"KR_SYNCH",KR_SYNCH},
	{"KR_TIMEOUT",KR_TIMEOUT},
	{"KR_BAN",KR_BAN},
	{"KR_LEAVE",KR_LEAVE},

	// translation colormaps
	{"TC_DEFAULT",TC_DEFAULT},
	{"TC_BOSS",TC_BOSS},
	{"TC_METALSONIC",TC_METALSONIC},
	{"TC_ALLWHITE",TC_ALLWHITE},
	{"TC_RAINBOW",TC_RAINBOW},
	{"TC_BLINK",TC_BLINK},
	{"TC_DASHMODE",TC_DASHMODE},

	{NULL,0}
};

static mobjtype_t get_mobjtype(const char *word)
{ // Returns the value of MT_ enumerations
	mobjtype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("MT_",word,3))
		word += 3; // take off the MT_
	for (i = 0; i < NUMMOBJFREESLOTS; i++) {
		if (!FREE_MOBJS[i])
			break;
		if (fastcmp(word, FREE_MOBJS[i]))
			return MT_FIRSTFREESLOT+i;
	}
	for (i = 0; i < MT_FIRSTFREESLOT; i++)
		if (fastcmp(word, MOBJTYPE_LIST[i]+3))
			return i;
	deh_warning("Couldn't find mobjtype named 'MT_%s'",word);
	return MT_NULL;
}

static statenum_t get_state(const char *word)
{ // Returns the value of S_ enumerations
	statenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("S_",word,2))
		word += 2; // take off the S_
	for (i = 0; i < NUMSTATEFREESLOTS; i++) {
		if (!FREE_STATES[i])
			break;
		if (fastcmp(word, FREE_STATES[i]))
			return S_FIRSTFREESLOT+i;
	}
	for (i = 0; i < S_FIRSTFREESLOT; i++)
		if (fastcmp(word, STATE_LIST[i]+2))
			return i;
	deh_warning("Couldn't find state named 'S_%s'",word);
	return S_NULL;
}

static spritenum_t get_sprite(const char *word)
{ // Returns the value of SPR_ enumerations
	spritenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SPR_",word,4))
		word += 4; // take off the SPR_
	for (i = 0; i < NUMSPRITES; i++)
		if (!sprnames[i][4] && memcmp(word,sprnames[i],4)==0)
			return i;
	deh_warning("Couldn't find sprite named 'SPR_%s'",word);
	return SPR_NULL;
}

static playersprite_t get_sprite2(const char *word)
{ // Returns the value of SPR2_ enumerations
	playersprite_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SPR2_",word,5))
		word += 5; // take off the SPR2_
	for (i = 0; i < NUMPLAYERSPRITES; i++)
		if (!spr2names[i][4] && memcmp(word,spr2names[i],4)==0)
			return i;
	deh_warning("Couldn't find sprite named 'SPR2_%s'",word);
	return SPR2_STND;
}

static sfxenum_t get_sfx(const char *word)
{ // Returns the value of SFX_ enumerations
	sfxenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SFX_",word,4))
		word += 4; // take off the SFX_
	else if (fastncmp("DS",word,2))
		word += 2; // take off the DS
	for (i = 0; i < NUMSFX; i++)
		if (S_sfx[i].name && fasticmp(word, S_sfx[i].name))
			return i;
	deh_warning("Couldn't find sfx named 'SFX_%s'",word);
	return sfx_None;
}

#ifdef MUSICSLOT_COMPATIBILITY
static UINT16 get_mus(const char *word, UINT8 dehacked_mode)
{ // Returns the value of MUS_ enumerations
	UINT16 i;
	char lumptmp[4];

	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (!word[2] && toupper(word[0]) >= 'A' && toupper(word[0]) <= 'Z')
		return (UINT16)M_MapNumber(word[0], word[1]);

	if (fastncmp("MUS_",word,4))
		word += 4; // take off the MUS_
	else if (fastncmp("O_",word,2) || fastncmp("D_",word,2))
		word += 2; // take off the O_ or D_

	strncpy(lumptmp, word, 4);
	lumptmp[3] = 0;
	if (fasticmp("MAP",lumptmp))
	{
		word += 3;
		if (toupper(word[0]) >= 'A' && toupper(word[0]) <= 'Z')
			return (UINT16)M_MapNumber(word[0], word[1]);
		else if ((i = atoi(word)))
			return i;

		word -= 3;
		if (dehacked_mode)
			deh_warning("Couldn't find music named 'MUS_%s'",word);
		return 0;
	}
	for (i = 0; compat_special_music_slots[i][0]; ++i)
		if (fasticmp(word, compat_special_music_slots[i]))
			return i + 1036;
	if (dehacked_mode)
		deh_warning("Couldn't find music named 'MUS_%s'",word);
	return 0;
}
#endif

static hudnum_t get_huditem(const char *word)
{ // Returns the value of HUD_ enumerations
	hudnum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("HUD_",word,4))
		word += 4; // take off the HUD_
	for (i = 0; i < NUMHUDITEMS; i++)
		if (fastcmp(word, HUDITEMS_LIST[i]))
			return i;
	deh_warning("Couldn't find huditem named 'HUD_%s'",word);
	return HUD_LIVES;
}

static menutype_t get_menutype(const char *word)
{ // Returns the value of MN_ enumerations
	menutype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("MN_",word,3))
		word += 3; // take off the MN_
	for (i = 0; i < NUMMENUTYPES; i++)
		if (fastcmp(word, MENUTYPES_LIST[i]))
			return i;
	deh_warning("Couldn't find menutype named 'MN_%s'",word);
	return MN_NONE;
}

/*static INT16 get_gametype(const char *word)
{ // Returns the value of GT_ enumerations
	INT16 i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("GT_",word,3))
		word += 3; // take off the GT_
	for (i = 0; i < NUMGAMETYPES; i++)
		if (fastcmp(word, Gametype_ConstantNames[i]+3))
			return i;
	deh_warning("Couldn't find gametype named 'GT_%s'",word);
	return GT_COOP;
}

static powertype_t get_power(const char *word)
{ // Returns the value of pw_ enumerations
	powertype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("PW_",word,3))
		word += 3; // take off the pw_
	for (i = 0; i < NUMPOWERS; i++)
		if (fastcmp(word, POWERS_LIST[i]))
			return i;
	deh_warning("Couldn't find power named 'pw_%s'",word);
	return pw_invulnerability;
}*/

/// \todo Make ANY of this completely over-the-top math craziness obey the order of operations.
static fixed_t op_mul(fixed_t a, fixed_t b) { return a*b; }
static fixed_t op_div(fixed_t a, fixed_t b) { return a/b; }
static fixed_t op_add(fixed_t a, fixed_t b) { return a+b; }
static fixed_t op_sub(fixed_t a, fixed_t b) { return a-b; }
static fixed_t op_or(fixed_t a, fixed_t b) { return a|b; }
static fixed_t op_and(fixed_t a, fixed_t b) { return a&b; }
static fixed_t op_lshift(fixed_t a, fixed_t b) { return a<<b; }
static fixed_t op_rshift(fixed_t a, fixed_t b) { return a>>b; }

struct {
	const char c;
	fixed_t (*v)(fixed_t,fixed_t);
} OPERATIONS[] = {
	{'*',op_mul},
	{'/',op_div},
	{'+',op_add},
	{'-',op_sub},
	{'|',op_or},
	{'&',op_and},
	{'<',op_lshift},
	{'>',op_rshift},
	{0,NULL}
};

// Returns the full word, cut at the first symbol or whitespace
/*static char *read_word(const char *line)
{
	// Part 1: You got the start of the word, now find the end.
  const char *p;
	INT32 i;
	for (p = line+1; *p; p++) {
		if (*p == ' ' || *p == '\t')
			break;
		for (i = 0; OPERATIONS[i].c; i++)
			if (*p == OPERATIONS[i].c) {
				i = -1;
				break;
			}
		if (i == -1)
			break;
	}

	// Part 2: Make a copy of the word and return it.
	{
		size_t len = (p-line);
		char *word = malloc(len+1);
		M_Memcpy(word,line,len);
		word[len] = '\0';
		return word;
	}
}

static INT32 operation_pad(const char **word)
{ // Brings word the next operation and returns the operation number.
	INT32 i;
	for (; **word; (*word)++) {
		if (**word == ' ' || **word == '\t')
			continue;
		for (i = 0; OPERATIONS[i].c; i++)
			if (**word == OPERATIONS[i].c)
			{
				if ((**word == '<' && *(*word+1) == '<') || (**word == '>' && *(*word+1) == '>')) (*word)++; // These operations are two characters long.
				else if (**word == '<' || **word == '>') continue; // ... do not accept one character long.
				(*word)++;
				return i;
			}
		deh_warning("Unknown operation '%c'",**word);
		return -1;
	}
	return -1;
}

static void const_warning(const char *type, const char *word)
{
	deh_warning("Couldn't find %s named '%s'",type,word);
}

static fixed_t find_const(const char **rword)
{ // Finds the value of constants and returns it, bringing word to the next operation.
	INT32 i;
	fixed_t r;
	char *word = read_word(*rword);
	*rword += strlen(word);
	if ((*word >= '0' && *word <= '9') || *word == '-') { // Parse a number
		r = atoi(word);
		free(word);
		return r;
	}
	if (!*(word+1) && // Turn a single A-z symbol into numbers, like sprite frames.
	 ((*word >= 'A' && *word <= 'Z') || (*word >= 'a' && *word <= 'z'))) {
		r = R_Char2Frame(*word);
		free(word);
		return r;
	}
	if (fastncmp("MF_", word, 3)) {
		char *p = word+3;
		for (i = 0; MOBJFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj flag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("MF2_", word, 4)) {
		char *p = word+4;
		for (i = 0; MOBJFLAG2_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG2_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj flag2",word);
		free(word);
		return 0;
	}
	else if (fastncmp("MFE_", word, 4)) {
		char *p = word+4;
		for (i = 0; MOBJEFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJEFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj eflag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("PF_", word, 3)) {
		char *p = word+3;
		for (i = 0; PLAYERFLAG_LIST[i]; i++)
			if (fastcmp(p, PLAYERFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}
		if (fastcmp(p, "FULLSTASIS"))
			return PF_FULLSTASIS;

		// Not found error
		const_warning("player flag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("S_",word,2)) {
		r = get_state(word);
		free(word);
		return r;
	}
	else if (fastncmp("MT_",word,3)) {
		r = get_mobjtype(word);
		free(word);
		return r;
	}
	else if (fastncmp("SPR_",word,4)) {
		r = get_sprite(word);
		free(word);
		return r;
	}
	else if (fastncmp("SFX_",word,4) || fastncmp("DS",word,2)) {
		r = get_sfx(word);
		free(word);
		return r;
	}
#ifdef MUSICSLOT_COMPATIBILITY
	else if (fastncmp("MUS_",word,4) || fastncmp("O_",word,2)) {
		r = get_mus(word, true);
		free(word);
		return r;
	}
#endif
	else if (fastncmp("PW_",word,3)) {
		r = get_power(word);
		free(word);
		return r;
	}
	else if (fastncmp("MN_",word,3)) {
		r = get_menutype(word);
		free(word);
		return r;
	}
	else if (fastncmp("GT_",word,3)) {
		r = get_gametype(word);
		free(word);
		return r;
	}
	else if (fastncmp("GTR_", word, 4)) {
		char *p = word+4;
		for (i = 0; GAMETYPERULE_LIST[i]; i++)
			if (fastcmp(p, GAMETYPERULE_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("game type rule",word);
		free(word);
		return 0;
	}
	else if (fastncmp("TOL_", word, 4)) {
		char *p = word+4;
		for (i = 0; TYPEOFLEVEL[i].name; i++)
			if (fastcmp(p, TYPEOFLEVEL[i].name)) {
				free(word);
				return TYPEOFLEVEL[i].flag;
			}

		// Not found error
		const_warning("typeoflevel",word);
		free(word);
		return 0;
	}
	else if (fastncmp("HUD_",word,4)) {
		r = get_huditem(word);
		free(word);
		return r;
	}
	else if (fastncmp("SKINCOLOR_",word,10)) {
		char *p = word+10;
		for (i = 0; i < MAXTRANSLATIONS; i++)
			if (fastcmp(p, COLOR_ENUMS[i])) {
				free(word);
				return i;
			}
		const_warning("color",word);
		free(word);
		return 0;
	}
	else if (fastncmp("GRADE_",word,6))
	{
		char *p = word+6;
		for (i = 0; NIGHTSGRADE_LIST[i]; i++)
			if (*p == NIGHTSGRADE_LIST[i])
			{
				free(word);
				return i;
			}
		const_warning("NiGHTS grade",word);
		free(word);
		return 0;
	}
	for (i = 0; INT_CONST[i].n; i++)
		if (fastcmp(word,INT_CONST[i].n)) {
			free(word);
			return INT_CONST[i].v;
		}

	// Not found error.
	const_warning("constant",word);
	free(word);
	return 0;
}*/

// Loops through every constant and operation in word and performs its calculations, returning the final value.
fixed_t get_number(const char *word)
{
	return LUA_EvalMath(word);

	/*// DESPERATELY NEEDED: Order of operations support! :x
	fixed_t i = find_const(&word);
	INT32 o;
	while(*word) {
		o = operation_pad(&word);
		if (o != -1)
			i = OPERATIONS[o].v(i,find_const(&word));
		else
			break;
	}
	return i;*/
}

void DEH_Check(void)
{
#if defined(_DEBUG) || defined(PARANOIA)
	const size_t dehstates = sizeof(STATE_LIST)/sizeof(const char*);
	const size_t dehmobjs  = sizeof(MOBJTYPE_LIST)/sizeof(const char*);
	const size_t dehpowers = sizeof(POWERS_LIST)/sizeof(const char*);
	const size_t dehcolors = sizeof(COLOR_ENUMS)/sizeof(const char*);

	if (dehstates != S_FIRSTFREESLOT)
		I_Error("You forgot to update the Dehacked states list, you dolt!\n(%d states defined, versus %s in the Dehacked list)\n", S_FIRSTFREESLOT, sizeu1(dehstates));

	if (dehmobjs != MT_FIRSTFREESLOT)
		I_Error("You forgot to update the Dehacked mobjtype list, you dolt!\n(%d mobj types defined, versus %s in the Dehacked list)\n", MT_FIRSTFREESLOT, sizeu1(dehmobjs));

	if (dehpowers != NUMPOWERS)
		I_Error("You forgot to update the Dehacked powers list, you dolt!\n(%d powers defined, versus %s in the Dehacked list)\n", NUMPOWERS, sizeu1(dehpowers));

	if (dehcolors != MAXTRANSLATIONS)
		I_Error("You forgot to update the Dehacked colors list, you dolt!\n(%d colors defined, versus %s in the Dehacked list)\n", MAXTRANSLATIONS, sizeu1(dehcolors));
#endif
}

#include "lua_script.h"
#include "lua_libs.h"

// freeslot takes a name (string only!)
// and allocates it to the appropriate free slot.
// Returns the slot number allocated for it or nil if failed.
// ex. freeslot("MT_MYTHING","S_MYSTATE1","S_MYSTATE2")
// TODO: Error checking! @.@; There's currently no way to know which ones failed and why!
//
static inline int lib_freeslot(lua_State *L)
{
	int n = lua_gettop(L);
	int r = 0; // args returned
	char *s, *type,*word;

	if (!lua_lumploading)
		return luaL_error(L, "This function cannot be called from within a hook or coroutine!");

	while (n-- > 0)
	{
		s = Z_StrDup(luaL_checkstring(L,1));
		type = strtok(s, "_");
		if (type)
			strupr(type);
		else {
			Z_Free(s);
			return luaL_error(L, "Unknown enum type in '%s'\n", luaL_checkstring(L, 1));
		}

		word = strtok(NULL, "\n");
		if (word)
			strupr(word);
		else {
			Z_Free(s);
			return luaL_error(L, "Missing enum name in '%s'\n", luaL_checkstring(L, 1));
		}
		if (fastcmp(type, "SFX")) {
			sfxenum_t sfx;
			strlwr(word);
			CONS_Printf("Sound sfx_%s allocated.\n",word);
			sfx = S_AddSoundFx(word, false, 0, false);
			if (sfx != sfx_None) {
				lua_pushinteger(L, sfx);
				r++;
			} else
				CONS_Alert(CONS_WARNING, "Ran out of free SFX slots!\n");
		}
		else if (fastcmp(type, "SPR"))
		{
			char wad;
			spritenum_t j;
			lua_getfield(L, LUA_REGISTRYINDEX, "WAD");
			wad = (char)lua_tointeger(L, -1);
			lua_pop(L, 1);
			for (j = SPR_FIRSTFREESLOT; j <= SPR_LASTFREESLOT; j++)
			{
				if (used_spr[(j-SPR_FIRSTFREESLOT)/8] & (1<<(j%8)))
				{
					if (!sprnames[j][4] && memcmp(sprnames[j],word,4)==0)
						sprnames[j][4] = wad;
					continue; // Already allocated, next.
				}
				// Found a free slot!
				CONS_Printf("Sprite SPR_%s allocated.\n",word);
				strncpy(sprnames[j],word,4);
				//sprnames[j][4] = 0;
				used_spr[(j-SPR_FIRSTFREESLOT)/8] |= 1<<(j%8); // Okay, this sprite slot has been named now.
				lua_pushinteger(L, j);
				r++;
				break;
			}
			if (j > SPR_LASTFREESLOT)
				CONS_Alert(CONS_WARNING, "Ran out of free sprite slots!\n");
		}
		else if (fastcmp(type, "S"))
		{
			statenum_t i;
			for (i = 0; i < NUMSTATEFREESLOTS; i++)
				if (!FREE_STATES[i]) {
					CONS_Printf("State S_%s allocated.\n",word);
					FREE_STATES[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
					strcpy(FREE_STATES[i],word);
					lua_pushinteger(L, i);
					r++;
					break;
				}
			if (i == NUMSTATEFREESLOTS)
				CONS_Alert(CONS_WARNING, "Ran out of free State slots!\n");
		}
		else if (fastcmp(type, "MT"))
		{
			mobjtype_t i;
			for (i = 0; i < NUMMOBJFREESLOTS; i++)
				if (!FREE_MOBJS[i]) {
					CONS_Printf("MobjType MT_%s allocated.\n",word);
					FREE_MOBJS[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
					strcpy(FREE_MOBJS[i],word);
					lua_pushinteger(L, i);
					r++;
					break;
				}
			if (i == NUMMOBJFREESLOTS)
				CONS_Alert(CONS_WARNING, "Ran out of free MobjType slots!\n");
		}
		else if (fastcmp(type, "SPR2"))
		{
			// Search if we already have an SPR2 by that name...
			playersprite_t i;
			for (i = SPR2_FIRSTFREESLOT; i < free_spr2; i++)
				if (memcmp(spr2names[i],word,4) == 0)
					break;
			// We don't, so allocate a new one.
			if (i >= free_spr2) {
				if (free_spr2 < NUMPLAYERSPRITES)
				{
					CONS_Printf("Sprite SPR2_%s allocated.\n",word);
					strncpy(spr2names[free_spr2],word,4);
					spr2defaults[free_spr2] = 0;
					spr2names[free_spr2++][4] = 0;
				} else
					CONS_Alert(CONS_WARNING, "Ran out of free SPR2 slots!\n");
			}
			r++;
		}
		else if (fastcmp(type, "TOL"))
		{
			// Search if we already have a typeoflevel by that name...
			int i;
			for (i = 0; TYPEOFLEVEL[i].name; i++)
				if (fastcmp(word, TYPEOFLEVEL[i].name))
					break;

			// We don't, so allocate a new one.
			if (TYPEOFLEVEL[i].name == NULL) {
				if (lastcustomtol == (UINT32)MAXTOL) // Unless you have way too many, since they're flags.
					CONS_Alert(CONS_WARNING, "Ran out of free typeoflevel slots!\n");
				else {
					CONS_Printf("TypeOfLevel TOL_%s allocated.\n",word);
					G_AddTOL(lastcustomtol, word);
					lua_pushinteger(L, lastcustomtol);
					lastcustomtol <<= 1;
					r++;
				}
			}
		}
		Z_Free(s);
		lua_remove(L, 1);
		continue;
	}
	return r;
}

// Wrapper for ALL A_Action functions.
// Arguments: mobj_t actor, int var1, int var2
static int action_call(lua_State *L)
{
	//actionf_t *action = lua_touserdata(L,lua_upvalueindex(1));
	actionf_t *action = *((actionf_t **)luaL_checkudata(L, 1, META_ACTION));
	mobj_t *actor = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
	var1 = (INT32)luaL_optinteger(L, 3, 0);
	var2 = (INT32)luaL_optinteger(L, 4, 0);
	if (!actor)
		return LUA_ErrInvalid(L, "mobj_t");
	action->acp1(actor);
	return 0;
}

// Hardcoded A_Action name to call for super() or NULL if super() would be invalid.
// Set in lua_infolib.
const char *superactions[MAXRECURSION];
UINT8 superstack = 0;

static int lib_dummysuper(lua_State *L)
{
	return luaL_error(L, "Can't call super() outside of hardcode-replacing A_Action functions being called by state changes!"); // convoluted, I know. @_@;;
}

static inline int lib_getenum(lua_State *L)
{
	const char *word, *p;
	fixed_t i;
	boolean mathlib = lua_toboolean(L, lua_upvalueindex(1));
	if (lua_type(L,2) != LUA_TSTRING)
		return 0;
	word = lua_tostring(L,2);
	if (strlen(word) == 1) { // Assume sprite frame if length 1.
		if (*word >= 'A' && *word <= '~')
		{
			lua_pushinteger(L, *word-'A');
			return 1;
		}
		if (mathlib) return luaL_error(L, "constant '%s' could not be parsed.\n", word);
		return 0;
	}
	else if (fastncmp("MF_", word, 3)) {
		p = word+3;
		for (i = 0; MOBJFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "mobjflag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("MF2_", word, 4)) {
		p = word+4;
		for (i = 0; MOBJFLAG2_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG2_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "mobjflag2 '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("MFE_", word, 4)) {
		p = word+4;
		for (i = 0; MOBJEFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJEFLAG_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "mobjeflag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("MTF_", word, 4)) {
		p = word+4;
		for (i = 0; i < 4; i++)
			if (MAPTHINGFLAG_LIST[i] && fastcmp(p, MAPTHINGFLAG_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "mapthingflag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("PF_", word, 3)) {
		p = word+3;
		for (i = 0; PLAYERFLAG_LIST[i]; i++)
			if (fastcmp(p, PLAYERFLAG_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (fastcmp(p, "FULLSTASIS"))
		{
			lua_pushinteger(L, (lua_Integer)PF_FULLSTASIS);
			return 1;
		}
		if (mathlib) return luaL_error(L, "playerflag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("GT_", word, 3)) {
		p = word;
		for (i = 0; Gametype_ConstantNames[i]; i++)
			if (fastcmp(p, Gametype_ConstantNames[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "gametype '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("GTR_", word, 4)) {
		p = word+4;
		for (i = 0; GAMETYPERULE_LIST[i]; i++)
			if (fastcmp(p, GAMETYPERULE_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "game type rule '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("TOL_", word, 4)) {
		p = word+4;
		for (i = 0; TYPEOFLEVEL[i].name; i++)
			if (fastcmp(p, TYPEOFLEVEL[i].name)) {
				lua_pushinteger(L, TYPEOFLEVEL[i].flag);
				return 1;
			}
		if (mathlib) return luaL_error(L, "typeoflevel '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("ML_", word, 3)) {
		p = word+3;
		for (i = 0; i < 16; i++)
			if (ML_LIST[i] && fastcmp(p, ML_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "linedef flag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("S_",word,2)) {
		p = word+2;
		for (i = 0; i < NUMSTATEFREESLOTS; i++) {
			if (!FREE_STATES[i])
				break;
			if (fastcmp(p, FREE_STATES[i])) {
				lua_pushinteger(L, S_FIRSTFREESLOT+i);
				return 1;
			}
		}
		for (i = 0; i < S_FIRSTFREESLOT; i++)
			if (fastcmp(p, STATE_LIST[i]+2)) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "state '%s' does not exist.\n", word);
	}
	else if (fastncmp("MT_",word,3)) {
		p = word+3;
		for (i = 0; i < NUMMOBJFREESLOTS; i++) {
			if (!FREE_MOBJS[i])
				break;
			if (fastcmp(p, FREE_MOBJS[i])) {
				lua_pushinteger(L, MT_FIRSTFREESLOT+i);
				return 1;
			}
		}
		for (i = 0; i < MT_FIRSTFREESLOT; i++)
			if (fastcmp(p, MOBJTYPE_LIST[i]+3)) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "mobjtype '%s' does not exist.\n", word);
	}
	else if (fastncmp("SPR_",word,4)) {
		p = word+4;
		for (i = 0; i < NUMSPRITES; i++)
			if (!sprnames[i][4] && fastncmp(p,sprnames[i],4)) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "sprite '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("SPR2_",word,5)) {
		p = word+5;
		for (i = 0; i < (fixed_t)free_spr2; i++)
			if (!spr2names[i][4])
			{
				// special 3-char cases, e.g. SPR2_RUN
				// the spr2names entry will have "_" on the end, as in "RUN_"
				if (spr2names[i][3] == '_' && !p[3]) {
					if (fastncmp(p,spr2names[i],3)) {
						lua_pushinteger(L, i);
						return 1;
					}
				}
				else if (fastncmp(p,spr2names[i],4)) {
					lua_pushinteger(L, i);
					return 1;
				}
			}
		if (mathlib) return luaL_error(L, "player sprite '%s' could not be found.\n", word);
		return 0;
	}
	else if (!mathlib && fastncmp("sfx_",word,4)) {
		p = word+4;
		for (i = 0; i < NUMSFX; i++)
			if (S_sfx[i].name && fastcmp(p, S_sfx[i].name)) {
				lua_pushinteger(L, i);
				return 1;
			}
		return 0;
	}
	else if (mathlib && fastncmp("SFX_",word,4)) { // SOCs are ALL CAPS!
		p = word+4;
		for (i = 0; i < NUMSFX; i++)
			if (S_sfx[i].name && fasticmp(p, S_sfx[i].name)) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "sfx '%s' could not be found.\n", word);
	}
	else if (mathlib && fastncmp("DS",word,2)) {
		p = word+2;
		for (i = 0; i < NUMSFX; i++)
			if (S_sfx[i].name && fasticmp(p, S_sfx[i].name)) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "sfx '%s' could not be found.\n", word);
		return 0;
	}
#ifdef MUSICSLOT_COMPATIBILITY
	else if (!mathlib && fastncmp("mus_",word,4)) {
		p = word+4;
		if ((i = get_mus(p, false)) == 0)
			return 0;
		lua_pushinteger(L, i);
		return 1;
	}
	else if (mathlib && fastncmp("MUS_",word,4)) { // SOCs are ALL CAPS!
		p = word+4;
		if ((i = get_mus(p, false)) == 0)
			return luaL_error(L, "music '%s' could not be found.\n", word);
		lua_pushinteger(L, i);
		return 1;
	}
	else if (mathlib && (fastncmp("O_",word,2) || fastncmp("D_",word,2))) {
		p = word+2;
		if ((i = get_mus(p, false)) == 0)
			return luaL_error(L, "music '%s' could not be found.\n", word);
		lua_pushinteger(L, i);
		return 1;
	}
#endif
	else if (!mathlib && fastncmp("pw_",word,3)) {
		p = word+3;
		for (i = 0; i < NUMPOWERS; i++)
			if (fasticmp(p, POWERS_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		return 0;
	}
	else if (mathlib && fastncmp("PW_",word,3)) { // SOCs are ALL CAPS!
		p = word+3;
		for (i = 0; i < NUMPOWERS; i++)
			if (fastcmp(p, POWERS_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "power '%s' could not be found.\n", word);
	}
	else if (fastncmp("HUD_",word,4)) {
		p = word+4;
		for (i = 0; i < NUMHUDITEMS; i++)
			if (fastcmp(p, HUDITEMS_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "huditem '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("SKINCOLOR_",word,10)) {
		p = word+10;
		for (i = 0; i < MAXTRANSLATIONS; i++)
			if (fastcmp(p, COLOR_ENUMS[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "skincolor '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("GRADE_",word,6))
	{
		p = word+6;
		for (i = 0; NIGHTSGRADE_LIST[i]; i++)
			if (*p == NIGHTSGRADE_LIST[i])
			{
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "NiGHTS grade '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("MN_",word,3)) {
		p = word+3;
		for (i = 0; i < NUMMENUTYPES; i++)
			if (fastcmp(p, MENUTYPES_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "menutype '%s' could not be found.\n", word);
		return 0;
	}
	else if (!mathlib && fastncmp("A_",word,2)) {
		char *caps;
		// Try to get a Lua action first.
		/// \todo Push a closure that sets superactions[] and superstack.
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_ACTIONS);
		// actions are stored in all uppercase.
		caps = Z_StrDup(word);
		strupr(caps);
		lua_getfield(L, -1, caps);
		Z_Free(caps);
		if (!lua_isnil(L, -1))
			return 1; // Success! :D That was easy.
		// Welp, that failed.
		lua_pop(L, 2); // pop nil and LREG_ACTIONS

		// Hardcoded actions as callable Lua functions!
		// Retrieving them from this metatable allows them to be case-insensitive!
		for (i = 0; actionpointers[i].name; i++)
			if (fasticmp(word, actionpointers[i].name)) {
				// We push the actionf_t* itself as userdata!
				LUA_PushUserdata(L, &actionpointers[i].action, META_ACTION);
				return 1;
			}
		return 0;
	}
	else if (!mathlib && fastcmp("super",word))
	{
		if (!superstack)
		{
			lua_pushcfunction(L, lib_dummysuper);
			return 1;
		}
		for (i = 0; actionpointers[i].name; i++)
			if (fasticmp(superactions[superstack-1], actionpointers[i].name)) {
				LUA_PushUserdata(L, &actionpointers[i].action, META_ACTION);
				return 1;
			}
		return 0;
	}

	for (i = 0; INT_CONST[i].n; i++)
		if (fastcmp(word,INT_CONST[i].n)) {
			lua_pushinteger(L, INT_CONST[i].v);
			return 1;
		}

	if (mathlib) return luaL_error(L, "constant '%s' could not be parsed.\n", word);

	// DYNAMIC variables too!!
	// Try not to add anything that would break netgames or timeattack replays here.
	// You know, like consoleplayer, displayplayer, secondarydisplayplayer, or gametime.
	return LUA_PushGlobals(L, word);
}

int LUA_EnumLib(lua_State *L)
{
	if (lua_gettop(L) == 0)
		lua_pushboolean(L, 0);

	// Set the global metatable
	lua_createtable(L, 0, 1);
	lua_pushvalue(L, 1); // boolean passed to LUA_EnumLib as first argument.
	lua_pushcclosure(L, lib_getenum, 1);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, LUA_GLOBALSINDEX);
	return 0;
}

// getActionName(action) -> return action's string name
static int lib_getActionName(lua_State *L)
{
	if (lua_isuserdata(L, 1)) // arg 1 is built-in action, expect action userdata
	{
		actionf_t *action = *((actionf_t **)luaL_checkudata(L, 1, META_ACTION));
		const char *name = NULL;
		if (!action)
			return luaL_error(L, "not a valid action?");
		name = LUA_GetActionName(action);
		if (!name) // that can't be right?
			return luaL_error(L, "no name string could be found for this action");
		lua_pushstring(L, name);
		return 1;
	}
	else if (lua_isfunction(L, 1)) // arg 1 is a function (either C or Lua)
	{
		lua_settop(L, 1); // set top of stack to 1 (removing any extra args, which there shouldn't be)
		// get the name for this action, if possible.
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_ACTIONS);
		lua_pushnil(L);
		// Lua stack at this point:
		//  1   ...       -2              -1
		// arg  ...   LREG_ACTIONS        nil
		while (lua_next(L, -2))
		{
			// Lua stack at this point:
			//  1   ...       -3              -2           -1
			// arg  ...   LREG_ACTIONS    "A_ACTION"    function
			if (lua_rawequal(L, -1, 1)) // is this the same as the arg?
			{
				// make sure the key (i.e. "A_ACTION") is a string first
				// (note: we don't use lua_isstring because it also returns true for numbers)
				if (lua_type(L, -2) == LUA_TSTRING)
				{
					lua_pushvalue(L, -2); // push "A_ACTION" string to top of stack
					return 1;
				}
				lua_pop(L, 2); // pop the name and function
				break; // probably should have succeeded but we didn't, so end the loop
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1); // pop LREG_ACTIONS
		return 0; // return nothing (don't error)
	}

	return luaL_typerror(L, 1, "action userdata or Lua function");
}



int LUA_SOCLib(lua_State *L)
{
	lua_register(L,"freeslot",lib_freeslot);
	lua_register(L,"getActionName",lib_getActionName);

	luaL_newmetatable(L, META_ACTION);
		lua_pushcfunction(L, action_call);
		lua_setfield(L, -2, "__call");
	lua_pop(L, 1);

	return 0;
}

const char *LUA_GetActionName(void *action)
{
	actionf_t *act = (actionf_t *)action;
	size_t z;
	for (z = 0; actionpointers[z].name; z++)
	{
		if (actionpointers[z].action.acv == act->acv)
			return actionpointers[z].name;
	}
	return NULL;
}

void LUA_SetActionByName(void *state, const char *actiontocompare)
{
	state_t *st = (state_t *)state;
	size_t z;
	for (z = 0; actionpointers[z].name; z++)
	{
		if (fasticmp(actiontocompare, actionpointers[z].name))
		{
			st->action = actionpointers[z].action;
			st->action.acv = actionpointers[z].action.acv; // assign
			st->action.acp1 = actionpointers[z].action.acp1;
			return;
		}
	}
}
