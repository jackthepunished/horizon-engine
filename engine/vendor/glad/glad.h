/**
 * GLAD OpenGL Loader - OpenGL 4.1 Core Profile
 * Generated for macOS compatibility
 *
 * This is a minimal GLAD implementation for OpenGL 4.1 Core Profile.
 */

#ifndef GLAD_GL_H_
#define GLAD_GL_H_

#ifdef __gl_h_
#error "OpenGL header already included!"
#endif
#define __gl_h_ 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform-specific calling convention */
#if defined(_WIN32)
#define GLAPIENTRY __stdcall
#define GLAPI extern
#else
#define GLAPIENTRY
#define GLAPI extern
#endif

/* OpenGL types */
typedef void GLvoid;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef char GLchar;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned short GLushort;
typedef intptr_t GLintptr;
typedef ptrdiff_t GLsizeiptr;
typedef float GLclampf;
typedef double GLclampd;
typedef int64_t GLint64;
typedef uint64_t GLuint64;
typedef struct __GLsync* GLsync;

/* Debug callback types */
typedef void(GLAPIENTRY* GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity,
                                      GLsizei length, const GLchar* message, const void* userParam);

/* Version info */
typedef struct {
    int major;
    int minor;
} gladGLversionStruct;
GLAPI gladGLversionStruct GLVersion;

/* Loader function */
GLAPI int gladLoadGLLoader(void* (*load)(const char* name));

/* ========================================================================== */
/* OpenGL Constants */
/* ========================================================================== */

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NONE 0

/* Data types */
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_DOUBLE 0x140A

/* Primitives */
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006

/* Buffers */
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STREAM_DRAW 0x88E0

/* Textures */
#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_MIRRORED_REPEAT 0x8370

/* Pixel formats */
#define GL_RED 0x1903
#define GL_RG 0x8227
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_DEPTH_COMPONENT 0x1902
#define GL_R8 0x8229
#define GL_RG8 0x822B
#define GL_RGB8 0x8051
#define GL_RGBA8 0x8058
#define GL_SRGB8 0x8C41
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_R16F 0x822D
#define GL_RG16F 0x822F
#define GL_RGB16F 0x881B
#define GL_RGBA16F 0x881A
#define GL_R32F 0x822E
#define GL_RG32F 0x8230
#define GL_RGB32F 0x8815
#define GL_RGBA32F 0x8814
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH24_STENCIL8 0x88F0

/* UBO Constants */
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_UNIFORM_BUFFER_BINDING 0x8A28
#define GL_UNIFORM_BUFFER_START 0x8A29
#define GL_UNIFORM_BUFFER_SIZE 0x8A2A
#define GL_MAX_UNIFORM_BUFFER_BINDINGS 0x8A2F
#define GL_MAX_UNIFORM_BLOCK_SIZE 0x8A30
#define GL_MAX_VERTEX_UNIFORM_BLOCKS 0x8A2B
#define GL_MAX_GEOMETRY_UNIFORM_BLOCKS 0x8A2C
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS 0x8A2D
#define GL_UNIFORM_BLOCK_DATA_SIZE 0x8A40
#define GL_UNIFORM_BLOCK_NAME_LENGTH 0x8A41
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS 0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES 0x8A43
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER 0x8A44
#define GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER 0x8A45
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER 0x8A46
#define GL_INVALID_INDEX 0xFFFFFFFFu

/* Shaders */
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84

/* Enable/Disable */
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_SCISSOR_TEST 0x0C11
#define GL_MULTISAMPLE 0x809D

/* Blend */
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307

/* Face culling */
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CW 0x0900
#define GL_CCW 0x0901

/* Depth test */
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207

/* Clear */
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400

/* Errors */
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_OUT_OF_MEMORY 0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506

/* Framebuffer */
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD

/* Cubemap */
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A

/* Debug */
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x8249
#define GL_DEBUG_SOURCE_APPLICATION 0x824A
#define GL_DEBUG_SOURCE_OTHER 0x824B
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_OTHER 0x8251
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_CONTEXT_FLAGS 0x821E
#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002
#define GL_DONT_CARE 0x1100

/* Get */
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C

/* ========================================================================== */
/* OpenGL Function Pointers */
/* ========================================================================== */

