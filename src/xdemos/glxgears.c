/*
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This is a port of the infamous "gears" demo to straight GLX (i.e. no GLUT)
 * Port by Brian Paul  23 March 2001
 *
 * See usage() below for command line options.
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/keysym.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GLX_MESA_swap_control
#define GLX_MESA_swap_control 1
typedef int (*PFNGLXGETSWAPINTERVALMESAPROC)(void);
#endif

#define BENCHMARK

#ifdef BENCHMARK

/* XXX this probably isn't very portable */

#include <sys/time.h>
#include <unistd.h>

/* return current time (in seconds) */
static double current_time(void) {
  struct timeval tv;
#ifdef __VMS
  (void)gettimeofday(&tv, NULL);
#else
  struct timezone tz;
  (void)gettimeofday(&tv, &tz);
#endif
  return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
}

#else /*BENCHMARK*/

/* dummy */
static double current_time(void) {
  /* update this function for other platforms! */
  static double t = 0.0;
  static int warn = 1;
  if (warn) {
    fprintf(stderr, "Warning: current_time() not implemented!!\n");
    warn = 0;
  }
  return t += 1.0;
}

#endif /*BENCHMARK*/

#ifndef M_PI
#define M_PI 3.14159265
#endif

/** Event handler results: */
#define NOP 0
#define EXIT 1
#define DRAW 2

static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static GLboolean fullscreen = GL_FALSE; /* Create a single fullscreen window */
static GLboolean stereo = GL_FALSE;     /* Enable stereo.  */
static GLint samples = 0;           /* Choose visual with at least N samples. */
static GLboolean animate = GL_TRUE; /* Animation */
static GLfloat eyesep = 5.0;        /* Eye separation. */
static GLfloat fix_point = 40.0;    /* Fixation point distance.  */
static GLfloat left, right, asp;    /* Stereo frustum params.  */

static XRenderPictFormat *pict_format;
static GLXFBConfig *fbconfigs, fbconfig;
static int numfbconfigs;
static int VisData[] = {GLX_RENDER_TYPE,
                        GLX_RGBA_BIT,
                        GLX_DRAWABLE_TYPE,
                        GLX_WINDOW_BIT,
                        GLX_DOUBLEBUFFER,
                        True,
                        GLX_RED_SIZE,
                        8,
                        GLX_GREEN_SIZE,
                        8,
                        GLX_BLUE_SIZE,
                        8,
                        GLX_ALPHA_SIZE,
                        8,
                        GLX_DEPTH_SIZE,
                        16,
                        None};

/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 *
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */
static void gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
                 GLint teeth, GLfloat tooth_depth) {
  GLint i;
  GLfloat r0, r1, r2;
  GLfloat angle, da;
  GLfloat u, v, len;

  r0 = inner_radius;
  r1 = outer_radius - tooth_depth / 2.0;
  r2 = outer_radius + tooth_depth / 2.0;

  da = 2.0 * M_PI / teeth / 4.0;

  glShadeModel(GL_FLAT);

  glNormal3f(0.0, 0.0, 1.0);

  /* draw front face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    if (i < teeth) {
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 width * 0.5);
    }
  }
  glEnd();

  /* draw front sides of teeth */
  glBegin(GL_QUADS);
  da = 2.0 * M_PI / teeth / 4.0;
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;

    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
  }
  glEnd();

  glNormal3f(0.0, 0.0, -1.0);

  /* draw back face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    if (i < teeth) {
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    }
  }
  glEnd();

  /* draw back sides of teeth */
  glBegin(GL_QUADS);
  da = 2.0 * M_PI / teeth / 4.0;
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;

    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
               -width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
               -width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
  }
  glEnd();

  /* draw outward faces of teeth */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;

    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    u = r2 * cos(angle + da) - r1 * cos(angle);
    v = r2 * sin(angle + da) - r1 * sin(angle);
    len = sqrt(u * u + v * v);
    u /= len;
    v /= len;
    glNormal3f(v, -u, 0.0);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
    glNormal3f(cos(angle), sin(angle), 0.0);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
               -width * 0.5);
    u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
    v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
    glNormal3f(v, -u, 0.0);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
               -width * 0.5);
    glNormal3f(cos(angle), sin(angle), 0.0);
  }

  glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
  glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

  glEnd();

  glShadeModel(GL_SMOOTH);

  /* draw inside radius cylinder */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glNormal3f(-cos(angle), -sin(angle), 0.0);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
  }
  glEnd();
}

