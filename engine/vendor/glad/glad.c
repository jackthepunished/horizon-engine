/**
 * GLAD OpenGL Loader Implementation
 */

#include "glad.h"

#include <string.h>

gladGLversionStruct GLVersion = {0, 0};

/* Function pointer definitions */
void(GLAPIENTRY* glClear)(GLbitfield mask) = NULL;
void(GLAPIENTRY* glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) = NULL;
void(GLAPIENTRY* glClearDepth)(GLdouble depth) = NULL;

/* Missing 4.1 Core Definitions */
void(GLAPIENTRY* glDrawBuffer)(GLenum buf) = NULL;
void(GLAPIENTRY* glReadBuffer)(GLenum mode) = NULL;
void(GLAPIENTRY* glTexParameterfv)(GLenum target, GLenum pname, const GLfloat* params) = NULL;

void(GLAPIENTRY* glViewport)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
void(GLAPIENTRY* glEnable)(GLenum cap) = NULL;
void(GLAPIENTRY* glDisable)(GLenum cap) = NULL;
void(GLAPIENTRY* glBlendFunc)(GLenum sfactor, GLenum dfactor) = NULL;
void(GLAPIENTRY* glCullFace)(GLenum mode) = NULL;
void(GLAPIENTRY* glFrontFace)(GLenum mode) = NULL;
void(GLAPIENTRY* glDepthFunc)(GLenum func) = NULL;
void(GLAPIENTRY* glDepthMask)(GLboolean flag) = NULL;
GLenum(GLAPIENTRY* glGetError)(void) = NULL;
const GLubyte*(GLAPIENTRY* glGetString)(GLenum name) = NULL;
void(GLAPIENTRY* glGetIntegerv)(GLenum pname, GLint* data) = NULL;
void(GLAPIENTRY* glPolygonMode)(GLenum face, GLenum mode) = NULL;

void(GLAPIENTRY* glGenBuffers)(GLsizei n, GLuint* buffers) = NULL;
void(GLAPIENTRY* glDeleteBuffers)(GLsizei n, const GLuint* buffers) = NULL;
void(GLAPIENTRY* glBindBuffer)(GLenum target, GLuint buffer) = NULL;
void(GLAPIENTRY* glBufferData)(GLenum target, GLsizeiptr size, const void* data,
                               GLenum usage) = NULL;
void(GLAPIENTRY* glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size,
                                  const void* data) = NULL;

void(GLAPIENTRY* glBindBufferBase)(GLenum target, GLuint index, GLuint buffer) = NULL;
void(GLAPIENTRY* glBindBufferRange)(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                    GLsizeiptr size) = NULL;
GLuint(GLAPIENTRY* glGetUniformBlockIndex)(GLuint program, const GLchar* uniformBlockName) = NULL;
void(GLAPIENTRY* glUniformBlockBinding)(GLuint program, GLuint uniformBlockIndex,
                                        GLuint uniformBlockBinding) = NULL;

void(GLAPIENTRY* glGenVertexArrays)(GLsizei n, GLuint* arrays) = NULL;
void(GLAPIENTRY* glDeleteVertexArrays)(GLsizei n, const GLuint* arrays) = NULL;
void(GLAPIENTRY* glBindVertexArray)(GLuint array) = NULL;
void(GLAPIENTRY* glEnableVertexAttribArray)(GLuint index) = NULL;
void(GLAPIENTRY* glDisableVertexAttribArray)(GLuint index) = NULL;
void(GLAPIENTRY* glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                        GLsizei stride, const void* pointer) = NULL;
void(GLAPIENTRY* glVertexAttribIPointer)(GLuint index, GLint size, GLenum type, GLsizei stride,
                                         const void* pointer) = NULL;

void(GLAPIENTRY* glDrawArrays)(GLenum mode, GLint first, GLsizei count) = NULL;
void(GLAPIENTRY* glDrawElements)(GLenum mode, GLsizei count, GLenum type,
                                 const void* indices) = NULL;

/* Instanced Rendering */
void(GLAPIENTRY* glDrawArraysInstanced)(GLenum mode, GLint first, GLsizei count,
                                        GLsizei instancecount) = NULL;
void(GLAPIENTRY* glDrawElementsInstanced)(GLenum mode, GLsizei count, GLenum type,
                                          const void* indices, GLsizei instancecount) = NULL;
void(GLAPIENTRY* glVertexAttribDivisor)(GLuint index, GLuint divisor) = NULL;

GLuint(GLAPIENTRY* glCreateShader)(GLenum type) = NULL;
void(GLAPIENTRY* glDeleteShader)(GLuint shader) = NULL;
void(GLAPIENTRY* glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string,
                                 const GLint* length) = NULL;
