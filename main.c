#include <raylib.h>
#include <stdint.h>
#include <assert.h>

typedef enum {
    FLEX_TOP_TO_BOTTOM = 0,
    FLEX_LEFT_TO_RIGHT,
} FlexDirection;

typedef struct { int x, y, w, h; } Rect;
typedef struct { int l, t, r, b; } Bound;

typedef struct {
    FlexDirection flex_direction;
    Bound padding;
    Bound margin;
    Color color;
    struct {
        void *font;
        int font_size;
    } text;
} BoxConfig;

typedef struct Box Box;
typedef struct {
    bool fixed_sized;
    Rect inner;
    Rect outer;
    uint32_t cursor_x;
    uint32_t cursor_y;
} BoxLayout;

struct Box {
    uint32_t id;
    uint32_t level;
    Box *next;
    Box *parent;
    struct {
        Box *begin;
        Box *end;
        uint32_t count;
    } children;
    const char *text;

    BoxLayout layout;
    BoxConfig config;
};

typedef int  (*MeasureTextPfn)(void *font, const char *text, int font_size);
typedef void (*DrawTextPfn)(void *font, const char *text, int font_size, int x, int y, Color tint);
typedef void (*DrawRectPfn)(Rect rect, Color color, float roundness);
typedef void (*DrawRectOutlinePfn)(Rect rect, Color color, int border_width);

#define BOXES_CAP 1024
typedef struct Ctx {
    Box  root;
    Box *curr;
    uint32_t level;
    Box boxes[BOXES_CAP];
    uint32_t count_boxes;

    struct {
        MeasureTextPfn measure_text;
        DrawTextPfn draw_text;
        DrawRectPfn draw_rect;
        DrawRectOutlinePfn draw_rect_outline;
    } config;
} Ctx;

static int measure_text(Ctx *ctx, void *font, const char *text, int font_size)
{
    if(ctx->config.measure_text) return ctx->config.measure_text(font, text, font_size);
    return 0;
}

static void draw_text(Ctx *ctx, void *font, const char *text, int font_size, int x, int y, Color tint)
{
    if(ctx->config.draw_text)
        ctx->config.draw_text(font, text, font_size, x, y, tint);
}

static void draw_rect(Ctx *ctx, Rect rect, Color color, float roundness)
{
    if(ctx->config.draw_rect)
        ctx->config.draw_rect(rect, color, roundness);
}

static inline void _add_box_child(Box *parent, Box *child)
{
    if(parent->children.begin == NULL) {
        parent->children.begin = child;
        parent->children.end   = child;
    } else {
        parent->children.end->next = child;
        parent->children.end  = child;
    }
    parent->children.count += 1;
    child->parent = parent;
}

static inline void _reset_box(Box *box)
{
    box->children.begin = NULL;
    box->children.end   = NULL;
    box->children.count = 0;
    box->parent = NULL;
    box->next   = NULL;
    box->layout = (BoxLayout){0};
}

void begin_frame(Ctx *ctx, uint32_t root_width, uint32_t root_height)
{
    ctx->count_boxes = 0;
    Box *root = &ctx->root;
    _reset_box(root);
    root->layout.inner = (Rect){ .x = 0, .y = 0, .w = root_width, .h = root_height };
    root->layout.outer = (Rect){ .x = 0, .y = 0, .w = root_width, .h = root_height };
    root->id = 0;
    ctx->curr = root;
}

void open_box(Ctx *ctx, BoxConfig config)
{
    assert(ctx->count_boxes + 1 <= BOXES_CAP);
    ctx->level += 1;
    Box *prev = ctx->curr;
    int id = ctx->count_boxes + 1;
    Box *curr = &ctx->boxes[ctx->count_boxes++];
    _reset_box(curr);
    curr->id = id;
    curr->level = ctx->level;
    curr->config = config;
    _add_box_child(prev, curr);
    ctx->curr = curr;
}

void close_box(Ctx *ctx)
{
    ctx->level -= 1;
    ctx->curr = ctx->curr->parent;
}

void text_box(Ctx *ctx, const char *text, BoxConfig config)
{
    open_box(ctx, config);
    ctx->curr->text = text;
    close_box(ctx);
}

