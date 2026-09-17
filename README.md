# Yui
A small UI library which is in the middle of retained and immediate mode UI.

## Getting Started
1. Grab the `yui.h` and `yui.c`
2. Open your `main.c` and write the following code (assuming you're using raylib)
```c
#include "yui.h"
#include "raylib.h"

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

int main(void)
{
    // Initialize the context
    yui_Backend backend = {0};
    backend.measure_text = raylib_measure_text;
    backend.draw_text    = raylib_draw_text;
    backend.draw_rect    = raylib_draw_rect;
    backend.draw_rect_outline = raylib_draw_rect_outline;
    backend.begin_scissor_mode = raylib_begin_scissor_mode;
    backend.end_scissor_mode = raylib_end_scissor_mode;

    yui_Ctx _ctx = {0};
    yui_Ctx *ctx = &_ctx;
    yui_init(ctx, backend);

    Font font = GetFontDefault();

    // Initialize raylib (or your own backend)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Simple UI");

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        yui_begin_frame(ctx, GetScreenWidth(), GetScreenHeight());
        yui_text_box(ctx, "Fuwa Fuwa Time!", (yui_TextConfig){ 
            .color = (yui_Color){ 0x20, 0x21, 0x22, 0xFF },
            .font = &font, /* It's just a void* and it will be passed to your draw_text function */ 
            .font_size = 18,
        });
        yui_end_frame(ctx);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
```