static void draw(void) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glRotatef(view_rotx, 1.0, 0.0, 0.0);
  glRotatef(view_roty, 0.0, 1.0, 0.0);
  glRotatef(view_rotz, 0.0, 0.0, 1.0);

  glPushMatrix();
  glTranslatef(-3.0, -2.0, 0.0);
  glRotatef(angle, 0.0, 0.0, 1.0);
  glCallList(gear1);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(3.1, -2.0, 0.0);
  glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
  glCallList(gear2);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-3.1, 4.2, 0.0);
  glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
  glCallList(gear3);
  glPopMatrix();

  glPopMatrix();
}

static void draw_gears(void) {
  if (stereo) {
    /* First left eye.  */
    glDrawBuffer(GL_BACK_LEFT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(left, right, -asp, asp, 5.0, 60.0);

    glMatrixMode(GL_MODELVIEW);

    glPushMatrix();
    glTranslated(+0.5 * eyesep, 0.0, 0.0);
    draw();
    glPopMatrix();

    /* Then right eye.  */
    glDrawBuffer(GL_BACK_RIGHT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-right, -left, -asp, asp, 5.0, 60.0);

    glMatrixMode(GL_MODELVIEW);

    glPushMatrix();
    glTranslated(-0.5 * eyesep, 0.0, 0.0);
    draw();
    glPopMatrix();
  } else {
    draw();
  }
}

/** Draw single frame, do SwapBuffers, compute FPS */
static void draw_frame(Display *dpy, Window win) {
  static int frames = 0;
  static double tRot0 = -1.0, tRate0 = -1.0;
  double dt, t = current_time();

  if (tRot0 < 0.0)
    tRot0 = t;
  dt = t - tRot0;
  tRot0 = t;

  if (animate) {
    /* advance rotation for next frame */
    angle += 70.0 * dt; /* 70 degrees per second */
    if (angle > 3600.0)
      angle -= 3600.0;
  }

  draw_gears();
  glXSwapBuffers(dpy, win);

  frames++;

  if (tRate0 < 0.0)
    tRate0 = t;
  if (t - tRate0 >= 5.0) {
    GLfloat seconds = t - tRate0;
    GLfloat fps = frames / seconds;
    printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds, fps);
    fflush(stdout);
    tRate0 = t;
    frames = 0;
  }
}

/* new window size or exposure */
static void reshape(int width, int height) {
  glViewport(0, 0, (GLint)width, (GLint)height);

  if (stereo) {
    GLfloat w;

    asp = (GLfloat)height / (GLfloat)width;
    w = fix_point * (1.0 / 5.0);

    left = -5.0 * ((w - 0.5 * eyesep) / fix_point);
    right = 5.0 * ((w + 0.5 * eyesep) / fix_point);
  } else {
    GLfloat h = (GLfloat)height / (GLfloat)width;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
  }

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);
}

