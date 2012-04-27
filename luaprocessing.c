
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>

/******************************************************************************\
Macros
\******************************************************************************/

#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MIN3(a, b, c)   (MIN(a, MIN(b, c)))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define MAX3(a, b, c)   (MAX(a, MAX(b, c)))

#define LOW(a, b)       MIN(a, b)
#define HIGH(a, b)      MAX(a, b)
#define PEG(a)          MIN(255, MAX(a, 0))
#define MIX(a, b, f)    ((a) + ((((b) - (a)) * (f)) >> 8))

#define SQ(a)           ((a) * (a))

#define ALPHA_MASK      0xff000000
#define RED_MASK        0x00ff0000
#define GREEN_MASK      0x0000ff00
#define BLUE_MASK       0x000000ff

#define LP_PI           3.1415926535897932
#define LP_TWO_PI       (LP_PI * 2)
#define LP_HALF_PI      (LP_PI / 2)
#define LP_QUARTER_PI   (LP_PI / 4)

/******************************************************************************\
Constants
\******************************************************************************/

enum {CMWC_K=4096};

enum {LEFT, RIGHT, CENTER};

enum {RGB, HSB};
enum {
    BLEND, ADD, SUBTRACT, DARKEST, LIGHTEST, DIFFERENCE, EXCLUSION,
    MULTIPLY, SCREEN, OVERLAY, HARD_LIGHT, SOFT_LIGHT, DODGE, BURN
};

/******************************************************************************\
Data
\******************************************************************************/

struct cmwc {
    unsigned Q[CMWC_K];
    unsigned c;
    unsigned i;
};

#define RED(c) ((c).v[0])
#define HUE(c) ((c).v[0])
#define GREEN(c) ((c).v[1])
#define SATURATION(c) ((c).v[1])
#define BLUE(c) ((c).v[2])
#define VALUE(c) ((c).v[2])
#define ALPHA(c) ((c).v[3])
struct color {
    double v[4];
};

typedef unsigned blend_func(unsigned, unsigned);

/******************************************************************************\
Globals
\******************************************************************************/

/* Noise stuff. */
static unsigned octaves = 4;
static double fallout = 0.5;

static signed permutation[] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,
    117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,
    165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,
    105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,
    187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,
    3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,
    227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,
    221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,
    185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,
    115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,
    195,78,66,215,61,156,180,
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,
    117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,
    165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,
    105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,
    187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,
    3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,
    227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,
    221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,
    185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,
    115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,
    195,78,66,215,61,156,180,
};

static signed current_permutation[512] = {0};
static struct cmwc noise_cmwc;

static struct cmwc random_cmwc;

/* Screen. */
static unsigned screen_width = 100;
static unsigned screen_height = 100;

/* Modes for dawing. */
/*static struct color mode_stroke_color = {{0, 0, 0, 1}};
static struct color mode_fill_color = {{1, 1, 1, 1}};
static struct color mode_tint_color = {{0, 0, 0, 0}}; // None?
static signed mode_stroke_weight = 1;*/

/* Modes for color. */
static signed mode_color_mode = RGB;
static struct color mode_color_range = {{255, 255, 255, 255}};
/* Modes for depth testing. */
/*static signed mode_depth_test = 1;
static signed mode_texture = 0;*/


static SDL_Surface *surface = NULL;

/******************************************************************************\
Internal Functions
\******************************************************************************/

static struct color unsigned_to_color(unsigned i)
{
    struct color c;
    ALPHA(c) = ((i & ALPHA_MASK) >> 24) / 255.0;
    RED(c) = ((i & RED_MASK) >> 16) / 255.0;
    GREEN(c) = ((i & GREEN_MASK) >> 8) / 255.0;
    BLUE(c) = (i & BLUE_MASK) / 255.0;
    return c;
}



static unsigned color_to_unsigned(struct color c)
{
    unsigned a = ALPHA(c) * 255;
    unsigned r = RED(c) * 255;
    unsigned g = GREEN(c) * 255;
    unsigned b = BLUE(c) * 255;
    return a << 24 | r << 16 | g << 8 | b;
}



static double fade(double t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}


static double lerp(double t, double a, double b)
{
    return a + t * (b - a);
}



static double grad(signed hash, double x, double y, double z)
{
    signed h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 0x1) == 0 ? u : -u) + ((h & 0x2) == 0 ? v : -v);
}



