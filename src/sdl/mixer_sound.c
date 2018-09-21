/// \file
/// \brief SDL Mixer interface for sound

#include "../doomdef.h"

#if defined(HAVE_SDL) && defined(HAVE_MIXER) && SOUND==SOUND_MIXER

#include "../sounds.h"
#include "../s_sound.h"
#include "../i_sound.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../byteptr.h"

#ifdef _MSC_VER
#pragma warning(disable : 4214 4244)
#endif
#include "SDL.h"
#ifdef _MSC_VER
#pragma warning(default : 4214 4244)
#endif

#include "SDL_mixer.h"

/* This is the version number macro for the current SDL_mixer version: */
#ifndef SDL_MIXER_COMPILEDVERSION
#define SDL_MIXER_COMPILEDVERSION \
	SDL_VERSIONNUM(MIX_MAJOR_VERSION, MIX_MINOR_VERSION, MIX_PATCHLEVEL)
#endif

/* This macro will evaluate to true if compiled with SDL_mixer at least X.Y.Z */
#ifndef SDL_MIXER_VERSION_ATLEAST
#define SDL_MIXER_VERSION_ATLEAST(X, Y, Z) \
	(SDL_MIXER_COMPILEDVERSION >= SDL_VERSIONNUM(X, Y, Z))
#endif

#ifdef HAVE_LIBGME
#include "gme/gme.h"
#define GME_TREBLE 5.0
#define GME_BASS 1.0
#ifdef HAVE_PNG /// TODO: compile with zlib support without libpng

#define HAVE_ZLIB

#ifndef _MSC_VER
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#endif

#ifndef _LFS64_LARGEFILE
#define _LFS64_LARGEFILE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 0
#endif

#include "zlib.h"
#endif
#endif

static UINT16 BUFFERSIZE = 2048;
static UINT16 SAMPLERATE = 44100;

#ifdef HAVE_OPENMPT
#include "libopenmpt/libopenmpt.h"
#endif

UINT8 sound_started = false;

static boolean midimode;
static Mix_Music *music;
static UINT8 music_volume, midi_volume, sfx_volume;
static float loop_point;
static boolean songpaused;

#ifdef HAVE_LIBGME
static Music_Emu *gme;
static UINT16 current_track;
#endif

#ifdef HAVE_OPENMPT
static openmpt_module *mod = 0;
int mod_err = OPENMPT_ERROR_OK;
static const char *mod_err_str;
static UINT16 current_subsong;
#endif

///
/// OpenMPT Loading
///

#ifdef HAVE_OPENMPT

// Dynamic loading inspired by SDL Mixer
// Why: It's hard to compile for Windows without MSVC dependency, see https://trac.videolan.org/vlc/ticket/13055
// So let's not force that on the user, and they can download it if they want.
//
// ADD FUNCTIONS HERE AS YOU USE THEM!!!!!
typedef struct {
	int loaded;
	void *handle;

	// errors
	int (*module_error_get_last) ( openmpt_module * mod );
	const char *(*error_string) ( int error );
	const char *(*get_string) ( const char * key );

	// module loading
	void (*module_destroy) ( openmpt_module * mod );
	openmpt_module *(*module_create_from_memory2) ( const void * filedata, size_t filesize, openmpt_log_func logfunc, void * loguser, openmpt_error_func errfunc, void * erruser, int * error, const char * * error_message, const openmpt_module_initial_ctl * ctls );

	// audio callback
	size_t (*module_read_interleaved_stereo) ( openmpt_module * mod, int32_t samplerate, size_t count, int16_t * interleaved_stereo );

	// playback settings
	int (*module_set_render_param) ( openmpt_module * mod, int param, int32_t value );
	int (*module_set_repeat_count) ( openmpt_module * mod, int32_t repeat_count );
	int (*module_ctl_set) ( openmpt_module * mod, const char * ctl, const char * value );

	// positioning
	// double (*module_get_duration_seconds) ( openmpt_module * mod );
	// double (*module_get_position_seconds) ( openmpt_module * mod );
	// double (*module_set_position_seconds) ( openmpt_module * mod, double seconds );
	int32_t (*module_get_num_subsongs) ( openmpt_module * mod );
	int (*module_select_subsong) ( openmpt_module * mod, int32_t subsong );
} openmpt_loader;

static openmpt_loader openmpt = {
	0, NULL,
	NULL, NULL, NULL, // errors
	NULL, NULL, // module loading
	NULL, // audio callback
	NULL, NULL, NULL, // playback settings
	NULL, NULL//, NULL, NULL, NULL // positioning
};