/* Core */
GLAPI void(GLAPIENTRY* glClear)(GLbitfield mask);
GLAPI void(GLAPIENTRY* glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
GLAPI void(GLAPIENTRY* glClearDepth)(GLdouble depth);
GLAPI void(GLAPIENTRY* glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void(GLAPIENTRY* glEnable)(GLenum cap);
GLAPI void(GLAPIENTRY* glDisable)(GLenum cap);
GLAPI void(GLAPIENTRY* glBlendFunc)(GLenum sfactor, GLenum dfactor);
GLAPI void(GLAPIENTRY* glCullFace)(GLenum mode);
GLAPI void(GLAPIENTRY* glFrontFace)(GLenum mode);
GLAPI void(GLAPIENTRY* glDepthFunc)(GLenum func);
GLAPI void(GLAPIENTRY* glDepthMask)(GLboolean flag);
GLAPI GLenum(GLAPIENTRY* glGetError)(void);
GLAPI const GLubyte*(GLAPIENTRY* glGetString)(GLenum name);
GLAPI void(GLAPIENTRY* glGetIntegerv)(GLenum pname, GLint* data);
GLAPI void(GLAPIENTRY* glPolygonMode)(GLenum face, GLenum mode);

/* Buffers */
GLAPI void(GLAPIENTRY* glGenBuffers)(GLsizei n, GLuint* buffers);
GLAPI void(GLAPIENTRY* glDeleteBuffers)(GLsizei n, const GLuint* buffers);
GLAPI void(GLAPIENTRY* glBindBuffer)(GLenum target, GLuint buffer);
GLAPI void(GLAPIENTRY* glBufferData)(GLenum target, GLsizeiptr size, const void* data,
                                     GLenum usage);
GLAPI void(GLAPIENTRY* glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size,
                                        const void* data);

/* Function pointers */
GLAPI void(GLAPIENTRY* glBindBufferBase)(GLenum target, GLuint index, GLuint buffer);
GLAPI void(GLAPIENTRY* glBindBufferRange)(GLenum target, GLuint index, GLuint buffer,
                                          GLintptr offset, GLsizeiptr size);
GLAPI GLuint(GLAPIENTRY* glGetUniformBlockIndex)(GLuint program, const GLchar* uniformBlockName);
GLAPI void(GLAPIENTRY* glUniformBlockBinding)(GLuint program, GLuint uniformBlockIndex,
                                              GLuint uniformBlockBinding);

/* Vertex Arrays */
GLAPI void(GLAPIENTRY* glGenVertexArrays)(GLsizei n, GLuint* arrays);
GLAPI void(GLAPIENTRY* glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
GLAPI void(GLAPIENTRY* glBindVertexArray)(GLuint array);
GLAPI void(GLAPIENTRY* glEnableVertexAttribArray)(GLuint index);
GLAPI void(GLAPIENTRY* glDisableVertexAttribArray)(GLuint index);
GLAPI void(GLAPIENTRY* glVertexAttribPointer)(GLuint index, GLint size, GLenum type,
                                              GLboolean normalized, GLsizei stride,
                                              const void* pointer);
GLAPI void(GLAPIENTRY* glVertexAttribIPointer)(GLuint index, GLint size, GLenum type,
                                               GLsizei stride, const void* pointer);

/* Drawing */
GLAPI void(GLAPIENTRY* glDrawArrays)(GLenum mode, GLint first, GLsizei count);
GLAPI void(GLAPIENTRY* glDrawElements)(GLenum mode, GLsizei count, GLenum type,
                                       const void* indices);

/* Instanced Rendering (OpenGL 3.1+) */
GLAPI void(GLAPIENTRY* glDrawArraysInstanced)(GLenum mode, GLint first, GLsizei count,
                                              GLsizei instancecount);
GLAPI void(GLAPIENTRY* glDrawElementsInstanced)(GLenum mode, GLsizei count, GLenum type,
                                                const void* indices, GLsizei instancecount);
GLAPI void(GLAPIENTRY* glVertexAttribDivisor)(GLuint index, GLuint divisor);

/* Shaders */
GLAPI GLuint(GLAPIENTRY* glCreateShader)(GLenum type);
GLAPI void(GLAPIENTRY* glDeleteShader)(GLuint shader);
GLAPI void(GLAPIENTRY* glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string,
                                       const GLint* length);
GLAPI void(GLAPIENTRY* glCompileShader)(GLuint shader);
GLAPI void(GLAPIENTRY* glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
GLAPI void(GLAPIENTRY* glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length,
                                           GLchar* infoLog);

/* Programs */
GLAPI GLuint(GLAPIENTRY* glCreateProgram)(void);
GLAPI void(GLAPIENTRY* glDeleteProgram)(GLuint program);
GLAPI void(GLAPIENTRY* glAttachShader)(GLuint program, GLuint shader);
GLAPI void(GLAPIENTRY* glLinkProgram)(GLuint program);
GLAPI void(GLAPIENTRY* glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
GLAPI void(GLAPIENTRY* glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length,
                                            GLchar* infoLog);
GLAPI void(GLAPIENTRY* glUseProgram)(GLuint program);
GLAPI GLint(GLAPIENTRY* glGetUniformLocation)(GLuint program, const GLchar* name);

/* Uniforms */
GLAPI void(GLAPIENTRY* glUniform1i)(GLint location, GLint v0);
GLAPI void(GLAPIENTRY* glUniform1f)(GLint location, GLfloat v0);
GLAPI void(GLAPIENTRY* glUniform2fv)(GLint location, GLsizei count, const GLfloat* value);
GLAPI void(GLAPIENTRY* glUniform3fv)(GLint location, GLsizei count, const GLfloat* value);
GLAPI void(GLAPIENTRY* glUniform4fv)(GLint location, GLsizei count, const GLfloat* value);
GLAPI void(GLAPIENTRY* glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose,
                                           const GLfloat* value);
GLAPI void(GLAPIENTRY* glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
                                           const GLfloat* value);

/* Textures */
GLAPI void(GLAPIENTRY* glGenTextures)(GLsizei n, GLuint* textures);
GLAPI void(GLAPIENTRY* glDeleteTextures)(GLsizei n, const GLuint* textures);
GLAPI void(GLAPIENTRY* glBindTexture)(GLenum target, GLuint texture);
GLAPI void(GLAPIENTRY* glActiveTexture)(GLenum texture);
GLAPI void(GLAPIENTRY* glTexImage2D)(GLenum target, GLint level, GLint internalformat,
                                     GLsizei width, GLsizei height, GLint border, GLenum format,
                                     GLenum type, const void* pixels);
GLAPI void(GLAPIENTRY* glTexParameteri)(GLenum target, GLenum pname, GLint param);
GLAPI void(GLAPIENTRY* glGenerateMipmap)(GLenum target);

/* Framebuffers */
GLAPI void(GLAPIENTRY* glGenFramebuffers)(GLsizei n, GLuint* framebuffers);
GLAPI void(GLAPIENTRY* glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
GLAPI void(GLAPIENTRY* glBindFramebuffer)(GLenum target, GLuint framebuffer);
GLAPI void(GLAPIENTRY* glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget,
                                               GLuint texture, GLint level);
GLAPI GLenum(GLAPIENTRY* glCheckFramebufferStatus)(GLenum target);

/* Renderbuffers */
GLAPI void(GLAPIENTRY* glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
GLAPI void(GLAPIENTRY* glDeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers);
GLAPI void(GLAPIENTRY* glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
GLAPI void(GLAPIENTRY* glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width,
                                              GLsizei height);
GLAPI void(GLAPIENTRY* glFramebufferRenderbuffer)(GLenum target, GLenum attachment,
                                                  GLenum renderbuffertarget, GLuint renderbuffer);

/* Debug (OpenGL 4.3+, may not be available on macOS 4.1) */
GLAPI void(GLAPIENTRY* glDebugMessageCallback)(GLDEBUGPROC callback, const void* userParam);
GLAPI void(GLAPIENTRY* glDebugMessageControl)(GLenum source, GLenum type, GLenum severity,
                                              GLsizei count, const GLuint* ids, GLboolean enabled);

/* Missing 4.1 Core functions */
GLAPI void(GLAPIENTRY* glDrawBuffer)(GLenum buf);
GLAPI void(GLAPIENTRY* glReadBuffer)(GLenum mode);
GLAPI void(GLAPIENTRY* glTexParameterfv)(GLenum target, GLenum pname, const GLfloat* params);

/* Missing Defines */
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_TEXTURE_BORDER_COLOR 0x1004

/* MRT (Multiple Render Targets) - OpenGL 2.0+ */
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_COLOR_ATTACHMENT2 0x8CE2
#define GL_COLOR_ATTACHMENT3 0x8CE3
#define GL_COLOR_ATTACHMENT4 0x8CE4
#define GL_COLOR_ATTACHMENT5 0x8CE5
#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#define GL_MAX_DRAW_BUFFERS 0x8824

/* Additional texture units */
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE4 0x84C4
#define GL_TEXTURE5 0x84C5
#define GL_TEXTURE6 0x84C6
#define GL_TEXTURE7 0x84C7
#define GL_TEXTURE8 0x84C8
#define GL_TEXTURE9 0x84C9
#define GL_TEXTURE10 0x84CA
#define GL_TEXTURE11 0x84CB
#define GL_TEXTURE12 0x84CC
#define GL_TEXTURE13 0x84CD
#define GL_TEXTURE14 0x84CE
#define GL_TEXTURE15 0x84CF

/* MRT function */
GLAPI void(GLAPIENTRY* glDrawBuffers)(GLsizei n, const GLenum* bufs);

#ifdef __cplusplus
}
#endif

#endif /* GLAD_GL_H_ */