static double noise(double x, double y, double z)
{
    double u, v, w;
    signed X, Y, Z, A, AA, AB, B, BA, BB;

    signed *p = current_permutation;

    X = (signed)floor(x) & 0xff;
    Y = (signed)floor(y) & 0xff;
    Z = (signed)floor(z) & 0xff;

    x -= (signed)floor(x);
    y -= (signed)floor(y);
    z -= (signed)floor(z);

    u = fade(x);
    v = fade(y);
    w = fade(z);

    A  = p[X    ] + Y;
    AA = p[A    ] + Z;
    AB = p[A + 1] + Z;
    B  = p[X + 1] + Y;
    BA = p[B    ] + Z;
    BB = p[B + 1] + Z;

    return lerp(w, lerp(v, lerp(u, grad(p[AA    ], x    , y    , z    ),
                                   grad(p[BA    ], x - 1, y    , z    )),
                           lerp(u, grad(p[AB    ], x    , y - 1, z    ),
                                   grad(p[BB    ], x - 1, y - 1, z    ))),
                   lerp(v, lerp(u, grad(p[AA + 1], x    , y    , z - 1),
                                   grad(p[BA + 1], x - 1, y    , z - 1)),
                           lerp(u, grad(p[AB + 1], x    , y - 1, z - 1),
                                   grad(p[BB + 1], x - 1, y - 1, z - 1))));
}



void cmwc_seed(struct cmwc *cmwc, unsigned seed)
{
    unsigned i;

    srand(seed);
    for (i = 0; i < CMWC_K; ++i) {
        cmwc->Q[i] = rand();
    }
    cmwc->c = seed;
    cmwc->i = CMWC_K - 1;
}



unsigned cmwc_random(struct cmwc *cmwc)
{
    unsigned th, tl, a = 18782;
    unsigned x, r = 0xfffffffe;

    unsigned q, qh, ql, c;

    cmwc->i = (cmwc->i + 1) & (CMWC_K - 1);

    q = cmwc->Q[cmwc->i];
    qh = q >> 16;
    ql = q & 0xffff;
    c = cmwc->c;

    tl = a * q + cmwc->c;
    th = ((a * qh + ((c & 0xffff) + a * ql)) >> 16) + ((c >> 16) >> 16);

    cmwc->c = th;
    x = tl + th;
    if (x < cmwc->c) {
        x++;
        cmwc->c++;
    }
    return cmwc->Q[cmwc->i] = r - x;
}

/******************************************************************************\
System
\******************************************************************************/

static void call(lua_State *L, const char *name)
{
    lua_getglobal(L, name);
    if (!lua_isnil(L, -1)) {
        if (lua_pcall(L, 0, 0, 0)) {
            luaL_error(L, "%s\n", lua_tostring(L, -1));
        }
    }
    else {
        lua_pop(L, 1);
    }
}

static int lp_run(lua_State *L)
{
    int down = 0;
    int moved = 0;

    int running = 1;
    SDL_Event ev;

    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    call(L, "setup");

    if (!surface) {
        surface = SDL_SetVideoMode(screen_width, screen_height, 32,
                                   SDL_OPENGL | SDL_DOUBLEBUF);
        if (!surface) {
            luaL_error(L, "%s", SDL_GetError());
        }
    }

    lua_pushnumber(L, screen_width);
    lua_setglobal(L, "width");
    lua_pushnumber(L, screen_height);
    lua_setglobal(L, "height");

    glClearColor(0, 0, 0, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    SDL_WM_SetCaption("luaprocessing", NULL);

    while (running) {
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_ACTIVEEVENT:
                if (ev.active.gain) {
                    call(L, "mouseOver");
                }
                else {
                    call(L, "mouseOut");
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT)
                    lua_pushnumber(L, LEFT);
                else if (ev.button.button == SDL_BUTTON_RIGHT)
                    lua_pushnumber(L, RIGHT);
                else if (ev.button.button == SDL_BUTTON_MIDDLE)
                    lua_pushnumber(L, CENTER);
                else
                    lua_pushnumber(L, -1);;
                lua_setglobal(L, "mouseButton");
                call(L, "mousePressed");

                down = 1;
                moved = 0;
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT)
                    lua_pushnumber(L, LEFT);
                else if (ev.button.button == SDL_BUTTON_RIGHT)
                    lua_pushnumber(L, RIGHT);
                else if (ev.button.button == SDL_BUTTON_MIDDLE)
                    lua_pushnumber(L, CENTER);
                else
                    lua_pushnumber(L, -1);
                lua_setglobal(L, "mouseButton");
                call(L, "mouseReleased");

                if (down && !moved) {
                    call(L, "mouseClicked");
                }

                down = 0;
                moved = 0;
                break;

            case SDL_MOUSEMOTION:
                lua_getglobal(L, "mouseX");
                lua_setglobal(L, "pmouseX");
                lua_getglobal(L, "mouseY");
                lua_setglobal(L, "pmouseY");

                lua_pushnumber(L, ev.motion.x);
                lua_setglobal(L, "mouseX");
                lua_pushnumber(L, ev.motion.y);
                lua_setglobal(L, "mouseY");
                call(L, "mouseMoved");

                if (down) {
                    call(L, "mouseDragged");
                }

                moved = 1;

                break;

            default:
                break;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        call(L, "draw");
        SDL_GL_SwapBuffers();
    }

    SDL_Quit();

    return 0;
}

