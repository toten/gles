#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <algorithm>

#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

#define DEBUG_SHADER_COMPILE 0
#define SCALE_0_1 1

static Display *x_display = NULL;
static Atom s_wmDeleteMessage;

static GLuint programObject;

static uint32_t width = 1280;
static uint32_t height = 720;

enum FragmentColorType
{
    ST                  = 0,
    POSITION            = 1,
    TANGENT             = 2,
    BITANGENT           = 3,
    NORMAL              = 4,
    UV_COORD            = 5,
    TANGENT_REMAP       = 6,
    BITANGENT_REMAP     = 7,
    UV_COORD_L          = 8,
    TANGENT_REMAP_L     = 9,
    BITANGENT_REMAP_L   = 10,
    TYPE_COUNT          = 11,
};

const char FragmentColorHotkey[TYPE_COUNT] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a'};

const char* FragmentColorString[TYPE_COUNT] =
{
    "st",
    "position",
    "unmapped tangent",
    "unmapped bitangent",
    "normal",
    "bicubic interpolated uv",
    "bicubic mapped tangent",
    "bicubic mapped bitangent",
    "linear interpolated uv",
    "linear mapped tangent",
    "linear mapped bitangent"
};

FragmentColorType g_fragColorType = TANGENT_REMAP;
bool g_fragColorNormalized = false;
bool g_disturbPosition = true;
bool g_disturbUV = true;
bool g_linearContinousInOneDirection = true;
int g_tessLevel = 8;
float g_tangentScale = 0.25f;

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
    XEvent xev;
    KeySym key;
    GLboolean userinterrupt = GL_FALSE;
    char text;

    while ( XPending ( x_display ) )
    {
        XEvent xev;
        XNextEvent( x_display, &xev );
        if ( xev.type == KeyPress )
        {
            if (XLookupString(&xev.xkey,&text,1,&key,0)==1)
            {
                auto iter = std::find(&FragmentColorHotkey[0], &FragmentColorHotkey[0] + TYPE_COUNT, key);
                if (iter != &FragmentColorHotkey[0] + TYPE_COUNT)
                {
                    g_fragColorType = static_cast<FragmentColorType>(iter - &FragmentColorHotkey[0]);
                    printf ("fragment color: %s\n", FragmentColorString[static_cast<int>(g_fragColorType)]);
                }
                else if (key == 'n')
                {
                    g_fragColorNormalized = !g_fragColorNormalized;
                    printf ("fragment color normalized: %s\n", g_fragColorNormalized ? "true" : "false");
                }
                else if (key == 'p')
                {
                    g_disturbPosition = !g_disturbPosition;
                    printf ("disturb position: %s\n", g_disturbPosition ? "true" : "false");
                }
                else if (key == 'u')
                {
                    g_disturbUV = !g_disturbUV;
                    printf ("disturb uv: %s\n", g_disturbPosition ? "true" : "false");
                }
                else if (key == 'c')
                {
                    g_linearContinousInOneDirection = !g_linearContinousInOneDirection;
                    printf ("linear uv continous in one direction: %s\n", g_linearContinousInOneDirection ? "true" : "false");
                }
                else if (key == '=')
                {
                    g_tessLevel++;
                    g_tessLevel = std::min(g_tessLevel, 64);
                    printf ("tess level: %d\n", g_tessLevel);
                }
                else if (key == '-')
                {
                    g_tessLevel--;
                    g_tessLevel = std::max(1, g_tessLevel);
                    printf ("tess level: %d\n", g_tessLevel);
                }
                else if (key == '.')
                {
                    g_tangentScale *= 2.0f;
                    printf ("tangent scale: %g\n", g_tangentScale);
                }
                else if (key == ',')
                {
                    g_tangentScale *= 0.5f;
                    printf ("tangent scale: %g\n", g_tangentScale);
                }
                else if (key == 'h')
                {
                    printf ("hotkeys:\n\t"
                            "\'0~a\':\tdifferent attribute shown as fragment color; (default, bicubic mapped tangent);\n\t"
                            "\'n\':\ttoggle normalized fragment color within [0-1]; (default, false);\n\t"
                            "\'p\':\ttoggle disturb position; (default, true);\n\t"
                            "\'u\':\ttoggle disturb uv; (default, true);\n\t"
                            "\'c\':\ttoggle make linear uv continous in one direction; (default, true)\n\t"
                            "\'=/-\':\tincrement/decrement tessellation level; (default, 8);\n\t"
                            "\'./,\':\tincrement/decrement tangent scale factor; (default, 0.25);\n\t");
                }
            }
        }
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

void Draw ()
{
    float disturb = 0.2f;
    float scale = 2.0f;
    int pattern = 1;
    GLfloat vVertices[] = { -0.4f,0.4f,0.0f, -0.2f,0.4f,0.0f, 0.0f,0.4f,0.0f, 0.2f,0.4f,0.0f, 0.4f,0.4f,0.0f,
                            -0.4f,0.2f,0.0f, -0.2f,0.2f,0.0f, 0.0f,0.2f,0.0f, 0.2f,0.2f,0.0f, 0.4f,0.2f,0.0f,
                            -0.4f,0.0f,0.0f, -0.2f,0.0f,0.0f, 0.0f,0.0f,0.0f, 0.2f,0.0f,0.0f, 0.4f,0.0f,0.0f,
                            -0.4f,-0.2f,0.0f, -0.2f,-0.2f,0.0f, 0.0f,-0.2f,0.0f, 0.2f,-0.2f,0.0f, 0.4f,-0.2f,0.0f,
                            -0.4f,-0.4f,0.0f, -0.2f,-0.4f,0.0f, 0.0f,-0.4f,0.0f, 0.2f,-0.4f,0.0f, 0.4f,-0.4f,0.0f
                          };
    for (uint32_t i = 0; i < sizeof(vVertices)/sizeof(GLfloat); ++i)
    {
        vVertices[i] *= scale;
    }
    if (g_disturbPosition)
    {
        for (uint32_t i = 0; i < 5; ++i)
        {
            if (pattern == 0)
            {
                vVertices[(i * 5 + 0) * 3 + 1] += disturb * scale;
                vVertices[(i * 5 + 1) * 3 + 1] -= disturb * scale;
                vVertices[(i * 5 + 2) * 3 + 1] += disturb * scale;
                vVertices[(i * 5 + 3) * 3 + 1] -= disturb * scale;
                vVertices[(i * 5 + 4) * 3 + 1] += disturb * scale;
            }
            else if (pattern == 1)
            {
                vVertices[(i * 5 + 0) * 3 + 1] += disturb * scale;
                vVertices[(i * 5 + 1) * 3 + 1] -= disturb * scale;

                vVertices[(i * 5 + 3) * 3 + 1] += disturb * scale;
                vVertices[(i * 5 + 4) * 3 + 1] -= disturb * scale;
            }
        }
    }

    GLuint indices[] = {
        0,5,6, 1,6,7, 2,7,8, 3,8,9,
        0,6,1, 1,7,2, 2,8,3, 3,9,4,
        5,10,11, 6,11,12, 7,12,13, 8,13,14,
        5,11,6, 6,12,7, 7,13,8, 8,14,9,
        10,15,16, 11,16,17, 12,17,18, 13,18,19,
        10,16,11, 11,17,12, 12,18,13, 13,19,14,
        15,20,21, 16,21,22, 17,22,23, 18,23,24,
        15,21,16, 16,22,17, 17,23,18, 18,24,19
    };
    GLuint indices_half[] = {
        0,5,6, 1,6,7, 2,7,8, 3,8,9,
        5,10,11, 6,11,12, 7,12,13, 8,13,14,
        10,15,16, 11,16,17, 12,17,18, 13,18,19,
        15,20,21, 16,21,22, 17,22,23, 18,23,24
    };
    GLuint patches[] = {
        0,1,2,3,
        5,6,7,8,
        10,11,12,13,
        15,16,17,18,

        1,2,3,4,
        6,7,8,9,
        11,12,13,14,
        16,17,18,19,

        5,6,7,8,
        10,11,12,13,
        15,16,17,18,
        20,21,22,23,

        6,7,8,9,
        11,12,13,14,
        16,17,18,19,
        21,22,23,24
    };
    float uv[] = {
        0.0f,0.0f, 0.25f,0.0f, 0.5f,0.0f, 0.75f,0.0f, 1.0f,0.0f,
        0.0f,0.25f, 0.25f,0.25f, 0.5f,0.25f, 0.75f,0.25f, 1.0f,0.25f,
        0.0f,0.5f, 0.25f,0.5f, 0.5f,0.5f, 0.75f,0.5f, 1.0f,0.5f,
        0.0f,0.75f, 0.25f,0.75f, 0.5f,0.75f, 0.75f,0.75f, 1.0f,0.75f,
        0.0f,1.0f, 0.25f,1.0f, 0.5f,1.0f, 0.75f,1.0f, 1.0f,1.0f,
    };
    if (g_disturbUV)
    {
        uv[11 * 2 + 1] -= 0.1f;
        uv[12 * 2 + 1] += 0.1f;
        uv[13 * 2 + 1] -= 0.1f;

        if (g_linearContinousInOneDirection)
        {
            uv[6 * 2 + 1] -= 0.1f;
            uv[16 * 2 + 1] -= 0.1f;

            uv[7 * 2 + 1] += 0.1f;
            uv[17 * 2 + 1] += 0.1f;

            uv[8 * 2 + 1] -= 0.1f;
            uv[18 * 2 + 1] -= 0.1f;
        }
    }
    bool uv_scale = true;
    float uv_scale_factor = 1.0f;
    if (uv_scale)
    {
        for (unsigned int i = 0; i < sizeof(uv)/sizeof(float); ++i)
        {
            uv[i] *= uv_scale_factor;
        }
    }
    float patch_uv_linear[32];
    for (unsigned int p = 0; p < 4; ++p)
    {
        for (unsigned int c = 0; c < 4; ++c)
        {
            static unsigned int table[] = {5, 6, 9, 10};

            unsigned int vid = 16 * p + table[c];
            unsigned int id = 4 * p + c;
            patch_uv_linear[id * 2] = uv[patches[vid] * 2];
            patch_uv_linear[id * 2 + 1] = uv[patches[vid] * 2 + 1];
        }
    }
    float patch_uv_bicubic[128];
    for (unsigned int p = 0; p < 4; ++p)
    {
        for (unsigned int c = 0; c < 16; ++c)
        {
            unsigned int vid = 16 * p + c;
            patch_uv_bicubic[vid * 2] = uv[patches[vid] * 2];
            patch_uv_bicubic[vid * 2 + 1] = uv[patches[vid] * 2 + 1];
        }
    }
    glUniform2fv(glGetUniformLocation(programObject, "patch_uv_linear"), 16, patch_uv_linear);
    glUniform2fv(glGetUniformLocation(programObject, "patch_uv_bicubic"), 64, patch_uv_bicubic);
    glUniform1i(glGetUniformLocation(programObject, "fragColorType"), static_cast<int>(g_fragColorType));
    glUniform1i(glGetUniformLocation(programObject, "fragColorNormalized"), static_cast<int>(g_fragColorNormalized));
    glUniform1f(glGetUniformLocation(programObject, "level"), g_tessLevel);
    glUniform1f(glGetUniformLocation(programObject, "scale"), g_tangentScale);

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(programObject);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
    glEnableVertexAttribArray(0);

    glPatchParameteri(GL_PATCH_VERTICES, 16);

    glDrawElements(GL_PATCHES, sizeof(patches)/sizeof(GLuint), GL_UNSIGNED_INT, patches);
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

    if (!WinCreate("special", width, height, eglNativeDisplay, eglNativeWindow))
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

bool Init()
{
    char vShaderStr[] =
        "#version 310 es                                    \n"
        "layout(location = 0) in vec4 vPosition;            \n"
        "void main()                                        \n"
        "{                                                  \n"
        "   gl_Position = vPosition;                        \n"
        "}                                                  \n";

    char fShaderStr[] =
        "#version 310 es                                                        \n"
        "precision mediump float;                                               \n"
        "uniform int fragColorType;                                             \n"
        "uniform bool fragColorNormalized;                                      \n"
        "uniform float scale;                                                   \n"
        "in vec2 st;                                                            \n"
        "in vec3 position;                                                      \n"
        "in vec3 tangent;                                                       \n"
        "in vec3 bitangent;                                                     \n"
        "in vec3 normal;                                                        \n"
        "in vec2 uv_coord;                                                      \n"
        "in vec3 tangent_remap;                                                 \n"
        "in vec3 bitangent_remap;                                               \n"
        "in vec2 uv_coord_l;                                                    \n"
        "in vec3 tangent_remap_l;                                               \n"
        "in vec3 bitangent_remap_l;                                             \n"
        "out vec4 fragColor;                                                    \n"
        "vec3 map(in vec3 v)                                                    \n"
        "{                                                                      \n"
#if SCALE_0_1
        "   return 0.5f*(normalize(v) + vec3(1.0f));                            \n"
#endif
        "   return normalize(v);                                                \n"
        "}                                                                      \n"
        "void main()                                                            \n"
        "{                                                                      \n"
        "   switch (fragColorType)                                              \n"
        "   {                                                                   \n"
        "       case 0:                                                         \n"
        "           fragColor = vec4(st, 0.0, 1.0);                             \n"
        "           break;                                                      \n"
        "       case 1:                                                         \n"
        "           if (fragColorNormalized)                                    \n"
        "           {                                                           \n"
        "               fragColor = vec4(map(position), 1.0);                   \n"
        "           }                                                           \n"
        "           else                                                        \n"
        "           {                                                           \n"
        "               fragColor = vec4(position, 1.0);                        \n"
        "           }                                                           \n"
        "           break;                                                      \n"
        "       case 2:                                                         \n"
        "           if (fragColorNormalized)                                    \n"
        "           {                                                           \n"
        "               fragColor = vec4(map(tangent), 1.0);                    \n"
        "           }                                                           \n"
        "           else                                                        \n"
        "           {                                                           \n"
        "               fragColor = vec4(tangent, 1.0);                         \n"
        "           }                                                           \n"
        "           break;                                                      \n"
        "       case 3:                                                         \n"
        "           if (fragColorNormalized)                                    \n"
        "           {                                                           \n"
        "               fragColor = vec4(map(bitangent), 1.0);                  \n"
        "           }                                                           \n"
        "           else                                                        \n"
        "           {                                                           \n"
        "               fragColor = vec4(bitangent, 1.0);                       \n"
        "           }                                                           \n"
        "           break;                                                      \n"
        "       case 4:                                                         \n"
        "           fragColor = vec4(normal, 1.0);                              \n"
        "           break;                                                      \n"
        "       case 5:                                                         \n"
        "           fragColor = vec4(uv_coord, 0.0, 1.0);                       \n"
        "           break;                                                      \n"
        "       case 6:                                                         \n"
        "           if (fragColorNormalized)                                    \n"
        "           {                                                           \n"
        "               fragColor = vec4(map(tangent_remap), 1.0);              \n"
        "           }                                                           \n"
        "           else                                                        \n"
        "           {                                                           \n"
        "               fragColor = vec4(tangent_remap * scale, 1.0);           \n"
        "           }                                                           \n"
        "           break;                                                      \n"
        "       case 7:                                                         \n"
        "           if (fragColorNormalized)                                    \n"
        "           {                                                           \n"
        "               fragColor = vec4(map(bitangent_remap), 1.0);            \n"
        "           }                                                           \n"
        "           else                                                        \n"
        "           {                                                           \n"
        "               fragColor = vec4(bitangent_remap * scale, 1.0);         \n"
        "           }                                                           \n"
        "           break;                                                      \n"
        "       case 8:                                                         \n"
        "           fragColor = vec4(uv_coord_l, 0.0, 1.0);                     \n"
        "           break;                                                      \n"
        "       case 9:                                                         \n"
        "           if (fragColorNormalized)                                    \n"
        "           {                                                           \n"
        "               fragColor = vec4(map(tangent_remap_l), 1.0);            \n"
        "           }                                                           \n"
        "           else                                                        \n"
        "           {                                                           \n"
        "               fragColor = vec4(tangent_remap_l * scale, 1.0);         \n"
        "           }                                                           \n"
        "           break;                                                      \n"
        "       case 10:                                                        \n"
        "           if (fragColorNormalized)                                    \n"
        "           {                                                           \n"
        "               fragColor = vec4(map(bitangent_remap_l), 1.0);          \n"
        "           }                                                           \n"
        "           else                                                        \n"
        "           {                                                           \n"
        "               fragColor = vec4(bitangent_remap_l * scale, 1.0);       \n"
        "           }                                                           \n"
        "           break;                                                      \n"
        "       default:                                                        \n"
        "           if (fragColorNormalized)                                    \n"
        "           {                                                           \n"
        "               fragColor = vec4(map(tangent_remap), 1.0);              \n"
        "           }                                                           \n"
        "           else                                                        \n"
        "           {                                                           \n"
        "               fragColor = vec4(tangent_remap * scale, 1.0);           \n"
        "           }                                                           \n"
        "   }                                                                   \n"
        "}                                                                      \n";

    char tcShaderStr[] =
        "#version 310 es                                                                \n"
        "#extension GL_EXT_tessellation_shader : require                                \n"
        "precision mediump float;                                                       \n"
        "uniform float level;                                                           \n"
        "layout(vertices = 16) out;                                                     \n"
        "void main()                                                                    \n"
        "{                                                                              \n"
        "   gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;   \n"
        "   gl_TessLevelOuter[0] = level;                                               \n"
        "   gl_TessLevelOuter[1] = level;                                               \n"
        "   gl_TessLevelOuter[2] = level;                                               \n"
        "   gl_TessLevelOuter[3] = level;                                               \n"
        "   gl_TessLevelInner[0] = level;                                               \n"
        "   gl_TessLevelInner[1] = level;                                               \n"
        "}                                                                              \n";

    char teShaderStr[] =
        "#version 310 es                                                                \n"
        "#extension GL_EXT_tessellation_shader : require                                \n"
        "precision mediump float;                                                       \n"
        "layout(quads, equal_spacing, ccw) in;                                          \n"
        "uniform vec2 patch_uv_linear[16];                                              \n"
        "uniform vec2 patch_uv_bicubic[64];                                             \n"
        "out vec2 st;                                                                   \n"
        "out vec3 position;                                                             \n"
        "out vec3 tangent;                                                              \n"
        "out vec3 bitangent;                                                            \n"
        "out vec3 normal;                                                               \n"
        "out vec2 uv_coord;                                                             \n"
        "out vec3 tangent_remap;                                                        \n"
        "out vec3 bitangent_remap;                                                      \n"
        "out vec2 uv_coord_l;                                                           \n"
        "out vec3 tangent_remap_l;                                                      \n"
        "out vec3 bitangent_remap_l;                                                    \n"
        "void cubicBSplineCoefficient(float u, out float b[4])                          \n"
        "{                                                                              \n"
        "   float u2 = u * u;                                                           \n"
        "   float u3 = u * u2;                                                          \n"
        "   float s = 1.0f / 6.0f;                                                      \n"
        "   b[0] = (1.0f - 3.0f*u + 3.0f*u2 - u3) * s;                                  \n"
        "   b[1] = (4.0f - 6.0f*u2 + 3.0f*u3) * s;                                      \n"
        "   b[2] = (1.0f + 3.0f*u + 3.0f*u2 - 3.0f*u3) * s;                             \n"
        "   b[3] = u3 * s;                                                              \n"
        "}                                                                              \n"
        "vec4 cubicBSpline4d(float u, in vec4 p[4])                                     \n"
        "{                                                                              \n"
        "   float b[4];                                                                 \n"
        "   cubicBSplineCoefficient(u, b);                                              \n"
        "   return p[0]*b[0] + p[1]*b[1] + p[2]*b[2] + p[3]*b[3];                       \n"
        "}                                                                              \n"
        "vec2 cubicBSpline2d(float u, in vec2 p[4])                                     \n"
        "{                                                                              \n"
        "   float b[4];                                                                 \n"
        "   cubicBSplineCoefficient(u, b);                                              \n"
        "   return p[0]*b[0] + p[1]*b[1] + p[2]*b[2] + p[3]*b[3];                       \n"
        "}                                                                              \n"
        "void cubicBSplineDerivativeCoefficient(float u, out float b[4])                \n"
        "{                                                                              \n"
        "   float u2 = u * u;                                                           \n"
        "   float s = 0.5f;                                                             \n"
        "   b[0] = (-1.0f + 2.0f*u - u2) * s;                                           \n"
        "   b[1] = (-4.0f*u + 3.0f*u2) * s;                                             \n"
        "   b[2] = (1.0f + 2.0f*u - 3.0f*u2) * s;                                       \n"
        "   b[3] = u2 * s;                                                              \n"
        "}                                                                              \n"
        "vec4 cubicBSplineDerivative4d(float u, in vec4 p[4])                           \n"
        "{                                                                              \n"
        "   float b[4];                                                                 \n"
        "   cubicBSplineDerivativeCoefficient(u, b);                                    \n"
        "   return p[0]*b[0] + p[1]*b[1] + p[2]*b[2] + p[3]*b[3];                       \n"
        "}                                                                              \n"
        "vec2 cubicBSplineDerivative2d(float u, in vec2 p[4])                           \n"
        "{                                                                              \n"
        "   float b[4];                                                                 \n"
        "   cubicBSplineDerivativeCoefficient(u, b);                                    \n"
        "   return p[0]*b[0] + p[1]*b[1] + p[2]*b[2] + p[3]*b[3];                       \n"
        "}                                                                              \n"
        "vec2 bilinear(vec2 uv, in vec2 p[4])                                           \n"
        "{                                                                              \n"
        "   vec2 a = (1.0f-uv.x)*p[0] + uv.x*p[1];                                      \n"
        "   vec2 b = (1.0f-uv.x)*p[2] + uv.x*p[3];                                      \n"
        "   return (1.0f-uv.y)*a + uv.y*b;                                              \n"
        "}                                                                              \n"
        "void bilinearDerivative(vec2 uv, in vec2 p[4], out vec2 du, out vec2 dv)       \n"
        "{                                                                              \n"
        "   vec2 a = (1.0f-uv.x)*p[0] + uv.x*p[1];                                      \n"
        "   vec2 b = (1.0f-uv.x)*p[2] + uv.x*p[3];                                      \n"
        "   dv = -a + b;                                                                \n"
        "   vec2 da = -p[0] + p[1];                                                     \n"
        "   vec2 db = -p[2] + p[3];                                                     \n"
        "   du = (1.0f-uv.y)*da + uv.y*db;                                              \n"
        "}                                                                              \n"
        "void remap(in vec3 dpds, in vec3 dpdt, out vec3 dpdu, out vec3 dpdv,           \n"
        "           in vec2 duvds, in vec2 duvdt)                                       \n"
        "{                                                                              \n"
        "   float invd = 1.0f / (duvds.x * duvdt.y - duvds.y * duvdt.x);                \n"
        "   dpdu = (duvdt.y * dpds - duvds.y * dpdt) * invd;                            \n"
        "   dpdv = (-duvdt.x * dpds + duvds.x * dpdt) * invd;                           \n"
        "}                                                                              \n"
        "void main()                                                                    \n"
        "{                                                                              \n"
        "   vec4 cp[4];                                                                 \n"
        "   vec4 p[4];                                                                  \n"
        "   vec4 ds[4];                                                                 \n"
        "   cp[0] = gl_in[0].gl_Position;                                               \n"
        "   cp[1] = gl_in[1].gl_Position;                                               \n"
        "   cp[2] = gl_in[2].gl_Position;                                               \n"
        "   cp[3] = gl_in[3].gl_Position;                                               \n"
        "   p[0] = cubicBSpline4d(gl_TessCoord[0], cp);                                 \n"
        "   ds[0] = cubicBSplineDerivative4d(gl_TessCoord[0], cp);                      \n"
        "   cp[0] = gl_in[4].gl_Position;                                               \n"
        "   cp[1] = gl_in[5].gl_Position;                                               \n"
        "   cp[2] = gl_in[6].gl_Position;                                               \n"
        "   cp[3] = gl_in[7].gl_Position;                                               \n"
        "   p[1] = cubicBSpline4d(gl_TessCoord[0], cp);                                 \n"
        "   ds[1] = cubicBSplineDerivative4d(gl_TessCoord[0], cp);                      \n"
        "   cp[0] = gl_in[8].gl_Position;                                               \n"
        "   cp[1] = gl_in[9].gl_Position;                                               \n"
        "   cp[2] = gl_in[10].gl_Position;                                              \n"
        "   cp[3] = gl_in[11].gl_Position;                                              \n"
        "   p[2] = cubicBSpline4d(gl_TessCoord[0], cp);                                 \n"
        "   ds[2] = cubicBSplineDerivative4d(gl_TessCoord[0], cp);                      \n"
        "   cp[0] = gl_in[12].gl_Position;                                              \n"
        "   cp[1] = gl_in[13].gl_Position;                                              \n"
        "   cp[2] = gl_in[14].gl_Position;                                              \n"
        "   cp[3] = gl_in[15].gl_Position;                                              \n"
        "   p[3] = cubicBSpline4d(gl_TessCoord[0], cp);                                 \n"
        "   ds[3] = cubicBSplineDerivative4d(gl_TessCoord[0], cp);                      \n"
        "   st = gl_TessCoord.xy;                                                       \n"
        "   position = cubicBSpline4d(gl_TessCoord[1], p).xyz;                          \n"
        "   tangent = cubicBSpline4d(gl_TessCoord[1], ds).xyz;                          \n"
        "   bitangent = -cubicBSplineDerivative4d(gl_TessCoord[1], p).xyz;              \n"
        "   normal = normalize(cross(tangent, bitangent));                              \n"
        "   gl_Position = vec4(position, 1.0f);                                         \n"
        "   vec2 cuv[4];                                                                \n"
        "   vec2 uv[4];                                                                 \n"
        "   vec2 du[4];                                                                 \n"
        "   int offset = gl_PrimitiveID * 16;                                           \n"
        "   cuv[0] = patch_uv_bicubic[offset];                                          \n"
        "   cuv[1] = patch_uv_bicubic[offset + 1];                                      \n"
        "   cuv[2] = patch_uv_bicubic[offset + 2];                                      \n"
        "   cuv[3] = patch_uv_bicubic[offset + 3];                                      \n"
        "   uv[0] = cubicBSpline2d(gl_TessCoord[0], cuv);                               \n"
        "   du[0] = cubicBSplineDerivative2d(gl_TessCoord[0], cuv);                     \n"
        "   cuv[0] = patch_uv_bicubic[offset + 4];                                      \n"
        "   cuv[1] = patch_uv_bicubic[offset + 5];                                      \n"
        "   cuv[2] = patch_uv_bicubic[offset + 6];                                      \n"
        "   cuv[3] = patch_uv_bicubic[offset + 7];                                      \n"
        "   uv[1] = cubicBSpline2d(gl_TessCoord[0], cuv);                               \n"
        "   du[1] = cubicBSplineDerivative2d(gl_TessCoord[0], cuv);                     \n"
        "   cuv[0] = patch_uv_bicubic[offset + 8];                                      \n"
        "   cuv[1] = patch_uv_bicubic[offset + 9];                                      \n"
        "   cuv[2] = patch_uv_bicubic[offset + 10];                                     \n"
        "   cuv[3] = patch_uv_bicubic[offset + 11];                                     \n"
        "   uv[2] = cubicBSpline2d(gl_TessCoord[0], cuv);                               \n"
        "   du[2] = cubicBSplineDerivative2d(gl_TessCoord[0], cuv);                     \n"
        "   cuv[0] = patch_uv_bicubic[offset + 12];                                     \n"
        "   cuv[1] = patch_uv_bicubic[offset + 13];                                     \n"
        "   cuv[2] = patch_uv_bicubic[offset + 14];                                     \n"
        "   cuv[3] = patch_uv_bicubic[offset + 15];                                     \n"
        "   uv[3] = cubicBSpline2d(gl_TessCoord[0], cuv);                               \n"
        "   du[3] = cubicBSplineDerivative2d(gl_TessCoord[0], cuv);                     \n"
        "   uv_coord = cubicBSpline2d(gl_TessCoord[1], uv);                             \n"
        "   vec2 duvds = cubicBSpline2d(gl_TessCoord[1], du);                           \n"
        "   vec2 duvdt = cubicBSplineDerivative2d(gl_TessCoord[1], uv);                 \n"
        "   remap(tangent, bitangent, tangent_remap, bitangent_remap, duvds, duvdt);    \n"
        "   vec2 cuv_l[4];                                                              \n"
        "   vec2 duvds_l;                                                               \n"
        "   vec2 duvdt_l;                                                               \n"
        "   int offset_l = gl_PrimitiveID * 4;                                          \n"
        "   cuv_l[0] = patch_uv_linear[offset_l];                                       \n"
        "   cuv_l[1] = patch_uv_linear[offset_l + 1];                                   \n"
        "   cuv_l[2] = patch_uv_linear[offset_l + 2];                                   \n"
        "   cuv_l[3] = patch_uv_linear[offset_l + 3];                                   \n"
        "   uv_coord_l = bilinear(gl_TessCoord.xy, cuv_l);                              \n"
        "   bilinearDerivative(gl_TessCoord.xy, cuv_l, duvds_l, duvdt_l);               \n"
        "   remap(tangent, bitangent, tangent_remap_l, bitangent_remap_l,               \n"
        "           duvds_l, duvdt_l);                                                  \n"
        "}                                                                              \n";

    GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, vShaderStr);
    GLuint fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fShaderStr);
#if DEBUG_SHADER_COMPILE
    printf("compile tcs\n");
#endif
    GLuint tcShader = LoadShader(GL_TESS_CONTROL_SHADER, tcShaderStr);
#if DEBUG_SHADER_COMPILE
    printf("compile tes\n");
#endif
    GLuint teShader = LoadShader(GL_TESS_EVALUATION_SHADER, teShaderStr);

    programObject = glCreateProgram();
    if (programObject == 0)
    {
        return 0;
    }

    glAttachShader(programObject, vertexShader);
#if DEBUG_SHADER_COMPILE
    printf("attach tcs\n");
#endif
    glAttachShader(programObject, tcShader);
#if DEBUG_SHADER_COMPILE
    printf("attach tes\n");
#endif
    glAttachShader(programObject, teShader);
    glAttachShader(programObject, fragmentShader);

#if DEBUG_SHADER_COMPILE
    printf("link\n");
#endif
    glLinkProgram(programObject);

    GLint linked;
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1)
        {
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);
            glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
            printf("Error linking program:\n%s\n", infoLog);
            free(infoLog);
        }

        glDeleteProgram(programObject);
        return false;
    }

    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    return true;
}

void ShutDown()
{
    glDeleteProgram(programObject);
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