#ifdef OPENMPT_DYNAMIC
#define FUNCTION_LOADER(NAME, FUNC, SIG) \
    openmpt.NAME = (SIG) SDL_LoadFunction(openmpt.handle, #FUNC); \
    if (openmpt.NAME == NULL) { SDL_UnloadObject(openmpt.handle); openmpt.handle = NULL; return; }
#else
#define FUNCTION_LOADER(NAME, FUNC, SIG) \
    openmpt.NAME = FUNC;
#endif

static void load_openmpt(void)
{
	if (openmpt.loaded)
		return;

#ifdef OPENMPT_DYNAMIC
#if defined(_WIN32) || defined(_WIN64)
	openmpt.handle = SDL_LoadObject("libopenmpt.dll");
#else
	openmpt.handle = SDL_LoadObject("libopenmpt.so");
#endif
	if (openmpt.handle == NULL)
	{
		CONS_Printf("libopenmpt not found, not loading.\n");
		return;
	}
#endif

	// errors
	FUNCTION_LOADER(module_error_get_last, openmpt_module_error_get_last, int (*) ( openmpt_module * mod ))
	FUNCTION_LOADER(error_string, openmpt_error_string, const char *(*) ( int error ))
	FUNCTION_LOADER(get_string, openmpt_get_string, const char *(*) ( const char * key ))

	// module loading
	FUNCTION_LOADER(module_destroy, openmpt_module_destroy, void (*) ( openmpt_module * mod ))
	FUNCTION_LOADER(module_create_from_memory2, openmpt_module_create_from_memory2, openmpt_module *(*) ( const void * filedata, size_t filesize, openmpt_log_func logfunc, void * loguser, openmpt_error_func errfunc, void * erruser, int * error, const char * * error_message, const openmpt_module_initial_ctl * ctls ))

	// audio callback
	FUNCTION_LOADER(module_read_interleaved_stereo, openmpt_module_read_interleaved_stereo, size_t (*) ( openmpt_module * mod, int32_t samplerate, size_t count, int16_t * interleaved_stereo ))

	// playback settings
	FUNCTION_LOADER(module_set_render_param, openmpt_module_set_render_param, int (*) ( openmpt_module * mod, int param, int32_t value ))
	FUNCTION_LOADER(module_set_repeat_count, openmpt_module_set_repeat_count, int (*) ( openmpt_module * mod, int32_t repeat_count ))
	FUNCTION_LOADER(module_ctl_set, openmpt_module_ctl_set, int (*) ( openmpt_module * mod, const char * ctl, const char * value ))

	// positioning
	// FUNCTION_LOADER(module_get_duration_seconds, openmpt_module_get_duration_seconds, double (*) ( openmpt_module * mod ))
	// FUNCTION_LOADER(module_get_position_seconds, openmpt_module_get_position_seconds, double (*) ( openmpt_module * mod ))
	// FUNCTION_LOADER(module_set_position_seconds, openmpt_module_set_position_seconds, double (*) ( openmpt_module * mod, double seconds ))
	FUNCTION_LOADER(module_get_num_subsongs, openmpt_module_get_num_subsongs, int32_t (*) ( openmpt_module * mod ))
	FUNCTION_LOADER(module_select_subsong, openmpt_module_select_subsong, int (*) ( openmpt_module * mod, int32_t subsong ))

#ifdef OPENMPT_DYNAMIC
	// this will be unset if a function failed to load
	if (openmpt.handle == NULL)
	{
		CONS_Printf("libopenmpt found but failed to load.\n");
		return;
	}
#endif

	CONS_Printf("libopenmpt version: %s\n", openmpt.get_string("library_version"));
	CONS_Printf("libopenmpt build date: %s\n", openmpt.get_string("build"));

	openmpt.loaded = 1;
}

static void unload_openmpt(void)
{
#ifdef OPENMPT_DYNAMIC
	if (openmpt.loaded)
	{
		SDL_UnloadObject(openmpt.handle);
		openmpt.handle = NULL;
		openmpt.loaded = 0;
	}
#endif
}

#undef FUNCTION_LOADER

#endif

#ifdef HAVE_OPENMPT
/**	\Helper function to log OpenMPT warnings and errors
*/
static void openmptlogger(const char *message, void * userdata)
{
	(void)userdata;
	if (message)
	{
		CONS_Printf("%s\n", message);
	}
}
#endif

void I_StartupSound(void)
{
	I_Assert(!sound_started);

	// EE inits audio first so we're following along.
	if (SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO)
		CONS_Printf("SDL Audio already started\n");
	else if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		CONS_Alert(CONS_ERROR, "Error initializing SDL Audio: %s\n", SDL_GetError());
		// call to start audio failed -- we do not have it
		return;
	}

	midimode = false;
	music = NULL;
	music_volume = midi_volume = sfx_volume = 0;

#if SDL_MIXER_VERSION_ATLEAST(1,2,11)
	Mix_Init(MIX_INIT_FLAC|MIX_INIT_MP3|MIX_INIT_OGG|MIX_INIT_MOD);
#endif

	if (Mix_OpenAudio(SAMPLERATE, AUDIO_S16SYS, 2, BUFFERSIZE) < 0)
	{
		CONS_Alert(CONS_ERROR, "Error starting SDL_Mixer: %s\n", Mix_GetError());
		// call to start audio failed -- we do not have it
		return;
	}

#ifdef HAVE_OPENMPT
	load_openmpt();
#endif

	sound_started = true;
	songpaused = false;
	Mix_AllocateChannels(256);
}