static int lp_size(lua_State *L)
{
    screen_width = luaL_checknumber(L, 1);
    screen_height = luaL_checknumber(L, 2);

    lua_setglobal(L, "height");
    lua_setglobal(L, "width");

    surface = SDL_SetVideoMode(screen_width, screen_height, 32,
                               SDL_OPENGL | SDL_DOUBLEBUF);
    if (!surface) {
        luaL_error(L, "%s", SDL_GetError());
    }
    return 0;
}

/******************************************************************************\
Color
\******************************************************************************/

static struct color hsb_to_rgb(struct color hsv)
{
    double min;
    double chroma;
    double hue_;
    double x;
    struct color rgb;

    chroma = SATURATION(hsv) * VALUE(hsv);
    hue_ = HUE(hsv) * 5.9999;
    x = floor(hue_ / 2.0); /* Modulus */
    x = hue_ - (x * 2.0); /* Modulus */
    x = chroma * (1.0 - fabs(x - 1.0));

    if (hue_ < 1.0) {
        RED(rgb) = chroma;
        GREEN(rgb) = x;
    }
    else if (hue_ < 2.0) {
        RED(rgb) = x;
        GREEN(rgb) = chroma;
    }
    else if (hue_ < 3.0) {
        GREEN(rgb) = chroma;
        BLUE(rgb) = x;
    }
    else if (hue_ < 4.0) {
        GREEN(rgb) = x;
        BLUE(rgb) = chroma;
    }
    else if (hue_ < 5.0) {
        RED(rgb) = x;
        BLUE(rgb) = chroma;
    }
    else if (hue_ < 6.0) {
        RED(rgb) = chroma;
        BLUE(rgb) = x;
    }

    min = VALUE(hsv) - chroma;

    RED(rgb) += min;
    GREEN(rgb) += min;
    BLUE(rgb) += min;
    ALPHA(rgb) = ALPHA(hsv);

    return rgb;
}



static struct color rgb_to_hsb(struct color rgb)
{
    struct color hsv;
    double min = MIN3(RED(rgb), GREEN(rgb), BLUE(rgb));
    double max = MAX3(RED(rgb), GREEN(rgb), BLUE(rgb));
    double chroma = max - min;

    HUE(hsv) = SATURATION(hsv) = VALUE(hsv) = ALPHA(hsv) = 0.0;

    if (chroma != 0) {
        if (RED(rgb) == max) {
            HUE(hsv) = (GREEN(rgb) - BLUE(rgb)) / chroma;
            if (HUE(hsv) < 0.0) {
                HUE(hsv) += 6.0;
            }
        }
        else if (GREEN(rgb) == max) {
            HUE(hsv) = ((BLUE(rgb) - RED(rgb)) / chroma) + 2.0;
        }
        else /* BLUE(rgb) == max */ {
            HUE(hsv) = ((RED(rgb) - GREEN(rgb)) / chroma) + 4.0;
        }
        HUE(hsv) *= 60.0;
        SATURATION(hsv) = chroma / max;
    }

    VALUE(hsv) = max;
    ALPHA(hsv) = ALPHA(rgb);

    HUE(hsv) /= 360.0;

    return hsv;
}



