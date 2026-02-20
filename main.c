#include <raylib.h>
#include <stdint.h>
#include <assert.h>
#include "yui.h"
#include <stdio.h>

#define TRANSLATE_COLOR(YUI) (Color){ .r=(YUI).r, .g=(YUI).g, .b=(YUI).b, .a =(YUI).a }

void raylib_begin_scissor_mode(yui_Rect r) 
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

void raylib_draw_text(void *font_ptr, const char *text, int font_size, int x, int y, yui_Color tint)
{
    Font font = *(Font*)font_ptr;
    DrawTextEx(font, text, (Vector2){ x, y }, font_size, 1, TRANSLATE_COLOR(tint));
}

void raylib_draw_rect(yui_Rect r, yui_Color color, float roundness)
{
    DrawRectangleRounded((Rectangle){r.x,r.y,r.w,r.h}, roundness, 20, TRANSLATE_COLOR(color));
}

void raylib_draw_rect_outline(yui_Rect r, yui_Color color, int border_width)
{
    DrawRectangleLinesEx((Rectangle){r.x,r.y,r.w,r.h}, border_width, TRANSLATE_COLOR(color));
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
yui_Color normal_background_color;
yui_Color hover_background_color;
yui_Color active_background_color;
yui_Color background_color;
yui_Color normal_text_color;
yui_Color hover_text_color;
yui_Color active_text_color;
yui_Color text_color;
bool  is_active;
Font font;

void init(yui_Ctx *ctx)
{
    (void)ctx;
    /*Font font = GetFontDefault();*/
    font = LoadFont("./assets/fonts/JetBrainsMono/ttf/JetBrainsMono-Regular.ttf");
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    ator.items = buf;
    ator.size  = sizeof(buf);
    normal_background_color = (yui_Color) { .r=0xF3, .g=0xF2, .b=0xF1, .a=0xFF };
    hover_background_color  = (yui_Color) { .r=0x10, .g=0x6E, .b=0xBE, .a=0xFF };
    active_background_color = (yui_Color) { .r=0x00, .g=0x5A, .b=0x9E, .a=0xFF };
    normal_text_color = YUI_COLOR_BLACK;
    hover_text_color  = YUI_COLOR_WHITE;
    active_text_color = YUI_COLOR_WHITE;
    is_active = false;
    text_color = normal_text_color;
    background_color = normal_background_color;
}

void draw(yui_Ctx *ctx)
{
    yui_begin_frame(ctx, GetScreenWidth(), GetScreenHeight());
    yui_Box *top = yui_open_box(ctx, (yui_BoxConfig){ 
        .content_dir = YUI_CONTENT_LEFT_TO_RIGHT, 
        .sizing = { YUI_BOX_SIZING_GROW, YUI_BOX_SIZING_GROW },
        .background_color = normal_background_color,
    });
        yui_open_box(ctx, (yui_BoxConfig) { 
            .padding = (yui_Bound){.l=5,.t=5,.r=5,.b=5}, 
            .sizing  = { YUI_BOX_SIZING_GROW, YUI_BOX_SIZING_GROW } 
        });
            yui_text_box(ctx, "TEST", (yui_TextConfig){ .color = normal_text_color, .font = &font, .font_size = 18 });
        yui_close_box(ctx);
        yui_open_box(ctx, (yui_BoxConfig) {
            .content_dir = YUI_CONTENT_TOP_TO_BOTTOM,
            .padding = (yui_Bound){ .l = 200, .t = 10, .r = 200, .b = 10 },
        });
            yui_Box *box = yui_open_box(ctx, (yui_BoxConfig){ 
                    .margin = (yui_Bound){ .b = 10 }, 
                    .background_color = background_color, });
                yui_text_box(ctx, "Hello, A", (yui_TextConfig){ .color = text_color, .font = &font, .font_size = 18 });
            yui_close_box(ctx);
            yui_open_box(ctx, (yui_BoxConfig){ .margin = (yui_Bound){ .b = 10 } });
                yui_text_box(ctx, "Hello, B", (yui_TextConfig){ .color = normal_text_color, .font = &font, .font_size = 18 });
            yui_close_box(ctx);
        yui_close_box(ctx);
        yui_open_box(ctx, (yui_BoxConfig) { 
                .padding = (yui_Bound){.l=5,.t=5,.r=5,.b=5}, 
                .sizing  = { YUI_BOX_SIZING_GROW, YUI_BOX_SIZING_GROW } });
            yui_text_box(ctx, "TEST", (yui_TextConfig){ .color = normal_text_color, .font = &font, .font_size = 18 });
        yui_close_box(ctx);
    yui_close_box(ctx);
    yui_end_frame(ctx);
    Vector2 v = GetMousePosition();
    yui_Box *hit;
    hit = yui_hit_test(box, v.x, v.y);
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
    yui_Ctx _ctx = {0};
    yui_Ctx *ctx = &_ctx;
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