void I_ShutdownSound(void)
{
	if (!sound_started)
		return; // not an error condition
	sound_started = false;

	Mix_CloseAudio();
#if SDL_MIXER_VERSION_ATLEAST(1,2,11)
	Mix_Quit();
#endif

	SDL_QuitSubSystem(SDL_INIT_AUDIO);

#ifdef HAVE_LIBGME
	if (gme)
		gme_delete(gme);
#endif
#ifdef HAVE_OPENMPT
	if (mod)
		openmpt.module_destroy(mod);
	unload_openmpt();
#endif
}

FUNCMATH void I_UpdateSound(void)
{
}

// this is as fast as I can possibly make it.
// sorry. more asm needed.
static Mix_Chunk *ds2chunk(void *stream)
{
	UINT16 ver,freq;
	UINT32 samples, i, newsamples;
	UINT8 *sound;

	SINT8 *s;
	INT16 *d;
	INT16 o;
	fixed_t step, frac;

	// lump header
	ver = READUINT16(stream); // sound version format?
	if (ver != 3) // It should be 3 if it's a doomsound...
		return NULL; // onos! it's not a doomsound!
	freq = READUINT16(stream);
	samples = READUINT32(stream);

	// convert from signed 8bit ???hz to signed 16bit 44100hz.
	switch(freq)
	{
	case 44100:
		if (samples >= UINT32_MAX>>2)
			return NULL; // would wrap, can't store.
		newsamples = samples;
		break;
	case 22050:
		if (samples >= UINT32_MAX>>3)
			return NULL; // would wrap, can't store.
		newsamples = samples<<1;
		break;
	case 11025:
		if (samples >= UINT32_MAX>>4)
			return NULL; // would wrap, can't store.
		newsamples = samples<<2;
		break;
	default:
		frac = (44100 << FRACBITS) / (UINT32)freq;
		if (!(frac & 0xFFFF)) // other solid multiples (change if FRACBITS != 16)
			newsamples = samples * (frac >> FRACBITS);
		else // strange and unusual fractional frequency steps, plus anything higher than 44100hz.
			newsamples = FixedMul(FixedDiv(samples, freq), 44100) + 1; // add 1 to counter truncation.
		if (newsamples >= UINT32_MAX>>2)
			return NULL; // would and/or did wrap, can't store.
		break;
	}
	sound = malloc(newsamples<<2); // samples * frequency shift * bytes per sample * channels

	s = (SINT8 *)stream;
	d = (INT16 *)sound;

	i = 0;
	switch(freq)
	{
	case 44100: // already at the same rate? well that makes it simple.
		while(i++ < samples)
		{
			o = ((INT16)(*s++)+0x80)<<8; // changed signedness and shift up to 16 bits
			*d++ = o; // left channel
			*d++ = o; // right channel
		}
		break;
	case 22050: // unwrap 2x
		while(i++ < samples)
		{
			o = ((INT16)(*s++)+0x80)<<8; // changed signedness and shift up to 16 bits
			*d++ = o; // left channel
			*d++ = o; // right channel
			*d++ = o; // left channel
			*d++ = o; // right channel
		}
		break;
	case 11025: // unwrap 4x
		while(i++ < samples)
		{
			o = ((INT16)(*s++)+0x80)<<8; // changed signedness and shift up to 16 bits
			*d++ = o; // left channel
			*d++ = o; // right channel
			*d++ = o; // left channel
			*d++ = o; // right channel
			*d++ = o; // left channel
			*d++ = o; // right channel
			*d++ = o; // left channel
			*d++ = o; // right channel
		}
		break;
	default: // convert arbitrary hz to 44100.
		step = 0;
		frac = ((UINT32)freq << FRACBITS) / 44100 + 1; //Add 1 to counter truncation.
		while (i < samples)
		{
			o = (INT16)(*s+0x80)<<8; // changed signedness and shift up to 16 bits
			while (step < FRACUNIT) // this is as fast as I can make it.
			{
				*d++ = o; // left channel
				*d++ = o; // right channel
				step += frac;
			}
			do {
				i++; s++;
				step -= FRACUNIT;
			} while (step >= FRACUNIT);
		}
		break;
	}

	// return Mixer Chunk.
	return Mix_QuickLoad_RAW(sound, (Uint32)((UINT8*)d-sound));
}

