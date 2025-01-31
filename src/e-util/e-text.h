/*
 * e-text.h - Text item for evolution.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Jon Trowbridge <trow@ximian.com>
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TEXT_H
#define E_TEXT_H

#include <gtk/gtk.h>

#include <e-util/e-canvas.h>
#include <e-util/e-text-event-processor.h>
#include <e-util/e-text-model.h>

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
 * justification	GtkJustification	RW		Justification for multiline text
 * fill-color		GdkRGBA*		RW		Color specification for text
 * clip_width		gdouble			RW		Width of clip rectangle
 * clip_height		gdouble			RW		Height of clip rectangle
 * clip			boolean			RW		Use clipping rectangle?
 * fill_clip_rect       boolean                 RW              Whether the text item represents itself as being the size of the clipping rectangle.
 * x_offset		gdouble			RW		Horizontal offset distance from anchor position
 * y_offset		gdouble			RW		Vertical offset distance from anchor position
 * text_width		gdouble			R		Used to query the width of the rendered text
 * text_height		gdouble			R		Used to query the rendered height of the text
 * width                gdouble                  RW              A synonym for clip_width
 * height               gdouble                  R               A synonym for text_height
 *
 * These are currently ignored in the AA version:
 * editable             boolean                 RW              Can this item be edited
 * use_ellipsis         boolean                 RW              Whether to use ellipsises if text gets cut off.  Meaningless if clip == false.
 * ellipsis             string                  RW              The characters to use as ellipsis.  NULL = "...".
 * line_wrap            boolean                 RW              Line wrap when not editing.
 * break_characters     string                  RW              List of characters to optionally break on.
 * max_lines            gint                     RW              Number of lines possible when doing line wrap.
 */

/* Standard GObject macros */
#define E_TYPE_TEXT \
	(e_text_get_type ())
#define E_TEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TEXT, EText))
#define E_TEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TEXT, ETextClass))
#define E_IS_TEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TEXT))
#define E_IS_TEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TEXT))
#define E_TEXT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TEXT, ETextClass))

G_BEGIN_DECLS

typedef struct _EText EText;
typedef struct _ETextClass ETextClass;

struct _EText {
	GnomeCanvasItem item;

	ETextModel *model;
	gint model_changed_signal_id;
	gint model_repos_signal_id;

	const gchar *text;              /* Text to display --- from the ETextModel */
	gint preedit_len;		/* preedit length to display */
	gint preedit_pos;		/* preedit cursor position */
	PangoLayout *layout;
	gint num_lines;			/* Number of lines of text */

	gchar *revert;                  /* Text to revert to */

	GtkJustification justification;	/* Justification for text */

	gdouble clip_width;		/* Width of optional clip rectangle */
	gdouble clip_height;		/* Height of optional clip rectangle */

	gdouble xofs, yofs;		/* Text offset distance from anchor position */

	gint cx, cy;			/* Top-left canvas coordinates for text */
	gint text_cx, text_cy;		/* Top-left canvas coordinates for text */
	gint clip_cx, clip_cy;		/* Top-left canvas coordinates for clip rectangle */
	gint clip_cwidth, clip_cheight;	/* Size of clip rectangle in pixels */
	gint max_width;			/* Maximum width of text lines */
	gint width;                      /* Rendered text width in pixels */
	gint height;			/* Rendered text height in pixels */

	GdkRGBA rgba;			/* RGBA color for text */
	gboolean rgba_set;		/* whether RGBA is set */

	gchar *ellipsis;                 /* The ellipsis characters.  NULL = "...". */
	gdouble ellipsis_width;          /* The width of the ellipsis. */

	gint xofs_edit;                  /* Offset because of editing */
	gint yofs_edit;                  /* Offset because of editing */

	/* This needs to be reworked a bit once we get line wrapping. */
	gint selection_start;            /* Start of selection IN BYTES */
	gint selection_end;              /* End of selection IN BYTES */
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

	guint line_wrap : 1;            /* Do line wrap */

	guint needs_redraw : 1;         /* Needs redraw */
	guint needs_recalc_bounds : 1;  /* Need recalc_bounds */
	guint needs_calc_height : 1;    /* Need calc_height */
	guint needs_split_into_lines : 1; /* Needs split_into_lines */
	guint needs_reset_layout : 1; /* Needs split_into_lines */

	guint bold : 1;
	guint strikeout : 1;
	guint italic : 1;

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

	gint last_type_request;         /* Last selection type requested. */
	guint32 last_time_request;      /* The time of the last selection request. */
	GdkAtom last_selection_request; /* The time of the last selection request. */
	GList *queued_requests;         /* Queued selection requests. */

	GtkIMContext *im_context;
	gboolean need_im_reset;
	gboolean im_context_signals_registered;

	gboolean handle_popup;

	PangoFontDescription *font_desc;
};

struct _ETextClass {
	GnomeCanvasItemClass parent_class;

	void		(*changed)		(EText *text);
	void		(*activate)		(EText *text);
	void		(*keypress)		(EText *text,
						 guint keyval,
						 guint state);
	void		(*populate_popup)	(EText *text,
						 GdkEvent *button_event,
						 gint pos,
						 GtkMenu *menu);
	void		(*style_updated)	(EText *text);
};

GType		e_text_get_type			(void) G_GNUC_CONST;
void		e_text_cancel_editing		(EText *text);
void		e_text_stop_editing		(EText *text);
void		e_text_delete_selection		(EText *text);
void		e_text_cut_clipboard		(EText *text);
void		e_text_copy_clipboard		(EText *text);
void		e_text_paste_clipboard		(EText *text);
void		e_text_select_all		(EText *text);

G_END_DECLS

#endif /* E_TEXT_H */