void(GLAPIENTRY* glCompileShader)(GLuint shader) = NULL;
void(GLAPIENTRY* glGetShaderiv)(GLuint shader, GLenum pname, GLint* params) = NULL;
void(GLAPIENTRY* glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length,
                                     GLchar* infoLog) = NULL;

GLuint(GLAPIENTRY* glCreateProgram)(void) = NULL;
void(GLAPIENTRY* glDeleteProgram)(GLuint program) = NULL;
void(GLAPIENTRY* glAttachShader)(GLuint program, GLuint shader) = NULL;
void(GLAPIENTRY* glLinkProgram)(GLuint program) = NULL;
void(GLAPIENTRY* glGetProgramiv)(GLuint program, GLenum pname, GLint* params) = NULL;
void(GLAPIENTRY* glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length,
                                      GLchar* infoLog) = NULL;
void(GLAPIENTRY* glUseProgram)(GLuint program) = NULL;
GLint(GLAPIENTRY* glGetUniformLocation)(GLuint program, const GLchar* name) = NULL;

void(GLAPIENTRY* glUniform1i)(GLint location, GLint v0) = NULL;
void(GLAPIENTRY* glUniform1f)(GLint location, GLfloat v0) = NULL;
void(GLAPIENTRY* glUniform2fv)(GLint location, GLsizei count, const GLfloat* value) = NULL;
void(GLAPIENTRY* glUniform3fv)(GLint location, GLsizei count, const GLfloat* value) = NULL;
void(GLAPIENTRY* glUniform4fv)(GLint location, GLsizei count, const GLfloat* value) = NULL;
void(GLAPIENTRY* glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat* value) = NULL;
void(GLAPIENTRY* glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat* value) = NULL;

void(GLAPIENTRY* glGenTextures)(GLsizei n, GLuint* textures) = NULL;
void(GLAPIENTRY* glDeleteTextures)(GLsizei n, const GLuint* textures) = NULL;
void(GLAPIENTRY* glBindTexture)(GLenum target, GLuint texture) = NULL;
void(GLAPIENTRY* glActiveTexture)(GLenum texture) = NULL;
void(GLAPIENTRY* glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width,
                               GLsizei height, GLint border, GLenum format, GLenum type,
                               const void* pixels) = NULL;
void(GLAPIENTRY* glTexParameteri)(GLenum target, GLenum pname, GLint param) = NULL;
void(GLAPIENTRY* glGenerateMipmap)(GLenum target) = NULL;

void(GLAPIENTRY* glGenFramebuffers)(GLsizei n, GLuint* framebuffers) = NULL;
void(GLAPIENTRY* glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers) = NULL;
void(GLAPIENTRY* glBindFramebuffer)(GLenum target, GLuint framebuffer) = NULL;
void(GLAPIENTRY* glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget,
                                         GLuint texture, GLint level) = NULL;
GLenum(GLAPIENTRY* glCheckFramebufferStatus)(GLenum target) = NULL;

void(GLAPIENTRY* glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers) = NULL;
void(GLAPIENTRY* glDeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers) = NULL;
void(GLAPIENTRY* glBindRenderbuffer)(GLenum target, GLuint renderbuffer) = NULL;
void(GLAPIENTRY* glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width,
                                        GLsizei height) = NULL;
void(GLAPIENTRY* glFramebufferRenderbuffer)(GLenum target, GLenum attachment,
                                            GLenum renderbuffertarget, GLuint renderbuffer) = NULL;

void(GLAPIENTRY* glDebugMessageCallback)(GLDEBUGPROC callback, const void* userParam) = NULL;
void(GLAPIENTRY* glDebugMessageControl)(GLenum source, GLenum type, GLenum severity, GLsizei count,
                                        const GLuint* ids, GLboolean enabled) = NULL;

/* MRT (Multiple Render Targets) */
void(GLAPIENTRY* glDrawBuffers)(GLsizei n, const GLenum* bufs) = NULL;

/* Type for function pointers */
typedef void* (*GLloadproc)(const char* name);

