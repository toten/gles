#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

bool Init();
void Draw();
void ShutDown();

static Display *x_display = NULL;
static Atom s_wmDeleteMessage;

int g_width = 512;
int g_height = 512;

EGLBoolean WinCreate(const char *title, int width, int height,
    EGLNativeDisplayType& eglNativeDisplay, EGLNativeWindowType& eglNativeWindow)
{
    x_display = XOpenDisplay(NULL);
    if (x_display == NULL)
    {
        return EGL_FALSE;
    }

    Window root = DefaultRootWindow(x_display);

    XSetWindowAttributes swa;
    swa.event_mask  =  ExposureMask | PointerMotionMask | KeyPressMask;
    Window win = XCreateWindow(
        x_display, root,
        0, 0, width, height, 0,
        CopyFromParent, InputOutput,
        CopyFromParent, CWEventMask,
        &swa );

    s_wmDeleteMessage = XInternAtom(x_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x_display, win, &s_wmDeleteMessage, 1);

    XSetWindowAttributes xattr;
    xattr.override_redirect = 0;
    XChangeWindowAttributes(x_display, win, CWOverrideRedirect, &xattr);

    XWMHints hints;
    hints.input = 1;
    hints.flags = InputHint;
    XSetWMHints(x_display, win, &hints);

    XMapWindow(x_display, win);
    XStoreName(x_display, win, title);

    Atom wm_state = XInternAtom (x_display, "_NET_WM_STATE", 0);

    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type                 = ClientMessage;
    xev.xclient.window       = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = 1;
    xev.xclient.data.l[1]    = 0;
    XSendEvent(
       x_display,
       DefaultRootWindow ( x_display ),
       0,
       SubstructureNotifyMask,
       &xev);

    eglNativeDisplay = (EGLNativeDisplayType) x_display;    // typedef Display *EGLNativeDisplayType; (EGL/eglplatform.h)
    eglNativeWindow = (EGLNativeWindowType) win;    // typedef Window   EGLNativeWindowType; (EGL/eglplatform.h)
}

GLboolean userInterrupt()
{
    GLboolean userinterrupt = GL_FALSE;

    while ( XPending ( x_display ) )
    {
        XEvent xev;
        XNextEvent( x_display, &xev );
        if (xev.type == ClientMessage) {
            if (xev.xclient.data.l[0] == s_wmDeleteMessage) {
                userinterrupt = GL_TRUE;
            }
        }
        if ( xev.type == DestroyNotify )
            userinterrupt = GL_TRUE;
    }
    return userinterrupt;
}

void WinLoop(EGLDisplay eglDisplay, EGLSurface eglSurface)
{
    while(userInterrupt() == GL_FALSE)
    {
        Draw();

        eglSwapBuffers(eglDisplay, eglSurface);
    }
}

///
// GetContextRenderableType()
//
//    Check whether EGL_KHR_create_context extension is supported.  If so,
//    return EGL_OPENGL_ES3_BIT_KHR instead of EGL_OPENGL_ES2_BIT
//
EGLint GetContextRenderableType ( EGLDisplay eglDisplay )
{
#ifdef EGL_KHR_create_context
   const char *extensions = eglQueryString ( eglDisplay, EGL_EXTENSIONS );

   // check whether EGL_KHR_create_context is in the extension string
   if ( extensions != NULL && strstr( extensions, "EGL_KHR_create_context" ) )
   {
      // extension is supported
      return EGL_OPENGL_ES3_BIT_KHR;
   }
#endif
   // extension is not supported
   return EGL_OPENGL_ES2_BIT;
}

