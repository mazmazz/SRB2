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
/// \file r_gles.c
/// \brief OpenGL ES 1.1 API for Sonic Robo Blast 2

#include <stdarg.h>
#include <math.h>

#include "r_gles.h"
#include "../r_opengl/r_vbo.h"

#if defined (HWRENDER) && !defined (NOROPENGL)

struct GLRGBAFloat
{
	GLfloat red;
	GLfloat green;
	GLfloat blue;
	GLfloat alpha;
};
typedef struct GLRGBAFloat GLRGBAFloat;
static const GLfloat white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

// ==========================================================================
//                                                                  CONSTANTS
// ==========================================================================

// With OpenGL 1.1+, the first texture should be 1
static GLuint NOTEXTURE_NUM = 0;

#define      N_PI_DEMI               (M_PIl/2.0f) //(1.5707963268f)

#define      ASPECT_RATIO            (1.0f)  //(320.0f/200.0f)
#define      FAR_CLIPPING_PLANE      32768.0f // Draw further! Tails 01-21-2001
static float NEAR_CLIPPING_PLANE =   NZCLIP_PLANE;

// **************************************************************************
//                                                                    GLOBALS
// **************************************************************************


static  GLuint      tex_downloaded  = 0;
static  GLfloat     fov             = 90.0f;
static  FBITFIELD   CurrentPolyFlags;

static  FTextureInfo *gr_cachetail = NULL;
static  FTextureInfo *gr_cachehead = NULL;

RGBA_t  myPaletteData[256];
GLint   screen_width    = 0;               // used by Draw2DLine()
GLint   screen_height   = 0;
GLbyte  screen_depth    = 0;
GLint   textureformatGL = 0;
GLint maximumAnisotropy = 0;
static GLboolean MipMap = GL_FALSE;
static GLint min_filter = GL_LINEAR;
static GLint mag_filter = GL_LINEAR;
static GLint anisotropic_filter = 0;
static boolean model_lighting = true;

const GLubyte *gl_extensions = NULL;

// Sryder:	NextTexAvail is broken for these because palette changes or changes to the texture filter or antialiasing
//			flush all of the stored textures, leaving them unavailable at times such as between levels
//			These need to start at 0 and be set to their number, and be reset to 0 when deleted so that intel GPUs
//			can know when the textures aren't there, as textures are always considered resident in their virtual memory
static GLuint screentexture = 0;
static GLuint startScreenWipe = 0;
static GLuint endScreenWipe = 0;
static GLuint finalScreenTexture = 0;

// -----------------+
// DBG_Printf       : Output error messages
// -----------------+
FUNCPRINTF void DBG_Printf(const char *lpFmt, ...)
{
	char    str[4096] = "";
	va_list arglist;

	va_start (arglist, lpFmt);
	vsnprintf (str, 4096, lpFmt, arglist);
	va_end   (arglist);
	CONS_Printf("%s\n", str);
}

#ifdef STATIC_OPENGL
/* 1.0 functions */
/* Miscellaneous */
#define pglClearColor glClearColor
#define pglColorMask glColorMask
#define pglAlphaFunc glAlphaFunc
#define pglBlendFunc glBlendFunc
#define pglCullFace glCullFace
#define pglPolygonOffset glPolygonOffset
#define pglScissor glScissor
#define pglEnable glEnable
#define pglDisable glDisable
#define pglGetFloatv glGetFloatv

/* Depth Buffer */
#define pglClearDepthf glClearDepthf
#define pglDepthFunc glDepthFunc
#define pglDepthMask glDepthMask
#define pglDepthRangef glDepthRangef

/* Transformation */
#define pglMatrixMode glMatrixMode
#define pglViewport glViewport
#define pglPushMatrix glPushMatrix
#define pglPopMatrix glPopMatrix
#define pglLoadIdentity glLoadIdentity
#define pglMultMatrixf glMultMatrixf
#define pglRotatef glRotatef
#define pglScalef glScalef
#define pglTranslatef glTranslatef

/* Drawing Functions */
#define pglColor4f glColor4f
#define pglVertexPointer glVertexPointer
#define pglNormalPointer glNormalPointer
#define pglTexCoordPointer glTexCoordPointer
#define pglColorPointer glColorPointer
#define pglDrawArrays glDrawArrays
#define pglDrawElements glDrawElements
#define pglEnableClientState glEnableClientState
#define pglDisableClientState glDisableClientState

/* Lighting */
#define pglShadeModel glShadeModel
#define pglLightfv glLightfv
#define pglLightModelfv glLightModelfv
#define pglMaterialfv glMaterialfv

/* Raster functions */
#define pglPixelStorei glPixelStorei
#define pglReadPixels glReadPixels

/* Texture mapping */
#define pglTexEnvi glTexEnvi
#define pglTexParameteri glTexParameteri
#define pglTexImage2D glTexImage2D

/* Fog */
#define pglFogf glFogf
#define pglFogfv glFogfv

/* 1.1 functions */
/* texture objects */ //GL_EXT_texture_object
#define pglGenTextures glGenTextures
#define pglDeleteTextures glDeleteTextures
#define pglBindTexture glBindTexture
/* texture mapping */ //GL_EXT_copy_texture
#define pglCopyTexImage2D glCopyTexImage2D
#define pglCopyTexSubImage2D glCopyTexSubImage2D
#define pglGenerateMipmap glGenerateMipmap

/* 1.3 functions for multitexturing */
#define pglActiveTexture glActiveTexture
#define pglMultiTexCoord2f glMultiTexCoord2f
#define pglMultiTexCoord2fv glMultiTexCoord2fv
#define pglClientActiveTexture glClientActiveTexture

/* 1.5 functions for buffers */
#define pglGenBuffers glGenBuffers
#define pglBindBuffer glBindBuffer
#define pglBufferData glBufferData
#define pglDeleteBuffers glDeleteBuffers

#else //!STATIC_OPENGL

