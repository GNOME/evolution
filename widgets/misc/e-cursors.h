#ifndef CURSORS_H
#define CURSORS_H

typedef struct {
	GdkCursor *cursor;
	int       hot_x, hot_y;
	char      **xpm;
} GnumericCursorDef;

#define GNUMERIC_CURSOR_FAT_CROSS  0
#define GNUMERIC_CURSOR_THIN_CROSS 1
#define GNUMERIC_CURSOR_ARROW      2

extern GnumericCursorDef  gnumeric_cursors [];

void    cursors_init      (void);
void    cursors_shutdown  (void);

#define cursor_set(win,c) \
	gdk_window_set_cursor (win, gnumeric_cursors [c].cursor)

#define cursor_set_widget(w,c) \
	gdk_window_set_cursor (GTK_WIDGET (w)->window, gnumeric_cursors [c].cursor)

#endif