bool InitializeEGL(EGLDisplay& eglDisplay, EGLSurface& eglSurface)
{
    EGLNativeDisplayType eglNativeDisplay;
    EGLNativeWindowType eglNativeWindow;

    if (!WinCreate("angular normal map", g_width, g_height, eglNativeDisplay, eglNativeWindow))
    {
        return false;
    }

    eglDisplay = eglGetDisplay(eglNativeDisplay);
    if (eglDisplay == EGL_NO_DISPLAY)
    {
        return false;
    }

    EGLint majorVersion;
    EGLint minorVersion;
    if (!eglInitialize(eglDisplay, &majorVersion, &minorVersion))
    {
        return false;
    }

    EGLConfig config;
    {
        EGLint attribList[] =
        {
            EGL_RED_SIZE,       8,
            EGL_GREEN_SIZE,     8,
            EGL_BLUE_SIZE,      8,
            // if EGL_KHR_create_context extension is supported, then we will use
            // EGL_OPENGL_ES3_BIT_KHR instead of EGL_OPENGL_ES2_BIT in the attribute list
            EGL_RENDERABLE_TYPE, GetContextRenderableType ( eglDisplay ),
            EGL_NONE
        };

        EGLint numConfigs = 0;
        if ( !eglChooseConfig ( eglDisplay, attribList, &config, 1, &numConfigs ) )
        {
            return false;
        }

        if ( numConfigs < 1 )
        {
            return false;
        }
    }

    eglSurface = eglCreateWindowSurface(eglDisplay, config, eglNativeWindow, NULL);
    if (eglSurface == EGL_NO_SURFACE)
    {
        return false;
    }

    // EGL_CONTEXT_CLIENT_VERSION:
    // Specifies the type of context associated with the version of OpenGL ES that you are using
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    if (eglContext == EGL_NO_CONTEXT)
    {
        return false;
    }

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext))
    {
        return false;
    }

    return true;
}

int main ( int argc, char *argv[] )
{
    EGLDisplay eglDisplay;
    EGLSurface eglSurface;

    if (!InitializeEGL(eglDisplay, eglSurface))
    {
        return 1;
    }

    if (!Init())
    {
        return 1;
    }

    WinLoop (eglDisplay, eglSurface);

    ShutDown();

    return 0;
}

#include "anmaps_vs.h"
#include "anmaps_fs.h"
#include "light_vs.h"
#include "light_fs.h"
#include "nmaps_light_vs.h"
#include "nmaps_light_fs.h"
#include "nmaps.h"

#include "math.h"

static GLuint g_anmap_prog;
static GLuint g_scene_prog;
static GLuint g_light_prog;
static GLuint g_nmap_tex;
static GLuint g_anmap_tex;
static GLuint g_light_tex;
static GLuint g_anmap_fb;
static GLuint g_light_fb;

GLuint LoadShader(GLenum type, const char* shaderSrc)
{
    GLuint shader = glCreateShader(type);
    if (shader == 0)
    {
        return 0;
    }

    glShaderSource(shader, 1, &shaderSrc, NULL);

    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen > 1)
        {
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);

            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            printf("Error compiling shader:\n%s\n", infoLog);
            free (infoLog);
        }

        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint SetupProgram(GLuint vs, GLuint fs)
{
    GLuint prog = glCreateProgram();

    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1)
        {
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);
            glGetProgramInfoLog(prog, infoLen, NULL, infoLog);
            printf("Error linking program:\n%s\n", infoLog);
            free(infoLog);
        }

        glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

