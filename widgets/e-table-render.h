#ifndef E_TABLE_RENDER_H
#define E_TABLE_RENDER_H

#include <libgnomeui/gnome-canvas.h>

struct ERenderContext {
	ETableCol       *etc;
	int              row;
	int              base_x, base_y;
	GnomeCanvasItem *gnome_canvas_item;
	GdkDrawable     *drawable;
	int              drawable_width;
	int              drawable_height;
	void            *render_data;
	void            *closure;
};

void e_table_render_string (ERenderContext *ctxt);


#endif
