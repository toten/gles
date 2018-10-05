#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

static Display *x_display = NULL;
static Atom s_wmDeleteMessage;

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
        glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

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

    if (!WinCreate("first", 320, 240, eglNativeDisplay, eglNativeWindow))
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
        return 1;  
    
    WinLoop (eglDisplay, eglSurface);

    return 0;
}