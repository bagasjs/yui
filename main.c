#include <raylib.h>
#include <stdint.h>
#include <assert.h>

typedef enum {
    FLEX_TOP_TO_BOTTOM = 0,
    FLEX_LEFT_TO_RIGHT,
} FlexDirection;

typedef struct {
    uint32_t child_gap;
    FlexDirection flex_direction;
    Color color;
    struct {
        void *font;
        int font_size;
    } text;
} BoxConfig;

typedef struct Box Box;
typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t min_width;
    uint32_t min_height;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t x_ptr;
    uint32_t y_ptr;
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
typedef void (*DrawRectPfn)(int x, int y, int width, int height, Color color);

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
    } config;
} Ctx;

int measure_text(Ctx *ctx, void *font, const char *text, int font_size)
{
    if(ctx->config.measure_text) return ctx->config.measure_text(font, text, font_size);
    return 0;
}

void draw_text(Ctx *ctx, void *font, const char *text, int font_size, int x, int y, Color tint)
{
    if(ctx->config.draw_text)
        ctx->config.draw_text(font, text, font_size, x, y, tint);
}

void draw_rect(Ctx *ctx, int x, int y, int width, int height, Color color)
{
    if(ctx->config.draw_rect)
        ctx->config.draw_rect(x, y, width, height, color);
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
    root->layout.width  = root_width;
    root->layout.height = root_height;
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
        printf("%s: %*s [%d] \"%s\" w=%d h=%d x=%d y=%d\n", 
                label, box->level*4, "",
                box->id, box->text, box->layout.width, box->layout.height, box->layout.x, box->layout.y);
    } else {
        printf("%s: %*s [%d] w=%d h=%d x=%d y=%d\n", 
                label, box->level*4, "",
                box->id, box->layout.width, box->layout.height, box->layout.x, box->layout.y);
    }
}

#define MY_MIN(A, B) ((A) < (B) ? (A) : (B))
#define MY_MAX(A, B) ((A) > (B) ? (A) : (B))

static void _compute_size(Ctx *ctx, Box *parent, Box *box)
{
    if(box->text) { // text box is a special box
        int height = box->config.text.font_size;
        int width  = measure_text(ctx, box->config.text.font, box->text, height);
        box->layout.width  = width;
        box->layout.height = height;
    } else {
        Box *child = box->children.begin;
        if(!child) return;
        _compute_size(ctx, box, child);
        int min_width  = child->layout.width;
        int min_height = child->layout.height;
        int max_width  = min_width;
        int max_height = min_height;
        int box_width  = min_width;
        int box_height = min_height;
        child = child->next;
        for(; child != NULL; child = child->next) {
            _compute_size(ctx, box, child);
            int width  = child->layout.width;
            int height = child->layout.height;
            min_width  = MY_MIN(width, min_width);
            min_height = MY_MIN(height, min_height);
            max_width  = MY_MAX(width, max_width);
            max_height = MY_MAX(height, max_height);
            box_width  += width;
            box_height += height;
        }
        box->layout.min_width  = min_width;
        box->layout.min_height = min_height;
        box->layout.max_width  = max_width; 
        box->layout.max_height = max_height;
        box->layout.width  = box_width;
        box->layout.height = box_height;
    }
}

static inline void _incr_ptr(Box *box, bool x, uint32_t value)
{
    if(x) box->layout.x_ptr += value;
    else  box->layout.y_ptr += value;
}

static void _compute_pos(Ctx *ctx, Box *parent, Box *box)
{
    box->layout.x = parent->layout.x_ptr;
    box->layout.y = parent->layout.y_ptr;
    parent->layout.x_ptr += box->layout.width;
    parent->layout.y_ptr += box->layout.height;
    for(Box *child = box->children.begin; child != NULL; child = child->next)
        _compute_pos(ctx, box, child);
}

static void _render(Ctx *ctx, Box *parent, Box *box)
{
    if(box->text) {
        draw_text(ctx, box->config.text.font, box->text, box->config.text.font_size, 
                box->layout.x, box->layout.y, box->config.color);
    } else {
        for(Box *child = box->children.begin; child != NULL; child = child->next)
            _render(ctx, box, child);
    }
}

void end_frame(Ctx *ctx)
{
    // layout pass-es
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

void raylib_draw_rect(int x, int y, int width, int height, Color color)
{
    DrawRectangle(x, y, width, height, color);
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
    default_config.flex_direction  = FLEX_TOP_TO_BOTTOM;
    default_config.child_gap = 10;
    BoxConfig text_config = {0};
    text_config.text.font_size = 16;
    text_config.text.font = &font;

    while(!WindowShouldClose()) {
        BeginDrawing();
        begin_frame(ctx, GetScreenWidth(), GetScreenHeight());
        open_box(ctx, default_config);
            open_box(ctx, default_config);
                text_box(ctx, "Hello A", text_config);
                text_box(ctx, "Hello B", text_config);
            close_box(ctx);
            text_box(ctx, "Hello C", text_config);
        close_box(ctx);
        end_frame(ctx);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
