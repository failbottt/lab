// font_renderer.c
// Text renderer with per-glyph caching, kerning cache, and newline support

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define WIDTH       800
#define HEIGHT      600
#define FONT_SIZE   24
#define FIRST_CHAR  32
#define NUM_CHARS   96  // ASCII 32..127

// X11 globals
static Display   *dpy;
static Window     win;
static GC         gc;
static XImage    *ximage;
static uint32_t  *pixels;

// Font globals
static stbtt_fontinfo   font;
static float            scale;
static float            ascent, descent, lineGap;
static unsigned char   *font_buffer;

// Glyph cache entry
typedef struct {
    int     loaded;
    unsigned char *bitmap;
    int     w, h;
    int     xoff, yoff;
    float   advance;
} CachedGlyph;
static CachedGlyph cache[NUM_CHARS];

// Kerning cache
static float         kern_cache[NUM_CHARS][NUM_CHARS];
static unsigned char kern_loaded[NUM_CHARS][NUM_CHARS];

static double now_sec(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec/1e9;
}

void init_x11(void) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) { perror("XOpenDisplay"); exit(1); }
    int screen = DefaultScreen(dpy);
    Visual *vis = DefaultVisual(dpy, screen);
    int depth  = DefaultDepth(dpy, screen);

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                              50, 50, WIDTH, HEIGHT, 1,
                              BlackPixel(dpy, screen),
                              BlackPixel(dpy, screen));
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    gc = XCreateGC(dpy, win, 0, NULL);
    XMapWindow(dpy, win);

    pixels = calloc(WIDTH * HEIGHT, sizeof(uint32_t));
    if (!pixels) { perror("calloc"); exit(1); }
    ximage = XCreateImage(dpy, vis, depth, ZPixmap, 0,
                          (char*)pixels, WIDTH, HEIGHT,
                          32, 0);
    if (!ximage) { fprintf(stderr, "XCreateImage failed\n"); exit(1); }
}

void load_font(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); exit(1); }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    font_buffer = malloc(sz);
    if (fread(font_buffer, 1, sz, f) != sz) { perror("fread"); exit(1); }
    fclose(f);

    if (!stbtt_InitFont(&font, font_buffer, 0)) {
        fprintf(stderr, "Failed to init font\n"); exit(1);
    }
    scale = stbtt_ScaleForPixelHeight(&font, FONT_SIZE);
    int ia, id, ig;
    stbtt_GetFontVMetrics(&font, &ia, &id, &ig);
    ascent   = ia * scale;
    descent  = id * scale;
    lineGap  = ig * scale;

    memset(cache,       0, sizeof(cache));
    memset(kern_loaded, 0, sizeof(kern_loaded));
}

static CachedGlyph* get_glyph(int cp) {
    if (cp < FIRST_CHAR || cp >= FIRST_CHAR + NUM_CHARS) return NULL;
    CachedGlyph *cg = &cache[cp - FIRST_CHAR];
    if (!cg->loaded) {
        int w, h, xoff, yoff;
        unsigned char *bm = stbtt_GetCodepointBitmap(&font, 0, scale,
                                                     cp, &w, &h,
                                                     &xoff, &yoff);
        int adv_i, lsb;
        stbtt_GetCodepointHMetrics(&font, cp, &adv_i, &lsb);
        cg->bitmap  = bm;
        cg->w       = w;
        cg->h       = h;
        cg->xoff    = xoff;
        cg->yoff    = yoff;
        cg->advance = adv_i * scale;
        cg->loaded  = 1;
    }
    return cg;
}

static float get_kerning(int cp1, int cp2) {
    int i1 = cp1 - FIRST_CHAR;
    int i2 = cp2 - FIRST_CHAR;
    if (i1 < 0 || i1 >= NUM_CHARS || i2 < 0 || i2 >= NUM_CHARS) return 0;
    if (!kern_loaded[i1][i2]) {
        int kern_i = stbtt_GetCodepointKernAdvance(&font, cp1, cp2);
        kern_cache[i1][i2] = kern_i * scale;
        kern_loaded[i1][i2] = 1;
    }
    return kern_cache[i1][i2];
}

void free_cache(void) {
    for (int i = 0; i < NUM_CHARS; ++i) {
        if (cache[i].loaded) {
            stbtt_FreeBitmap(cache[i].bitmap, NULL);
        }
    }
}