/* 1.0 functions */
/* Miscellaneous */
typedef void (*PFNglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
static PFNglClearColor pglClearColor;
typedef void (*PFNglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
static PFNglColorMask pglColorMask;
typedef void (*PFNglAlphaFunc) (GLenum func, GLclampf ref);
static PFNglAlphaFunc pglAlphaFunc;
typedef void (*PFNglBlendFunc) (GLenum sfactor, GLenum dfactor);
static PFNglBlendFunc pglBlendFunc;
typedef void (*PFNglCullFace) (GLenum mode);
static PFNglCullFace pglCullFace;
typedef void (*PFNglPolygonOffset) (GLfloat factor, GLfloat units);
static PFNglPolygonOffset pglPolygonOffset;
typedef void (*PFNglScissor) (GLint x, GLint y, GLsizei width, GLsizei height);
static PFNglScissor pglScissor;
typedef void (*PFNglEnable) (GLenum cap);
static PFNglEnable pglEnable;
typedef void (*PFNglDisable) (GLenum cap);
static PFNglDisable pglDisable;
typedef void (*PFNglGetFloatv) (GLenum pname, GLfloat *params);
static PFNglGetFloatv pglGetFloatv;

/* Depth Buffer */
typedef void (*PFNglClearDepthf) (GLclampf depth);
static PFNglClearDepthf pglClearDepthf;
typedef void (*PFNglDepthFunc) (GLenum func);
static PFNglDepthFunc pglDepthFunc;
typedef void (*PFNglDepthMask) (GLboolean flag);
static PFNglDepthMask pglDepthMask;
typedef void (*PFNglDepthRangef) (GLclampf near_val, GLclampf far_val);
static PFNglDepthRangef pglDepthRangef;

/* Transformation */
typedef void (*PFNglMatrixMode) (GLenum mode);
static PFNglMatrixMode pglMatrixMode;
typedef void (*PFNglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);
static PFNglViewport pglViewport;
typedef void (*PFNglPushMatrix) (void);
static PFNglPushMatrix pglPushMatrix;
typedef void (*PFNglPopMatrix) (void);
static PFNglPopMatrix pglPopMatrix;
typedef void (*PFNglLoadIdentity) (void);
static PFNglLoadIdentity pglLoadIdentity;
typedef void (*PFNglMultMatrixf) (const GLfloat *m);
static PFNglMultMatrixf pglMultMatrixf;
typedef void (*PFNglRotatef) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
static PFNglRotatef pglRotatef;
typedef void (*PFNglScalef) (GLfloat x, GLfloat y, GLfloat z);
static PFNglScalef pglScalef;
typedef void (*PFNglTranslatef) (GLfloat x, GLfloat y, GLfloat z);
static PFNglTranslatef pglTranslatef;

/* Drawing Functions */
typedef void (*PFNglColor4f) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
static PFNglColor4f pglColor4f;
typedef void (*PFNglVertexPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
static PFNglVertexPointer pglVertexPointer;
typedef void (*PFNglNormalPointer) (GLenum type, GLsizei stride, const GLvoid *pointer);
static PFNglNormalPointer pglNormalPointer;
typedef void (*PFNglTexCoordPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
static PFNglTexCoordPointer pglTexCoordPointer;
typedef void (*PFNglColorPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
static PFNglColorPointer pglColorPointer;
typedef void (*PFNglDrawArrays) (GLenum mode, GLint first, GLsizei count);
static PFNglDrawArrays pglDrawArrays;
typedef void (*PFNglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
static PFNglDrawElements pglDrawElements;
typedef void (*PFNglEnableClientState) (GLenum cap);
static PFNglEnableClientState pglEnableClientState;
typedef void (*PFNglDisableClientState) (GLenum cap);
static PFNglDisableClientState pglDisableClientState;

/* Lighting */
typedef void (*PFNglShadeModel) (GLenum mode);
static PFNglShadeModel pglShadeModel;
typedef void (*PFNglLightfv) (GLenum light, GLenum pname, GLfloat *params);
static PFNglLightfv pglLightfv;
typedef void (*PFNglLightModelfv) (GLenum pname, GLfloat *params);
static PFNglLightModelfv pglLightModelfv;
typedef void (*PFNglMaterialfv) (GLint face, GLenum pname, GLfloat *params);
static PFNglMaterialfv pglMaterialfv;

/* Raster functions */
typedef void (*PFNglPixelStorei) (GLenum pname, GLint param);
static PFNglPixelStorei pglPixelStorei;
typedef void (*PFNglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
static PFNglReadPixels pglReadPixels;

/* Texture mapping */
typedef void (*PFNglTexEnvi) (GLenum target, GLenum pname, GLint param);
static PFNglTexEnvi pglTexEnvi;
typedef void (*PFNglTexParameteri) (GLenum target, GLenum pname, GLint param);
static PFNglTexParameteri pglTexParameteri;
typedef void (*PFNglTexImage2D) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
static PFNglTexImage2D pglTexImage2D;

/* Fog */
typedef void (*PFNglFogf) (GLenum pname, GLfloat param);
static PFNglFogf pglFogf;
typedef void (*PFNglFogfv) (GLenum pname, const GLfloat *params);
static PFNglFogfv pglFogfv;

/* 1.1 functions */
/* texture objects */ //GL_EXT_texture_object
typedef void (*PFNglGenTextures) (GLsizei n, const GLuint *textures);
static PFNglGenTextures pglGenTextures;
typedef void (*PFNglDeleteTextures) (GLsizei n, const GLuint *textures);
static PFNglDeleteTextures pglDeleteTextures;
typedef void (*PFNglBindTexture) (GLenum target, GLuint texture);
static PFNglBindTexture pglBindTexture;
/* texture mapping */ //GL_EXT_copy_texture
typedef void (*PFNglCopyTexImage2D) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
static PFNglCopyTexImage2D pglCopyTexImage2D;
typedef void (*PFNglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
static PFNglCopyTexSubImage2D pglCopyTexSubImage2D;
typedef void (*PFNglGenerateMipmap) (GLenum target);
static PFNglGenerateMipmap pglGenerateMipmap;

/* 1.3 functions for multitexturing */
typedef void (*PFNglActiveTexture) (GLenum);
static PFNglActiveTexture pglActiveTexture;
typedef void (*PFNglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
static PFNglMultiTexCoord2f pglMultiTexCoord2f;
typedef void (*PFNglMultiTexCoord2fv) (GLenum target, const GLfloat *v);
static PFNglMultiTexCoord2fv pglMultiTexCoord2fv;
typedef void (*PFNglClientActiveTexture) (GLenum);
static PFNglClientActiveTexture pglClientActiveTexture;

/* 1.5 functions for buffers */
typedef void (*PFNglGenBuffers) (GLsizei n, GLuint *buffers);
static PFNglGenBuffers pglGenBuffers;
typedef void (*PFNglBindBuffer) (GLenum target, GLuint buffer);
static PFNglBindBuffer pglBindBuffer;
typedef void (*PFNglBufferData) (GLenum target, GLsizei size, const GLvoid *data, GLenum usage);
static PFNglBufferData pglBufferData;
typedef void (*PFNglDeleteBuffers) (GLsizei n, const GLuint *buffers);
static PFNglDeleteBuffers pglDeleteBuffers;
#endif

/* 1.2 Parms */
/* GL_CLAMP_TO_EDGE_EXT */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

/* 1.3 GL_TEXTUREi */
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

/* 1.5 Parms */
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

boolean SetupGLfunc(void)
{
#ifndef STATIC_OPENGL
#define GETOPENGLFUNC(func, proc) \
	func = GetGLFunc(#proc); \
	if (!func) \
	{ \
		DBG_Printf("failed to get OpenGL function: %s", #proc); \
	} \

	GETOPENGLFUNC(pglClearColor, glClearColor)

	GETOPENGLFUNC(pglClear, glClear)
	GETOPENGLFUNC(pglColorMask, glColorMask)
	GETOPENGLFUNC(pglAlphaFunc, glAlphaFunc)
	GETOPENGLFUNC(pglBlendFunc, glBlendFunc)
	GETOPENGLFUNC(pglCullFace, glCullFace)
	GETOPENGLFUNC(pglPolygonOffset, glPolygonOffset)
	GETOPENGLFUNC(pglScissor, glScissor)
	GETOPENGLFUNC(pglEnable, glEnable)
	GETOPENGLFUNC(pglDisable, glDisable)
	GETOPENGLFUNC(pglGetFloatv, glGetFloatv)
	GETOPENGLFUNC(pglGetIntegerv, glGetIntegerv)
	GETOPENGLFUNC(pglGetString, glGetString)

	GETOPENGLFUNC(pglClearDepthf , glClearDepthf)
	GETOPENGLFUNC(pglDepthFunc , glDepthFunc)
	GETOPENGLFUNC(pglDepthMask , glDepthMask)
	GETOPENGLFUNC(pglDepthRangef , glDepthRangef)

	GETOPENGLFUNC(pglMatrixMode , glMatrixMode)
	GETOPENGLFUNC(pglViewport , glViewport)
	GETOPENGLFUNC(pglPushMatrix , glPushMatrix)
	GETOPENGLFUNC(pglPopMatrix , glPopMatrix)
	GETOPENGLFUNC(pglLoadIdentity , glLoadIdentity)
	GETOPENGLFUNC(pglMultMatrixf , glMultMatrixf)
	GETOPENGLFUNC(pglRotatef , glRotatef)
	GETOPENGLFUNC(pglScalef , glScalef)
	GETOPENGLFUNC(pglTranslatef , glTranslatef)

	GETOPENGLFUNC(pglColor4f, glColor4f)
	GETOPENGLFUNC(pglVertexPointer, glVertexPointer)
	GETOPENGLFUNC(pglNormalPointer, glNormalPointer)
	GETOPENGLFUNC(pglTexCoordPointer, glTexCoordPointer)
	GETOPENGLFUNC(pglColorPointer, glColorPointer)
	GETOPENGLFUNC(pglDrawArrays, glDrawArrays)
	GETOPENGLFUNC(pglDrawElements, glDrawElements)
	GETOPENGLFUNC(pglEnableClientState, glEnableClientState)
	GETOPENGLFUNC(pglDisableClientState, glDisableClientState)

	GETOPENGLFUNC(pglShadeModel , glShadeModel)
	GETOPENGLFUNC(pglLightfv, glLightfv)
	GETOPENGLFUNC(pglLightModelfv , glLightModelfv)
	GETOPENGLFUNC(pglMaterialfv , glMaterialfv)

	GETOPENGLFUNC(pglPixelStorei , glPixelStorei)
	GETOPENGLFUNC(pglReadPixels , glReadPixels)

	GETOPENGLFUNC(pglTexEnvi , glTexEnvi)
	GETOPENGLFUNC(pglTexParameteri , glTexParameteri)
	GETOPENGLFUNC(pglTexImage2D , glTexImage2D)

	GETOPENGLFUNC(pglFogf , glFogf)
	GETOPENGLFUNC(pglFogfv , glFogfv)

	GETOPENGLFUNC(pglGenTextures , glGenTextures)
	GETOPENGLFUNC(pglDeleteTextures , glDeleteTextures)
	GETOPENGLFUNC(pglBindTexture , glBindTexture)

	GETOPENGLFUNC(pglCopyTexImage2D , glCopyTexImage2D)
	GETOPENGLFUNC(pglCopyTexSubImage2D , glCopyTexSubImage2D)
	GETOPENGLFUNC(pglGenerateMipmap , glGenerateMipmap)

#undef GETOPENGLFUNC

#endif
	return true;
}

// This has to be done after the context is created so the version number can be obtained
// This is stupid -- even some of the oldest usable OpenGL hardware today supports 1.3-level featureset.
boolean SetupGLFunc13(void)
{
#ifndef STATIC_OPENGL
	pglActiveTexture = GetGLFunc("glActiveTexture");
	pglMultiTexCoord2f = GetGLFunc("glMultiTexCoord2f");
	pglClientActiveTexture = GetGLFunc("glClientActiveTexture");
	pglMultiTexCoord2fv = GetGLFunc("glMultiTexCoord2fv");

	/* 1.5 funcs */
	pglGenBuffers = GetGLFunc("glGenBuffers");
	pglBindBuffer = GetGLFunc("glBindBuffer");
	pglBufferData = GetGLFunc("glBufferData");
	pglDeleteBuffers = GetGLFunc("glDeleteBuffers");
#endif
	return true;
}

// -----------------+
// SetNoTexture     : Disable texture
// -----------------+
static void SetNoTexture(void)
{
	// Set small white texture.
	if (tex_downloaded != NOTEXTURE_NUM)
	{
		if (NOTEXTURE_NUM == 0)
			pglGenTextures(1, &NOTEXTURE_NUM);
		pglBindTexture(GL_TEXTURE_2D, NOTEXTURE_NUM);
		tex_downloaded = NOTEXTURE_NUM;
	}
}

static void GLPerspective(GLfloat fovy, GLfloat aspect)
{
	GLfloat m[4][4] =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f},
		{ 0.0f, 1.0f, 0.0f, 0.0f},
		{ 0.0f, 0.0f, 1.0f,-1.0f},
		{ 0.0f, 0.0f, 0.0f, 0.0f},
	};
	const GLfloat zNear = NEAR_CLIPPING_PLANE;
	const GLfloat zFar = FAR_CLIPPING_PLANE;
	const GLfloat radians = (GLfloat)(fovy / 2.0f * M_PIl / 180.0f);
	const GLfloat sine = sinf(radians);
	const GLfloat deltaZ = zFar - zNear;
	GLfloat cotangent;

	if ((fabsf((float)deltaZ) < 1.0E-36f) || fpclassify(sine) == FP_ZERO || fpclassify(aspect) == FP_ZERO)
	{
		return;
	}
	cotangent = cosf(radians) / sine;

	m[0][0] = cotangent / aspect;
	m[1][1] = cotangent;
	m[2][2] = -(zFar + zNear) / deltaZ;
	m[3][2] = -2.0f * zNear * zFar / deltaZ;

	pglMultMatrixf(&m[0][0]);
}

// -----------------+
// SetModelView     :
// -----------------+
void SetModelView(GLint w, GLint h)
{
//	DBG_Printf("SetModelView(): %dx%d\n", (int)w, (int)h);

	// The screen textures need to be flushed if the width or height change so that they be remade for the correct size
	if (screen_width != w || screen_height != h)
		FlushScreenTextures();

	screen_width = w;
	screen_height = h;

	pglViewport(0, 0, w, h);

	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();

	pglMatrixMode(GL_MODELVIEW);
	pglLoadIdentity();

	GLPerspective(fov, ASPECT_RATIO);
}


// -----------------+
// SetStates        : Set permanent states
// -----------------+
void SetStates(void)
{
#ifdef GL_LIGHT_MODEL_AMBIENT
	GLfloat LightDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
#endif

//	DBG_Printf("SetStates()\n");

	pglEnableClientState(GL_VERTEX_ARRAY); // We always use this one

	pglShadeModel(GL_SMOOTH);      // iterate vertice colors

	pglEnable(GL_TEXTURE_2D);      // two-dimensional texturing
	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	pglAlphaFunc(GL_NOTEQUAL, 0.0f);
	pglEnable(GL_BLEND);           // enable color blending

	pglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	pglEnable(GL_DEPTH_TEST);    // check the depth buffer
	pglDepthMask(GL_TRUE);             // enable writing to depth buffer
	pglClearDepthf(1.0f);
	pglDepthRangef(0.0f, 1.0f);
	pglDepthFunc(GL_LEQUAL);

	// this set CurrentPolyFlags to the acctual configuration
	CurrentPolyFlags = 0xffffffff;
	SetBlend(0);

	tex_downloaded = 0;
	SetNoTexture();

	pglPolygonOffset(-1.0f, -1.0f);

	// Lighting for models
#ifdef GL_LIGHT_MODEL_AMBIENT
	pglLightModelfv(GL_LIGHT_MODEL_AMBIENT, LightDiffuse);
	pglEnable(GL_LIGHT0);
#endif

	// bp : when no t&l :)
	pglLoadIdentity();
	pglScalef(1.0f, 1.0f, -1.0f);
}


// -----------------+
// Flush            : flush OpenGL textures
//                  : Clear list of downloaded mipmaps
// -----------------+
void Flush(void)
{
	//DBG_Printf ("HWR_Flush()\n");

	while (gr_cachehead)
	{
		if (gr_cachehead->downloaded)
			pglDeleteTextures(1, (GLuint *)&gr_cachehead->downloaded);
		gr_cachehead->downloaded = 0;
		gr_cachehead = gr_cachehead->nextmipmap;
	}
	gr_cachetail = gr_cachehead = NULL; //Hurdler: well, gr_cachehead is already NULL

	tex_downloaded = 0;
}


// -----------------+
// isExtAvailable   : Look if an OpenGL extension is available
// Returns          : true if extension available
// -----------------+
INT32 isExtAvailable(const char *extension, const GLubyte *start)
{
	GLubyte         *where, *terminator;

	if (!extension || !start) return 0;
	where = (GLubyte *) strchr(extension, ' ');
	if (where || *extension == '\0')
		return 0;

	for (;;)
	{
		where = (GLubyte *) strstr((const char *) start, extension);
		if (!where)
			break;
		terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return 1;
		start = terminator;
	}
	return 0;
}


// -----------------+
// Init             : Initialise the OpenGL ES interface API
// Returns          :
// -----------------+
EXPORT boolean HWRAPI(Init) (I_Error_t FatalErrorFunction)
{
	(void)FatalErrorFunction;
	DBG_Printf ("HWRAPI Init(): SRB2 %s renderer %s\n", DRIVER_STRING, VERSIONSTRING);
	return LoadGL();
}


// -----------------+
// ClearMipMapCache : Flush OpenGL textures from memory
// -----------------+
EXPORT void HWRAPI(ClearMipMapCache) (void)
{
	// DBG_Printf ("HWR_Flush(exe)\n");
	Flush();
}


// -----------------+
// ReadRect         : Read a rectangle region of the truecolor framebuffer
//                  : store pixels as RGBA8888
// Returns          : RGBA8888 pixel array stored in dst_data
// -----------------+
EXPORT void HWRAPI(ReadRect) (INT32 x, INT32 y, INT32 width, INT32 height,
                                INT32 dst_stride, UINT16 * dst_data)
{
	INT32 i;
	GLubyte*top = (GLvoid*)dst_data, *bottom = top + dst_stride * (height - 1);
	GLubyte *row = malloc(dst_stride);
	// DBG_Printf ("ReadRect()\n");
	if (!row) return;
	pglPixelStorei(GL_PACK_ALIGNMENT, 1);
	pglReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dst_data);
	pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for(i = 0; i < height/2; i++)
	{
		memcpy(row, top, dst_stride);
		memcpy(top, bottom, dst_stride);
		memcpy(bottom, row, dst_stride);
		top += dst_stride;
		bottom -= dst_stride;
	}
	free(row);
}


// -----------------+
// GClipRect        : Defines the 2D hardware clipping window
// -----------------+
EXPORT void HWRAPI(GClipRect) (INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip)
{
	// DBG_Printf ("GClipRect(%d, %d, %d, %d)\n", minx, miny, maxx, maxy);

	pglViewport(minx, screen_height-maxy, maxx-minx, maxy-miny);
	NEAR_CLIPPING_PLANE = nearclip;

	//pglScissor(minx, screen_height-maxy, maxx-minx, maxy-miny);
	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();
	GLPerspective(fov, ASPECT_RATIO);
	pglMatrixMode(GL_MODELVIEW);
}


// -----------------+
// ClearBuffer      : Clear the color/alpha/depth buffer(s)
// -----------------+
EXPORT void HWRAPI(ClearBuffer) (FBOOLEAN ColorMask,
                                    FBOOLEAN DepthMask,
                                    FRGBAFloat * ClearColor)
{
	// DBG_Printf ("ClearBuffer(%d)\n", alpha);
	GLbitfield ClearMask = 0;

	if (ColorMask)
	{
		if (ClearColor)
			pglClearColor(ClearColor->red,
			              ClearColor->green,
			              ClearColor->blue,
			              ClearColor->alpha);
		ClearMask |= GL_COLOR_BUFFER_BIT;
	}
	if (DepthMask)
	{
		pglClearDepthf(1.0f);     //Hurdler: all that are permanen states
		pglDepthRangef(0.0f, 1.0f);
		pglDepthFunc(GL_LEQUAL);
		ClearMask |= GL_DEPTH_BUFFER_BIT;
	}

	SetBlend(DepthMask ? PF_Occlude | CurrentPolyFlags : CurrentPolyFlags&~PF_Occlude);

	pglClear(ClearMask);
	pglEnableClientState(GL_TEXTURE_COORD_ARRAY); // We mostly use this one
}


// -----------------+
// HWRAPI Draw2DLine: Render a 2D line
// -----------------+
EXPORT void HWRAPI(Draw2DLine) (F2DCoord * v1,
                                   F2DCoord * v2,
                                   RGBA_t Color)
{
	// DBG_Printf ("DrawLine() (%f %f %f) %d\n", v1->x, -v1->y, -v1->z, v1->argb);
	GLfloat fcolor[4];
	GLfloat p[12];
	GLfloat dx, dy;
	GLfloat angle;

	// BP: we should reflect the new state in our variable
	//SetBlend(PF_Modulated|PF_NoTexture);

	pglDisable(GL_TEXTURE_2D);

	// This is the preferred, 'modern' way of rendering lines -- creating a polygon.
	if (fabsf(v2->x - v1->x) > FLT_EPSILON)
		angle = (float)atan((v2->y-v1->y)/(v2->x-v1->x));
	else
		angle = (float)N_PI_DEMI;
	dx = (float)sin(angle) / (float)screen_width;
	dy = (float)cos(angle) / (float)screen_height;

	p[0] = v1->x - dx;  p[1] = -(v1->y + dy); p[2] = 1;
	p[3] = v2->x - dx;  p[4] = -(v2->y + dy); p[5] = 1;
	p[6] = v2->x + dx;  p[7] = -(v2->y - dy); p[8] = 1;
	p[9] = v1->x + dx;  p[10] = -(v1->y - dy); p[11] = 1;

	fcolor[0] = (Color.s.red/255.0f);
	fcolor[1] = (Color.s.green/255.0f);
	fcolor[2] = (Color.s.blue/255.0f);
	fcolor[3] = (Color.s.alpha/255.0f);

	pglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	pglColor4f(fcolor[0], fcolor[1], fcolor[2], fcolor[3]);
	pglVertexPointer(3, GL_FLOAT, 0, p);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	pglEnable(GL_TEXTURE_2D);
}

static void Clamp2D(GLenum pname)
{
	pglTexParameteri(GL_TEXTURE_2D, pname, GL_CLAMP_TO_EDGE);
}


// -----------------+
// SetBlend         : Set render mode
// -----------------+
// PF_Masked - we could use an ALPHA_TEST of GL_EQUAL, and alpha ref of 0,
//             is it faster when pixels are discarded ?
EXPORT void HWRAPI(SetBlend) (FBITFIELD PolyFlags)
{
	FBITFIELD Xor;
	Xor = CurrentPolyFlags^PolyFlags;
	if (Xor & (PF_Blending|PF_RemoveYWrap|PF_ForceWrapX|PF_ForceWrapY|PF_Occlude|PF_NoTexture|PF_Modulated|PF_NoDepthTest|PF_Decal|PF_Invisible|PF_NoAlphaTest))
	{
		if (Xor&(PF_Blending)) // if blending mode must be changed
		{
			switch (PolyFlags & PF_Blending) {
				case PF_Translucent & PF_Blending:
					pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // alpha = level of transparency
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Masked & PF_Blending:
					// Hurdler: does that mean lighting is only made by alpha src?
					// it sounds ok, but not for polygonsmooth
					pglBlendFunc(GL_SRC_ALPHA, GL_ZERO);                // 0 alpha = holes in texture
					pglAlphaFunc(GL_GREATER, 0.5f);
					break;
				case PF_Additive & PF_Blending:
					pglBlendFunc(GL_SRC_ALPHA, GL_ONE);                 // src * alpha + dest
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Environment & PF_Blending:
					pglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Substractive & PF_Blending:
					// good for shadow
					// not really but what else ?
					pglBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Fog & PF_Fog:
					// Sryder: Fog
					// multiplies input colour by input alpha, and destination colour by input colour, then adds them
					pglBlendFunc(GL_SRC_ALPHA, GL_SRC_COLOR);
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				default : // must be 0, otherwise it's an error
					// No blending
					pglBlendFunc(GL_ONE, GL_ZERO);   // the same as no blending
					pglAlphaFunc(GL_GREATER, 0.5f);
					break;
			}
		}
		if (Xor & PF_NoAlphaTest)
		{
			if (PolyFlags & PF_NoAlphaTest)
				pglDisable(GL_ALPHA_TEST);
			else
				pglEnable(GL_ALPHA_TEST);      // discard 0 alpha pixels (holes in texture)
		}

		if (Xor & PF_Decal)
		{
			if (PolyFlags & PF_Decal)
				pglEnable(GL_POLYGON_OFFSET_FILL);
			else
				pglDisable(GL_POLYGON_OFFSET_FILL);
		}

		if (Xor&PF_NoDepthTest)
		{
			if (PolyFlags & PF_NoDepthTest)
				pglDepthFunc(GL_ALWAYS); //pglDisable(GL_DEPTH_TEST);
			else
				pglDepthFunc(GL_LEQUAL); //pglEnable(GL_DEPTH_TEST);
		}

		if (Xor&PF_RemoveYWrap)
		{
			if (PolyFlags & PF_RemoveYWrap)
				Clamp2D(GL_TEXTURE_WRAP_T);
		}

		if (Xor&PF_ForceWrapX)
		{
			if (PolyFlags & PF_ForceWrapX)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		}

		if (Xor&PF_ForceWrapY)
		{
			if (PolyFlags & PF_ForceWrapY)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		if (Xor&PF_Modulated)
		{
			if (PolyFlags & PF_Modulated)
			{   // mix texture colour with Surface->FlatColor
				pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
			else
			{   // colour from texture is unchanged before blending
				pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
		}

		if (Xor & PF_Occlude) // depth test but (no) depth write
		{
			if (PolyFlags&PF_Occlude)
			{
				pglDepthMask(1);
			}
			else
				pglDepthMask(0);
		}
		////Hurdler: not used if we don't define POLYSKY
		if (Xor & PF_Invisible)
		{
			if (PolyFlags&PF_Invisible)
				pglBlendFunc(GL_ZERO, GL_ONE);         // transparent blending
			else
			{   // big hack: (TODO: manage that better)
				// we test only for PF_Masked because PF_Invisible is only used
				// (for now) with it (yeah, that's crappy, sorry)
				if ((PolyFlags&PF_Blending)==PF_Masked)
					pglBlendFunc(GL_SRC_ALPHA, GL_ZERO);
			}
		}
		if (PolyFlags & PF_NoTexture)
		{
			SetNoTexture();
		}
	}
	CurrentPolyFlags = PolyFlags;
}


// -----------------+
// SetTexture       : The mipmap becomes the current texture source
// -----------------+
EXPORT void HWRAPI(SetTexture) (FTextureInfo *pTexInfo)
{
	if (!pTexInfo)
	{
		SetNoTexture();
		return;
	}
	else if (pTexInfo->downloaded)
	{
		if (pTexInfo->downloaded != tex_downloaded)
		{
			pglBindTexture(GL_TEXTURE_2D, pTexInfo->downloaded);
			tex_downloaded = pTexInfo->downloaded;
		}
	}
	else
	{
		// Download a mipmap
		static RGBA_t   tex[2048*2048];
		const GLvoid   *ptex = tex;
		INT32             w, h;
		GLuint texnum = 0;

		pglGenTextures(1, &texnum);
		//DBG_Printf ("DownloadMipmap %d %x\n",(INT32)texnum,pTexInfo->grInfo.data);

		w = pTexInfo->width;
		h = pTexInfo->height;

		if ((pTexInfo->grInfo.format == GR_TEXFMT_P_8) ||
			(pTexInfo->grInfo.format == GR_TEXFMT_AP_88))
		{
			const GLubyte *pImgData = (const GLubyte *)pTexInfo->grInfo.data;
			INT32 i, j;

			for (j = 0; j < h; j++)
			{
				for (i = 0; i < w; i++)
				{
					if ((*pImgData == HWR_PATCHES_CHROMAKEY_COLORINDEX) &&
					    (pTexInfo->flags & TF_CHROMAKEYED))
					{
						tex[w*j+i].s.red   = 0;
						tex[w*j+i].s.green = 0;
						tex[w*j+i].s.blue  = 0;
						tex[w*j+i].s.alpha = 0;
						pTexInfo->flags |= TF_TRANSPARENT; // there is a hole in it
					}
					else
					{
						tex[w*j+i].s.red   = myPaletteData[*pImgData].s.red;
						tex[w*j+i].s.green = myPaletteData[*pImgData].s.green;
						tex[w*j+i].s.blue  = myPaletteData[*pImgData].s.blue;
						tex[w*j+i].s.alpha = myPaletteData[*pImgData].s.alpha;
					}

					pImgData++;

					if (pTexInfo->grInfo.format == GR_TEXFMT_AP_88)
					{
						if (!(pTexInfo->flags & TF_CHROMAKEYED))
							tex[w*j+i].s.alpha = *pImgData;
						pImgData++;
					}
				}
			}
		}
		else if (pTexInfo->grInfo.format == GR_RGBA)
		{
			// corona test : passed as ARGB 8888, which is not in glide formats
			// Hurdler: not used for coronas anymore, just for dynamic lighting
			ptex = pTexInfo->grInfo.data;
		}
		else if (pTexInfo->grInfo.format == GR_TEXFMT_ALPHA_INTENSITY_88)
		{
			const GLubyte *pImgData = (const GLubyte *)pTexInfo->grInfo.data;
			INT32 i, j;

			for (j = 0; j < h; j++)
			{
				for (i = 0; i < w; i++)
				{
					tex[w*j+i].s.red   = *pImgData;
					tex[w*j+i].s.green = *pImgData;
					tex[w*j+i].s.blue  = *pImgData;
					pImgData++;
					tex[w*j+i].s.alpha = *pImgData;
					pImgData++;
				}
			}
		}
		else if (pTexInfo->grInfo.format == GR_TEXFMT_ALPHA_8) // Used for fade masks
		{
			const GLubyte *pImgData = (const GLubyte *)pTexInfo->grInfo.data;
			INT32 i, j;

			for (j = 0; j < h; j++)
			{
				for (i = 0; i < w; i++)
				{
					tex[w*j+i].s.red   = 255; // 255 because the fade mask is modulated with the screen texture, so alpha affects it while the colours don't
					tex[w*j+i].s.green = 255;
					tex[w*j+i].s.blue  = 255;
					tex[w*j+i].s.alpha = *pImgData;
					pImgData++;
				}
			}
		}
		else
			DBG_Printf ("SetTexture(bad format) %ld\n", pTexInfo->grInfo.format);

		// the texture number was already generated by pglGenTextures
		pglBindTexture(GL_TEXTURE_2D, texnum);
		pTexInfo->downloaded = texnum;
		tex_downloaded = texnum;

		// disable texture filtering on any texture that has holes so there's no dumb borders or blending issues
		if (pTexInfo->flags & TF_TRANSPARENT)
		{
			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
		else
		{
			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
		}

		pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);

		if (MipMap)
			pglGenerateMipmap(GL_TEXTURE_2D);

		if (pTexInfo->flags & TF_WRAPX)
			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		else
			Clamp2D(GL_TEXTURE_WRAP_S);

		if (pTexInfo->flags & TF_WRAPY)
			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		else
			Clamp2D(GL_TEXTURE_WRAP_T);

		if (maximumAnisotropy)
			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropic_filter);

		pTexInfo->nextmipmap = NULL;
		if (gr_cachetail)
		{ // insertion at the tail
			gr_cachetail->nextmipmap = pTexInfo;
			gr_cachetail = pTexInfo;
		}
		else // initialization of the linked list
			gr_cachetail = gr_cachehead =  pTexInfo;
	}
}


// -----------------+
// DrawPolygon      : Render a polygon, set the texture, set render mode
// -----------------+
EXPORT void HWRAPI(DrawPolygon) (FSurfaceInfo  *pSurf,
                                    //FTextureInfo  *pTexInfo,
                                    FOutVector    *pOutVerts,
                                    FUINT         iNumPts,
                                    FBITFIELD     PolyFlags)
{
	GLfloat fcolor[4];
	FUINT i;
	FUINT j;

	if (PolyFlags & PF_Corona)
		PolyFlags &= ~(PF_NoDepthTest|PF_Corona);

	SetBlend(PolyFlags);    //TODO: inline (#pragma..)

	if (pSurf)
	{
		fcolor[0] = (pSurf->FlatColor.s.red/255.0f);
		fcolor[1] = (pSurf->FlatColor.s.green/255.0f);
		fcolor[2] = (pSurf->FlatColor.s.blue/255.0f);
		fcolor[3] = (pSurf->FlatColor.s.alpha/255.0f);
	}
	else
		fcolor[0] = fcolor[1] = fcolor[2] = fcolor[3] = 1.0f;

	// If Modulated, mix the surface colour to the texture
	if ((CurrentPolyFlags & PF_Modulated) && pSurf)
		pglColor4f(fcolor[0], fcolor[1], fcolor[2], fcolor[3]);

	pglVertexPointer(3, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].x);
	pglTexCoordPointer(2, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].sow);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, iNumPts);

	if (PolyFlags & PF_RemoveYWrap)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (PolyFlags & PF_ForceWrapX)
		Clamp2D(GL_TEXTURE_WRAP_S);

	if (PolyFlags & PF_ForceWrapY)
		Clamp2D(GL_TEXTURE_WRAP_T);
}

typedef struct vbo_vertex_s
{
	float x, y, z;
	float u, v;
	unsigned char r, g, b, a;
} vbo_vertex_t;

typedef struct
{
	int mode;
	int vertexcount;
	int vertexindex;
	int use_texture;
} GLSkyLoopDef;

typedef struct
{
	unsigned int id;
	int rows, columns;
	int loopcount;
	GLSkyLoopDef *loops;
	vbo_vertex_t *data;
} GLSkyVBO;

static const boolean gl_ext_arb_vertex_buffer_object = true;

#define NULL_VBO_VERTEX ((vbo_vertex_t*)NULL)
#define sky_vbo_x (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->x : &vbo->data[0].x)
#define sky_vbo_u (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->u : &vbo->data[0].u)
#define sky_vbo_r (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->r : &vbo->data[0].r)

// The texture offset to be applied to the texture coordinates in SkyVertex().
static int rows, columns;
static signed char yflip;
static int texw, texh;
static boolean foglayer;
static float delta = 0.0f;

static int gl_sky_detail = 16;

static INT32 lasttex = -1;

#define MAP_COEFF 128.0f

static void SkyVertex(vbo_vertex_t *vbo, int r, int c)
{
	const float radians = (float)(M_PIl / 180.0f);
	const float scale = 10000.0f;
	const float maxSideAngle = 60.0f;

	float topAngle = (c / (float)columns * 360.0f);
	float sideAngle = (maxSideAngle * (rows - r) / rows);
	float height = (float)(sin(sideAngle * radians));
	float realRadius = (float)(scale * cos(sideAngle * radians));
	float x = (float)(realRadius * cos(topAngle * radians));
	float y = (!yflip) ? scale * height : -scale * height;
	float z = (float)(realRadius * sin(topAngle * radians));
	float timesRepeat = (4 * (256.0f / texw));
	if (fpclassify(timesRepeat) == FP_ZERO)
		timesRepeat = 1.0f;

	if (!foglayer)
	{
		vbo->r = 255;
		vbo->g = 255;
		vbo->b = 255;
		vbo->a = (r == 0 ? 0 : 255);

		// And the texture coordinates.
		vbo->u = (-timesRepeat * c / (float)columns);
		if (!yflip)	// Flipped Y is for the lower hemisphere.
			vbo->v = (r / (float)rows) + 0.5f;
		else
			vbo->v = 1.0f + ((rows - r) / (float)rows) + 0.5f;
	}

	if (r != 4)
	{
		y += 300.0f;
	}

	// And finally the vertex.
	vbo->x = x;
	vbo->y = y + delta;
	vbo->z = z;
}

static GLSkyVBO sky_vbo;

static void gld_BuildSky(int row_count, int col_count)
{
	int c, r;
	vbo_vertex_t *vertex_p;
	int vertex_count = 2 * row_count * (col_count * 2 + 2) + col_count * 2;

	GLSkyVBO *vbo = &sky_vbo;

	if ((vbo->columns != col_count) || (vbo->rows != row_count))
	{
		free(vbo->loops);
		free(vbo->data);
		memset(vbo, 0, sizeof(&vbo));
	}

	if (!vbo->data)
	{
		memset(vbo, 0, sizeof(&vbo));
		vbo->loops = malloc((row_count * 2 + 2) * sizeof(vbo->loops[0]));
		// create vertex array
		vbo->data = malloc(vertex_count * sizeof(vbo->data[0]));
	}

	vbo->columns = col_count;
	vbo->rows = row_count;

	vertex_p = &vbo->data[0];
	vbo->loopcount = 0;

	for (yflip = 0; yflip < 2; yflip++)
	{
		vbo->loops[vbo->loopcount].mode = GL_TRIANGLE_FAN;
		vbo->loops[vbo->loopcount].vertexindex = vertex_p - &vbo->data[0];
		vbo->loops[vbo->loopcount].vertexcount = col_count;
		vbo->loops[vbo->loopcount].use_texture = false;
		vbo->loopcount++;

		delta = 0.0f;
		foglayer = true;
		for (c = 0; c < col_count; c++)
		{
			SkyVertex(vertex_p, 1, c);
			vertex_p->r = 255;
			vertex_p->g = 255;
			vertex_p->b = 255;
			vertex_p->a = 255;
			vertex_p++;
		}
		foglayer = false;

		delta = (yflip ? 5.0f : -5.0f) / MAP_COEFF;

		for (r = 0; r < row_count; r++)
		{
			vbo->loops[vbo->loopcount].mode = GL_TRIANGLE_STRIP;
			vbo->loops[vbo->loopcount].vertexindex = vertex_p - &vbo->data[0];
			vbo->loops[vbo->loopcount].vertexcount = 2 * col_count + 2;
			vbo->loops[vbo->loopcount].use_texture = true;
			vbo->loopcount++;

			for (c = 0; c <= col_count; c++)
			{
				SkyVertex(vertex_p++, r + (yflip ? 1 : 0), (c ? c : 0));
				SkyVertex(vertex_p++, r + (yflip ? 0 : 1), (c ? c : 0));
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderDome(INT32 skytexture)
{
	int i, j;
	int vbosize;
	GLSkyVBO *vbo = &sky_vbo;

	rows = 4;
	columns = 4 * gl_sky_detail;

	vbosize = 2 * rows * (columns * 2 + 2) + columns * 2;

	// Build the sky dome! Yes!
	if (lasttex != skytexture)
	{
		// delete VBO when already exists
		if (gl_ext_arb_vertex_buffer_object)
		{
			if (vbo->id)
				pglDeleteBuffers(1, &vbo->id);
		}

		lasttex = skytexture;
		gld_BuildSky(rows, columns);

		if (gl_ext_arb_vertex_buffer_object)
		{
			// generate a new VBO and get the associated ID
			pglGenBuffers(1, &vbo->id);

			// bind VBO in order to use
			pglBindBuffer(GL_ARRAY_BUFFER, vbo->id);

			// upload data to VBO
			pglBufferData(GL_ARRAY_BUFFER, vbosize * sizeof(vbo->data[0]), vbo->data, GL_STATIC_DRAW);
		}
	}

	// bind VBO in order to use
	if (gl_ext_arb_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, vbo->id);

	// activate and specify pointers to arrays
	pglVertexPointer(3, GL_FLOAT, sizeof(vbo->data[0]), sky_vbo_x);
	pglTexCoordPointer(2, GL_FLOAT, sizeof(vbo->data[0]), sky_vbo_u);
	pglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vbo->data[0]), sky_vbo_r);

	// activate color arrays
	pglEnableClientState(GL_COLOR_ARRAY);

	// set transforms
	pglScalef(1.0f, (float)texh / 230.0f, 1.0f);
	pglRotatef(270.0f, 0.0f, 1.0f, 0.0f);

	for (j = 0; j < 2; j++)
	{
		for (i = 0; i < vbo->loopcount; i++)
		{
			GLSkyLoopDef *loop = &vbo->loops[i];

			if (j == 0 ? loop->use_texture : !loop->use_texture)
				continue;

			pglDrawArrays(loop->mode, loop->vertexindex, loop->vertexcount);
		}
	}

	pglScalef(1.0f, 1.0f, 1.0f);
	pglColor4f(white[0], white[1], white[2], white[3]);

	// bind with 0, so, switch back to normal pointer operation
	if (gl_ext_arb_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, 0);

	// deactivate color array
	pglDisableClientState(GL_COLOR_ARRAY);
}

EXPORT void HWRAPI(RenderSkyDome) (INT32 tex, INT32 texture_width, INT32 texture_height, FTransform transform)
{
	SetBlend(PF_Translucent|PF_NoDepthTest|PF_Modulated);
	SetTransform(&transform);
	texw = texture_width;
	texh = texture_height;
	RenderDome(tex);
	SetBlend(0);
}

// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI(SetSpecialState) (hwdspecialstate_t IdState, INT32 Value)
{
	switch (IdState)
	{
		case HWD_SET_MODEL_LIGHTING:
			model_lighting = Value;
			break;

		case HWD_SET_FOG_COLOR:
		{
			GLfloat fogcolor[4];
			fogcolor[0] = ((Value>>16)&0xff) / 255.0f;
			fogcolor[1] = ((Value>>8)&0xff) / 255.0f;
			fogcolor[2] = ((Value)&0xff) / 255.0f;
			fogcolor[3] = 0.0f; // wut?? 0x0???
			pglFogfv(GL_FOG_COLOR, fogcolor);
			break;
		}

		case HWD_SET_FOG_DENSITY:
			pglFogf(GL_FOG_DENSITY, Value*1200/(500*1000000.0f));
			break;

		case HWD_SET_FOG_MODE:
			if (Value)
			{
				pglEnable(GL_FOG);
				// experimental code
				/*
				switch (Value)
				{
					case 1:
						glFogi(GL_FOG_MODE, GL_LINEAR);
						pglFogf(GL_FOG_START, -1000.0f);
						pglFogf(GL_FOG_END, 2000.0f);
						break;
					case 2:
						glFogi(GL_FOG_MODE, GL_EXP);
						break;
					case 3:
						glFogi(GL_FOG_MODE, GL_EXP2);
						break;
				}
				*/
			}
			else
				pglDisable(GL_FOG);
			break;

		case HWD_SET_TEXTUREFILTERMODE:
			switch (Value)
			{
				case HWD_SET_TEXTUREFILTER_TRILINEAR:
					min_filter = GL_LINEAR_MIPMAP_LINEAR;
					mag_filter = GL_LINEAR;
					MipMap = GL_TRUE;
					break;
				case HWD_SET_TEXTUREFILTER_BILINEAR:
					min_filter = mag_filter = GL_LINEAR;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_POINTSAMPLED:
					min_filter = mag_filter = GL_NEAREST;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED1:
					min_filter = GL_NEAREST;
					mag_filter = GL_LINEAR;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED2:
					min_filter = GL_LINEAR;
					mag_filter = GL_NEAREST;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED3:
					min_filter = GL_LINEAR_MIPMAP_LINEAR;
					mag_filter = GL_NEAREST;
					MipMap = GL_TRUE;
					break;
				default:
					mag_filter = GL_LINEAR;
					min_filter = GL_NEAREST;
			}
			Flush(); //??? if we want to change filter mode by texture, remove this
			break;

		case HWD_SET_TEXTUREANISOTROPICMODE:
			anisotropic_filter = min(Value,maximumAnisotropy);
			if (maximumAnisotropy)
				Flush(); //??? if we want to change filter mode by texture, remove this
			break;

		default:
			break;
	}
}

static float *vertBuffer = NULL;
static float *normBuffer = NULL;
static size_t lerpBufferSize = 0;
static short *vertTinyBuffer = NULL;
static char *normTinyBuffer = NULL;
static size_t lerpTinyBufferSize = 0;

// Static temporary buffer for doing frame interpolation
// 'size' is the vertex size
static void AllocLerpBuffer(size_t size)
{
	if (lerpBufferSize >= size)
		return;

	if (vertBuffer != NULL)
		free(vertBuffer);

	if (normBuffer != NULL)
		free(normBuffer);

	lerpBufferSize = size;
	vertBuffer = malloc(lerpBufferSize);
	normBuffer = malloc(lerpBufferSize);
}

// Static temporary buffer for doing frame interpolation
// 'size' is the vertex size
static void AllocLerpTinyBuffer(size_t size)
{
	if (lerpTinyBufferSize >= size)
		return;

	if (vertTinyBuffer != NULL)
		free(vertTinyBuffer);

	if (normTinyBuffer != NULL)
		free(normTinyBuffer);

	lerpTinyBufferSize = size;
	vertTinyBuffer = malloc(lerpTinyBufferSize);
	normTinyBuffer = malloc(lerpTinyBufferSize / 2);
}

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif

static void CreateModelVBO(mesh_t *mesh, mdlframe_t *frame)
{
	int bufferSize = sizeof(vbo64_t)*mesh->numTriangles * 3;
	vbo64_t *buffer = (vbo64_t*)malloc(bufferSize);
	vbo64_t *bufPtr = buffer;

	float *vertPtr = frame->vertices;
	float *normPtr = frame->normals;
	float *tanPtr = frame->tangents;
	float *uvPtr = mesh->uvs;
	float *lightPtr = mesh->lightuvs;
	char *colorPtr = frame->colors;

	int i;
	for (i = 0; i < mesh->numTriangles * 3; i++)
	{
		bufPtr->x = *vertPtr++;
		bufPtr->y = *vertPtr++;
		bufPtr->z = *vertPtr++;

		bufPtr->nx = *normPtr++;
		bufPtr->ny = *normPtr++;
		bufPtr->nz = *normPtr++;

		bufPtr->s0 = *uvPtr++;
		bufPtr->t0 = *uvPtr++;

		if (tanPtr != NULL)
		{
			bufPtr->tan0 = *tanPtr++;
			bufPtr->tan1 = *tanPtr++;
			bufPtr->tan2 = *tanPtr++;
		}

		if (lightPtr != NULL)
		{
			bufPtr->s1 = *lightPtr++;
			bufPtr->t1 = *lightPtr++;
		}

		if (colorPtr)
		{
			bufPtr->r = *colorPtr++;
			bufPtr->g = *colorPtr++;
			bufPtr->b = *colorPtr++;
			bufPtr->a = *colorPtr++;
		}
		else
		{
			bufPtr->r = 255;
			bufPtr->g = 255;
			bufPtr->b = 255;
			bufPtr->a = 255;
		}

		bufPtr++;
	}

	pglGenBuffers(1, &frame->vboID);
	pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
	pglBufferData(GL_ARRAY_BUFFER, bufferSize, buffer, GL_STATIC_DRAW);
	free(buffer);

	// Don't leave the array buffer bound to the model,
	// since this is called mid-frame
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void CreateModelVBOTiny(mesh_t *mesh, tinyframe_t *frame)
{
	int bufferSize = sizeof(vbotiny_t)*mesh->numTriangles * 3;
	vbotiny_t *buffer = (vbotiny_t*)malloc(bufferSize);
	vbotiny_t *bufPtr = buffer;

	short *vertPtr = frame->vertices;
	char *normPtr = frame->normals;
	float *uvPtr = mesh->uvs;
	char *tanPtr = frame->tangents;

	int i;
	for (i = 0; i < mesh->numVertices; i++)
	{
		bufPtr->x = *vertPtr++;
		bufPtr->y = *vertPtr++;
		bufPtr->z = *vertPtr++;

		bufPtr->nx = *normPtr++;
		bufPtr->ny = *normPtr++;
		bufPtr->nz = *normPtr++;

		bufPtr->s0 = *uvPtr++;
		bufPtr->t0 = *uvPtr++;

		if (tanPtr)
		{
			bufPtr->tanx = *tanPtr++;
			bufPtr->tany = *tanPtr++;
			bufPtr->tanz = *tanPtr++;
		}

		bufPtr++;
	}

	pglGenBuffers(1, &frame->vboID);
	pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
	pglBufferData(GL_ARRAY_BUFFER, bufferSize, buffer, GL_STATIC_DRAW);
	free(buffer);

	// Don't leave the array buffer bound to the model,
	// since this is called mid-frame
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

EXPORT void HWRAPI(CreateModelVBOs) (model_t *model)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (mesh->frames)
		{
			int j;
			for (j = 0; j < model->meshes[i].numFrames; j++)
			{
				mdlframe_t *frame = &mesh->frames[j];
				if (frame->vboID)
					pglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
				CreateModelVBO(mesh, frame);
			}
		}
		else if (mesh->tinyframes)
		{
			int j;
			for (j = 0; j < model->meshes[i].numFrames; j++)
			{
				tinyframe_t *frame = &mesh->tinyframes[j];
				if (frame->vboID)
					pglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
				CreateModelVBOTiny(mesh, frame);
			}
		}
	}
}

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

static void DrawModelEx(model_t *model, INT32 frameIndex, INT32 duration, INT32 tics, INT32 nextFrameIndex, FTransform *pos, float scale, UINT8 flipped, UINT8 *color)
{
	GLfloat fcolor[4];
	GLfloat ambient[4];
	GLfloat diffuse[4];

	float pol = 0.0f;
	float scalex, scaley, scalez;

	boolean useTinyFrames;

	int i;

	// Because Otherwise, scaling the screen negatively vertically breaks the lighting
	GLfloat LightPos[] = {0.0f, 1.0f, 0.0f, 0.0f};

	// Affect input model scaling
	scale *= 0.5f;
	scalex = scale;
	scaley = scale;
	scalez = scale;

	if (duration != 0 && duration != -1 && tics != -1) // don't interpolate if instantaneous or infinite in length
	{
		UINT32 newtime = (duration - tics); // + 1;

		pol = (newtime)/(float)duration;

		if (pol > 1.0f)
			pol = 1.0f;

		if (pol < 0.0f)
			pol = 0.0f;
	}

	if (color)
	{
		fcolor[0] = (color[0]/255.0f);
		fcolor[1] = (color[1]/255.0f);
		fcolor[2] = (color[2]/255.0f);
		fcolor[3] = (color[3]/255.0f);

		ambient[0] = fcolor[0];
		ambient[1] = fcolor[1];
		ambient[2] = fcolor[2];
		ambient[3] = fcolor[3];
		diffuse[0] = fcolor[0];
		diffuse[1] = fcolor[1];
		diffuse[2] = fcolor[2];
		diffuse[3] = fcolor[3];

		if (ambient[0] > 0.75f)
			ambient[0] = 0.75f;
		if (ambient[1] > 0.75f)
			ambient[1] = 0.75f;
		if (ambient[2] > 0.75f)
			ambient[2] = 0.75f;
	}

	pglEnable(GL_CULL_FACE);
	pglEnable(GL_NORMALIZE);

#ifdef USE_FTRANSFORM_MIRROR
	// flipped is if the object is flipped
	// pos->flip is if the screen is flipped vertically
	// pos->mirror is if the screen is flipped horizontally
	// XOR all the flips together to figure out what culling to use!
	{
		boolean reversecull = (flipped ^ pos->flip ^ pos->mirror);
		if (reversecull)
			pglCullFace(GL_FRONT);
		else
			pglCullFace(GL_BACK);
	}
#else
	// pos->flip is if the screen is flipped too
	if (flipped != pos->flip) // If either are active, but not both, invert the model's culling
	{
		pglCullFace(GL_FRONT);
	}
	else
	{
		pglCullFace(GL_BACK);
	}
#endif

	if (model_lighting)
	{
		pglLightfv(GL_LIGHT0, GL_POSITION, LightPos);
		pglShadeModel(GL_SMOOTH);
	}

	if (color)
	{
#ifdef GL_LIGHT_MODEL_AMBIENT
		if (model_lighting)
		{
			pglEnable(GL_LIGHTING);
			pglMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
			pglMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
		}
		else
#endif
			pglColor4f(fcolor[0], fcolor[1], fcolor[2], fcolor[3]);

		if (color[3] < 255)
			SetBlend(PF_Translucent|PF_Modulated|PF_Clip);
		else
			SetBlend(PF_Masked|PF_Modulated|PF_Occlude|PF_Clip);
	}

	pglPushMatrix(); // should be the same as glLoadIdentity
	//Hurdler: now it seems to work
	pglTranslatef(pos->x, pos->z, pos->y);
	if (flipped)
		scaley = -scaley;

#ifdef USE_FTRANSFORM_ANGLEZ
	pglRotatef(pos->anglez, 0.0f, 0.0f, -1.0f); // rotate by slope from Kart
#endif
	pglRotatef(pos->angley, 0.0f, -1.0f, 0.0f);
	pglRotatef(pos->anglex, 1.0f, 0.0f, 0.0f);

	if (pos->roll)
	{
		float roll = (1.0f * pos->rollflip);
		pglTranslatef(pos->centerx, pos->centery, 0);
		if (pos->rotaxis == 2) // Z
			pglRotatef(pos->rollangle, 0.0f, 0.0f, roll);
		else if (pos->rotaxis == 1) // Y
			pglRotatef(pos->rollangle, 0.0f, roll, 0.0f);
		else // X
			pglRotatef(pos->rollangle, roll, 0.0f, 0.0f);
		pglTranslatef(-pos->centerx, -pos->centery, 0);
	}

	pglScalef(scalex, scaley, scalez);

	useTinyFrames = model->meshes[0].tinyframes != NULL;

	if (useTinyFrames)
		pglScalef(1 / 64.0f, 1 / 64.0f, 1 / 64.0f);

	pglEnableClientState(GL_NORMAL_ARRAY);

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (useTinyFrames)
		{
			tinyframe_t *frame = &mesh->tinyframes[frameIndex % mesh->numFrames];
			tinyframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
				pglVertexPointer(3, GL_SHORT, sizeof(vbotiny_t), BUFFER_OFFSET(0));
				pglNormalPointer(GL_BYTE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short)*3));
				pglTexCoordPointer(2, GL_FLOAT, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short) * 3 + sizeof(char) * 6));

				pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
				pglBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			else
			{
				short *vertPtr;
				char *normPtr;
				int j;

				// Dangit, I soooo want to do this in a GLSL shader...
				AllocLerpTinyBuffer(mesh->numVertices * sizeof(short) * 3);
				vertPtr = vertTinyBuffer;
				normPtr = normTinyBuffer;
				j = 0;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = (short)(frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j])));
					*normPtr++ = (char)(frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j])));
				}

				pglVertexPointer(3, GL_SHORT, 0, vertTinyBuffer);
				pglNormalPointer(GL_BYTE, 0, normTinyBuffer);
				pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
				pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
			}
		}
		else
		{
			mdlframe_t *frame = &mesh->frames[frameIndex % mesh->numFrames];
			mdlframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				// Zoom! Take advantage of just shoving the entire arrays to the GPU.
/*				pglVertexPointer(3, GL_FLOAT, 0, frame->vertices);
				pglNormalPointer(GL_FLOAT, 0, frame->normals);
				pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
				pglDrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);*/

				pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
				pglVertexPointer(3, GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(0));
				pglNormalPointer(GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 3));
				pglTexCoordPointer(2, GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 6));

				pglDrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);
				// No tinyframes, no mesh indices
				//pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
				pglBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			else
			{
				float *vertPtr;
				float *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				AllocLerpBuffer(mesh->numVertices * sizeof(float) * 3);
				vertPtr = vertBuffer;
				normPtr = normBuffer;
				//int j = 0;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j]));
					*normPtr++ = frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j]));
				}

				pglVertexPointer(3, GL_FLOAT, 0, vertBuffer);
				pglNormalPointer(GL_FLOAT, 0, normBuffer);
				pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
				pglDrawArrays(GL_TRIANGLES, 0, mesh->numVertices);
			}
		}
	}

	pglDisableClientState(GL_NORMAL_ARRAY);

	pglPopMatrix(); // should be the same as glLoadIdentity
	if (color)
		pglDisable(GL_LIGHTING);
	pglShadeModel(GL_FLAT);
	pglDisable(GL_CULL_FACE);
	pglDisable(GL_NORMALIZE);
}