static int lp_color(lua_State *L)
{
    struct color *color, *tmp;
    unsigned c;
    signed n = lua_gettop(L);

    color = lua_newuserdata(L, sizeof *color);

    luaL_getmetatable(L, "color");
    lua_setmetatable(L, -2);

    /* Extra argument on the stack because of color. */
    switch (n) {
    case 1:
        if (lua_isnumber(L, 1)) {
            c = luaL_checknumber(L, 1);
            /* Assume non-grey scale. */
            if (c > 255) {
                *color = unsigned_to_color(c);
            }
            else {
                RED(*color) = GREEN(*color) = BLUE(*color) = c;
                ALPHA(*color) = ALPHA(mode_color_range);
            }
        }
        else {
            tmp = luaL_checkudata(L, 1, "color");
            luaL_argcheck(L, tmp != NULL, 1, "'color' expected");
            RED(*color) = RED(*tmp);
            GREEN(*color) = GREEN(*tmp);
            BLUE(*color) = BLUE(*tmp);
            ALPHA(*color) = ALPHA(*tmp);
            /* No normalizing necessary. */
            return 1;
        }
        break;

    case 2:
        RED(*color) = GREEN(*color) = BLUE(*color) = luaL_checknumber(L, 1);
        ALPHA(*color) = luaL_checknumber(L, 2);
        break;

    case 3:
        RED(*color) = luaL_checknumber(L, 1);
        GREEN(*color) = luaL_checknumber(L, 2);
        BLUE(*color) = luaL_checknumber(L, 3);
        ALPHA(*color) = ALPHA(mode_color_range);
        break;

    case 4:
        RED(*color) = luaL_checknumber(L, 1);
        GREEN(*color) = luaL_checknumber(L, 2);
        BLUE(*color) = luaL_checknumber(L, 3);
        ALPHA(*color) = luaL_checknumber(L, 4);
        break;
    }

    RED(*color) /= RED(mode_color_range);
    GREEN(*color) /= GREEN(mode_color_range);
    BLUE(*color) /= BLUE(mode_color_range);
    ALPHA(*color) /= ALPHA(mode_color_range);

    if (mode_color_mode == HSB) {
        *color = hsb_to_rgb(*color);
    }

    return 1;
}



static int lp_red(lua_State *L)
{
    struct color *color;
    lp_color(L);
    color = lua_touserdata(L, -1); lua_pop(L, 1);
    lua_pushnumber(L, RED(*color) * RED(mode_color_range));
    return 1;
}



static int lp_green(lua_State *L)
{
    struct color *color;
    lp_color(L);
    color = lua_touserdata(L, -1); lua_pop(L, 1);
    lua_pushnumber(L, GREEN(*color) * GREEN(mode_color_range));
    return 1;
}



static int lp_blue(lua_State *L)
{
    struct color *color;
    lp_color(L);
    color = lua_touserdata(L, -1); lua_pop(L, 1);
    lua_pushnumber(L, BLUE(*color) * BLUE(mode_color_range));
    return 1;
}



static int lp_alpha(lua_State *L)
{
    struct color *color;
    lp_color(L);
    color = lua_touserdata(L, -1); lua_pop(L, 1);
    lua_pushnumber(L, ALPHA(*color) * ALPHA(mode_color_range));
    return 1;
}



static int lp_hue(lua_State *L)
{
    struct color *color, tmp;
    lp_color(L);
    color = lua_touserdata(L, -1); lua_pop(L, 1);
    tmp = rgb_to_hsb(*color);
    lua_pushnumber(L, HUE(tmp) * HUE(mode_color_range));
    return 1;
}



static int lp_saturation(lua_State *L)
{
    struct color *color, tmp;
    lp_color(L);
    color = lua_touserdata(L, -1); lua_pop(L, 1);
    tmp = rgb_to_hsb(*color);
    lua_pushnumber(L, SATURATION(tmp) * SATURATION(mode_color_range));
    return 1;
}



static int lp_brightness(lua_State *L)
{
    struct color *color, tmp;
    lp_color(L);
    color = lua_touserdata(L, -1); lua_pop(L, 1);
    tmp = rgb_to_hsb(*color);
    lua_pushnumber(L, VALUE(tmp) * VALUE(mode_color_range));
    return 1;
}



static int lp_lerp_color(lua_State *L)
{
    struct color *c1, *c2, *ret;
    double amtb;

    c1 = luaL_checkudata(L, 1, "color");
    luaL_argcheck(L, c1 != NULL, 1, "'color' expected");
    c2 = luaL_checkudata(L, 2, "color");
    luaL_argcheck(L, c2 != NULL, 2, "'color' expected");

    amtb = luaL_checknumber(L, 3);

    ret = lua_newuserdata(L, sizeof *ret);
    luaL_getmetatable(L, "color");
    lua_setmetatable(L, -2);

    RED(*ret) = amtb * RED(*c1) + amtb * RED(*c2);
    GREEN(*ret) = amtb * GREEN(*c1) + amtb * GREEN(*c2);
    BLUE(*ret) = amtb * BLUE(*c1) + amtb * BLUE(*c2);
    ALPHA(*ret) = amtb * ALPHA(*c1) + amtb * ALPHA(*c2);
    return 1;
}



