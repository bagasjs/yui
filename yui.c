#include "yui.h"
#include <assert.h>

#define internal static

internal int measure_text(yui_Ctx *ctx, void *font, const char *text, int font_size)
{
    if(ctx->config.measure_text) return ctx->config.measure_text(font, text, font_size);
    return 0;
}

internal void draw_text(yui_Ctx *ctx, void *font, const char *text, int font_size, int x, int y, yui_Color tint)
{
    if(ctx->config.draw_text)
        ctx->config.draw_text(font, text, font_size, x, y, tint);
}

internal void draw_rect(yui_Ctx *ctx, yui_Rect rect, yui_Color color, float roundness)
{
    if(ctx->config.draw_rect)
        ctx->config.draw_rect(rect, color, roundness);
}

internal void draw_rect_outline(yui_Ctx *ctx, yui_Rect rect, yui_Color color, int thickness)
{
    if(ctx->config.draw_rect)
        ctx->config.draw_rect_outline(rect, color, thickness);
}

internal inline void _add_box_child(yui_Box *parent, yui_Box *child)
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

internal inline void _reset_box(yui_Box *box)
{
    box->children.begin = NULL;
    box->children.end   = NULL;
    box->children.count = 0;
    box->parent = NULL;
    box->next   = NULL;
    box->layout = (yui_BoxLayout){0};
}

#define POINT_IN_RECT(R, X, Y) (((R).x <= (X) && (X) < (R).x + (R).w) && ((R).y <= (Y) && (Y) < (R).y + (R).h))

yui_Box *yui_hit_test(yui_Box *box, int x, int y)
{
    if(POINT_IN_RECT(box->layout.padding_box, x, y)) {
        for(yui_Box *child = box->children.begin; child != NULL; child = child->next) {
            yui_Box *result = yui_hit_test(child, x, y);
            if(result != NULL)
                return result;
        }
        return box;
    }
    return NULL;
}

void yui_begin_frame(yui_Ctx *ctx, uint32_t root_width, uint32_t root_height)
{
    ctx->count_boxes = 0;
    yui_Box *root = &ctx->root;
    _reset_box(root);
    root->config.sizing.x_axis = YUI_BOX_SIZING_FIXED;
    root->config.sizing.y_axis = YUI_BOX_SIZING_FIXED;
    root->config.fixed_width  = root_width;
    root->config.fixed_height = root_height;
    root->id = 0;
    ctx->curr = root;
}

yui_Box *yui_open_box(yui_Ctx *ctx, yui_BoxConfig config)
{
    assert(ctx->count_boxes + 1 <= YUI_BOXES_CAP);
    ctx->level += 1;
    yui_Box *prev = ctx->curr;
    int id = ctx->count_boxes + 1;
    yui_Box *curr = &ctx->boxes[ctx->count_boxes++];
    _reset_box(curr);
    curr->id = id;
    curr->level = ctx->level;
    curr->config = config;
    _add_box_child(prev, curr);
    ctx->curr = curr;
    return curr;
}

void yui_close_box(yui_Ctx *ctx)
{
    ctx->level -= 1;
    ctx->curr = ctx->curr->parent;
}

void yui_text_box(yui_Ctx *ctx, const char *text, yui_TextConfig text_config)
{
    yui_BoxConfig config = {0};
    config.text = text_config;
    config.sizing.x_axis = YUI_BOX_SIZING_FIXED;
    config.sizing.y_axis = YUI_BOX_SIZING_FIXED;
    int height = config.text.font_size;
    int width  = measure_text(ctx, config.text.font, text, height);
    config.fixed_width  = width;
    config.fixed_height = height;
    yui_open_box(ctx, config);
    ctx->curr->text = text;
    yui_close_box(ctx);
}

#include <stdio.h>
internal void dumb_box(yui_Box *box, const char *label)
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

