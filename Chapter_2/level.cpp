#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>

#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

static Display *x_display = NULL;
static Atom s_wmDeleteMessage;

static GLuint programObject;

static const uint32_t width = 256;
static const uint32_t height = 256;

static float g_scale = 0.5f;
static uint32_t g_base_level = 1;
static uint32_t g_max_level = 2;
enum TEXTURE_LEVEL_UPDATE
{
    BASE_LEVEL  = 0,
    MAX_LEVEL   = 1,
    INVALID_LEVEL,
};
static TEXTURE_LEVEL_UPDATE g_tex_level_update = MAX_LEVEL;

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
                if (key == '=')
                {
                    g_scale += 0.05f;
                    g_scale = std::min(g_scale, 1.0f);
                    printf ("scale: %f\n", g_scale);
                }
                else if (key == '-')
                {
                    g_scale -= 0.05f;
                    g_scale = std::max(g_scale, 0.1f);
                    printf ("scale: %f\n", g_scale);
                }
                else if (key == 'l')
                {
                    g_tex_level_update = (TEXTURE_LEVEL_UPDATE)((g_tex_level_update + 1) % INVALID_LEVEL);
                    printf ("%s level is to update\n", g_tex_level_update == BASE_LEVEL ? "base" : "max");
                }
                else if (key >='0' && key <= '9')
                {
                    if (g_tex_level_update == BASE_LEVEL)
                    {
                        g_base_level = key - '0';
                        g_max_level = std::max(g_base_level, g_max_level);
                    }
                    else
                    {
                        g_max_level = key - '0';
                        g_base_level = std::min(g_base_level, g_max_level);
                    }
                    printf ("base level = %d; max level = %d\n", g_base_level, g_max_level);
                }
                else
                {
                    printf ("hotkeys:\n\t"
                            "\'=/-\':\tincrement/decrement square size; (default, 0.5);\n\t"
                            "\'0-9\':\tchange texture base or max level value; (default, base = 0, max = 1);\n\t"
                            "\'l\':\ttoogle base/max level to update; (default, max level);\n\t");
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

GLuint tex;
void Draw ()
{
    glViewport(0, 0, width, height);

    glClear(GL_COLOR_BUFFER_BIT);

    glUniform1f(glGetUniformLocation(programObject, "scale"), g_scale);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, g_base_level);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, g_max_level);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
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

    if (!WinCreate("level", width, height, eglNativeDisplay, eglNativeWindow))
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

struct v4f
{
    v4f()
    {
    }
    v4f(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w)
    {
    }

    float x;
    float y;
    float z;
    float w;
};

struct v2f
{
    v2f()
    {
    }
    v2f(float _x, float _y) : x(_x), y(_y)
    {
    }
    float x;
    float y;
};

bool Init()
{
    char vShaderStr[] =
        "#version 310 es                                    \n"
        "layout(location = 0) in vec4 vPosition;            \n"
        "layout(location = 1) in vec2 vTexCoord;            \n"
        "out vec2 varTexCoord;                              \n"
        "uniform float scale;                               \n"
        "void main()                                        \n"
        "{                                                  \n"
        "     gl_Position = vPosition;                      \n"
        "     gl_Position.xy *= scale;                      \n"
        "     varTexCoord = vTexCoord;                      \n"
        "}                                                  \n";

    char fShaderStr[] =
        "#version 310 es                                                        \n"
        "precision highp float;                                                 \n"
        "in vec2 varTexCoord;                                                   \n"        
        "out vec4 fragColor;                                                    \n"
        "uniform sampler2D sampler2d;                                           \n"
        "void main()                                                            \n"
        "{                                                                      \n"
        "    fragColor = texture(sampler2d, varTexCoord);                       \n"
        "}                                                                      \n";

    GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, vShaderStr);
    GLuint fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fShaderStr);

    programObject = glCreateProgram();
    if (programObject == 0)
    {
        return 0;
    }

    glAttachShader(programObject, vertexShader);
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
    glUseProgram(programObject);

    v4f tri_data[] = {
        {-1.0f, -1.0f, 0.5f, 1.0f},
        { 1.0f, -1.0f, 0.5f, 1.0f},
        { 1.0f,  1.0f, 0.5f, 1.0f},
        {-1.0f,  1.0f, 0.5f, 1.0f}
    };

    v2f tex_coord_data[] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };

    uint32_t index_data[] = {
        0, 1, 2,
        0, 2, 3
    };

    GLuint tri_buf;
    glGenBuffers(1, &tri_buf);
    glBindBuffer(GL_ARRAY_BUFFER, tri_buf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri_data), tri_data, GL_STATIC_DRAW);

    GLuint pos_location = glGetAttribLocation(programObject, "vPosition");
    glEnableVertexAttribArray(pos_location);
    glVertexAttribPointer(pos_location, 4, GL_FLOAT, GL_FALSE, 0, 0);

    GLuint tex_coord_buf;
    glGenBuffers(1, &tex_coord_buf);
    glBindBuffer(GL_ARRAY_BUFFER, tex_coord_buf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord_data), tex_coord_data, GL_STATIC_DRAW);

    GLuint texcoord_location = glGetAttribLocation(programObject, "vTexCoord");
    glEnableVertexAttribArray(texcoord_location);
    glVertexAttribPointer(texcoord_location, 2, GL_FLOAT, GL_FALSE, 0, 0);

    GLuint index_buf;
    glGenBuffers(1, &index_buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_data), index_data, GL_STATIC_DRAW);

    const uint32_t tex_width_l0 = width;
    const uint32_t tex_height_l0 = height;
    GLuint tex_data_l0[tex_width_l0 * tex_height_l0];
    for(auto& pixel : tex_data_l0)
    {
        pixel = 0xFF0000FF;
    }

    const uint32_t tex_width_l1 = tex_width_l0 / 2;
    const uint32_t tex_height_l1 = tex_height_l0 / 2;
    GLuint tex_data_l1[tex_width_l1 * tex_height_l1];
    for(auto& pixel : tex_data_l1)
    {
        pixel = 0xFF00FF00;
    }

    const uint32_t tex_width_l2 = tex_width_l1 / 2;
    const uint32_t tex_height_l2 = tex_height_l1 / 2;
    GLuint tex_data_l2[tex_width_l2 * tex_height_l2];
    for(auto& pixel : tex_data_l2)
    {
        pixel = 0xFFFF0000;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width_l0, tex_height_l0, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data_l0);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, tex_width_l1, tex_height_l1, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data_l1);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, tex_width_l2, tex_height_l2, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data_l2);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLuint tex_location = glGetUniformLocation(programObject, "sampler2d");
    glUniform1i(tex_location, 0);
    glActiveTexture(GL_TEXTURE0);

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