void *I_GetSfx(sfxinfo_t *sfx)
{
	void *lump;
	Mix_Chunk *chunk;
#ifdef HAVE_LIBGME
	Music_Emu *emu;
	gme_info_t *info;
#endif

	if (sfx->lumpnum == LUMPERROR)
		sfx->lumpnum = S_GetSfxLumpNum(sfx);
	sfx->length = W_LumpLength(sfx->lumpnum);

	lump = W_CacheLumpNum(sfx->lumpnum, PU_SOUND);

	// convert from standard DoomSound format.
	chunk = ds2chunk(lump);
	if (chunk)
	{
		Z_Free(lump);
		return chunk;
	}

	// Not a doom sound? Try something else.
#ifdef HAVE_LIBGME
	// VGZ format
	if (((UINT8 *)lump)[0] == 0x1F
		&& ((UINT8 *)lump)[1] == 0x8B)
	{
#ifdef HAVE_ZLIB
		UINT8 *inflatedData;
		size_t inflatedLen;
		z_stream stream;
		int zErr; // Somewhere to handle any error messages zlib tosses out

		memset(&stream, 0x00, sizeof (z_stream)); // Init zlib stream
		// Begin the inflation process
		inflatedLen = *(UINT32 *)lump + (sfx->length-4); // Last 4 bytes are the decompressed size, typically
		inflatedData = (UINT8 *)Z_Malloc(inflatedLen, PU_SOUND, NULL); // Make room for the decompressed data
		stream.total_in = stream.avail_in = sfx->length;
		stream.total_out = stream.avail_out = inflatedLen;
		stream.next_in = (UINT8 *)lump;
		stream.next_out = inflatedData;

		zErr = inflateInit2(&stream, 32 + MAX_WBITS);
		if (zErr == Z_OK) // We're good to go
		{
			zErr = inflate(&stream, Z_FINISH);
			if (zErr == Z_STREAM_END) {
				// Run GME on new data
				if (!gme_open_data(inflatedData, inflatedLen, &emu, 44100))
				{
					short *mem;
					UINT32 len;
					gme_equalizer_t eq = {GME_TREBLE, GME_BASS, 0,0,0,0,0,0,0,0};

					Z_Free(inflatedData); // GME supposedly makes a copy for itself, so we don't need this lying around
					Z_Free(lump); // We're done with the uninflated lump now, too.

					gme_start_track(emu, 0);
					gme_set_equalizer(emu, &eq);
					gme_track_info(emu, &info, 0);

					len = (info->play_length * 441 / 10) << 2;
					mem = malloc(len);
					gme_play(emu, len >> 1, mem);
					gme_delete(emu);

					return Mix_QuickLoad_RAW((Uint8 *)mem, len);
				}
			}
			else
			{
				const char *errorType;
				switch (zErr)
				{
					case Z_ERRNO:
						errorType = "Z_ERRNO"; break;
					case Z_STREAM_ERROR:
						errorType = "Z_STREAM_ERROR"; break;
					case Z_DATA_ERROR:
						errorType = "Z_DATA_ERROR"; break;
					case Z_MEM_ERROR:
						errorType = "Z_MEM_ERROR"; break;
					case Z_BUF_ERROR:
						errorType = "Z_BUF_ERROR"; break;
					case Z_VERSION_ERROR:
						errorType = "Z_VERSION_ERROR"; break;
					default:
						errorType = "unknown error";
				}
				CONS_Alert(CONS_ERROR,"Encountered %s when running inflate: %s\n", errorType, stream.msg);
			}
			(void)inflateEnd(&stream);
		}
		else // Hold up, zlib's got a problem
		{
			const char *errorType;
			switch (zErr)
			{
				case Z_ERRNO:
					errorType = "Z_ERRNO"; break;
				case Z_STREAM_ERROR:
					errorType = "Z_STREAM_ERROR"; break;
				case Z_DATA_ERROR:
					errorType = "Z_DATA_ERROR"; break;
				case Z_MEM_ERROR:
					errorType = "Z_MEM_ERROR"; break;
				case Z_BUF_ERROR:
					errorType = "Z_BUF_ERROR"; break;
				case Z_VERSION_ERROR:
					errorType = "Z_VERSION_ERROR"; break;
				default:
					errorType = "unknown error";
			}
			CONS_Alert(CONS_ERROR,"Encountered %s when running inflateInit: %s\n", errorType, stream.msg);
		}
		Z_Free(inflatedData); // GME didn't open jack, but don't let that stop us from freeing this up
#else
		//CONS_Alert(CONS_ERROR,"Cannot decompress VGZ; no zlib support\n");
#endif
	}
	// Try to read it as a GME sound
	else if (!gme_open_data(lump, sfx->length, &emu, 44100))
	{
		short *mem;
		UINT32 len;
		gme_equalizer_t eq = {GME_TREBLE, GME_BASS, 0,0,0,0,0,0,0,0};

		Z_Free(lump);

		gme_start_track(emu, 0);
		gme_set_equalizer(emu, &eq);
		gme_track_info(emu, &info, 0);

		len = (info->play_length * 441 / 10) << 2;
		mem = malloc(len);
		gme_play(emu, len >> 1, mem);
		gme_delete(emu);

		return Mix_QuickLoad_RAW((Uint8 *)mem, len);
	}
#endif

	// Try to load it as a WAVE or OGG using Mixer.
	return Mix_LoadWAV_RW(SDL_RWFromMem(lump, sfx->length), 1);
}