// -----------------+
// HWRAPI DrawMD2   : Draw an MD2 model with glcommands
// -----------------+
EXPORT void HWRAPI(DrawModel) (model_t *model, INT32 frameIndex, INT32 duration, INT32 tics, INT32 nextFrameIndex, FTransform *pos, float scale, UINT8 flipped, UINT8 *color)
{
	DrawModelEx(model, frameIndex, duration, tics,  nextFrameIndex, pos, scale, flipped, color);
}

// -----------------+
// SetTransform     :
// -----------------+
EXPORT void HWRAPI(SetTransform) (FTransform *stransform)
{
	static boolean special_splitscreen;
	float used_fov;
	pglLoadIdentity();
	if (stransform)
	{
		used_fov = stransform->fovxangle;
#ifdef USE_FTRANSFORM_MIRROR
		// mirroring from Kart
		if (stransform->mirror)
			pglScalef(-stransform->scalex, stransform->scaley, -stransform->scalez);
		else
#endif
		if (stransform->flip)
			pglScalef(stransform->scalex, -stransform->scaley, -stransform->scalez);
		else
			pglScalef(stransform->scalex, stransform->scaley, -stransform->scalez);

		if (stransform->roll)
			pglRotatef(stransform->rollangle, 0.0f, 0.0f, 1.0f);
		pglRotatef(stransform->anglex       , 1.0f, 0.0f, 0.0f);
		pglRotatef(stransform->angley+270.0f, 0.0f, 1.0f, 0.0f);
		pglTranslatef(-stransform->x, -stransform->z, -stransform->y);

		special_splitscreen = stransform->splitscreen;
	}
	else
	{
		used_fov = fov;
		pglScalef(1.0f, 1.0f, -1.0f);
	}

	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();

	if (special_splitscreen)
	{
		used_fov = atan(tan(used_fov*M_PI/360)*0.8)*360/M_PI;
		GLPerspective(used_fov, 2*ASPECT_RATIO);
	}
	else
		GLPerspective(used_fov, ASPECT_RATIO);

	pglMatrixMode(GL_MODELVIEW);
}