static int lp_color_mode(lua_State *L)
{
    mode_color_mode = luaL_checknumber(L, 1);

    /* -1 for the extra mode arg. */
    switch (lua_gettop(L)) {
    case 2:
        RED(mode_color_range) =
        GREEN(mode_color_range) =
        BLUE(mode_color_range) =
        ALPHA(mode_color_range) = luaL_checknumber(L, 2);
        break;

    case 4:
        RED(mode_color_range) = luaL_checknumber(L, 2);
        GREEN(mode_color_range) = luaL_checknumber(L, 3);
        BLUE(mode_color_range) = luaL_checknumber(L, 4);
        /* ALPHA(mode_color_range) remains unchanged. */
        break;

    case 5:
        RED(mode_color_range) = luaL_checknumber(L, 2);
        GREEN(mode_color_range) = luaL_checknumber(L, 3);
        BLUE(mode_color_range) = luaL_checknumber(L, 4);
        ALPHA(mode_color_range) = luaL_checknumber(L, 5);
        break;

    default:
        break;
    }

    return 0;
}



static unsigned blend(unsigned a, unsigned b)
{
    unsigned f = (b & ALPHA_MASK) >> 24;

    return (LOW(((a & ALPHA_MASK) >> 24) + f, 0xff)) << 24 |
           (MIX(a & RED_MASK, b & RED_MASK, f) & RED_MASK) |
           (MIX(a & GREEN_MASK, b & GREEN_MASK, f) & GREEN_MASK) |
           (MIX(a & BLUE_MASK, b & BLUE_MASK, f));
}



static int lp_blend_color(lua_State *L)
{
    struct color *c1, *c2, *ret;
    unsigned a, b, c;
    signed mode;

    static blend_func *blend_funcs[] = {
        blend,
    };

    c1 = luaL_checkudata(L, 1, "color");
    luaL_argcheck(L, c1 != NULL, 1, "'color' expected");
    c2 = luaL_checkudata(L, 2, "color");
    luaL_argcheck(L, c2 != NULL, 2, "'color' expected");

    mode = luaL_checknumber(L, 3);

    a = color_to_unsigned(*c1);
    b = color_to_unsigned(*c2);
    c = blend_funcs[mode](a, b);

    ret = lua_newuserdata(L, sizeof *ret);
    luaL_getmetatable(L, "color");
    lua_setmetatable(L, -2);

    *ret = unsigned_to_color(c);

    return 1;
}

/******************************************************************************\
Random/Noise
\******************************************************************************/

static int lp_noise(lua_State *L)
{
    double x, y, z, num, effect, k;
    unsigned i;

    x = y = z = 0.0;

    if (lua_gettop(L) == 1) {
        x = luaL_checknumber(L, 1);
    }
    else if (lua_gettop(L) == 2) {
        x = luaL_checknumber(L, 1);
        y = luaL_checknumber(L, 2);
    }
    else if (lua_gettop(L) == 3) {
        x = luaL_checknumber(L, 1);
        y = luaL_checknumber(L, 2);
        z = luaL_checknumber(L, 3);
    }
    else {
        luaL_error(L, "incorrect number of arguments");
    }

    num = 0;
    effect = 1;
    k = 1;
    for (i = 0; i < octaves; ++i) {
        effect *= fallout;
        num += effect * (1 + noise(k * x, k * y, k * z)) / 2;
        k *= 2;
    }

    lua_pushnumber(L, num);
    return 1;
}



static int lp_noise_seed(lua_State *L)
{
    unsigned i, j, t;
    unsigned seed = luaL_checknumber(L, 1);

    cmwc_seed(&noise_cmwc, seed);

    for (i = 0; i < 512; ++i) {
        current_permutation[i] = permutation[i];
    }

    for (i = 0; i < 256; ++i) {
        j = cmwc_random(&noise_cmwc) & 255;
        t = current_permutation[j];
        current_permutation[j] = current_permutation[i];
        current_permutation[i] = t;
    }
    for (i = 0; i < 256; ++i) {
        current_permutation[i + 256] = current_permutation[i];
    }

    return 0;
}



static int lp_noise_detail(lua_State *L)
{
    octaves = luaL_checknumber(L, 1);
    fallout = luaL_checknumber(L, 2);
    return 0;
}