#include <stdio.h>
static void dumb_box(Box *box, const char *label)
{
    if(box->text) {
        printf("%s: %*s [%d] \"%s\" outer[x=%d y=%d w=%d h=%d] inner[x=%d y=%d w=%d h=%d] ptr[x=%d y=%d]\n", 
                label, box->level*4, "", box->id, box->text, 
                box->layout.outer.x, box->layout.outer.y, box->layout.outer.w, box->layout.outer.h,
                box->layout.inner.x, box->layout.inner.y, box->layout.inner.w, box->layout.inner.h,
                box->layout.cursor_x, box->layout.cursor_y
                );
    } else {
        printf("%s: %*s [%d] outer[x=%d y=%d w=%d h=%d] inner[x=%d y=%d w=%d h=%d] ptr[x=%d y=%d]\n", 
                label, box->level*4, "", box->id,
                box->layout.outer.x, box->layout.outer.y, box->layout.outer.w, box->layout.outer.h,
                box->layout.inner.x, box->layout.inner.y, box->layout.inner.w, box->layout.inner.h,
                box->layout.cursor_x, box->layout.cursor_y
                );
    }
}

#define MY_MIN(A, B) ((A) < (B) ? (A) : (B))
#define MY_MAX(A, B) ((A) > (B) ? (A) : (B))

static void _compute_size(Ctx *ctx, Box *parent, Box *box)
{
    (void)parent;

    if(box->text) { // text box is a special box
        int height = box->config.text.font_size;
        int width  = measure_text(ctx, box->config.text.font, box->text, height);
        box->layout.inner.w = width;
        box->layout.inner.h = height;
        box->layout.outer.w = width;
        box->layout.outer.h = height;
        box->layout.fixed_sized = true;
    } else {
        Box *child = box->children.begin;
        if(!child) return;
        _compute_size(ctx, box, child);
        int box_width  = child->layout.outer.w + child->config.margin.l + child->config.margin.r;
        int box_height = child->layout.outer.h + child->config.margin.t + child->config.margin.b;
        child = child->next;
        for(; child != NULL; child = child->next) {
            _compute_size(ctx, box, child);
            int width  = child->layout.outer.w;
            int height = child->layout.outer.h;
            if(box->config.flex_direction == FLEX_LEFT_TO_RIGHT) 
                box_width  += width  + child->config.margin.l + child->config.margin.r;
            else 
                box_height += height + child->config.margin.t + child->config.margin.b;
        }
        box->layout.inner.w = box_width;
        box->layout.inner.h = box_height;
        box->layout.outer.w = box_width  + box->config.padding.l + box->config.padding.r;
        box->layout.outer.h = box_height + box->config.padding.t + box->config.padding.b;
    }
}

static void _compute_pos(Ctx *ctx, Box *parent, Box *box)
{
    if(parent->config.flex_direction == FLEX_LEFT_TO_RIGHT) {
        box->layout.cursor_x = parent->layout.cursor_x;
        box->layout.cursor_y = parent->layout.inner.y;
    } else {
        box->layout.cursor_x = parent->layout.inner.x;
        box->layout.cursor_y = parent->layout.cursor_y;
    }
    // apply top-left margin
    box->layout.cursor_x += box->config.margin.l;
    box->layout.cursor_y += box->config.margin.t;
    box->layout.outer.x = box->layout.cursor_x;
    box->layout.outer.y = box->layout.cursor_y;
    // apply top-left padding
    box->layout.cursor_x += box->config.padding.l;
    box->layout.cursor_y += box->config.padding.t;
    box->layout.inner.x = box->layout.cursor_x;
    box->layout.inner.y = box->layout.cursor_y;

    if(box->layout.fixed_sized) {
        box->layout.cursor_x += box->layout.inner.w;
        box->layout.cursor_y += box->layout.inner.h;
    } else {
        for(Box *child = box->children.begin; child != NULL; child = child->next) {
            _compute_pos(ctx, box, child);
        }
    }
    // apply right-bottom padding
    box->layout.cursor_x += box->config.padding.r;
    box->layout.cursor_y += box->config.padding.b;
    // apply right-bottom margin
    box->layout.cursor_x += box->config.margin.r;
    box->layout.cursor_y += box->config.margin.b;

    parent->layout.cursor_x = box->layout.cursor_x;
    parent->layout.cursor_y = box->layout.cursor_y;
}