void I_FreeSfx(sfxinfo_t *sfx)
{
	if (sfx->data)
		Mix_FreeChunk(sfx->data);
	sfx->data = NULL;
	sfx->lumpnum = LUMPERROR;
}

INT32 I_StartSound(sfxenum_t id, UINT8 vol, UINT8 sep, UINT8 pitch, UINT8 priority)
{
	UINT8 volume = (((UINT16)vol + 1) * (UINT16)sfx_volume) / 62; // (256 * 31) / 62 == 127
	INT32 handle = Mix_PlayChannel(-1, S_sfx[id].data, 0);
	Mix_Volume(handle, volume);
	Mix_SetPanning(handle, min((UINT16)(0xff-sep)<<1, 0xff), min((UINT16)(sep)<<1, 0xff));
	(void)pitch; // Mixer can't handle pitch
	(void)priority; // priority and channel management is handled by SRB2...
	return handle;
}

void I_StopSound(INT32 handle)
{
	Mix_HaltChannel(handle);
}

boolean I_SoundIsPlaying(INT32 handle)
{
	return Mix_Playing(handle);
}

void I_UpdateSoundParams(INT32 handle, UINT8 vol, UINT8 sep, UINT8 pitch)
{
	UINT8 volume = (((UINT16)vol + 1) * (UINT16)sfx_volume) / 62; // (256 * 31) / 62 == 127
	Mix_Volume(handle, volume);
	Mix_SetPanning(handle, min((UINT16)(0xff-sep)<<1, 0xff), min((UINT16)(sep)<<1, 0xff));
	(void)pitch;
}

void I_SetSfxVolume(UINT8 volume)
{
	sfx_volume = volume;
}

//
// Music
//

// Music hooks
static void music_loop(void)
{
	Mix_PlayMusic(music, 0);
	Mix_SetMusicPosition(loop_point);
}

#ifdef HAVE_LIBGME
static void mix_gme(void *udata, Uint8 *stream, int len)
{
	int i;
	short *p;

	(void)udata;

	// no gme? no music.
	if (!gme || gme_track_ended(gme) || songpaused)
		return;

	// play gme into stream
	gme_play(gme, len/2, (short *)stream);

	// apply volume to stream
	for (i = 0, p = (short *)stream; i < len/2; i++, p++)
		*p = ((INT32)*p) * music_volume*2 / 42;
}
#endif

#ifdef HAVE_OPENMPT
static void mix_openmpt(void *udata, Uint8 *stream, int len)
{
	int i;
	short *p;

	if (!mod || songpaused)
		return;

	(void)udata;

	openmpt.module_set_render_param(mod, OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH, cv_modfilter.value);
	openmpt.module_set_repeat_count(mod, -1); // Always repeat
	openmpt.module_read_interleaved_stereo(mod, SAMPLERATE, BUFFERSIZE, (short *)stream);

	// apply volume to stream
	for (i = 0, p = (short *)stream; i < len/2; i++, p++)
		*p = ((INT32)*p) * music_volume / 31;
}
#endif

