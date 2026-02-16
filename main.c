#include <raylib.h>
#include <stdint.h>
#include <assert.h>

typedef enum {
    OVERFLOW_SCROLL = 0,
    OVERFLOW_HIDDEN,
} OverflowMode;

typedef enum {
    CONTENT_TOP_TO_BOTTOM = 0,
    CONTENT_LEFT_TO_RIGHT,
} ContentDirection;

typedef enum {
    BOX_SIZING_FIT = 0,
    BOX_SIZING_FIXED,
    BOX_SIZING_GROW,
} BoxSizing;

typedef struct { int x, y, w, h; } Rect;
typedef struct { int l, t, r, b; } Bound;

typedef struct {
    void *font;
    int font_size;
    Color color;
} TextConfig;

typedef struct {
    // TODO
    struct {
        OverflowMode x_axis;
        OverflowMode y_axis;
    } overflow;
    // END of TODO
    struct {
        BoxSizing x_axis;
        BoxSizing y_axis;
    } sizing;

    ContentDirection content_dir;
    int fixed_width;
    int fixed_height;
    Bound padding;
    Bound margin;
    Color background_color;
    TextConfig text;
} BoxConfig;

typedef struct Box Box;
typedef struct {
    Rect padding_box;
    Rect margin_box;
    Rect content_box;
    uint32_t cursor_x;
    uint32_t cursor_y;

    uint32_t count_grow_box_children;
    uint32_t count_children_with_grow_box_on_x_axis;
    uint32_t count_children_with_grow_box_on_y_axis;
    int filled_width;
    int filled_height;
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
typedef void (*BeginScissorModePfn)(Rect rect);
typedef void (*EndScissorModePfn)(void);

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
        BeginScissorModePfn begin_scissor_mode;
        EndScissorModePfn end_scissor_mode;
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
    root->config.sizing.x_axis = BOX_SIZING_FIXED;
    root->config.sizing.y_axis = BOX_SIZING_FIXED;
    root->config.fixed_width  = root_width;
    root->config.fixed_height = root_height;
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

void text_box(Ctx *ctx, const char *text, TextConfig text_config)
{
    BoxConfig config = {0};
    config.text = text_config;
    config.sizing.x_axis = BOX_SIZING_FIXED;
    config.sizing.y_axis = BOX_SIZING_FIXED;
    int height = config.text.font_size;
    int width  = measure_text(ctx, config.text.font, text, height);
    config.fixed_width  = width;
    config.fixed_height = height;
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

static void _compute_fit_sizing_on(Ctx *ctx, Box *parent, Box *box, bool x_axis)
{
    int content_size = 0;
    BoxSizing sizing = x_axis ? box->config.sizing.x_axis : box->config.sizing.y_axis;
    ContentDirection aligned_direction = CONTENT_LEFT_TO_RIGHT;
    if(!x_axis) aligned_direction = CONTENT_TOP_TO_BOTTOM;

    for(Box *child = box->children.begin; child != NULL; child = child->next) {
        _compute_fit_sizing_on(ctx, box, child, x_axis);
        int child_margin_box_size = 0;
        if(x_axis) {
            child_margin_box_size = child->layout.content_box.w + child->config.padding.l + child->config.padding.r +
                child->config.margin.l + child->config.margin.r;
        } else {
            child_margin_box_size = child->layout.content_box.h + child->config.padding.t + child->config.padding.b +
                child->config.margin.t + child->config.margin.b;
        }

        if(box->config.content_dir == aligned_direction) {
            content_size += child_margin_box_size;
        } else {
            content_size  = MY_MAX(content_size, child_margin_box_size);
        }

        if(x_axis) {
            if(child->config.sizing.x_axis == BOX_SIZING_GROW) {
                box->layout.count_children_with_grow_box_on_x_axis += 1;
            }
        } else {
            if(child->config.sizing.y_axis == BOX_SIZING_GROW) {
                box->layout.count_children_with_grow_box_on_y_axis += 1;
            }
        }
    }

    if(sizing == BOX_SIZING_FIXED) {
        if(x_axis) {
            box->layout.content_box.w = box->config.fixed_width;
        } else {
            box->layout.content_box.h = box->config.fixed_height;
        }
    } else {
        if(x_axis) {
            box->layout.content_box.w = content_size;
        } else {
            box->layout.content_box.h = content_size;
        }
    }
    if(x_axis) {
        box->layout.filled_width  = content_size;
    } else {
        box->layout.filled_height = content_size;
    }
}

static void _compute_grow_sizing_on(Ctx *ctx, Box *parent, Box *box, bool x_axis)
{
    BoxSizing sizing = x_axis ? box->config.sizing.x_axis : box->config.sizing.y_axis;
    ContentDirection aligned_direction = CONTENT_LEFT_TO_RIGHT;
    if(!x_axis) aligned_direction = CONTENT_TOP_TO_BOTTOM;
    switch(sizing) {
    case BOX_SIZING_FIXED:
    case BOX_SIZING_FIT:
        {
            if(x_axis) {
                box->layout.padding_box.w = box->layout.content_box.w + box->config.padding.l + box->config.padding.r;
                box->layout.margin_box.w = box->layout.padding_box.w + box->config.margin.l + box->config.margin.r;
            } else {
                box->layout.padding_box.h = box->layout.content_box.h + box->config.padding.t + box->config.padding.b;
                box->layout.margin_box.h = box->layout.padding_box.h + box->config.margin.t + box->config.margin.b;
            }
        } break;
    case BOX_SIZING_GROW:
        {
            if(parent) {
                int pgbc = x_axis 
                    ? parent->layout.count_children_with_grow_box_on_x_axis
                    : parent->layout.count_children_with_grow_box_on_y_axis;
                if(pgbc == 0) pgbc = 0;
                if(parent->config.content_dir == aligned_direction) {
                    if(x_axis) {
                        box->layout.content_box.w += (parent->layout.content_box.w - parent->layout.filled_width)/pgbc;
                    } else {
                        box->layout.content_box.h += (parent->layout.content_box.h - parent->layout.filled_height)/pgbc;
                    }
                } else {
                    if(x_axis) {
                        box->layout.content_box.w  = parent->layout.content_box.w;
                    } else {
                        box->layout.content_box.h  =  parent->layout.content_box.h;
                    }
                }
            }

            // TODO: This will make the width of the paddding_box & margin_box bigger than the parent's content_box
            //       we need to handle padding and margin using the free space not like this
            if(x_axis) {
                box->layout.padding_box.w = box->layout.content_box.w + box->config.padding.l + box->config.padding.r;
                box->layout.margin_box.w = box->layout.padding_box.w + box->config.margin.l + box->config.margin.r;
            } else {
                box->layout.padding_box.h = box->layout.content_box.h + box->config.padding.t + box->config.padding.b;
                box->layout.margin_box.h = box->layout.padding_box.h + box->config.margin.t + box->config.margin.b;
            }
        } break;
    }

    for(Box *child = box->children.begin; child != NULL; child = child->next) {
        _compute_grow_sizing_on(ctx, box, child, x_axis);
    }
}

static void _compute_pos_on(Ctx *ctx, Box *parent, Box *box, bool x_axis) 
{
    if(parent->config.content_dir == CONTENT_LEFT_TO_RIGHT) {
        if(x_axis) box->layout.cursor_x = parent->layout.cursor_x;
        else box->layout.cursor_y = parent->layout.content_box.y;
    } else {
        if(x_axis) box->layout.cursor_x = parent->layout.content_box.x;
        else box->layout.cursor_y = parent->layout.cursor_y;
    }

    // Apply margin and padding
    if(x_axis) {
        box->layout.margin_box.x = box->layout.cursor_x;
        box->layout.cursor_x += box->config.margin.l;
        box->layout.padding_box.x = box->layout.cursor_x;
        box->layout.cursor_x += box->config.padding.l;
        box->layout.content_box.x = box->layout.cursor_x;
    } else {
        box->layout.margin_box.y = box->layout.cursor_y;
        box->layout.cursor_y += box->config.margin.t;
        box->layout.padding_box.y = box->layout.cursor_y;
        box->layout.cursor_y += box->config.padding.t;
        box->layout.content_box.y = box->layout.cursor_y;
    }

    if(x_axis) {
        if(box->config.sizing.x_axis == BOX_SIZING_FIXED) {
            box->layout.cursor_x += box->layout.content_box.w;
        } else {
            for(Box *child = box->children.begin; child != NULL; child = child->next) {
                _compute_pos_on(ctx, box, child, x_axis);
            }
        }
        parent->layout.cursor_x += box->layout.margin_box.w;
    } else {
        if(box->config.sizing.y_axis == BOX_SIZING_FIXED) {
            box->layout.cursor_y += box->layout.content_box.h;
        } else {
            for(Box *child = box->children.begin; child != NULL; child = child->next) {
                _compute_pos_on(ctx, box, child, x_axis);
            }
        }
        parent->layout.cursor_y += box->layout.margin_box.h;
    }
}

static void _render(Ctx *ctx, Box *parent, Box *box)
{
    static int v = 0;
    if(v < ctx->count_boxes + 1) {
        dumb_box(box, __FUNCTION__);
        v++;
    }
    if(box->text) {
        draw_text(ctx, box->config.text.font, box->text, box->config.text.font_size, 
                box->layout.content_box.x, box->layout.content_box.y, box->config.text.color);
    } else {
        draw_rect(ctx, box->layout.padding_box, box->config.background_color, 0);
        for(Box *child = box->children.begin; child != NULL; child = child->next)
            _render(ctx, box, child);
        draw_rect_outline(ctx, box->layout.margin_box, RED, 1);
    }
}

void end_frame(Ctx *ctx)
{
    Box *root = &ctx->root;
    _compute_fit_sizing_on(ctx, NULL, root, true);
    _compute_fit_sizing_on(ctx, NULL, root, false);
    _compute_grow_sizing_on(ctx, NULL, root, true);
    _compute_grow_sizing_on(ctx, NULL, root, false);
    for(Box *child = ctx->root.children.begin; child != NULL; child = child->next) {
        _compute_pos_on(ctx, root, child, true);
        _compute_pos_on(ctx, root, child, false);
    }
    _render(ctx, NULL, root);
}

// main

void raylib_begin_scissor_mode(Rect r) 
{
    BeginScissorMode(r.x, r.y, r.w, r.h);
}

void raylib_end_scissor_mode(void)
{
    EndScissorMode();
}

int raylib_measure_text(void *font_ptr, const char *text, int font_size)
{
    Font font = *(Font*)font_ptr;
    Vector2 size = MeasureTextEx(font, text, font_size, 1);
    return size.x;
}

void raylib_draw_text(void *font_ptr, const char *text, int font_size, int x, int y, Color tint)
{
    Font font = *(Font*)font_ptr;
    DrawTextEx(font, text, (Vector2){ x, y }, font_size, 1, tint);
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

TempAtor ator = {0};
char buf[1024];
Color normal_background_color;
Color hover_background_color;
Color active_background_color;
Color background_color;
Color normal_text_color;
Color hover_text_color;
Color active_text_color;
Color text_color;
bool  is_active;
Font font;

void init(Ctx *ctx)
{
    (void)ctx;
    /*Font font = GetFontDefault();*/
    font = LoadFont("./assets/fonts/JetBrainsMono/ttf/JetBrainsMono-Regular.ttf");
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    ator.items = buf;
    ator.size  = sizeof(buf);

    normal_background_color = GetColor(0xF3F2F1FF);
    hover_background_color  = GetColor(0x106EBEFF);
    active_background_color = GetColor(0x005A9EFF);
    normal_text_color = BLACK;
    hover_text_color  = WHITE;
    active_text_color = WHITE;
    is_active = false;
    text_color = normal_text_color;
    background_color = normal_background_color;
}

void draw(Ctx *ctx)
{
    begin_frame(ctx, GetScreenWidth(), GetScreenHeight());
    Box *top = open_box(ctx, (BoxConfig){ 
        .content_dir = CONTENT_LEFT_TO_RIGHT, 
        .sizing = { BOX_SIZING_GROW, BOX_SIZING_GROW },
        .background_color = normal_background_color,
    });
        open_box(ctx, (BoxConfig) { .padding = (Bound){.l=5,.t=5,.r=5,.b=5}, .sizing = { BOX_SIZING_GROW, BOX_SIZING_GROW } });
            text_box(ctx, "TEST", (TextConfig){ .color = normal_text_color, .font = &font, .font_size = 18 });
        close_box(ctx);
        open_box(ctx, (BoxConfig) {
            .content_dir = CONTENT_TOP_TO_BOTTOM,
            .padding = (Bound){ .l = 200, .t = 10, .r = 200, .b = 10 },
        });
            Box *box = open_box(ctx, (BoxConfig){ .margin = (Bound){ .b = 10 }, .background_color = background_color, });
                text_box(ctx, "Hello, A", (TextConfig){ .color = text_color, .font = &font, .font_size = 18 });
            close_box(ctx);
            open_box(ctx, (BoxConfig){ .margin = (Bound){ .b = 10 } });
                text_box(ctx, "Hello, B", (TextConfig){ .color = normal_text_color, .font = &font, .font_size = 18 });
            close_box(ctx);
        close_box(ctx);
        open_box(ctx, (BoxConfig) { .padding = (Bound){.l=5,.t=5,.r=5,.b=5}, .sizing = { BOX_SIZING_GROW, BOX_SIZING_GROW } });
            text_box(ctx, "TEST", (TextConfig){ .color = normal_text_color, .font = &font, .font_size = 18 });
        close_box(ctx);
    close_box(ctx);
    end_frame(ctx);
    Vector2 v = GetMousePosition();
    Box *hit;
    hit = _hit_test(box, v.x, v.y);
    if(hit) {
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            is_active = !is_active;
        }
        background_color = is_active ? active_background_color : hover_background_color;
        text_color = is_active ? active_text_color : hover_text_color;
    } else {
        background_color = is_active ? active_background_color : normal_background_color;
        text_color = is_active ? active_text_color : normal_text_color;
    }
}

int main(void)
{
    Ctx _ctx = {0};
    Ctx *ctx = &_ctx;
    ctx->config.measure_text = raylib_measure_text;
    ctx->config.draw_text    = raylib_draw_text;
    ctx->config.draw_rect    = raylib_draw_rect;
    ctx->config.draw_rect_outline = raylib_draw_rect_outline;
    ctx->config.begin_scissor_mode = raylib_begin_scissor_mode;
    ctx->config.end_scissor_mode = raylib_end_scissor_mode;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Simple UI");

    init(ctx);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        ator.allocated = 0;

        draw(ctx);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