EXPORT INT32  HWRAPI(GetTextureUsed) (void)
{
	FTextureInfo *tmp = gr_cachehead;
	INT32 res = 0;

	while (tmp)
	{
		// Figure out the correct bytes-per-pixel for this texture
		// I don't know which one the game actually _uses_ but this
		// follows format2bpp in hw_cache.c
		int bpp = 1;
		int format = tmp->grInfo.format;
		if (format == GR_RGBA)
			bpp = 4;
		else if (format == GR_TEXFMT_RGB_565
			|| format == GR_TEXFMT_ARGB_1555
			|| format == GR_TEXFMT_ARGB_4444
			|| format == GR_TEXFMT_ALPHA_INTENSITY_88
			|| format == GR_TEXFMT_AP_88)
			bpp = 2;

		// Add it up!
		res += tmp->height*tmp->width*bpp;
		tmp = tmp->nextmipmap;
	}

	return res;
}

EXPORT INT32  HWRAPI(GetRenderVersion) (void)
{
	return VERSION;
}

EXPORT void HWRAPI(PostImgRedraw) (float points[SCREENVERTS][SCREENVERTS][2])
{
	INT32 x, y;
	float float_x, float_y, float_nextx, float_nexty;
	float xfix, yfix;
	INT32 texsize = 2048;

	const float blackBack[16] =
	{
		-16.0f, -16.0f, 6.0f,
		-16.0f, 16.0f, 6.0f,
		16.0f, 16.0f, 6.0f,
		16.0f, -16.0f, 6.0f
	};

	// Use a power of two texture, dammit
	if(screen_width <= 1024)
		texsize = 1024;
	if(screen_width <= 512)
		texsize = 512;

	// X/Y stretch fix for all resolutions(!)
	xfix = (float)(texsize)/((float)((screen_width)/(float)(SCREENVERTS-1)));
	yfix = (float)(texsize)/((float)((screen_height)/(float)(SCREENVERTS-1)));

	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_BLEND);

	// const float blackBack[16]

	// Draw a black square behind the screen texture,
	// so nothing shows through the edges
	pglColor4f(white[0], white[1], white[2], white[3]);

	pglVertexPointer(3, GL_FLOAT, 0, blackBack);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	for(x=0;x<SCREENVERTS-1;x++)
	{
		for(y=0;y<SCREENVERTS-1;y++)
		{
			float stCoords[8];
			float vertCoords[12];

			// Used for texture coordinates
			// Annoying magic numbers to scale the square texture to
			// a non-square screen..
			float_x = (float)(x/(xfix));
			float_y = (float)(y/(yfix));
			float_nextx = (float)(x+1)/(xfix);
			float_nexty = (float)(y+1)/(yfix);

			// float stCoords[8];
			stCoords[0] = float_x;
			stCoords[1] = float_y;
			stCoords[2] = float_x;
			stCoords[3] = float_nexty;
			stCoords[4] = float_nextx;
			stCoords[5] = float_nexty;
			stCoords[6] = float_nextx;
			stCoords[7] = float_y;

			pglTexCoordPointer(2, GL_FLOAT, 0, stCoords);

			// float vertCoords[12];
			vertCoords[0] = points[x][y][0];
			vertCoords[1] = points[x][y][1];
			vertCoords[2] = 4.4f;
			vertCoords[3] = points[x][y + 1][0];
			vertCoords[4] = points[x][y + 1][1];
			vertCoords[5] = 4.4f;
			vertCoords[6] = points[x + 1][y + 1][0];
			vertCoords[7] = points[x + 1][y + 1][1];
			vertCoords[8] = 4.4f;
			vertCoords[9] = points[x + 1][y][0];
			vertCoords[10] = points[x + 1][y][1];
			vertCoords[11] = 4.4f;

			pglVertexPointer(3, GL_FLOAT, 0, vertCoords);

			pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	}

	pglEnable(GL_DEPTH_TEST);
	pglEnable(GL_BLEND);
}