FUNCMATH void I_InitMusic(void)
{
}

void I_ShutdownMusic(void)
{
	I_ShutdownDigMusic();
	I_ShutdownMIDIMusic();
}

void I_PauseSong(INT32 handle)
{
	(void)handle;
	Mix_PauseMusic();
	songpaused = true;
}

void I_ResumeSong(INT32 handle)
{
	(void)handle;
	Mix_ResumeMusic();
	songpaused = false;
}

//
// Digital Music
//

void I_InitDigMusic(void)
{
#ifdef HAVE_LIBGME
	gme = NULL;
	current_track = -1;
#endif

#ifdef HAVE_OPENMPT
	current_subsong = -1;
#endif
}

void I_ShutdownDigMusic(void)
{
	if (midimode)
		return;
#ifdef HAVE_LIBGME
	if (gme)
	{
		Mix_HookMusic(NULL, NULL);
		gme_delete(gme);
		gme = NULL;
	}
#endif

#ifdef HAVE_OPENMPT
	if (mod)
	{
		Mix_HookMusic(NULL, NULL);
		openmpt.module_destroy(mod);
		mod = NULL;
	}
#endif
	if (!music)
		return;
	Mix_HookMusicFinished(NULL);
	Mix_FreeMusic(music);
	music = NULL;
}