static int lp_random(lua_State *L)
{
    double hi, lo, num;

    if (lua_gettop(L) == 1) {
        lo = 0.0;
        hi = luaL_checknumber(L, 1);
    }
    else if (lua_gettop(L) == 2) {
        lo = luaL_checknumber(L, 1);
        hi = luaL_checknumber(L, 2);
    }

    num = ((double)cmwc_random(&random_cmwc) / 0xffffffff) * 0.9999999999;

    num *= (hi - lo);
    num += lo;

    lua_pushnumber(L, num);
    return 1;
}



static int lp_random_seed(lua_State *L)
{
    unsigned seed = luaL_checknumber(L, 1);

    cmwc_seed(&random_cmwc, seed);

    return 0;
}

/******************************************************************************\
Math
\******************************************************************************/



static int lp_binary(lua_State *L)
{
    char buf[32] = {0};
    unsigned num, i, len;
    struct color *c;

    if (lua_gettop(L) == 2) {
        len = luaL_checknumber(L, 2);
        if (len >= 32)
            len = 32;
    }
    else {
        len = 32;
    }

    if (lua_isnumber(L, 1)) {
        num = luaL_checknumber(L, 1);
    }
    else if (lua_isuserdata(L, 1)) {
        c = luaL_checkudata(L, 1, "color");
        luaL_argcheck(L, c != NULL, 1, "'color' expected");
        num = color_to_unsigned(*c);
    }

    for (i = 0; i < 32; ++i) {
        if (num & 0x1) {
            buf[32 - i - 1] = '1';
        }
        else {
            buf[32 - i - 1] = '0';
        }
        num >>= 1;
    }

    lua_pushlstring(L, buf + (32 - len), len);
    return 1;
}



static int lp_hex(lua_State *L)
{
    char buf[8 + 1] = {0};
    unsigned num, len;
    struct color *c;

    if (lua_gettop(L) == 2) {
        len = luaL_checknumber(L, 2);
        if (len >= 8)
            len = 8;
    }
    else {
        len = 8;
    }

    if (lua_isnumber(L, 1)) {
        num = luaL_checknumber(L, 1);
    }
    else if (lua_isuserdata(L, 1)) {
        c = luaL_checkudata(L, 1, "color");
        luaL_argcheck(L, c != NULL, 1, "'color' expected");
        num = color_to_unsigned(*c);
    }

    sprintf(buf, "%08X", num);

    lua_pushlstring(L, buf + (8 - len), len);
    return 1;
}



static int lp_unbinary(lua_State *L)
{
    char c;
    unsigned num, len, i;
    const char *str = luaL_checkstring(L, 1);
    len = strlen(str);

    num = 0;
    for (i = 0; i < len; ++i) {
        c = *(str + len - i - 1);
        if (c == '0' || c == '1') {
            c -= '0';
        }
        else {
            luaL_error(L, "character out of range");
        }
        num += c * pow(2, i);
    }

    lua_pushnumber(L, num);
    return 1;
}



static int lp_unhex(lua_State *L)
{
    char c;
    unsigned num, len, i;
    const char *str = luaL_checkstring(L, 1);
    len = strlen(str);

    num = 0;
    for (i = 0; i < len; ++i) {
        c = *(str + len - i - 1);
        if (isdigit(c)) {
            c -= '0';
        }
        else if (c >= 'a' && c <= 'f') {
            c -= 'a';
            c += 10;
        }
        else if (c >= 'A' && c <= 'F') {
            c -= 'A';
            c += 10;
        }
        else {
            luaL_error(L, "character out of range");
        }
        num += c * pow(16, i);
    }

    lua_pushnumber(L, num);
    return 1;
}



static int lp_constrain(lua_State *L)
{
    double v, min, max;
    v = luaL_checknumber(L, 1);
    min = luaL_checknumber(L, 2);
    max = luaL_checknumber(L, 3);
    lua_pushnumber(L, MIN(MAX(v, min), max));
    return 1;
}



static int lp_dist(lua_State *L)
{
    double x1, y1, z1, x2, y2, z2, num;

    if (lua_gettop(L) == 4) {
        x1 = luaL_checknumber(L, 1);
        y1 = luaL_checknumber(L, 2);
        x2 = luaL_checknumber(L, 3);
        y2 = luaL_checknumber(L, 4);
        num = sqrt(SQ(x1 - x2) + SQ(y1 - y2));
    }
    else if (lua_gettop(L) == 6) {
        x1 = luaL_checknumber(L, 1);
        y1 = luaL_checknumber(L, 2);
        z1 = luaL_checknumber(L, 3);
        x2 = luaL_checknumber(L, 4);
        y2 = luaL_checknumber(L, 5);
        z2 = luaL_checknumber(L, 6);
        num = sqrt(SQ(x1 - x2) + SQ(y1 - y2) + SQ(z1 - z2));
    }
    else {
        luaL_error(L, "incorrect number of arguments");
    }

    lua_pushnumber(L, num);
    return 1;
}



