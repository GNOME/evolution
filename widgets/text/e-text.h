/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-text.h - Text item for evolution.
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Jon Trowbridge <trow@ximian.com>
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
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef E_TEXT_H
#define E_TEXT_H

#include <gtk/gtkmenu.h>

#include <e-util/e-text-event-processor.h>
#include <text/e-text-model.h>
#include <widgets/misc/e-canvas.h>

G_BEGIN_DECLS


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
 * bold                 boolean                 RW              Bold?
 * anchor		GtkAnchorType		RW		Anchor side for the text
 * justification	GtkJustification	RW		Justification for multiline text
 * fill_color		string			W		X color specification for text
 * fill_color_gdk	GdkColor*		RW		Pointer to an allocated GdkColor
 * fill_stipple		GdkBitmap*		RW		Stipple pattern for filling the text
 * clip_width		double			RW		Width of clip rectangle
 * clip_height		double			RW		Height of clip rectangle
 * clip			boolean			RW		Use clipping rectangle?
 * fill_clip_rect       boolean                 RW              Whether the text item represents itself as being the size of the clipping rectangle.
 * x_offset		double			RW		Horizontal offset distance from anchor position
 * y_offset		double			RW		Vertical offset distance from anchor position
 * text_width		double			R		Used to query the width of the rendered text
 * text_height		double			R		Used to query the rendered height of the text
 * width                double                  RW              A synonym for clip_width
 * height               double                  R               A synonym for text_height
 *
 * These are currently ignored in the AA version:
 * editable             boolean                 RW              Can this item be edited
 * use_ellipsis         boolean                 RW              Whether to use ellipsises if text gets cut off.  Meaningless if clip == false.
 * ellipsis             string                  RW              The characters to use as ellipsis.  NULL = "...".
 * line_wrap            boolean                 RW              Line wrap when not editing.
 * break_characters     string                  RW              List of characters to optionally break on.
 * max_lines            int                     RW              Number of lines possible when doing line wrap.
 * draw_borders         boolean                 RW              Whether to draw borders.
 * draw_background      boolean                 RW              Whether to draw the background.
 * draw_button          boolean                 RW              This makes EText handle being the child of a button properly and highlighting as it should.
 */

#define E_TYPE_TEXT            (e_text_get_type ())
#define E_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TEXT, EText))
#define E_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_TEXT, ETextClass))
#define E_IS_TEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TEXT))
#define E_IS_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_TEXT))


typedef struct _EText EText;
typedef struct _ETextClass ETextClass;

struct _EText {
	GnomeCanvasItem item;
	
	ETextModel *model;
	gint model_changed_signal_id;
	gint model_repos_signal_id;

	const gchar *text;              /* Text to display --- from the ETextModel */
	gint preedit_len;      		/* preedit length to display */
	PangoLayout *layout;
	int num_lines;			/* Number of lines of text */

	gchar *revert;                  /* Text to revert to */

	GtkAnchorType anchor;		/* Anchor side for text */
	GtkJustification justification;	/* Justification for text */

	double clip_width;		/* Width of optional clip rectangle */
	double clip_height;		/* Height of optional clip rectangle */

	double xofs, yofs;		/* Text offset distance from anchor position */

	GdkColor color; 		/* Fill color */
	GdkBitmap *stipple;		/* Stipple for text */
	GdkGC *gc;			/* GC for drawing text */

	int cx, cy;			/* Top-left canvas coordinates for text */
	int text_cx, text_cy;		/* Top-left canvas coordinates for text */
	int clip_cx, clip_cy;		/* Top-left canvas coordinates for clip rectangle */
	int clip_cwidth, clip_cheight;	/* Size of clip rectangle in pixels */
	int max_width;			/* Maximum width of text lines */
	int width;                      /* Rendered text width in pixels */
	int height;			/* Rendered text height in pixels */

	guint32 rgba;			/* RGBA color for text */
	double affine[6];               /* The item -> canvas affine */

	char *ellipsis;                 /* The ellipsis characters.  NULL = "...". */
	double ellipsis_width;          /* The width of the ellipsis. */