static void load_GL_version_4_1(GLloadproc load) {
    glClear = (void(GLAPIENTRY*)(GLbitfield))load("glClear");
    glClearColor = (void(GLAPIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat))load("glClearColor");
    glClearDepth = (void(GLAPIENTRY*)(GLdouble))load("glClearDepth");
    glViewport = (void(GLAPIENTRY*)(GLint, GLint, GLsizei, GLsizei))load("glViewport");
    glEnable = (void(GLAPIENTRY*)(GLenum))load("glEnable");
    glDisable = (void(GLAPIENTRY*)(GLenum))load("glDisable");
    glBlendFunc = (void(GLAPIENTRY*)(GLenum, GLenum))load("glBlendFunc");
    glCullFace = (void(GLAPIENTRY*)(GLenum))load("glCullFace");
    glFrontFace = (void(GLAPIENTRY*)(GLenum))load("glFrontFace");
    glDepthFunc = (void(GLAPIENTRY*)(GLenum))load("glDepthFunc");
    glDepthMask = (void(GLAPIENTRY*)(GLboolean))load("glDepthMask");
    glGetError = (GLenum(GLAPIENTRY*)(void))load("glGetError");
    glGetIntegerv = (void(GLAPIENTRY*)(GLenum, GLint*))load("glGetIntegerv");
    glPolygonMode = (void(GLAPIENTRY*)(GLenum, GLenum))load("glPolygonMode");

    glGenBuffers = (void(GLAPIENTRY*)(GLsizei, GLuint*))load("glGenBuffers");
    glDeleteBuffers = (void(GLAPIENTRY*)(GLsizei, const GLuint*))load("glDeleteBuffers");
    glBindBuffer = (void(GLAPIENTRY*)(GLenum, GLuint))load("glBindBuffer");
    glBufferData = (void(GLAPIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum))load("glBufferData");
    glBufferSubData =
        (void(GLAPIENTRY*)(GLenum, GLintptr, GLsizeiptr, const void*))load("glBufferSubData");

    glBindBufferBase = (void(GLAPIENTRY*)(GLenum, GLuint, GLuint))load("glBindBufferBase");
    glBindBufferRange =
        (void(GLAPIENTRY*)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr))load("glBindBufferRange");
    glGetUniformBlockIndex =
        (GLuint(GLAPIENTRY*)(GLuint, const GLchar*))load("glGetUniformBlockIndex");
    glUniformBlockBinding =
        (void(GLAPIENTRY*)(GLuint, GLuint, GLuint))load("glUniformBlockBinding");

    glGenVertexArrays = (void(GLAPIENTRY*)(GLsizei, GLuint*))load("glGenVertexArrays");
    glDeleteVertexArrays = (void(GLAPIENTRY*)(GLsizei, const GLuint*))load("glDeleteVertexArrays");
    glBindVertexArray = (void(GLAPIENTRY*)(GLuint))load("glBindVertexArray");
    glEnableVertexAttribArray = (void(GLAPIENTRY*)(GLuint))load("glEnableVertexAttribArray");
    glDisableVertexAttribArray = (void(GLAPIENTRY*)(GLuint))load("glDisableVertexAttribArray");
    glVertexAttribPointer = (void(GLAPIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei,
                                               const void*))load("glVertexAttribPointer");
    glVertexAttribIPointer = (void(GLAPIENTRY*)(GLuint, GLint, GLenum, GLsizei, const void*))load(
        "glVertexAttribIPointer");

    glDrawArrays = (void(GLAPIENTRY*)(GLenum, GLint, GLsizei))load("glDrawArrays");
    glDrawElements =
        (void(GLAPIENTRY*)(GLenum, GLsizei, GLenum, const void*))load("glDrawElements");

    /* Instanced Rendering */
    glDrawArraysInstanced =
        (void(GLAPIENTRY*)(GLenum, GLint, GLsizei, GLsizei))load("glDrawArraysInstanced");
    glDrawElementsInstanced = (void(GLAPIENTRY*)(GLenum, GLsizei, GLenum, const void*,
                                                 GLsizei))load("glDrawElementsInstanced");
    glVertexAttribDivisor = (void(GLAPIENTRY*)(GLuint, GLuint))load("glVertexAttribDivisor");

    glCreateShader = (GLuint(GLAPIENTRY*)(GLenum))load("glCreateShader");
    glDeleteShader = (void(GLAPIENTRY*)(GLuint))load("glDeleteShader");
    glShaderSource = (void(GLAPIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*))load(
        "glShaderSource");
    glCompileShader = (void(GLAPIENTRY*)(GLuint))load("glCompileShader");
    glGetShaderiv = (void(GLAPIENTRY*)(GLuint, GLenum, GLint*))load("glGetShaderiv");
    glGetShaderInfoLog =
        (void(GLAPIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*))load("glGetShaderInfoLog");

    glCreateProgram = (GLuint(GLAPIENTRY*)(void))load("glCreateProgram");
    glDeleteProgram = (void(GLAPIENTRY*)(GLuint))load("glDeleteProgram");
    glAttachShader = (void(GLAPIENTRY*)(GLuint, GLuint))load("glAttachShader");
    glLinkProgram = (void(GLAPIENTRY*)(GLuint))load("glLinkProgram");
    glGetProgramiv = (void(GLAPIENTRY*)(GLuint, GLenum, GLint*))load("glGetProgramiv");
    glGetProgramInfoLog =
        (void(GLAPIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*))load("glGetProgramInfoLog");
    glUseProgram = (void(GLAPIENTRY*)(GLuint))load("glUseProgram");
    glGetUniformLocation = (GLint(GLAPIENTRY*)(GLuint, const GLchar*))load("glGetUniformLocation");

    glUniform1i = (void(GLAPIENTRY*)(GLint, GLint))load("glUniform1i");
    glUniform1f = (void(GLAPIENTRY*)(GLint, GLfloat))load("glUniform1f");
    glUniform2fv = (void(GLAPIENTRY*)(GLint, GLsizei, const GLfloat*))load("glUniform2fv");
    glUniform3fv = (void(GLAPIENTRY*)(GLint, GLsizei, const GLfloat*))load("glUniform3fv");
    glUniform4fv = (void(GLAPIENTRY*)(GLint, GLsizei, const GLfloat*))load("glUniform4fv");
    glUniformMatrix3fv =
        (void(GLAPIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*))load("glUniformMatrix3fv");
    glUniformMatrix4fv =
        (void(GLAPIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*))load("glUniformMatrix4fv");

    glGenTextures = (void(GLAPIENTRY*)(GLsizei, GLuint*))load("glGenTextures");
    glDeleteTextures = (void(GLAPIENTRY*)(GLsizei, const GLuint*))load("glDeleteTextures");
    glBindTexture = (void(GLAPIENTRY*)(GLenum, GLuint))load("glBindTexture");
    glActiveTexture = (void(GLAPIENTRY*)(GLenum))load("glActiveTexture");
    glTexImage2D = (void(GLAPIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                                      const void*))load("glTexImage2D");
    glTexParameteri = (void(GLAPIENTRY*)(GLenum, GLenum, GLint))load("glTexParameteri");
    glGenerateMipmap = (void(GLAPIENTRY*)(GLenum))load("glGenerateMipmap");

    glGenFramebuffers = (void(GLAPIENTRY*)(GLsizei, GLuint*))load("glGenFramebuffers");
    glDeleteFramebuffers = (void(GLAPIENTRY*)(GLsizei, const GLuint*))load("glDeleteFramebuffers");
    glBindFramebuffer = (void(GLAPIENTRY*)(GLenum, GLuint))load("glBindFramebuffer");
    glFramebufferTexture2D =
        (void(GLAPIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint))load("glFramebufferTexture2D");
    glCheckFramebufferStatus = (GLenum(GLAPIENTRY*)(GLenum))load("glCheckFramebufferStatus");

    glGenRenderbuffers = (void(GLAPIENTRY*)(GLsizei, GLuint*))load("glGenRenderbuffers");
    glDeleteRenderbuffers =
        (void(GLAPIENTRY*)(GLsizei, const GLuint*))load("glDeleteRenderbuffers");
    glBindRenderbuffer = (void(GLAPIENTRY*)(GLenum, GLuint))load("glBindRenderbuffer");
    glRenderbufferStorage =
        (void(GLAPIENTRY*)(GLenum, GLenum, GLsizei, GLsizei))load("glRenderbufferStorage");
    glFramebufferRenderbuffer =
        (void(GLAPIENTRY*)(GLenum, GLenum, GLenum, GLuint))load("glFramebufferRenderbuffer");

    /* Debug */
    glDebugMessageCallback =
        (void(GLAPIENTRY*)(GLDEBUGPROC, const void*))load("glDebugMessageCallback");
    glDebugMessageControl = (void(GLAPIENTRY*)(GLenum, GLenum, GLenum, GLsizei, const GLuint*,
                                               GLboolean))load("glDebugMessageControl");

    /* Added missing 4.1 functions */
    glDrawBuffer = (void(GLAPIENTRY*)(GLenum))load("glDrawBuffer");
    glReadBuffer = (void(GLAPIENTRY*)(GLenum))load("glReadBuffer");
    glTexParameterfv = (void(GLAPIENTRY*)(GLenum, GLenum, const GLfloat*))load("glTexParameterfv");

    /* MRT (Multiple Render Targets) */
    glDrawBuffers = (void(GLAPIENTRY*)(GLsizei, const GLenum*))load("glDrawBuffers");
}

int gladLoadGLLoader(void* (*load)(const char* name)) {
    if (!load)
        return 0;

    /* Get version string */
    glGetString = (const GLubyte*(GLAPIENTRY*)(GLenum))load("glGetString");
    if (!glGetString)
        return 0;

    const char* version = (const char*)glGetString(GL_VERSION);
    if (!version)
        return 0;

    /* Parse version (format: "X.Y ...") */
    GLVersion.major = version[0] - '0';
    GLVersion.minor = version[2] - '0';

    /* Load core functions */
    load_GL_version_4_1(load);

    return 1;
}