static void init(GLfloat red[4], GLfloat green[4], GLfloat blue[4],
                 GLfloat bg[4]) {
  static GLfloat pos[4] = {5.0, 5.0, 10.0, 0.0};

  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);

  /* make the gears */
  gear1 = glGenLists(1);
  glNewList(gear1, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
  gear(1.0, 4.0, 1.0, 20, 0.7);
  glEndList();

  gear2 = glGenLists(1);
  glNewList(gear2, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
  gear(0.5, 2.0, 2.0, 10, 0.7);
  glEndList();

  gear3 = glGenLists(1);
  glNewList(gear3, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
  gear(1.3, 2.0, 0.5, 10, 0.7);
  glEndList();

  // glClearColor(0, 0, 0, bg[3]);
  glClearColor(bg[0], bg[1], bg[2], bg[3]);

  glEnable(GL_NORMALIZE);
}

/**
 * Remove window border/decorations.
 */
static void no_border(Display *dpy, Window w) {
  static const unsigned MWM_HINTS_DECORATIONS = (1 << 1);
  static const int PROP_MOTIF_WM_HINTS_ELEMENTS = 5;

  typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
  } PropMotifWmHints;

  PropMotifWmHints motif_hints;
  Atom prop, proptype;
  unsigned long flags = 0;

  /* setup the property */
  motif_hints.flags = MWM_HINTS_DECORATIONS;
  motif_hints.decorations = flags;

  /* get the atom for the property */
  prop = XInternAtom(dpy, "_MOTIF_WM_HINTS", True);
  if (!prop) {
    /* something went wrong! */
    return;
  }

  /* not sure this is correct, seems to work, XA_WM_HINTS didn't work */
  proptype = prop;

  XChangeProperty(dpy, w,                        /* display, window */
                  prop, proptype,                /* property, type */
                  32,                            /* format: 32-bit datums */
                  PropModeReplace,               /* mode */
                  (unsigned char *)&motif_hints, /* data */
                  PROP_MOTIF_WM_HINTS_ELEMENTS   /* nelements */
  );
}

/*
 * Create an RGB, double-buffered window.
 * Return the window and context handles.
 */
static void make_window(Display *dpy, const char *name, int x, int y, int width,
                        int height, Window *winRet, GLXContext *ctxRet,
                        VisualID *visRet) {
  int attribs[64];
  int i = 0;

  int scrnum;
  XSetWindowAttributes attr;
  unsigned long mask;
  Window root;
  Window win;
  GLXContext ctx;
  XVisualInfo *visinfo;

  scrnum = DefaultScreen(dpy);
  root = RootWindow(dpy, scrnum);

  fbconfigs = glXChooseFBConfig(dpy, scrnum, VisData, &numfbconfigs);
  fbconfig = 0;

  for (int i = 0; i < numfbconfigs; i++) {
    visinfo = (XVisualInfo *)glXGetVisualFromFBConfig(dpy, fbconfigs[i]);
    if (!visinfo)
      continue;

    pict_format = XRenderFindVisualFormat(dpy, visinfo->visual);
    if (!pict_format)
      continue;

    fbconfig = fbconfigs[i];
    if (pict_format->direct.alphaMask > 0) {
      break;
    }
    XFree(visinfo);
    visinfo = NULL;
  }

  if (!visinfo) {
    printf("Error: couldn't get an RGB, Double-buffered");
    if (stereo)
      printf(", Stereo");
    if (samples > 0)
      printf(", Multisample");
    printf(" visual\n");
    exit(1);
  }

  /* window attributes */
  attr.background_pixel = 0;
  attr.border_pixel = 0;
  attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
  attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
  /* XXX this is a bad way to get a borderless window! */
  mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

  win = XCreateWindow(dpy, root, x, y, width, height, 0, visinfo->depth,
                      InputOutput, visinfo->visual, mask, &attr);

  if (fullscreen)
    no_border(dpy, win);

  /* set hints and properties */
  {
    XSizeHints sizehints;
    sizehints.x = x;
    sizehints.y = y;
    sizehints.width = width;
    sizehints.height = height;
    sizehints.flags = USSize | USPosition;
    XSetNormalHints(dpy, win, &sizehints);
    XSetStandardProperties(dpy, win, name, name, None, (char **)NULL, 0,
                           &sizehints);
  }

  ctx = glXCreateContext(dpy, visinfo, NULL, True);
  if (!ctx) {
    printf("Error: glXCreateContext failed\n");
    exit(1);
  }

  *winRet = win;
  *ctxRet = ctx;
  *visRet = visinfo->visualid;

  XFree(visinfo);
}


/**
 * Determine whether or not a GLX extension is supported.
 */
static int is_glx_extension_supported(Display *dpy, const char *query) {
  const int scrnum = DefaultScreen(dpy);
  const char *glx_extensions = NULL;
  const size_t len = strlen(query);
  const char *ptr;

  if (glx_extensions == NULL) {
    glx_extensions = glXQueryExtensionsString(dpy, scrnum);
  }

  ptr = strstr(glx_extensions, query);
  return ((ptr != NULL) && ((ptr[len] == ' ') || (ptr[len] == '\0')));
}

/**
 * Attempt to determine whether or not the display is synched to vblank.
 */
static void query_vsync(Display *dpy, GLXDrawable drawable) {
  int interval = 0;

#if defined(GLX_EXT_swap_control)
  if (is_glx_extension_supported(dpy, "GLX_EXT_swap_control")) {
    unsigned int tmp = -1;
    glXQueryDrawable(dpy, drawable, GLX_SWAP_INTERVAL_EXT, &tmp);
    interval = tmp;
  } else
#endif
      if (is_glx_extension_supported(dpy, "GLX_MESA_swap_control")) {
    PFNGLXGETSWAPINTERVALMESAPROC pglXGetSwapIntervalMESA =
        (PFNGLXGETSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXGetSwapIntervalMESA");

    interval = (*pglXGetSwapIntervalMESA)();
  } else if (is_glx_extension_supported(dpy, "GLX_SGI_swap_control")) {
    /* The default swap interval with this extension is 1.  Assume that it
     * is set to the default.
     *
     * Many Mesa-based drivers default to 0, but all of these drivers also
     * export GLX_MESA_swap_control.  In that case, this branch will never
     * be taken, and the correct result should be reported.
     */
    interval = 1;
  }

  if (interval > 0) {
    printf("Running synchronized to the vertical refresh.  The framerate "
           "should be\n");
    if (interval == 1) {
      printf("approximately the same as the monitor refresh rate.\n");
    } else if (interval > 1) {
      printf("approximately 1/%d the monitor refresh rate.\n", interval);
    }
  }
}

/**
 * Handle one X event.
 * \return NOP, EXIT or DRAW
 */
static int handle_event(Display *dpy, Window win, XEvent *event) {
  (void)dpy;
  (void)win;

  switch (event->type) {
  case Expose:
    return DRAW;
  case ConfigureNotify:
    reshape(event->xconfigure.width, event->xconfigure.height);
    break;
  case KeyPress: {
    char buffer[10];
    int code;
    code = XLookupKeysym(&event->xkey, 0);
    if (code == XK_Left) {
      view_roty += 5.0;
    } else if (code == XK_Right) {
      view_roty -= 5.0;
    } else if (code == XK_Up) {
      view_rotx += 5.0;
    } else if (code == XK_Down) {
      view_rotx -= 5.0;
    } else {
      XLookupString(&event->xkey, buffer, sizeof(buffer), NULL, NULL);
      if (buffer[0] == 27) {
        /* escape */
        return EXIT;
      } else if (buffer[0] == 'a' || buffer[0] == 'A') {
        animate = !animate;
      }
    }
    return DRAW;
  }
  }
  return NOP;
}

static void event_loop(Display *dpy, Window win) {
  while (1) {
    int op;
    while (!animate || XPending(dpy) > 0) {
      XEvent event;
      XNextEvent(dpy, &event);
      op = handle_event(dpy, win, &event);
      if (op == EXIT)
        return;
      else if (op == DRAW)
        break;
    }

    draw_frame(dpy, win);
  }
}

static void usage(void) {
  printf("Usage:\n");
  printf("  -display <displayname>       set the display to run on\n");
  printf("  -stereo                      run in stereo mode\n");
  printf("  -samples N                   run in multisample mode with at least "
         "N samples\n");
  printf("  -fullscreen                  run in fullscreen mode\n");
  printf("  -info                        display OpenGL renderer info\n");
  printf("  -geometry WxH+X+Y            window geometry\n");
  printf("  -col-red-gear (R,G,B)      select red gear color\n");
  printf("  -col-green-gear (R,G,B)    select green gear color\n");
  printf("  -col-blue-gear (R,G,B)     select blue gear color\n");
  printf("  -col-bg (R,G,B)    select background color\n");
}

void parseColor(char *s, GLfloat gearCol[]) {
  float r, g, b;
  float a = gearCol[3];
  if (strlen(s) == 9 || s[0] == '#') {
    // Extraire les composantes R, G, B et A
    char strR[3], strG[3], strB[3], strA[3];
    strncpy(strR, s + 1, 2);
    strncpy(strG, s + 3, 2);
    strncpy(strB, s + 5, 2);
    strncpy(strA, s + 7, 2);

    // Convertir les composantes en entiers
    r = (float)strtol(strR, NULL, 16) / 255;
    g = (float)strtol(strG, NULL, 16) / 255;
    b = (float)strtol(strB, NULL, 16) / 255;
    a = (float)strtol(strA, NULL, 16) / 255;
  } else {
    int res = sscanf(s, "(%f,%f,%f,%f)", &r, &g, &b, &a);
    if (res == 3) {
      a = gearCol[3];
    } else if (res != 4) {
      r = gearCol[0];
      g = gearCol[1];
      b = gearCol[2];
    }
  }

  int cond_r = (0.0 <= r) && (r <= 1.0);
  int cond_g = (0.0 <= g) && (g <= 1.0);
  int cond_b = (0.0 <= b) && (b <= 1.0);
  int cond_a = (0.0 <= a) && (a <= 1.0);
  if (cond_r && cond_g && cond_b) {
    gearCol[0] = r;
    gearCol[1] = g;
    gearCol[2] = b;
    gearCol[3] = a;
  } else {
    printf("Error: Format error on %s\n", s);
  }
}

char *config_path() {
  char relative_config_path[] = "/.config/glxgears/config.conf";
  char *HOME = getenv("HOME");
  if (HOME == NULL) {
    fprintf(stderr, "Couln'd find HOME envirment variable\n");
    exit(EXIT_FAILURE);
  }
  int config_path_len = strlen(HOME) + strlen(relative_config_path) + 1;
  char *config_path = (char *)malloc(config_path_len);
  if (config_path == NULL) {
    fprintf(stderr, "Error while Allocating memory\n");
    exit(EXIT_FAILURE);
  }

  strcpy(config_path, HOME);
  strcat(config_path, relative_config_path);

  return config_path;
}

void process_config(GLfloat red[], GLfloat green[], GLfloat blue[],
                    GLfloat bg[]) {
  char *path = config_path();
  FILE *fp = fopen(path, "r");
  if (fp != NULL) {
    char line[100];
    while (fgets(line, sizeof(line), fp) != NULL) {
      if (strncmp(line, "-col-red-gear", strlen("-col-red-gear")) == 0) {
        parseColor(line + strlen("-col-red-gear") + 1, red);
      } else if (strncmp(line, "-col-green-gear", strlen("-col-green-gear")) ==
                 0) {
        parseColor(line + strlen("-col-green-gear") + 1, green);
      } else if (strncmp(line, "-col-blue-gear", strlen("-col-blue-gear")) ==
                 0) {
        parseColor(line + strlen("-col-blue-gear") + 1, blue);
      } else if (strncmp(line, "-col-bg", strlen("-col-bg")) == 0) {
        parseColor(line + strlen("-col-bg") + 1, bg);
      }
    }
    fclose(fp);
  }

  free(path);
}

int main(int argc, char *argv[]) {
  unsigned int winWidth = 300, winHeight = 300;
  int x = 0, y = 0;
  Display *dpy;
  Window win;
  GLXContext ctx;
  char *dpyName = NULL;
  GLboolean printInfo = GL_FALSE;
  VisualID visId;
  int i;
  GLfloat red[4] = {0.8, 0.1, 0.0, 1.0};
  GLfloat green[4] = {0.0, 0.8, 0.2, 1.0};
  GLfloat blue[4] = {0.2, 0.2, 1.0, 1.0};
  GLfloat bg[4] = {0.0, 0.0, 0.0, 1.0};

  process_config(red, green, blue, bg);

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-display") == 0) {
      dpyName = argv[i + 1];
      i++;
    } else if (strcmp(argv[i], "-info") == 0) {
      printInfo = GL_TRUE;
    } else if (strcmp(argv[i], "-stereo") == 0) {
      stereo = GL_TRUE;
    } else if (i < argc - 1 && strcmp(argv[i], "-samples") == 0) {
      samples = strtod(argv[i + 1], NULL);
      ++i;
    } else if (strcmp(argv[i], "-fullscreen") == 0) {
      fullscreen = GL_TRUE;
    } else if (i < argc - 1 && strcmp(argv[i], "-geometry") == 0) {
      XParseGeometry(argv[i + 1], &x, &y, &winWidth, &winHeight);
      i++;
    } else if (strcmp(argv[i], "-col-red-gear") == 0) {
      parseColor(argv[i + 1], red);
      i++;
    } else if (strcmp(argv[i], "-col-green-gear") == 0) {
      parseColor(argv[i + 1], green);
      i++;
    } else if (strcmp(argv[i], "-col-blue-gear") == 0) {
      parseColor(argv[i + 1], blue);
      i++;
    } else if (strcmp(argv[i], "-col-bg") == 0) {
      parseColor(argv[i + 1], bg);
      i++;
    } else {
      usage();
      return -1;
    }
  }

  dpy = XOpenDisplay(dpyName);
  if (!dpy) {
    printf("Error: couldn't open display %s\n",
           dpyName ? dpyName : getenv("DISPLAY"));
    return -1;
  }

  if (fullscreen) {
    int scrnum = DefaultScreen(dpy);

    x = 0;
    y = 0;
    winWidth = DisplayWidth(dpy, scrnum);
    winHeight = DisplayHeight(dpy, scrnum);
  }

  make_window(dpy, "glxgears", x, y, winWidth, winHeight, &win, &ctx, &visId);
  XMapWindow(dpy, win);
  glXMakeCurrent(dpy, win, ctx);
  query_vsync(dpy, win);

  if (printInfo) {
    printf("GL_RENDERER   = %s\n", (char *)glGetString(GL_RENDERER));
    printf("GL_VERSION    = %s\n", (char *)glGetString(GL_VERSION));
    printf("GL_VENDOR     = %s\n", (char *)glGetString(GL_VENDOR));
    printf("GL_EXTENSIONS = %s\n", (char *)glGetString(GL_EXTENSIONS));
    printf("VisualID %d, 0x%x\n", (int)visId, (int)visId);
  }

  init(red, green, blue, bg);

  /* Set initial projection/viewing transformation.
   * We can't be sure we'll get a ConfigureNotify event when the window
   * first appears.
   */
  reshape(winWidth, winHeight);

  event_loop(dpy, win);

  glDeleteLists(gear1, 1);
  glDeleteLists(gear2, 1);
  glDeleteLists(gear3, 1);
  glXMakeCurrent(dpy, None, NULL);
  glXDestroyContext(dpy, ctx);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);

  return 0;
}