// Sryder:	This needs to be called whenever the screen changes resolution in order to reset the screen textures to use
//			a new size
EXPORT void HWRAPI(FlushScreenTextures) (void)
{
	pglDeleteTextures(1, &screentexture);
	pglDeleteTextures(1, &startScreenWipe);
	pglDeleteTextures(1, &endScreenWipe);
	pglDeleteTextures(1, &finalScreenTexture);
	screentexture = 0;
	startScreenWipe = 0;
	endScreenWipe = 0;
	finalScreenTexture = 0;
}

// Create Screen to fade from
EXPORT void HWRAPI(StartScreenWipe) (void)
{
	INT32 texsize = 2048;
	boolean firstTime = (startScreenWipe == 0);

	// Use a power of two texture, dammit
	if(screen_width <= 512)
		texsize = 512;
	else if(screen_width <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &startScreenWipe);
	pglBindTexture(GL_TEXTURE_2D, startScreenWipe);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = startScreenWipe;
}

// Create Screen to fade to
EXPORT void HWRAPI(EndScreenWipe)(void)
{
	INT32 texsize = 2048;
	boolean firstTime = (endScreenWipe == 0);

	// Use a power of two texture, dammit
	if(screen_width <= 512)
		texsize = 512;
	else if(screen_width <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &endScreenWipe);
	pglBindTexture(GL_TEXTURE_2D, endScreenWipe);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = endScreenWipe;
}