bool Init()
{
    // anmaps
    {
        printf("compiling anmaps vs...\n");
        GLuint vs = LoadShader(GL_VERTEX_SHADER, angular_normal_maps_vs_source);
        printf("compiling anmaps fs...\n");
        GLuint fs = LoadShader(GL_FRAGMENT_SHADER, angular_normal_maps_fs_source);

        printf("linking anmaps...\n");
        g_anmap_prog = SetupProgram(vs, fs);

        glGenTextures(1, &g_nmap_tex);
        glBindTexture(GL_TEXTURE_2D, g_nmap_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE, nmap_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // light
    {
        printf("compiling light vs...\n");
        GLuint vs = LoadShader(GL_VERTEX_SHADER, light_vs_source);
        printf("compiling light fs...\n");
        GLuint fs = LoadShader(GL_FRAGMENT_SHADER, light_fs_source);

        printf("linking light...\n");
        g_light_prog = SetupProgram(vs, fs);
    }

    // scene
    {
        printf("compiling scene vs...\n");
        GLuint vs = LoadShader(GL_VERTEX_SHADER, normal_maps_light_vs_source);
        printf("compiling scene fs...\n");
        GLuint fs = LoadShader(GL_FRAGMENT_SHADER, normal_maps_light_fs_source);

        printf("linking scene...\n");
        g_scene_prog = SetupProgram(vs, fs);

        glGenTextures(1, &g_anmap_tex);
        glBindTexture(GL_TEXTURE_2D, g_anmap_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glGenTextures(1, &g_light_tex);
        glBindTexture(GL_TEXTURE_2D, g_light_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glGenFramebuffers(1, &g_light_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, g_light_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_light_tex, 0);

    glGenFramebuffers(1, &g_anmap_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, g_anmap_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_anmap_tex, 0);

    return true;
}

#define ANMAP_PASS_SHOW 0
#if !ANMAP_PASS_SHOW
#define LIGHT_PASS_SHOW 0
#endif

#define ANGLE_DELTA 1
float g_angle_delta = 0.0f;

void Draw()
{
    GLsizei vertex_count = 4;
    GLfloat vertices[] = {
        0.0f,0.0f,0.0f, 0.0f,0.0f, 1.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,
        1.0f,0.0f,0.0f, 1.0f,0.0f, 1.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,
        1.0f,1.0f,0.0f, 1.0f,1.0f, 1.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f, 0.0f,1.0f, 1.0f,0.0f,0.0f, 0.0f,1.0f,0.0f
    };
    GLsizei stride = sizeof(vertices) / vertex_count;
    uint32_t indices[] = {
        0, 1, 2, 0, 2, 3,
    };

    glViewport(0, 0, g_width, g_height);

    // angle normal map pass
#if ANMAP_PASS_SHOW
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, g_anmap_fb);
#endif

    glUseProgram(g_anmap_prog);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, vertices + 3);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, vertices + 5);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, vertices + 8);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint ib;
    glGenBuffers(1, &ib);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    GLfloat model_matrix[] = {
        1.0f, 0.0f, 0.0f, -0.5f,
        0.0f, 1.0f, 0.0f, -0.5f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    glUniformMatrix4fv(glGetUniformLocation(g_anmap_prog, "model_matrix"), 1, GL_TRUE, model_matrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_nmap_tex);
    glUniform1i(glGetUniformLocation(g_anmap_prog, "normal_map"), 0);

    glDrawElements(GL_TRIANGLES, sizeof(indices)/sizeof(uint32_t), GL_UNSIGNED_INT, 0);

#if !ANMAP_PASS_SHOW
    // light pass
#if LIGHT_PASS_SHOW
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, g_light_fb);
#endif

    glUseProgram(g_light_prog);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, vertices + 3);

    glEnableVertexAttribArray(0);

    glDrawElements(GL_TRIANGLES, sizeof(indices)/sizeof(uint32_t), GL_UNSIGNED_INT, 0);

#if !LIGHT_PASS_SHOW
    // scene pass
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_scene_prog);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, vertices + 3);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    GLfloat view_matrix[] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    float incline = 15.0f / 180.0f * 3.1415926;
    view_matrix[5] = cos(incline);
    view_matrix[9] = sin(incline);
    view_matrix[6] = sin(incline);
    view_matrix[10] = -cos(incline);
    view_matrix[7] = 0.0f;
    view_matrix[11] = 1.2f;

    GLfloat n = 0.1f;
    GLfloat f = 100.0f;
    GLfloat fov = 60.0f / 180.0f * 3.1415926f;
    GLfloat ratio = (GLfloat)g_width / g_height;
    GLfloat projection_matrix[] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    projection_matrix[0] = 1.0f / tanf(0.5f * fov) / ratio;
    projection_matrix[5] = 1.0f / tanf(0.5f * fov);
    projection_matrix[10] = (f + n) / (f - n);
    projection_matrix[11] = -2.0f * f * n / (f - n);
    projection_matrix[14] = 1.0f;
    projection_matrix[15] = 0.0f;

    glUniformMatrix4fv(glGetUniformLocation(g_scene_prog, "model_matrix"), 1, GL_TRUE, model_matrix);
    glUniformMatrix4fv(glGetUniformLocation(g_scene_prog, "view_matrix"), 1, GL_TRUE, view_matrix);
    glUniformMatrix4fv(glGetUniformLocation(g_scene_prog, "projection_matrix"), 1, GL_TRUE, projection_matrix);

#if ANGLE_DELTA
    g_angle_delta += 0.005f;
    g_angle_delta = g_angle_delta > 1.0f ? 0.0f : g_angle_delta;
#endif
    glUniform1f(glGetUniformLocation(g_scene_prog, "angle_delta"), g_angle_delta);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_anmap_tex);
    glUniform1i(glGetUniformLocation(g_scene_prog, "angular_normal_map"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_light_tex);
    glUniform1i(glGetUniformLocation(g_scene_prog, "light_map"), 1);

    glDrawElements(GL_TRIANGLES, sizeof(indices)/sizeof(uint32_t), GL_UNSIGNED_INT, 0);
#endif  // LIGHT_PASS_SHOW
#endif  // ANMAP_PASS_SHOW
}

void ShutDown()
{
    glDeleteProgram(g_anmap_prog);
}