static void _render(Ctx *ctx, Box *parent, Box *box)
{
    (void)parent;
    if(box->text) {
        draw_rect(ctx, box->layout.outer, RED, 0);
        draw_text(ctx, box->config.text.font, box->text, box->config.text.font_size, 
                box->layout.inner.x, box->layout.inner.y, box->config.color);
    } else {
        for(Box *child = box->children.begin; child != NULL; child = child->next)
            _render(ctx, box, child);
    }
}

void end_frame(Ctx *ctx)
{
    for(Box *child = ctx->root.children.begin; child != NULL; child = child->next)
        _compute_size(ctx, &ctx->root, child);
    for(Box *child = ctx->root.children.begin; child != NULL; child = child->next)
        _compute_pos(ctx, &ctx->root, child);
    for(Box *child = ctx->root.children.begin; child != NULL; child = child->next)
        _render(ctx, &ctx->root, child);
}

// main

int raylib_measure_text(void *font_ptr, const char *text, int font_size)
{
    Font font = *(Font*)font_ptr;
    Vector2 size = MeasureTextEx(font, text, font_size, 1);
    return size.x;
}

void raylib_draw_text(void *font_ptr, const char *text, int font_size, int x, int y, Color tint)
{
    Font font = *(Font*)font_ptr;
    DrawTextEx(font, text, (Vector2){ x, y }, font_size, 1, WHITE);
}

void raylib_draw_rect(Rect r, Color color, float roundness)
{
    DrawRectangleRounded((Rectangle){r.x,r.y,r.w,r.h}, roundness, 20, color);
}

void raylib_draw_rect_outline(int x, int y, int width, int height, Color color, int border_width)
{
    DrawRectangleLinesEx((Rectangle){ .x=x, .y=y, .width=width, .height=height}, border_width, color);
}

typedef struct {
    char *items;
    uint32_t size;
    uint32_t allocated;
} TempAtor;

char *temp_sprintf(TempAtor *ator, const char *fmt, ...)
{
    va_list ap;
    va_start(ap,fmt);
    va_list copy_ap;
    va_copy(copy_ap, ap);
    int needed = vsnprintf(NULL, 0, fmt, copy_ap);
    va_end(copy_ap);

    if (needed <= 0) {
        va_end(ap);
        return NULL;
    }
    int required = ator->allocated + needed + 1;
    if(required > ator->size) {
        va_end(ap);
        return NULL;
    }

    char *dst = &ator->items[ator->allocated];

    vsnprintf(dst, needed + 1, fmt, ap);
    ator->allocated += (uint32_t)needed + 1;
    va_end(ap);

    return dst;
}

int main(void)
{
    Ctx _ctx = {0};
    Ctx *ctx = &_ctx;
    ctx->config.measure_text = raylib_measure_text;
    ctx->config.draw_text    = raylib_draw_text;
    ctx->config.draw_rect    = raylib_draw_rect;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Simple UI");

    Font font = GetFontDefault();

    BoxConfig default_config = {0};
    default_config.flex_direction = FLEX_TOP_TO_BOTTOM;
    default_config.margin = (Bound){ .l = 10, .t = 10, .r = 10, .b = 10 };
    BoxConfig text_config = {0};
    text_config.text.font_size = 24;
    text_config.text.font = &font;

    TempAtor ator = {0};
    char buf[1024];
    ator.items = buf;
    ator.size  = sizeof(buf);

    while(!WindowShouldClose()) {
        BeginDrawing();
        begin_frame(ctx, GetScreenWidth(), GetScreenHeight());
        default_config.flex_direction = FLEX_LEFT_TO_RIGHT;
        open_box(ctx, default_config);
        default_config.flex_direction = FLEX_TOP_TO_BOTTOM;
            open_box(ctx, default_config);
                for(int i = 0; i < 10; ++i) {
                    text_box(ctx, temp_sprintf(&ator, "Hello %d", i), text_config);
                }
            close_box(ctx);
            text_box(ctx, "Hello Right Part", text_config);
        close_box(ctx);
        end_frame(ctx);
        ator.allocated = 0;

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
