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


END_GNOME_DECLS

#endif
