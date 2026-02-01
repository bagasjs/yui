#include <raylib.h>
#include <stdint.h>
#include <assert.h>

typedef enum {
    FLEX_TOP_TO_BOTTOM = 0,
    FLEX_LEFT_TO_RIGHT,
} FlexDirection;

typedef enum {
    BOX_SIZING_FIT_TO_CONTENT = 0,
    BOX_SIZING_FIXED,
} BoxSizing;

typedef struct { int x, y, w, h; } Rect;
typedef struct { int l, t, r, b; } Bound;

typedef enum {
    VALUE_PIXELS = 0,
    VALUE_PERCENT,
} ValueKind;

typedef struct {
    ValueKind kind;
    float value;
} ConfigValue;

typedef struct { ConfigValue width, height; } ConfigSize;
typedef struct { ConfigValue l,t,r,b; } ConfigBound;

typedef struct {
    FlexDirection flex_direction;
    BoxSizing sizing;
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
    BoxSizing sizing;
    Rect padding_box;
    Rect margin_box;
    Rect content_box;
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

static void draw_rect_outline(Ctx *ctx, Rect rect, Color color, int thickness)
{
    if(ctx->config.draw_rect)
        ctx->config.draw_rect_outline(rect, color, thickness);
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

#define POINT_IN_RECT(R, X, Y) (((R).x <= (X) && (X) < (R).x + (R).w) && ((R).y <= (Y) && (Y) < (R).y + (R).h))

static Box *_hit_test(Box *box, int x, int y)
{
    if(POINT_IN_RECT(box->layout.padding_box, x, y)) {
        for(Box *child = box->children.begin; child != NULL; child = child->next) {
            Box *result = _hit_test(child, x, y);
            if(result != NULL)
                return result;
        }
        return box;
    }
    return NULL;
}

void begin_frame(Ctx *ctx, uint32_t root_width, uint32_t root_height)
{
    ctx->count_boxes = 0;
    Box *root = &ctx->root;
    _reset_box(root);
    root->layout.content_box = (Rect){ .x = 0, .y = 0, .w = root_width, .h = root_height };
    root->layout.padding_box = (Rect){ .x = 0, .y = 0, .w = root_width, .h = root_height };
    root->layout.margin_box  = (Rect){ .x = 0, .y = 0, .w = root_width, .h = root_height };
    root->layout.sizing = BOX_SIZING_FIXED;
    root->id = 0;
    ctx->curr = root;
}

Box *open_box(Ctx *ctx, BoxConfig config)
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
    return curr;
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
        printf("%s: %*s [%d] \"%s\" content[x=%d y=%d w=%d h=%d] padding[x=%d y=%d w=%d h=%d] margin[x=%d y=%d w=%d h=%d] cursor[x=%d y=%d]\n", 
                label, box->level*4, "", box->id, box->text, 
                box->layout.content_box.x, box->layout.content_box.y, 
                box->layout.content_box.w, box->layout.content_box.h,
                box->layout.padding_box.x, box->layout.padding_box.y, 
                box->layout.padding_box.w, box->layout.padding_box.h,
                box->layout.margin_box.x, box->layout.margin_box.y, 
                box->layout.margin_box.w, box->layout.margin_box.h,
                box->layout.cursor_x, box->layout.cursor_y
                );
    } else {
        printf("%s: %*s [%d] content[x=%d y=%d w=%d h=%d] padding[x=%d y=%d w=%d h=%d] margin[x=%d y=%d w=%d h=%d] cursor[x=%d y=%d]\n", 
                label, box->level*4, "", box->id,
                box->layout.content_box.x, box->layout.content_box.y, 
                box->layout.content_box.w, box->layout.content_box.h,
                box->layout.padding_box.x, box->layout.padding_box.y, 
                box->layout.padding_box.w, box->layout.padding_box.h,
                box->layout.margin_box.x, box->layout.margin_box.y, 
                box->layout.margin_box.w, box->layout.margin_box.h,
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
        box->layout.content_box.w = width;
        box->layout.content_box.h = height;
        box->layout.padding_box.w = width;
        box->layout.padding_box.h = height;
        box->layout.margin_box.w = width;
        box->layout.margin_box.h = height;
        box->layout.sizing  = BOX_SIZING_FIXED;
        return;
    } 

    // TODO: It would be cool if we separate x and y sizing
    box->layout.sizing  = box->config.sizing;
    int content_width  = 0;
    int content_height = 0;
    for(Box *child = box->children.begin; child != NULL; child = child->next) {
        _compute_size(ctx, box, child);
        if(box->config.flex_direction == FLEX_LEFT_TO_RIGHT) {
            content_width  += child->layout.margin_box.w;
            content_height = MY_MAX(content_height, child->layout.margin_box.h);
        } else {
            content_width   = MY_MAX(content_width, child->layout.margin_box.w);
            content_height += child->layout.margin_box.h;
        }
    }
    box->layout.content_box.w = content_width;
    box->layout.content_box.h = content_height;
    box->layout.padding_box.w = content_width  + box->config.padding.l + box->config.padding.r;
    box->layout.padding_box.h = content_height + box->config.padding.t + box->config.padding.b;
    box->layout.margin_box.w = box->layout.padding_box.w + box->config.margin.l + box->config.margin.r;
    box->layout.margin_box.h = box->layout.padding_box.h + box->config.margin.t + box->config.margin.b;
}

static void _compute_pos(Ctx *ctx, Box *parent, Box *box)
{
    if(parent->config.flex_direction == FLEX_LEFT_TO_RIGHT) {
        box->layout.cursor_x = parent->layout.cursor_x;
        box->layout.cursor_y = parent->layout.content_box.y;
    } else {
        box->layout.cursor_x = parent->layout.content_box.x;
        box->layout.cursor_y = parent->layout.cursor_y;
    }
    // apply top-left margin
    box->layout.margin_box.x = box->layout.cursor_x;
    box->layout.margin_box.y = box->layout.cursor_y;
    box->layout.cursor_x += box->config.margin.l;
    box->layout.cursor_y += box->config.margin.t;
    box->layout.padding_box.x = box->layout.cursor_x;
    box->layout.padding_box.y = box->layout.cursor_y;
    // apply top-left padding
    box->layout.cursor_x += box->config.padding.l;
    box->layout.cursor_y += box->config.padding.t;
    box->layout.content_box.x = box->layout.cursor_x;
    box->layout.content_box.y = box->layout.cursor_y;

    if(box->layout.sizing == BOX_SIZING_FIXED) {
        box->layout.cursor_x += box->layout.content_box.w;
        box->layout.cursor_y += box->layout.content_box.h;
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

static int v = 0;
static void _render(Ctx *ctx, Box *parent, Box *box)
{
    if(v < ctx->count_boxes) {
        dumb_box(box, "_render");
        v++;
    }
    (void)parent;
    if(box->text) {
        draw_text(ctx, box->config.text.font, box->text, box->config.text.font_size, 
                box->layout.content_box.x, box->layout.content_box.y, box->config.color);
    } else {
        draw_rect(ctx, box->layout.padding_box, box->config.color, 0);
        for(Box *child = box->children.begin; child != NULL; child = child->next)
            _render(ctx, box, child);
        draw_rect_outline(ctx, box->layout.padding_box, RED, 2);
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

void raylib_draw_rect_outline(Rect r, Color color, int border_width)
{
    DrawRectangleLinesEx((Rectangle){r.x,r.y,r.w,r.h}, border_width, color);
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
    ctx->config.draw_rect_outline = raylib_draw_rect_outline;

    /*SetConfigFlags(FLAG_WINDOW_RESIZABLE);*/
    InitWindow(800, 600, "Simple UI");

    /*Font font = GetFontDefault();*/
    Font font = LoadFont("./assets/fonts/JetBrainsMono/ttf/JetBrainsMono-Regular.ttf");
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    Bound margin  = (Bound){ .l = 10, .t = 10, .r = 10, .b = 10 };
    Bound padding = (Bound){ .l = 10, .t = 10, .r = 10, .b = 10 };
    BoxConfig text_config = {0};
    text_config.text.font_size = 24;
    text_config.text.font = &font;

    TempAtor ator = {0};
    char buf[1024];
    ator.items = buf;
    ator.size  = sizeof(buf);

    Color normal = GetColor(0x181818FF);
    Color hover  = GetColor(0xABABABFF);
    Color active = GetColor(0xBA1818FF);
    bool  is_active = false;
    Color color  = normal;

    while(!WindowShouldClose()) {
        BeginDrawing();
        begin_frame(ctx, GetScreenWidth(), GetScreenHeight());
        Box *top = open_box(ctx, (BoxConfig){ 
            .flex_direction = FLEX_LEFT_TO_RIGHT, 
            .margin = margin, 
            .color = color, 
            .padding = padding 
        });
            open_box(ctx, (BoxConfig){ 
                    .flex_direction = FLEX_TOP_TO_BOTTOM, 
                    .margin = margin, 
                    .padding = padding, 
                    .color = normal, 
            });
                text_box(ctx, "Hello Short", text_config);
                text_box(ctx, "Hello a long message 1", text_config);
                text_box(ctx, "Hello a long message 2", text_config);
            close_box(ctx);
            text_box(ctx, "Hello Right Part", text_config);
        close_box(ctx);
        end_frame(ctx);
        Vector2 v = GetMousePosition();
        Box *box  = _hit_test(top, v.x, v.y);
        if(box) {
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                is_active = !is_active;
            }
            color = is_active ? active : hover;
        } else {
            if(is_active) color = active;
            else color = normal;
        }
        ator.allocated = 0;

        EndDrawing();
    }

    CloseWindow();

    return 0;
}

/*
# TODO:
0. Implement image
1. Implement different sizing strategy for each axis. For example the width of 
   a box width would be BOX_SIZING_FIXED  but the height would be BOX_SIZING_FIT_TO_CONTENT.
   This will require _compute_pos and _compute_size to handle each axis separately
2. Implement border and shadow for left, right, top and bottom separately
3. Implement this:
```
typedef enum {
    VALUE_PX = 0,
    VALUE_PERCENT
} ValueKind;
typedef struct {
    ValueKind kind;
    float value; // -inf..inf for VALUE_PX but 0..1 for VALUE_PERCENT
} Value;
typedef struct {
    Value width, height;
} ConfigSize;
typedef struct {
    Value l, t, r, b;
} ConfigBound; // we name this ConfigXXX because we only use these data structures in BoxConfig
```
*/
