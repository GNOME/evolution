#ifndef GNUMERIC_CURSORS_H
#define GNUMERIC_CURSORS_H

typedef struct {
	GdkCursor *cursor;
	int       hot_x, hot_y;
	char      **xpm;
} GnumericCursorDef;

#define GNUMERIC_CURSOR_FAT_CROSS  0
#define GNUMERIC_CURSOR_THIN_CROSS 1
#define GNUMERIC_CURSOR_ARROW      2
#define GNUMERIC_CURSOR_MOVE       3
#define GNUMERIC_CURSOR_ZOOM_IN    4
#define GNUMERIC_CURSOR_ZOOM_OUT   5

extern GnumericCursorDef  gnumeric_cursors [];

void    cursors_init      (void);
void    cursors_shutdown  (void);

#define cursor_set(win,c)					   \
G_STMT_START {							   \
     if (win) 							   \
         gdk_window_set_cursor (win, gnumeric_cursors [c].cursor); \
} G_STMT_END

#define cursor_set_widget(w,c)							     \
G_STMT_START {									     \
     if (GTK_WIDGET (w)->window)						     \
	gdk_window_set_cursor (GTK_WIDGET (w)->window, gnumeric_cursors [c].cursor); \
} G_STMT_END
  

#endif /* GNUMERIC_CURSORS_H */

