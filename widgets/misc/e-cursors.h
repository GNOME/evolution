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

void    cursors_init     (void);
void    cursors_shutdown (void);
#endif