void render_text(const char *text, float x, float y_top) {
    float pen_x    = x;
    float baseline = y_top + ascent;
    int prev_cp    = -1;

    for (const unsigned char *p = (const unsigned char*)text; *p; ++p) {
        int cp = *p;
        if (cp == '\n') {
            pen_x    = x;
            baseline += (ascent - descent + lineGap);
            prev_cp  = -1;
            continue;
        }
        if (prev_cp >= 0) {
            pen_x += get_kerning(prev_cp, cp);
        }
        prev_cp = cp;

        CachedGlyph *cg = get_glyph(cp);
        if (!cg) continue;

        int x0 = (int)(pen_x + cg->xoff + 0.5f);
        int y0 = (int)(baseline + cg->yoff + 0.5f);
        for (int row = 0; row < cg->h; ++row) {
            for (int col = 0; col < cg->w; ++col) {
                unsigned char a = cg->bitmap[row * cg->w + col];
                if (!a) continue;
                int px = x0 + col;
                int py = y0 + row;
                if (px < 0 || px >= WIDTH || py < 0 || py >= HEIGHT)
                    continue;
                uint32_t dst = pixels[py * WIDTH + px];
                uint8_t dr = (dst >> 16) & 0xFF;
                uint8_t dg = (dst >>  8) & 0xFF;
                uint8_t db = (dst >>  0) & 0xFF;
                uint8_t r = (a * 255 + (255 - a) * dr) / 255;
                uint8_t g = (a * 255 + (255 - a) * dg) / 255;
                uint8_t b = (a * 255 + (255 - a) * db) / 255;
                pixels[py * WIDTH + px] = (r << 16) | (g << 8) | b;
            }
        }
        pen_x += cg->advance;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s font.ttf\n", argv[0]); return 1; }
    init_x11();
    load_font(argv[1]);
    memset(pixels, 0, WIDTH * HEIGHT * sizeof(uint32_t));


    render_text("Hello, world!\nThe second line", 50, 100);

    // perf
    // const int iterations = 200;
    // double t0 = now_sec();
    // for (int i = 0; i < iterations; i++)
    // {
    //     render_text("Hello, world!\nThe second line", 50, 100);
    // }
    // double t1 = now_sec();
    // double total_time_sec = t1 - t0;
    // double avg_time_sec = total_time_sec / (double)iterations;
    // double avg_time_ms = avg_time_sec * 1000.0;
    // fprintf(stderr, "Average time: %.3f ms\n", avg_time_ms);

    XPutImage(dpy, win, gc, ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
    XEvent ev;
    while (XNextEvent(dpy, &ev), ev.type != KeyPress) {
        if (ev.type == Expose)
            XPutImage(dpy, win, gc, ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
    }
    free_cache();
    stbtt_FreeBitmap(font_buffer, NULL);
    XDestroyImage(ximage);
    free(pixels);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}















// // font_renderer.c
// // Alternate text renderer using stb_truetype per-glyph bitmaps + kerning
//
// #include <stdio.h>
// #include <stdlib.h>
// #include <stdint.h>
// #include <string.h>
// #include <X11/Xlib.h>
// #include <X11/Xutil.h>
//
// #include <time.h>
//
// #define STB_TRUETYPE_IMPLEMENTATION
// #include "stb_truetype.h"
//
// #define WIDTH     1920
// #define HEIGHT    1080
// #define FONT_SIZE 32
//
// // X11 globals
// static Display *dpy;
// static Window   win;
// static GC       gc;
// static XImage  *ximage;
// static uint32_t*pixels;
//
// // Font globals
// static stbtt_fontinfo font;
// static float         scale, ascent, descent, lineGap;
//
// static double now_sec(void) {
//     struct timespec t;
//     clock_gettime(CLOCK_MONOTONIC, &t);
//     return t.tv_sec + t.tv_nsec/1e9;
// }
//
// void init_x11(void) {
//     dpy = XOpenDisplay(NULL);
//     if (!dpy) { perror("XOpenDisplay"); exit(1); }
//     int screen = DefaultScreen(dpy);
//     Visual *vis = DefaultVisual(dpy, screen);
//     int depth  = DefaultDepth(dpy, screen);
//
//     win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
//                               50, 50, WIDTH, HEIGHT, 1,
//                               BlackPixel(dpy, screen),
//                               BlackPixel(dpy, screen));
//     XSelectInput(dpy, win, ExposureMask | KeyPressMask);
//     gc = XCreateGC(dpy, win, 0, NULL);
//     XMapWindow(dpy, win);
//
//     pixels = calloc(WIDTH * HEIGHT, sizeof(uint32_t));
//     ximage = XCreateImage(dpy, vis, depth, ZPixmap, 0,
//                           (char*)pixels, WIDTH, HEIGHT,
//                           32, 0);
// }
//
// void load_font(const char *path) {
//     FILE *f = fopen(path, "rb");
//     if (!f) { perror("fopen"); exit(1); }
//     fseek(f, 0, SEEK_END);
//     size_t sz = ftell(f);
//     fseek(f, 0, SEEK_SET);
//
//     unsigned char *buf = malloc(sz);
//     fread(buf,1,sz,f); fclose(f);
//
//     if (!stbtt_InitFont(&font, buf, 0)) {
//         fprintf(stderr, "Failed to init font\n"); exit(1);
//     }
//     scale = stbtt_ScaleForPixelHeight(&font, FONT_SIZE);
//     int ia, id, ig;
//     stbtt_GetFontVMetrics(&font, &ia, &id, &ig);
//     ascent   = ia * scale;
//     descent  = id * scale;
//     lineGap  = ig * scale;
// }
//
// void render_text(const char *text, float x, float y_top) {
//     float pen_x = x;
//     float baseline = y_top + ascent;
//     int prev_cp = -1;
//
//     for (const unsigned char *p = (const unsigned char*)text; *p; ++p) {
//         int cp = *p;
//         if (cp == '\n')
//         {
//             pen_x = x;
//             baseline += (ascent - descent + lineGap);
//             prev_cp = -1;
//             continue;
//         }
//         // apply kerning
//         if (prev_cp >= 0) {
//             int kern = stbtt_GetCodepointKernAdvance(&font, prev_cp, cp);
//             pen_x += kern * scale;
//         }
//         prev_cp = cp;
//
//         // get glyph bitmap + metrics
//         int w,h,xoff,yoff;
//         unsigned char *bitmap = stbtt_GetCodepointBitmap(
//             &font, 0, scale, cp,
//             &w, &h, &xoff, &yoff
//         );
//
//         // draw glyph
//         int x0 = (int)(pen_x + xoff + 0.5f);
//         int y0 = (int)(baseline + yoff + 0.5f);
//         for (int row=0; row<h; ++row) {
//             for (int col=0; col<w; ++col) {
//                 unsigned char a = bitmap[row*w + col];
//                 if (!a) continue;
//                 int px = x0 + col;
//                 int py = y0 + row;
//                 if (px<0||px>=WIDTH||py<0||py>=HEIGHT) continue;
//                 uint32_t dst = pixels[py*WIDTH + px];
//                 uint8_t dr = dst>>16, dg=(dst>>8)&0xFF, db=dst&0xFF;
//                 uint8_t r = (a*255 + (255-a)*dr)/255;
//                 uint8_t g = (a*255 + (255-a)*dg)/255;
//                 uint8_t b = (a*255 + (255-a)*db)/255;
//                 pixels[py*WIDTH + px] = (r<<16)|(g<<8)|b;
//             }
//         }
//         stbtt_FreeBitmap(bitmap, NULL);
//
//         // advance pen by glyph advance
//         int adv, lsb;
//         stbtt_GetCodepointHMetrics(&font, cp, &adv, &lsb);
//         pen_x += adv * scale;
//     }
// }
//
// int main(int argc, char **argv) {
//     if (argc < 2) { fprintf(stderr,"Usage: %s font.ttf\n",argv[0]); return 1; }
//     init_x11(); load_font(argv[1]);
//     // clear
//     memset(pixels,0,WIDTH*HEIGHT*sizeof(uint32_t));
//
//     const int iterations = 200;
//     double t0 = now_sec();
//     for (int i = 0; i < iterations; i++)
//     {
//         render_text("Hello, world!\nThe second line", 50, 100);
//     }
//     double t1 = now_sec();
//
//     double total_time_sec = t1 - t0;
//
//     double avg_time_sec = total_time_sec / (double)iterations;
//
//     double avg_time_ms = avg_time_sec * 1000.0;
//
//     fprintf(stderr, "Average time: %.3f ms\n", avg_time_ms);
//
//     XPutImage(dpy, win, gc, ximage, 0,0,0,0,WIDTH,HEIGHT);
//     XEvent ev;
//     while (XNextEvent(dpy,&ev), ev.type!=KeyPress) {
//         if (ev.type==Expose)
//             XPutImage(dpy, win, gc, ximage, 0,0,0,0,WIDTH,HEIGHT);
//     }
//     // cleanup
//     XDestroyImage(ximage);
//     free(pixels);
//     XFreeGC(dpy, gc);
//     XDestroyWindow(dpy, win);
//     XCloseDisplay(dpy);
//     return 0;
// }
//