boolean I_StartDigSong(const char *musicname, boolean looping)
{
	char *data;
	size_t len;
	lumpnum_t lumpnum = W_CheckNumForName(va("O_%s",musicname));

	I_Assert(!music);
#ifdef HAVE_LIBGME
	I_Assert(!gme);
#endif

#ifdef HAVE_OPENMPT
	I_Assert(!mod);
#endif

	if (lumpnum == LUMPERROR)
		return false;
	midimode = false;

	data = (char *)W_CacheLumpNum(lumpnum, PU_MUSIC);
	len = W_LumpLength(lumpnum);

#ifdef HAVE_LIBGME
	if ((UINT8)data[0] == 0x1F
		&& (UINT8)data[1] == 0x8B)
	{
#ifdef HAVE_ZLIB
		UINT8 *inflatedData;
		size_t inflatedLen;
		z_stream stream;
		int zErr; // Somewhere to handle any error messages zlib tosses out

		memset(&stream, 0x00, sizeof (z_stream)); // Init zlib stream
		// Begin the inflation process
		inflatedLen = *(UINT32 *)(data + (len-4)); // Last 4 bytes are the decompressed size, typically
		inflatedData = (UINT8 *)Z_Calloc(inflatedLen, PU_MUSIC, NULL); // Make room for the decompressed data
		stream.total_in = stream.avail_in = len;
		stream.total_out = stream.avail_out = inflatedLen;
		stream.next_in = (UINT8 *)data;
		stream.next_out = inflatedData;

		zErr = inflateInit2(&stream, 32 + MAX_WBITS);
		if (zErr == Z_OK) // We're good to go
		{
			zErr = inflate(&stream, Z_FINISH);
			if (zErr == Z_STREAM_END) {
				// Run GME on new data
				if (!gme_open_data(inflatedData, inflatedLen, &gme, 44100))
				{
					gme_equalizer_t eq = {GME_TREBLE, GME_BASS, 0,0,0,0,0,0,0,0};
					gme_start_track(gme, 0);
					current_track = 0;
					gme_set_equalizer(gme, &eq);
					Mix_HookMusic(mix_gme, gme);
					Z_Free(inflatedData); // GME supposedly makes a copy for itself, so we don't need this lying around
					return true;
				}
			}
			else
			{
				const char *errorType;
				switch (zErr)
				{
					case Z_ERRNO:
						errorType = "Z_ERRNO"; break;
					case Z_STREAM_ERROR:
						errorType = "Z_STREAM_ERROR"; break;
					case Z_DATA_ERROR:
						errorType = "Z_DATA_ERROR"; break;
					case Z_MEM_ERROR:
						errorType = "Z_MEM_ERROR"; break;
					case Z_BUF_ERROR:
						errorType = "Z_BUF_ERROR"; break;
					case Z_VERSION_ERROR:
						errorType = "Z_VERSION_ERROR"; break;
					default:
						errorType = "unknown error";
				}
				CONS_Alert(CONS_ERROR,"Encountered %s when running inflate: %s\n", errorType, stream.msg);
			}
			(void)inflateEnd(&stream);
		}
		else // Hold up, zlib's got a problem
		{
			const char *errorType;
			switch (zErr)
			{
				case Z_ERRNO:
					errorType = "Z_ERRNO"; break;
				case Z_STREAM_ERROR:
					errorType = "Z_STREAM_ERROR"; break;
				case Z_DATA_ERROR:
					errorType = "Z_DATA_ERROR"; break;
				case Z_MEM_ERROR:
					errorType = "Z_MEM_ERROR"; break;
				case Z_BUF_ERROR:
					errorType = "Z_BUF_ERROR"; break;
				case Z_VERSION_ERROR:
					errorType = "Z_VERSION_ERROR"; break;
				default:
					errorType = "unknown error";
			}
			CONS_Alert(CONS_ERROR,"Encountered %s when running inflateInit: %s\n", errorType, stream.msg);
		}
		Z_Free(inflatedData); // GME didn't open jack, but don't let that stop us from freeing this up
#else
		//CONS_Alert(CONS_ERROR,"Cannot decompress VGZ; no zlib support\n");
#endif
	}
	else if (!gme_open_data(data, len, &gme, 44100))
	{
		gme_equalizer_t eq = {GME_TREBLE, GME_BASS, 0,0,0,0,0,0,0,0};
		gme_start_track(gme, 0);
		current_track = 0;
		gme_set_equalizer(gme, &eq);
		Mix_HookMusic(mix_gme, gme);
		return true;
	}
#endif

	music = Mix_LoadMUS_RW(SDL_RWFromMem(data, len), SDL_FALSE);
	if (!music)
	{
		CONS_Alert(CONS_ERROR, "Mix_LoadMUS_RW: %s\n", Mix_GetError());
		return true;
	}

#ifdef HAVE_OPENMPT
	switch(Mix_GetMusicType(music))
	{
		case MUS_MODPLUG_UNUSED:
		case MUS_MOD:
			if (openmpt.loaded) {
				mod = openmpt.module_create_from_memory2(data, len, &openmptlogger, NULL, NULL, NULL, NULL, NULL, NULL);
				if (!mod)
				{
					mod_err = openmpt.module_error_get_last(mod);
					mod_err_str = openmpt.error_string(mod_err);
					CONS_Alert(CONS_ERROR, "openmpt.module_create_from_memory2: %s\n", mod_err_str);
				}
				else
				{
					openmpt.module_select_subsong(mod, 0);
					current_subsong = 0;
					Mix_HookMusic(mix_openmpt, mod);
				}
				Mix_FreeMusic(music);
				music = NULL;
				return true;
			}
			// else, fall through
		case MUS_WAV:
		case MUS_MID:
		case MUS_OGG:
		case MUS_MP3:
		case MUS_FLAC:
			Mix_HookMusic(NULL, NULL);
			break;
		default:
			break;
	}
#endif

	// Find the OGG loop point.
	loop_point = 0.0f;
	if (looping)
	{
		const char *key1 = "LOOP";
		const char *key2 = "POINT=";
		const char *key3 = "MS=";
		const size_t key1len = strlen(key1);
		const size_t key2len = strlen(key2);
		const size_t key3len = strlen(key3);
		char *p = data;
		while ((UINT32)(p - data) < len)
		{
			if (strncmp(p++, key1, key1len))
				continue;
			p += key1len-1; // skip OOP (the L was skipped in strncmp)
			if (!strncmp(p, key2, key2len)) // is it LOOPPOINT=?
			{
				p += key2len; // skip POINT=
				loop_point = (float)((44.1L+atoi(p)) / 44100.0L); // LOOPPOINT works by sample count.
				// because SDL_Mixer is USELESS and can't even tell us
				// something simple like the frequency of the streaming music,
				// we are unfortunately forced to assume that ALL MUSIC is 44100hz.
				// This means a lot of tracks that are only 22050hz for a reasonable downloadable file size will loop VERY badly.
			}
			else if (!strncmp(p, key3, key3len)) // is it LOOPMS=?
			{
				p += key3len; // skip MS=
				loop_point = (float)(atoi(p) / 1000.0L); // LOOPMS works by real time, as miliseconds.
				// Everything that uses LOOPMS will work perfectly with SDL_Mixer.
			}
			// Neither?! Continue searching.
		}
	}

	if (Mix_PlayMusic(music, looping && loop_point == 0.0f ? -1 : 0) == -1)
	{
		CONS_Alert(CONS_ERROR, "Mix_PlayMusic: %s\n", Mix_GetError());
		return true;
	}
	Mix_VolumeMusic((UINT32)music_volume*128/31);

	if (loop_point != 0.0f)
		Mix_HookMusicFinished(music_loop);
	return true;
}

