#ifndef GNUMERIC_CURSORS_H
#define GNUMERIC_CURSORS_H

#include <gdk/gdk.h>

typedef enum {
	GNUMERIC_CURSOR_FAT_CROSS,
	GNUMERIC_CURSOR_THIN_CROSS,
	GNUMERIC_CURSOR_ARROW,
	GNUMERIC_CURSOR_MOVE,
	GNUMERIC_CURSOR_ZOOM_IN,
	GNUMERIC_CURSOR_ZOOM_OUT,
	GNUMERIC_CURSOR_SIZE_X,
	GNUMERIC_CURSOR_SIZE_Y,
	GNUMERIC_CURSOR_SIZE_TL,
	GNUMERIC_CURSOR_SIZE_TR,
	GNUMERIC_CURSOR_PRESS,
	GNUMERIC_CURSOR_HAND_OPEN,
	GNUMERIC_CURSOR_HAND_CLOSED,
	GNUMERIC_CURSOR_NUM_CURSORS
} CursorType;

void    cursors_init      (void);
void    cursors_shutdown  (void);

#define cursor_set(win, c)					   \
G_STMT_START {							   \
     if (win) 							   \
         gdk_window_set_cursor (win, cursor_get (c)); \
} G_STMT_END

#define cursor_set_widget(w, c)							     \
G_STMT_START {									     \
     if (GTK_WIDGET (w)->window)						     \
	gdk_window_set_cursor (GTK_WIDGET (w)->window, cursor_get (c)); \
} G_STMT_END

GdkCursor *cursor_get (CursorType type);

#endif /* GNUMERIC_CURSORS_H */