static int lp_map(lua_State *L)
{
    double v, low1, high1, low2, high2, num;

    v = luaL_checknumber(L, 1);
    low1 = luaL_checknumber(L, 2);
    high1 = luaL_checknumber(L, 3);
    low2 = luaL_checknumber(L, 4);
    high2 = luaL_checknumber(L, 5);

    num = (v - low1) / (high1 - low1) * (high2 - low2) + low2;

    lua_pushnumber(L, num);
    return 1;
}



static int lp_norm(lua_State *L)
{
    double v, low, high, num;

    v = luaL_checknumber(L, 1);
    low = luaL_checknumber(L, 2);
    high = luaL_checknumber(L, 3);

    num = (v - low) / (high - low);

    lua_pushnumber(L, num);
    return 1;
}



static int lp_mag(lua_State *L)
{
    double x, y, z, num;

    if (lua_gettop(L) == 2) {
        x = luaL_checknumber(L, 1);
        y = luaL_checknumber(L, 2);
        num = sqrt(SQ(x) + SQ(y));
    }
    else if (lua_gettop(L) == 3) {
        x = luaL_checknumber(L, 1);
        y = luaL_checknumber(L, 2);
        z = luaL_checknumber(L, 3);
        num = sqrt(SQ(x) + SQ(y) + SQ(z));
    }
    else {
        luaL_error(L, "incorrect number of arguments");
    }

    lua_pushnumber(L, num);
    return 1;
}



static int lp_lerp(lua_State *L)
{
    double a, b, t;

    a = luaL_checknumber(L, 1);
    b = luaL_checknumber(L, 2);
    t = luaL_checknumber(L, 3);

    lua_pushnumber(L, a + t * (b - a));
    return 1;
}



static int lp_sq(lua_State *L)
{
    double a;

    a = luaL_checknumber(L, 1);

    lua_pushnumber(L, SQ(a));
    return 1;
}

/******************************************************************************\
Lua Processing
\******************************************************************************/