internal void _compute_fit_sizing_on(yui_Ctx *ctx, yui_Box *parent, yui_Box *box, bool x_axis)
{
    int content_size = 0;
    yui_BoxSizing sizing = x_axis ? box->config.sizing.x_axis : box->config.sizing.y_axis;
    yui_ContentDirection aligned_direction = YUI_CONTENT_LEFT_TO_RIGHT;
    if(!x_axis) aligned_direction = YUI_CONTENT_TOP_TO_BOTTOM;

    for(yui_Box *child = box->children.begin; child != NULL; child = child->next) {
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
            if(child->config.sizing.x_axis == YUI_BOX_SIZING_GROW) {
                box->layout.count_children_with_grow_box_on_x_axis += 1;
            }
        } else {
            if(child->config.sizing.y_axis == YUI_BOX_SIZING_GROW) {
                box->layout.count_children_with_grow_box_on_y_axis += 1;
            }
        }
    }

    if(sizing == YUI_BOX_SIZING_FIXED) {
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

internal void _compute_grow_sizing_on(yui_Ctx *ctx, yui_Box *parent, yui_Box *box, bool x_axis)
{
    yui_BoxSizing sizing = x_axis ? box->config.sizing.x_axis : box->config.sizing.y_axis;
    yui_ContentDirection aligned_direction = YUI_CONTENT_LEFT_TO_RIGHT;
    if(!x_axis) aligned_direction = YUI_CONTENT_TOP_TO_BOTTOM;
    switch(sizing) {
    case YUI_BOX_SIZING_FIXED:
    case YUI_BOX_SIZING_FIT:
        {
            if(x_axis) {
                box->layout.padding_box.w = box->layout.content_box.w + box->config.padding.l + box->config.padding.r;
                box->layout.margin_box.w = box->layout.padding_box.w + box->config.margin.l + box->config.margin.r;
            } else {
                box->layout.padding_box.h = box->layout.content_box.h + box->config.padding.t + box->config.padding.b;
                box->layout.margin_box.h = box->layout.padding_box.h + box->config.margin.t + box->config.margin.b;
            }
        } break;
    case YUI_BOX_SIZING_GROW:
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

    for(yui_Box *child = box->children.begin; child != NULL; child = child->next) {
        _compute_grow_sizing_on(ctx, box, child, x_axis);
    }
}

internal void _compute_pos_on(yui_Ctx *ctx, yui_Box *parent, yui_Box *box, bool x_axis) 
{
    if(parent->config.content_dir == YUI_CONTENT_LEFT_TO_RIGHT) {
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
        if(box->config.sizing.x_axis == YUI_BOX_SIZING_FIXED) {
            box->layout.cursor_x += box->layout.content_box.w;
        } else {
            for(yui_Box *child = box->children.begin; child != NULL; child = child->next) {
                _compute_pos_on(ctx, box, child, x_axis);
            }
        }
        parent->layout.cursor_x += box->layout.margin_box.w;
    } else {
        if(box->config.sizing.y_axis == YUI_BOX_SIZING_FIXED) {
            box->layout.cursor_y += box->layout.content_box.h;
        } else {
            for(yui_Box *child = box->children.begin; child != NULL; child = child->next) {
                _compute_pos_on(ctx, box, child, x_axis);
            }
        }
        parent->layout.cursor_y += box->layout.margin_box.h;
    }
}

internal void _render(yui_Ctx *ctx, yui_Box *parent, yui_Box *box)
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
        for(yui_Box *child = box->children.begin; child != NULL; child = child->next)
            _render(ctx, box, child);
    }
}

void yui_end_frame(yui_Ctx *ctx)
{
    yui_Box *root = &ctx->root;
    _compute_fit_sizing_on(ctx, NULL, root, true);
    _compute_fit_sizing_on(ctx, NULL, root, false);
    _compute_grow_sizing_on(ctx, NULL, root, true);
    _compute_grow_sizing_on(ctx, NULL, root, false);
    for(yui_Box *child = ctx->root.children.begin; child != NULL; child = child->next) {
        _compute_pos_on(ctx, root, child, true);
        _compute_pos_on(ctx, root, child, false);
    }
    _render(ctx, NULL, root);
}