	int xofs_edit;                  /* Offset because of editing */
	int yofs_edit;                  /* Offset because of editing */

	/* This needs to be reworked a bit once we get line wrapping. */
	int selection_start;            /* Start of selection IN BYTES */
	int selection_end;              /* End of selection IN BYTES */
	gboolean select_by_word;        /* Current selection is by word */

	/* This section is for drag scrolling and blinking cursor. */
	gint timeout_id;                /* Current timeout id for scrolling */
	GTimer *timer;                  /* Timer for blinking cursor and scrolling */

	gint lastx, lasty;              /* Last x and y motion events */
	gint last_state;                /* Last state */
	gulong scroll_start;            /* Starting time for scroll (microseconds) */

	gint show_cursor;               /* Is cursor currently shown */
	gboolean button_down;           /* Is mouse button 1 down */

	ETextEventProcessor *tep;       /* Text Event Processor */
	gint tep_command_id;

	gboolean has_selection;         /* TRUE if we have the selection */

	guint clip : 1;			/* Use clip rectangle? */
	guint fill_clip_rectangle : 1;  /* Fill the clipping rectangle. */

	guint pointer_in : 1;           /* Is the pointer currently over us? */
	guint default_cursor_shown : 1; /* Is the default cursor currently shown? */
	guint draw_borders : 1;         /* Draw borders? */
	guint draw_background : 1;      /* Draw background? */
	guint draw_button : 1;          /* Draw button? */

	guint line_wrap : 1;            /* Do line wrap */

	guint needs_redraw : 1;         /* Needs redraw */
	guint needs_recalc_bounds : 1;  /* Need recalc_bounds */
	guint needs_calc_height : 1;    /* Need calc_height */
	guint needs_split_into_lines : 1; /* Needs split_into_lines */
	guint needs_reset_layout : 1; /* Needs split_into_lines */

	guint bold : 1;
	guint strikeout : 1;

	guint tooltip_owner : 1;
	guint allow_newlines : 1;

	guint use_ellipsis : 1;         /* Whether to use the ellipsis. */

	guint editable : 1;             /* Item is editable */
	guint editing : 1;              /* Item is currently being edited */

	gchar *break_characters;        /* Characters to optionally break after */

	gint max_lines;                 /* Max number of lines (-1 = infinite) */

	GdkCursor *default_cursor;      /* Default cursor (arrow) */
	GdkCursor *i_cursor;            /* I beam cursor */

	gint tooltip_timeout;           /* Timeout for the tooltip */
	gint tooltip_count;             /* GDK_ENTER_NOTIFY count. */

	gint dbl_timeout;               /* Double click timeout */
	gint tpl_timeout;               /* Triple click timeout */

	gint     last_type_request;       /* Last selection type requested. */
	guint32  last_time_request;       /* The time of the last selection request. */
	GdkAtom  last_selection_request;  /* The time of the last selection request. */
	GList   *queued_requests;         /* Queued selection requests. */

	GtkIMContext *im_context;
	gboolean      need_im_reset;
	gboolean      im_context_signals_registered;

	gboolean      handle_popup;
};

struct _ETextClass {
	GnomeCanvasItemClass parent_class;

	void (* changed)         (EText *text);
	void (* activate)        (EText *text);
	void (* keypress)        (EText *text, guint keyval, guint state);
	void (* populate_popup)  (EText *text, GdkEventButton *ev, gint pos, GtkMenu *menu);
	void (* style_set)       (EText *text, GtkStyle *previous_style);
};


/* Standard Gtk function */
GtkType  e_text_get_type        (void);
void     e_text_cancel_editing  (EText *text);
void     e_text_stop_editing    (EText *text);

void     e_text_delete_selection (EText *text);
void     e_text_cut_clipboard    (EText *text);
void     e_text_copy_clipboard   (EText *text);
void     e_text_paste_clipboard  (EText *text);
void     e_text_select_all       (EText *text);

G_END_DECLS

#endif