// Draw the last scene under the intermission
EXPORT void HWRAPI(DrawIntermissionBG)(void)
{
	float xfix, yfix;
	INT32 texsize = 2048;

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	if(screen_width <= 1024)
		texsize = 1024;
	if(screen_width <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	pglBindTexture(GL_TEXTURE_2D, screentexture);
	pglColor4f(white[0], white[1], white[2], white[3]);

	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	tex_downloaded = screentexture;
}

// Do screen fades!
EXPORT void HWRAPI(DoScreenWipe)(void)
{
	INT32 texsize = 2048;
	float xfix, yfix;

	INT32 fademaskdownloaded = tex_downloaded; // the fade mask that has been set

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	const float defaultST[8] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};

	// Use a power of two texture, dammit
	if(screen_width <= 1024)
		texsize = 1024;
	if(screen_width <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	SetBlend(PF_Modulated|PF_NoDepthTest|PF_Clip|PF_NoZClip);
	pglEnable(GL_TEXTURE_2D);

	// Draw the original screen
	pglBindTexture(GL_TEXTURE_2D, startScreenWipe);
	pglColor4f(white[0], white[1], white[2], white[3]);
	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	SetBlend(PF_Modulated|PF_Translucent|PF_NoDepthTest|PF_Clip|PF_NoZClip);

	// Draw the end screen that fades in
	pglActiveTexture(GL_TEXTURE0);
	pglEnable(GL_TEXTURE_2D);
	pglBindTexture(GL_TEXTURE_2D, endScreenWipe);
	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	pglActiveTexture(GL_TEXTURE1);
	pglEnable(GL_TEXTURE_2D);
	pglBindTexture(GL_TEXTURE_2D, fademaskdownloaded);

	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// const float defaultST[8]

	pglClientActiveTexture(GL_TEXTURE0);
	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglClientActiveTexture(GL_TEXTURE1);
	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	pglTexCoordPointer(2, GL_FLOAT, 0, defaultST);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglDisable(GL_TEXTURE_2D); // disable the texture in the 2nd texture unit
	pglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	pglActiveTexture(GL_TEXTURE0);
	pglClientActiveTexture(GL_TEXTURE0);
	tex_downloaded = endScreenWipe;
}

// Create a texture from the screen.
EXPORT void HWRAPI(MakeScreenTexture) (void)
{
	INT32 texsize = 2048;
	boolean firstTime = (screentexture == 0);

	// Use a power of two texture, dammit
	if(screen_width <= 512)
		texsize = 512;
	else if(screen_width <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &screentexture);
	pglBindTexture(GL_TEXTURE_2D, screentexture);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = screentexture;
}

EXPORT void HWRAPI(MakeScreenFinalTexture) (void)
{
	INT32 texsize = 2048;
	boolean firstTime = (finalScreenTexture == 0);

	// Use a power of two texture, dammit
	if(screen_width <= 512)
		texsize = 512;
	else if(screen_width <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &finalScreenTexture);
	pglBindTexture(GL_TEXTURE_2D, finalScreenTexture);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = finalScreenTexture;
}

EXPORT void HWRAPI(DrawScreenFinalTexture)(int width, int height)
{
	float xfix, yfix;
	float origaspect, newaspect;
	float xoff = 1, yoff = 1; // xoffset and yoffset for the polygon to have black bars around the screen
	FRGBAFloat clearColour;
	INT32 texsize = 2048;

	float off[12];
	float fix[8];

	if(screen_width <= 1024)
		texsize = 1024;
	if(screen_width <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	origaspect = (float)screen_width / screen_height;
	newaspect = (float)width / height;
	if (origaspect < newaspect)
	{
		xoff = origaspect / newaspect;
		yoff = 1;
	}
	else if (origaspect > newaspect)
	{
		xoff = 1;
		yoff = newaspect / origaspect;
	}

	// float off[12];
	off[0] = -xoff;
	off[1] = -yoff;
	off[2] = 1.0f;
	off[3] = -xoff;
	off[4] = yoff;
	off[5] = 1.0f;
	off[6] = xoff;
	off[7] = yoff;
	off[8] = 1.0f;
	off[9] = xoff;
	off[10] = -yoff;
	off[11] = 1.0f;

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglViewport(0, 0, width, height);

	clearColour.red = clearColour.green = clearColour.blue = 0;
	clearColour.alpha = 1;
	ClearBuffer(true, false, &clearColour);
	pglBindTexture(GL_TEXTURE_2D, finalScreenTexture);

	pglColor4f(white[0], white[1], white[2], white[3]);

	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, off);

	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	tex_downloaded = finalScreenTexture;
}

#endif //HWRENDER
