/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* EText - Text item for evolution.
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Chris Lahey <clahey@umich.edu>
 *
 * A majority of code taken from:
 *
 * Text item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent
 * canvas widget.  Tk is copyrighted by the Regents of the University
 * of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx> */

#ifndef E_TEXT_H
#define E_TEXT_H

#include <gnome.h>


BEGIN_GNOME_DECLS


/* Text item for the canvas.  Text items are positioned by an anchor point and an anchor direction.
 *
 * A clipping rectangle may be specified for the text.  The rectangle is anchored at the text's anchor
 * point, and is specified by clipping width and height parameters.  If the clipping rectangle is
 * enabled, it will clip the text.
 *
 * In addition, x and y offset values may be specified.  These specify an offset from the anchor
 * position.  If used in conjunction with the clipping rectangle, these could be used to implement
 * simple scrolling of the text within the clipping rectangle.
 *
 * The following object arguments are available:
 *
 * name			type			read/write	description
 * ------------------------------------------------------------------------------------------
 * text			string			RW		The string of the text label
 * x			double			RW		X coordinate of anchor point
 * y			double			RW		Y coordinate of anchor point
 * font			string			W		X logical font descriptor
 * fontset		string			W		X logical fontset descriptor
 * font_gdk		GdkFont*		RW		Pointer to a GdkFont
 * anchor		GtkAnchorType		RW		Anchor side for the text
 * justification	GtkJustification	RW		Justification for multiline text
 * fill_color		string			W		X color specification for text
 * fill_color_gdk	GdkColor*		RW		Pointer to an allocated GdkColor
 * fill_stipple		GdkBitmap*		RW		Stipple pattern for filling the text
 * clip_width		double			RW		Width of clip rectangle
 * clip_height		double			RW		Height of clip rectangle
 * clip			boolean			RW		Use clipping rectangle?
 * x_offset		double			RW		Horizontal offset distance from anchor position
 * y_offset		double			RW		Vertical offset distance from anchor position
 * text_width		double			R		Used to query the width of the rendered text
 * text_height		double			R		Used to query the rendered height of the text
 *
 * These are ignored in the AA version:
 * use_ellipsis         boolean                 RW              Whether to use ellipsises if text gets cut off.  Meaningless if clip == false.
 * ellipsis             string                  RW              The characters to use as ellipsis.  NULL = "...".
 */

#define E_TYPE_TEXT            (e_text_get_type ())
#define E_TEXT(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_TEXT, EText))
#define E_TEXT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_TEXT, ETextClass))
#define E_IS_TEXT(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_TEXT))
#define E_IS_TEXT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_TEXT))


typedef struct _EText EText;
typedef struct _ETextClass ETextClass;
typedef struct _ETextSuckFont ETextSuckFont;
typedef struct _ETextSuckChar ETextSuckChar;

struct _ETextSuckChar {
	int     left_sb;
	int     right_sb;
	int     width;
	int     ascent;
	int     descent;
	int     bitmap_offset; /* in pixels */
};

struct _ETextSuckFont {
	guchar *bitmap;
	gint    bitmap_width;
	gint    bitmap_height;
	gint    ascent;
	ETextSuckChar chars[256];
};

struct _EText {
	GnomeCanvasItem item;

	char *text;			/* Text to display */
	gpointer lines;			/* Text split into lines (private field) */
	int num_lines;			/* Number of lines of text */

	double x, y;			/* Position at anchor */
	GdkFont *font;			/* Font for text */
	GtkAnchorType anchor;		/* Anchor side for text */
	GtkJustification justification;	/* Justification for text */

	double clip_width;		/* Width of optional clip rectangle */
	double clip_height;		/* Height of optional clip rectangle */

	double xofs, yofs;		/* Text offset distance from anchor position */

	gulong pixel;			/* Fill color */
	GdkBitmap *stipple;		/* Stipple for text */
	GdkGC *gc;			/* GC for drawing text */

	int cx, cy;			/* Top-left canvas coordinates for text */
	int clip_cx, clip_cy;		/* Top-left canvas coordinates for clip rectangle */
	int clip_cwidth, clip_cheight;	/* Size of clip rectangle in pixels */
	int max_width;			/* Maximum width of text lines */
	int height;			/* Rendered text height in pixels */

	guint clip : 1;			/* Use clip rectangle? */

	/* Antialiased specific stuff follows */
	ETextSuckFont *suckfont; /* Sucked font */
	guint32 rgba;			/* RGBA color for text */
	double affine[6];               /* The item -> canvas affine */

	char *ellipsis;                 /* The ellipsis characters.  NULL = "...". */
	double ellipsis_width;          /* The width of the ellipsis. */
	gboolean use_ellipsis;          /* Whether to use the ellipsis. */
};

struct _ETextClass {
	GnomeCanvasItemClass parent_class;
};


/* Standard Gtk function */
GtkType e_text_get_type (void);


END_GNOME_DECLS

#endif
