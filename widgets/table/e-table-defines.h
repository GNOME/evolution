#ifndef __E_TABLE_DEFINES__
#define __E_TABLE_DEFINES__ 1

#define BUTTON_HEIGHT        10
#define BUTTON_PADDING       2
#define GROUP_INDENT         (BUTTON_HEIGHT + (BUTTON_PADDING * 2))

/* Padding around the contents of a header button */
#define HEADER_PADDING 1

#define MIN_ARROW_SIZE 10

typedef void (*ETableForeachFunc) (int model_row,
				   gpointer closure);

/* list selection modes */
typedef enum
{
	E_TABLE_CURSOR_LINE,
	E_TABLE_CURSOR_SIMPLE,
} ETableCursorMode;

#endif
