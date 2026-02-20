#ifndef YUI_H_
#define YUI_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    YUI_OVERFLOW_SCROLL = 0,
    YUI_OVERFLOW_HIDDEN,
} yui_OverflowMode;

typedef enum {
    YUI_CONTENT_TOP_TO_BOTTOM = 0,
    YUI_CONTENT_LEFT_TO_RIGHT,
} yui_ContentDirection;

typedef enum {
    YUI_BOX_SIZING_FIT = 0,
    YUI_BOX_SIZING_FIXED,
    YUI_BOX_SIZING_GROW,
} yui_BoxSizing;

typedef struct { uint8_t r, g, b, a; } yui_Color;
typedef struct { int     x, y, w, h; } yui_Rect;
typedef struct { int     l, t, r, b; } yui_Bound;
#define YUI_COLOR_BLACK (yui_Color) { .a=0xFF }
#define YUI_COLOR_WHITE (yui_Color) { .r=0xFF, .g=0xFF, .b=0xFF, .a=0xFF }

typedef struct {
    void *font;
    int font_size;
    yui_Color color;
} yui_TextConfig;

typedef struct {
    // TODO
    struct {
        yui_OverflowMode x_axis;
        yui_OverflowMode y_axis;
    } overflow;
    // END of TODO
    struct {
        yui_BoxSizing x_axis;
        yui_BoxSizing y_axis;
    } sizing;

    yui_ContentDirection content_dir;
    int fixed_width;
    int fixed_height;
    yui_Bound padding;
    yui_Bound margin;
    yui_Color background_color;
    yui_TextConfig text;
} yui_BoxConfig;

typedef struct {
    yui_Rect padding_box;
    yui_Rect margin_box;
    yui_Rect content_box;
    uint32_t cursor_x;
    uint32_t cursor_y;

    uint32_t count_grow_box_children;
    uint32_t count_children_with_grow_box_on_x_axis;
    uint32_t count_children_with_grow_box_on_y_axis;
    int filled_width;
    int filled_height;
} yui_BoxLayout;

typedef struct yui_Box yui_Box;
struct yui_Box {
    uint32_t id;
    uint32_t level;
    yui_Box *next;
    yui_Box *parent;
    struct {
        yui_Box *begin;
        yui_Box *end;
        uint32_t count;
    } children;
    const char *text;

    yui_BoxLayout layout;
    yui_BoxConfig config;
};

typedef int  (*yui_MeasureTextPfn)(void *font, const char *text, int font_size);
typedef void (*yui_DrawTextPfn)(void *font, const char *text, int font_size, int x, int y, yui_Color tint);
typedef void (*yui_DrawRectPfn)(yui_Rect rect, yui_Color color, float roundness);
typedef void (*yui_DrawRectOutlinePfn)(yui_Rect rect, yui_Color color, int border_width);
typedef void (*yui_BeginScissorModePfn)(yui_Rect rect);
typedef void (*yui_EndScissorModePfn)(void);

#define YUI_BOXES_CAP 1024
typedef struct {
    yui_Box  root;
    yui_Box *curr;
    uint32_t level;
    yui_Box boxes[YUI_BOXES_CAP];
    uint32_t count_boxes;

    struct {
        yui_MeasureTextPfn measure_text;
        yui_DrawTextPfn draw_text;
        yui_DrawRectPfn draw_rect;
        yui_DrawRectOutlinePfn draw_rect_outline;
        yui_BeginScissorModePfn begin_scissor_mode;
        yui_EndScissorModePfn end_scissor_mode;
    } config;
} yui_Ctx;

void yui_begin_frame(yui_Ctx *ctx, uint32_t w, uint32_t h);
void yui_end_frame(yui_Ctx *ctx);
yui_Box *yui_open_box(yui_Ctx *ctx, yui_BoxConfig config);
void yui_close_box(yui_Ctx *ctx);
void yui_text_box(yui_Ctx *ctx, const char *text, yui_TextConfig text_config);

yui_Box *yui_hit_test(yui_Box *box, int x, int y);

#endif // YUI_H_
