/* General-purpose monthly calendar canvas item for GNOME
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GNOME_MONTH_ITEM_H
#define GNOME_MONTH_ITEM_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtkpacker.h> /* why the hell is GtkAnchorType here and not in gtkenums.h? */
#include <libgnomeui/gnome-canvas.h>


BEGIN_GNOME_DECLS


/* These values are used to identify the canvas items that make up a complete GnomeMonthItem, which
 * is made up of the following "pieces":
 *
 *	Headings line:
 *		- 7 GnomeCanvasGroups:
 *			Each group contains one box (GnomeCanvasRectangle) and one label
 *			(GnomeCanvasText)
 *
 *	Day slots:
 *		- 42 GnomeCanvasGroups:
 *			Each group contains one box (GnomeCanvasRectangle) and one label
 *			(GnomeCanvasText)
 *
 * The headings are organized from left to right.  The day slots are organized as a table in
 * row-major order.
 *
 * If you want to access the individual items of the GnomeMonthItem, you can use these numbers with
 * the gnome_month_item_num2child() function.  If you want to convert a number into the
 * corresponding GnomeCanvasItem, you can use the gnome_month_item_child2num() function.
 */
typedef enum {
	GNOME_MONTH_ITEM_HEAD_GROUP = 0,	/* 7 groups for headings */
	GNOME_MONTH_ITEM_HEAD_BOX   = 7,	/* 7 boxes for headings */
	GNOME_MONTH_ITEM_HEAD_LABEL = 14,	/* 7 labels for headings */
	GNOME_MONTH_ITEM_DAY_GROUP  = 21,	/* 42 groups for days */
	GNOME_MONTH_ITEM_DAY_BOX    = 63,	/* 42 boxes for days */
	GNOME_MONTH_ITEM_DAY_LABEL  = 105,	/* 42 labels for days */
	GNOME_MONTH_ITEM_LAST       = 147	/* total number of items */
} GnomeMonthItemChild;

/* The MonthItem canvas item defines a simple monthly calendar.  It is made out of a number of
 * canvas items, which can be accessed using the functions provided.  The monthly calendar is
 * anchored with respect to a point.  The following arguments are available:
 *
 * name			type		read/write	description
 * ------------------------------------------------------------------------------------------
 * year			uint		RW		Full year (1-9999)
 * month		uint		RW		Number of month (0-11)
 * x			double		RW		X position of anchor point
 * y			double		RW		Y position of anchor point
 * width		double		RW		Width of calendar in canvas units
 * height		double		RW		Height of calendar in canvas units
 * anchor		GtkAnchorType	RW		Anchor side for calendar
 * heading_padding	double		RW		Padding inside heading boxes
 * day_padding		double		RW		Padding inside day boxes
 * day_names		char **		W		Array of strings corresponding to the day names (sun-sat)
 * heading_height	double		RW		Height of headings bar in canvas units
 * heading_anchor	GtkAnchorType	RW		Anchor side for headings inside heading boxes
 * day_anchor		GtkAnchorType	RW		Anchor side for day numbers inside day boxes
 * start_on_monday	boolean		RW		Specifies whether the week starts on Monday or Sunday
 * heading_font		string		W		X logical font descriptor for the headings
 * heading_fontset     	string		W		X logical fontset descriptor for the headings
 * heading_font_gdk	GdkFont *	RW		Pointer to GdkFont for the headings
 * day_font		string		W		X logical font descriptor for the day numbers
 * day_fontset 		string		W		X logical fontset descriptor for the day numbers
 * day_font_gdk		GdkFont *	RW		Pointer to GdkFont for the day numbers
 * heading_color	string		W		X color specification for heading labels
 * heading_color_gdk	GdkColor *	RW		Pointer to an allocated GdkColor for heading labels
 * outline_color	string		W		X color specification for outline (lines and fill of heading boxes)
 * outline_color_gdk	GdkColor *	RW		Pointer to an allocated GdkColor for outline
 * day_box_color	string		W		X color specification for day boxes
 * day_box_color_gdk	GdkColor *	RW		Pointer to an allocated GdkColor for day boxes
 * day_color		string		W		X color specification for day number labels
 * day_color_gdk	GdkColor *	RW		Pointer to an allocated GdkColor for day number labels
 */

#define GNOME_TYPE_MONTH_ITEM            (gnome_month_item_get_type ())
#define GNOME_MONTH_ITEM(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_MONTH_ITEM, GnomeMonthItem))
#define GNOME_MONTH_ITEM_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_MONTH_ITEM, GnomeMonthItemClass))
#define GNOME_IS_MONTH_ITEM(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_MONTH_ITEM))
#define GNOME_IS_MONTH_ITEM_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_MONTH_ITEM))


typedef struct _GnomeMonthItem GnomeMonthItem;
typedef struct _GnomeMonthItemClass GnomeMonthItemClass;

struct _GnomeMonthItem {
	GnomeCanvasGroup group;

	int year;			/* Year to show (full, no two-digit crap) */
	int month;			/* Month to show (0-11) */

	double x, y;			/* Position at anchor */
	double width, height;		/* Size of calendar */
	GtkAnchorType anchor;		/* Anchor side for calendar */

	double head_padding;		/* Padding to use between heading lines and text */
	double day_padding;		/* Padding to use between day number lines and text */

	char *day_names[7];		/* Names to use for the day labels, starting from Sunday */

	double head_height;		/* Height of the headings row */
	GtkAnchorType head_anchor;	/* Anchor side for the heading labels */
	GtkAnchorType day_anchor;	/* Anchor side for the day number labels */

	GnomeCanvasItem **items;	/* All the items that make up the calendar */
	int day_numbers[42];		/* The numbers of the days, as they are shown in the display */

	GdkFont *head_font;		/* Font for the headings */
	GdkFont *day_font;		/* Font for the day numbers */

	gulong head_pixel;		/* Color for heading labels */
	gulong outline_pixel;		/* Color for the outline (lines and heading boxes) */
	gulong day_box_pixel;		/* Color for the day boxes */
	gulong day_pixel;		/* Color for day number labels */

	int start_on_monday : 1;	/* Start the week on Monday?  If false, then start from Sunday */
};

struct _GnomeMonthItemClass {
	GnomeCanvasGroupClass parent_class;
};


/* Standard Gtk function */
GtkType gnome_month_item_get_type (void);

/* Creates a new month item with the specified group as parent */
GnomeCanvasItem *gnome_month_item_new (GnomeCanvasGroup *parent);

/* Constructor function useful for derived classes */
void gnome_month_item_construct (GnomeMonthItem *mitem);

/* Returns the child item defined by the child number (as specified on the GnomeMonthItemChild
 * enumeration above).
 */
GnomeCanvasItem *gnome_month_item_num2child (GnomeMonthItem *mitem, int child_num);

/* Returns the number of the specified child item, as defined on the GnomeMonthItemChild enumeration
 * above.  If the specified object is not found, it returns -1.
 */
int gnome_month_item_child2num (GnomeMonthItem *mitem, GnomeCanvasItem *child);

/* Returns the number of the day relevant to the specified child item.  Day numbers are 1-based.  If
 * the specified child is outside the range of displayed days, then it returns 0.
 */
int gnome_month_item_num2day (GnomeMonthItem *mitem, int child_num);

/* Returns the index (0-41) of the specified date within the table of days.  If the day number is
 * invalid for the current monthly calendar, then -1 is returned.
 */
int gnome_month_item_day2index (GnomeMonthItem *mitem, int day_num);


END_GNOME_DECLS

#endif
