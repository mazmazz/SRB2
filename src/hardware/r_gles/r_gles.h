// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1998-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_gles.h
/// \brief OpenGL ES API for Sonic Robo Blast 2

#ifndef _R_OPENGL_H_
#define _R_OPENGL_H_

#define GL_GLEXT_PROTOTYPES
#undef DRIVER_STRING

#if defined(__EMSCRIPTEN__)
#ifdef HAVE_REGAL
#include <GL/Regal.h>
#endif
#endif

#ifdef HAVE_GLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define DRIVER_STRING "OpenGL ES 1.1"
#else
#include <GLES/gl.h>
#include <GLES/glext.h>
#define DRIVER_STRING "OpenGL ES 2.0"
#endif

#define _CREATE_DLL_ // This is apparently required
#undef DEBUG_TO_FILE

#include "../../doomdef.h"
#include "../hw_drv.h"

// ==========================================================================
//                                                                     PROTOS
// ==========================================================================

boolean LoadGL(void);
void *GetGLFunc(const char *proc);
boolean SetupGLfunc(void);
boolean SetupGLFunc13(void);
void Flush(void);
INT32 isExtAvailable(const char *extension, const GLubyte *start);
int SetupPixelFormat(INT32 WantColorBits, INT32 WantStencilBits, INT32 WantDepthBits);
void SetModelView(GLint w, GLint h);
void SetStates(void);

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

/* 1.0 Miscellaneous functions */
#ifdef STATIC_OPENGL
#define pglClear glClear
#define pglGetIntegerv glGetIntegerv
#define pglGetString glGetString
#else
typedef void (*PFNglClear) (GLbitfield mask);
extern PFNglClear pglClear;
typedef void (*PFNglGetIntegerv) (GLenum pname, GLint *params);
extern PFNglGetIntegerv pglGetIntegerv;
typedef const GLubyte *(*PFNglGetString) (GLenum name);
extern PFNglGetString pglGetString;
#endif

// ==========================================================================
//                                                                     GLOBAL
// ==========================================================================

extern const GLubyte    *gl_extensions;
extern RGBA_t           myPaletteData[];
extern GLint            screen_width;
extern GLint            screen_height;
extern GLbyte           screen_depth;
extern GLint            maximumAnisotropy;

/**	\brief OpenGL flags for video driver
*/
extern GLint            textureformatGL;

#endif