LUALIB_API int luaopen_luaprocessing(lua_State *L)
{
    lua_pop(L, 1); /* Popping luaprocessing string. */

    lua_pushnumber(L, time(NULL));
    lp_noise_seed(L);
    lp_random_seed(L); lua_pop(L, 1);

    /* System */
    lua_pushnumber(L, 100);
    lua_setglobal(L, "width");
    lua_pushnumber(L, 100);
    lua_setglobal(L, "height");

    lua_pushcfunction(L, lp_run);
    lua_setglobal(L, "run");

    lua_pushcfunction(L, lp_size);
    lua_setglobal(L, "size");

    /* Input */
    lua_pushnumber(L, LEFT);
    lua_setglobal(L, "LEFT");
    lua_pushnumber(L, RIGHT);
    lua_setglobal(L, "RIGHT");
    lua_pushnumber(L, CENTER);
    lua_setglobal(L, "CENTER");

    lua_pushnumber(L, 0);
    lua_setglobal(L, "pmouseX");
    lua_pushnumber(L, 0);
    lua_setglobal(L, "pmouseY");
    lua_pushnumber(L, 0);
    lua_setglobal(L, "mouseX");
    lua_pushnumber(L, 0);
    lua_setglobal(L, "mouseY");

    lua_pushnumber(L, -1);
    lua_setglobal(L, "mouseButton");

    /* Color */
    luaL_newmetatable(L, "color"); lua_pop(L, 1);

    lua_pushnumber(L, RGB);
    lua_setglobal(L, "RGB");
    lua_pushnumber(L, HSB);
    lua_setglobal(L, "HSB");

    lua_pushnumber(L, BLEND);
    lua_setglobal(L, "BLEND");
    lua_pushnumber(L, ADD);
    lua_setglobal(L, "ADD");
    lua_pushnumber(L, SUBTRACT);
    lua_setglobal(L, "SUBTRACT");
    lua_pushnumber(L, DARKEST);
    lua_setglobal(L, "DARKEST");
    lua_pushnumber(L, LIGHTEST);
    lua_setglobal(L, "LIGHTEST");
    lua_pushnumber(L, DIFFERENCE);
    lua_setglobal(L, "DIFFERENCE");
    lua_pushnumber(L, EXCLUSION);
    lua_setglobal(L, "EXCLUSION");
    lua_pushnumber(L, MULTIPLY);
    lua_setglobal(L, "MULTIPLY");
    lua_pushnumber(L, SCREEN);
    lua_setglobal(L, "SCREEN");
    lua_pushnumber(L, OVERLAY);
    lua_setglobal(L, "OVERLAY");
    lua_pushnumber(L, HARD_LIGHT);
    lua_setglobal(L, "HARD_LIGHT");
    lua_pushnumber(L, SOFT_LIGHT);
    lua_setglobal(L, "SOFT_LIGHT");
    lua_pushnumber(L, DODGE);
    lua_setglobal(L, "DODGE");
    lua_pushnumber(L, BURN);
    lua_setglobal(L, "BURN");

    lua_pushcfunction(L, lp_color);
    lua_setglobal(L, "color");

    lua_pushcfunction(L, lp_red);
    lua_setglobal(L, "red");

    lua_pushcfunction(L, lp_green);
    lua_setglobal(L, "green");

    lua_pushcfunction(L, lp_blue);
    lua_setglobal(L, "blue");

    lua_pushcfunction(L, lp_alpha);
    lua_setglobal(L, "alpha");

    lua_pushcfunction(L, lp_hue);
    lua_setglobal(L, "hue");

    lua_pushcfunction(L, lp_saturation);
    lua_setglobal(L, "saturation");

    lua_pushcfunction(L, lp_brightness);
    lua_setglobal(L, "brightness");

    lua_pushcfunction(L, lp_lerp_color);
    lua_setglobal(L, "lerpColor");

    lua_pushcfunction(L, lp_color_mode);
    lua_setglobal(L, "colorMode");

    lua_pushcfunction(L, lp_blend_color);
    lua_setglobal(L, "blendColor");

    /* Radnom/Noise */
    lua_pushcfunction(L, lp_noise);
    lua_setglobal(L, "noise");

    lua_pushcfunction(L, lp_noise_seed);
    lua_setglobal(L, "noiseSeed");

    lua_pushcfunction(L, lp_noise_detail);
    lua_setglobal(L, "noiseDetail");

    lua_pushcfunction(L, lp_random);
    lua_setglobal(L, "random");

    lua_pushcfunction(L, lp_random_seed);
    lua_setglobal(L, "randomSeed");

    /* Math */
    lua_pushnumber(L, LP_PI);
    lua_setglobal(L, "PI");

    lua_pushnumber(L, LP_TWO_PI);
    lua_setglobal(L, "TWO_PI");

    lua_pushnumber(L, LP_HALF_PI);
    lua_setglobal(L, "HALF_PI");

    lua_pushnumber(L, LP_QUARTER_PI);
    lua_setglobal(L, "QUARTER_PI");

    lua_pushcfunction(L, lp_binary);
    lua_setglobal(L, "binary");

    lua_pushcfunction(L, lp_hex);
    lua_setglobal(L, "hex");

    lua_pushcfunction(L, lp_unbinary);
    lua_setglobal(L, "unbinary");

    lua_pushcfunction(L, lp_unhex);
    lua_setglobal(L, "unhex");

    lua_pushcfunction(L, lp_constrain);
    lua_setglobal(L, "constrain");

    lua_pushcfunction(L, lp_dist);
    lua_setglobal(L, "dist");

    lua_pushcfunction(L, lp_map);
    lua_setglobal(L, "map");

    lua_pushcfunction(L, lp_norm);
    lua_setglobal(L, "norm");

    lua_pushcfunction(L, lp_mag);
    lua_setglobal(L, "mag");

    lua_pushcfunction(L, lp_lerp);
    lua_setglobal(L, "lerp");

    lua_pushcfunction(L, lp_sq);
    lua_setglobal(L, "sq");
    return 0;
}



#ifdef TEST
#include <stdio.h>

int main(void)
{
    struct color c;
    HUE(c) = 91.0 / 360.0;
    SATURATION(c) = 0.996;
    VALUE(c) = 1.0;
    printf("Hue: %f\n", HUE(c));
    printf("Saturation: %f\n", SATURATION(c));
    printf("Value: %f\n", VALUE(c));

    c = hsb_to_rgb(c);
    printf("Red: %d\n", (int)(RED(c) * 255));
    printf("Green: %d\n", (int)(GREEN(c) * 255));
    printf("Blue: %d\n", (int)(BLUE(c) * 255));

    c = rgb_to_hsb(c);
    printf("Hue: %f\n", HUE(c));
    printf("Saturation: %f\n", SATURATION(c));
    printf("Value: %f\n", VALUE(c));
    return 0;
}
#endif
