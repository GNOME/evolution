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

extern GnumericCursorDef  gnumeric_cursors [];

void    cursors_init      (void);
void    cursors_shutdown  (void);

#define cursor_set(win,c) \
 do {  									\
     if (win) 								\
         gdk_window_set_cursor (win, gnumeric_cursors [c].cursor); 	\
} while (0)

#define cursor_set_widget(w,c) \
 do {  									\
     if (GTK_WIDGET (w)->window) 					\
	gdk_window_set_cursor (GTK_WIDGET (w)->window, gnumeric_cursors [c].cursor); \
} while (0)
  

#endif /* GNUMERIC_CURSORS_H */