void I_StopDigSong(void)
{
	if (midimode)
		return;
#ifdef HAVE_LIBGME
	if (gme)
	{
		Mix_HookMusic(NULL, NULL);
		gme_delete(gme);
		gme = NULL;
		current_track = -1;
		return;
	}
#endif

#ifdef HAVE_OPENMPT
	if (mod)
	{
		Mix_HookMusic(NULL, NULL);
		openmpt.module_destroy(mod);
		mod = NULL;
		current_subsong = -1;
	}
#endif
	if (!music)
		return;
	Mix_HookMusicFinished(NULL);
	Mix_FreeMusic(music);
	music = NULL;
}

void I_SetDigMusicVolume(UINT8 volume)
{
	music_volume = volume;
	if (midimode || !music)
		return;
	Mix_VolumeMusic((UINT32)volume*128/31);
}

boolean I_SetSongSpeed(float speed)
{
	if (speed > 250.0f)
		speed = 250.0f; //limit speed up to 250x
#ifdef HAVE_LIBGME
	if (gme)
	{
		SDL_LockAudio();
		gme_set_tempo(gme, speed);
		SDL_UnlockAudio();
		return true;
	}
#else
	(void)speed;
	return false;
#endif

#ifdef HAVE_OPENMPT
	char modspd[16];
	if (mod)
	{
		sprintf(modspd, "%g", speed);
		openmpt.module_ctl_set(mod, "play.tempo_factor", modspd);
		return true;
	}
#else
	(void)speed;
	return false;
#endif
	return false;
}

boolean I_SetSongTrack(int track)
{
#ifdef HAVE_LIBGME
	if (current_track == track)
		return false;

	// If the specified track is within the number of tracks playing, then change it
	if (gme)
	{
		SDL_LockAudio();
		if (track >= 0 && track < gme_track_count(gme)-1)
		{
			gme_err_t gme_e = gme_start_track(gme, track);
			if (gme_e != NULL)
			{
				CONS_Alert(CONS_ERROR, "GME error: %s\n", gme_e);
				return false;
			}
			current_track = track;
			SDL_UnlockAudio();
			return true;
		}
		SDL_UnlockAudio();
		return false;
	}
#endif

#ifdef HAVE_OPENMPT
	if (current_subsong == track)
		return false;

	if (mod)
	{
		SDL_LockAudio();
		if (track >= 0 && track < openmpt.module_get_num_subsongs(mod))
		{
			openmpt.module_select_subsong(mod, track);
			current_subsong = track;
			SDL_UnlockAudio();
			return true;
		}
		SDL_UnlockAudio();
		return false;
	}
#endif
return true;
}

//
// MIDI Music
//

FUNCMATH void I_InitMIDIMusic(void)
{
}

void I_ShutdownMIDIMusic(void)
{
	if (!midimode || !music)
		return;
	Mix_FreeMusic(music);
	music = NULL;
}

void I_SetMIDIMusicVolume(UINT8 volume)
{
	// HACK: Until we stop using native MIDI,
	// disable volume changes
	(void)volume;
	midi_volume = 31;
	//midi_volume = volume;

	if (!midimode || !music)
		return;
	Mix_VolumeMusic((UINT32)midi_volume*128/31);
}

INT32 I_RegisterSong(void *data, size_t len)
{
	music = Mix_LoadMUS_RW(SDL_RWFromMem(data, len), SDL_FALSE);
	if (!music)
	{
		CONS_Alert(CONS_ERROR, "Mix_LoadMUS_RW: %s\n", Mix_GetError());
		return -1;
	}
	return 1337;
}

boolean I_PlaySong(INT32 handle, boolean looping)
{
	(void)handle;

	midimode = true;

	if (Mix_PlayMusic(music, looping ? -1 : 0) == -1)
	{
		CONS_Alert(CONS_ERROR, "Mix_PlayMusic: %s\n", Mix_GetError());
		return false;
	}

	Mix_VolumeMusic((UINT32)midi_volume*128/31);
	return true;
}

void I_StopSong(INT32 handle)
{
	if (!midimode || !music)
		return;

	(void)handle;
	Mix_HaltMusic();
}

void I_UnRegisterSong(INT32 handle)
{
	if (!midimode || !music)
		return;

	(void)handle;
	Mix_FreeMusic(music);
	music = NULL;
}

#endif
