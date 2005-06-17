/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-text.c - Text item for evolution.
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

#include <config.h>

#include <math.h>
#include <ctype.h>
#include <string.h>

#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rgb_bitmap_affine.h>

#include "a11y/e-text/gal-a11y-e-text.h"
#include "widgets/misc/e-canvas.h"
#include "widgets/misc/e-canvas-utils.h"
#include "widgets/misc/e-unicode.h"
#include "e-util/e-i18n.h"
#include "e-util/e-text-event-processor-emacs-like.h"
#include "e-util/e-util.h"

#include "e-text.h"

#define PARENT_TYPE (gnome_canvas_item_get_type())

#define BORDER_INDENT 3
#define d(x)

enum {
	E_TEXT_CHANGED,
	E_TEXT_ACTIVATE,
	E_TEXT_KEYPRESS,
	E_TEXT_POPULATE_POPUP,
	E_TEXT_STYLE_SET,
	E_TEXT_LAST_SIGNAL
};

static GQuark e_text_signals[E_TEXT_LAST_SIGNAL] = { 0 };

/* Object argument IDs */
enum {
	PROP_0,
	PROP_MODEL,
	PROP_EVENT_PROCESSOR,
	PROP_TEXT,
	PROP_BOLD,
	PROP_STRIKEOUT,
	PROP_ANCHOR,
	PROP_JUSTIFICATION,
	PROP_CLIP_WIDTH,
	PROP_CLIP_HEIGHT,
	PROP_CLIP,
	PROP_FILL_CLIP_RECTANGLE,
	PROP_X_OFFSET,
	PROP_Y_OFFSET,
	PROP_FILL_COLOR,
	PROP_FILL_COLOR_GDK,
	PROP_FILL_COLOR_RGBA,
	PROP_FILL_STIPPLE,
	PROP_TEXT_WIDTH,
	PROP_TEXT_HEIGHT,
	PROP_EDITABLE,
	PROP_USE_ELLIPSIS,
	PROP_ELLIPSIS,
	PROP_LINE_WRAP,
	PROP_BREAK_CHARACTERS,
	PROP_MAX_LINES,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_DRAW_BORDERS,
	PROP_ALLOW_NEWLINES,
	PROP_DRAW_BACKGROUND,
	PROP_DRAW_BUTTON,
	PROP_CURSOR_POS,
	PROP_IM_CONTEXT,
	PROP_HANDLE_POPUP
};

static void e_text_command(ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data);

static void e_text_text_model_changed(ETextModel *model, EText *text);
static void e_text_text_model_reposition (ETextModel *model, ETextModelReposFn fn, gpointer repos_data, gpointer data);

static void _get_tep(EText *text);

static void calc_height (EText *text);

static gboolean show_pango_rectangle (EText *text, PangoRectangle rect);

static void e_text_do_popup (EText *text, GdkEventButton *button, int position);

static void e_text_update_primary_selection (EText *text);
static void e_text_paste (EText *text, GdkAtom selection);
static void e_text_insert(EText *text, const char *string);

static void reset_layout_attrs (EText *text);

#if 0
/* GtkEditable Methods */
static void e_text_editable_do_insert_text (GtkEditable    *editable,
					    const gchar    *text,
					    gint            length,
					    gint           *position);
static void e_text_editable_do_delete_text (GtkEditable    *editable,
					    gint            start_pos,
					    gint            end_pos);
static gchar* e_text_editable_get_chars (GtkEditable    *editable,
					 gint            start_pos,
					 gint            end_pos);
static void e_text_editable_set_selection_bounds (GtkEditable    *editable,
						  gint            start_pos,
						  gint            end_pos);
static gboolean e_text_editable_get_selection_bounds (GtkEditable    *editable,
						      gint           *start_pos,
						      gint           *end_pos);
static void e_text_editable_set_position (GtkEditable    *editable,
					  gint            position);
static gint e_text_editable_get_position (GtkEditable    *editable);
#endif
			   
/* IM Context Callbacks */
static void     e_text_commit_cb               (GtkIMContext *context,
						const gchar  *str,
						EText        *text);
static void     e_text_preedit_changed_cb      (GtkIMContext *context,
						EText        *text);
static gboolean e_text_retrieve_surrounding_cb (GtkIMContext *context,
						EText        *text);
static gboolean e_text_delete_surrounding_cb   (GtkIMContext *context,
						gint          offset,
						gint          n_chars,
						EText        *text);

static GnomeCanvasItemClass *parent_class;
static GdkAtom clipboard_atom = GDK_NONE;



/* Dispose handler for the text item */

#if 0
static void
e_text_style_set (EText *text, GtkStyle *previous_style)
{
	if ( text->line_wrap ) {
		text->needs_split_into_lines = 1;
	} else {
		text->needs_calc_height = 1;
	}
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (text));
}
#endif

static void
e_text_dispose (GObject *object)
{
	EText *text;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TEXT (object));

	text = E_TEXT (object);

	if (text->tooltip_owner)
		e_canvas_hide_tooltip (E_CANVAS(GNOME_CANVAS_ITEM(text)->canvas));
	text->tooltip_owner = 0;

	if (text->model_changed_signal_id)
		g_signal_handler_disconnect (text->model, 
					     text->model_changed_signal_id);
	text->model_changed_signal_id = 0;

	if (text->model_repos_signal_id)
		g_signal_handler_disconnect (text->model,
					     text->model_repos_signal_id);
	text->model_repos_signal_id = 0;

	if (text->model)
		g_object_unref(text->model);
	text->model = NULL;

	if (text->tep_command_id)
		g_signal_handler_disconnect(text->tep,
					    text->tep_command_id);
	text->tep_command_id = 0;

	if (text->tep)
		g_object_unref (text->tep);
	text->tep = NULL;

	g_free (text->revert);
	text->revert = NULL;

	if (text->stipple)
		gdk_bitmap_unref (text->stipple);
	text->stipple = NULL;

	if (text->timeout_id) {
		g_source_remove(text->timeout_id);
		text->timeout_id = 0;
	}
	
	if (text->timer) {
		g_timer_stop(text->timer);
		g_timer_destroy(text->timer);
		text->timer = NULL;
	}
				
	if ( text->tooltip_timeout ) {
		gtk_timeout_remove (text->tooltip_timeout);
		text->tooltip_timeout = 0;
	}

	if ( text->dbl_timeout ) {
		gtk_timeout_remove (text->dbl_timeout);
		text->dbl_timeout = 0;
	}

	if ( text->tpl_timeout ) {
		gtk_timeout_remove (text->tpl_timeout);
		text->tpl_timeout = 0;
	}

	if (text->layout) {
		g_object_unref (text->layout);
		text->layout = NULL;
	}

	if (text->im_context) {
		g_signal_handlers_disconnect_matched (text->im_context,
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL,
						      NULL, text);
		g_object_unref (text->im_context);
		text->im_context = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
insert_preedit_text (EText *text)
{
	PangoAttrList *attrs = NULL;
	PangoAttrList *preedit_attrs = NULL;
	gchar *preedit_string = NULL;
	GString *tmp_string = g_string_new (NULL);
	gint length = 0, cpos = 0, preedit_length = 0;
	gboolean new_attrs = FALSE;

	if (text->layout == NULL || !GTK_IS_IM_CONTEXT (text->im_context))
		return;

	text->text = e_text_model_get_text(text->model);
	length = strlen (text->text);

	g_string_prepend_len (tmp_string, text->text,length); 

	if (text->preedit_len)
		gtk_im_context_get_preedit_string (text->im_context,
					   &preedit_string, &preedit_attrs,
					   NULL);

	if (preedit_string && g_utf8_validate (preedit_string, -1, NULL))
		text->preedit_len = preedit_length = strlen (preedit_string);
	else
		text->preedit_len  = preedit_length = 0;

	cpos = g_utf8_offset_to_pointer (text->text, text->selection_start) - text->text;	

	if (preedit_length) {
		g_string_insert (tmp_string, cpos, preedit_string);

		reset_layout_attrs (text);

		attrs = pango_layout_get_attributes (text->layout);
		if (!attrs) {
			attrs = pango_attr_list_new ();
			new_attrs = TRUE;
		}

		pango_layout_set_text (text->layout, tmp_string->str, tmp_string->len);

		pango_attr_list_splice (attrs, preedit_attrs, cpos, preedit_length);

		if (new_attrs) {
			pango_layout_set_attributes (text->layout, attrs);
			pango_attr_list_unref (attrs);
		}
	}

	if (preedit_string)
		g_free (preedit_string);
	if (preedit_attrs)
		pango_attr_list_unref (preedit_attrs);
	if (tmp_string)
		g_string_free (tmp_string, TRUE);
}

static void
reset_layout_attrs (EText *text)
{
	PangoAttrList *attrs = NULL;
	int object_count;

	if (text->layout == NULL)
		return;

	object_count = e_text_model_object_count (text->model);

	if (text->bold || text->strikeout || object_count > 0) {
		int length = 0;
		int i;

		attrs = pango_attr_list_new ();

		for (i = 0; i < object_count; i++) {
			int start_pos, end_pos;
			PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);

			e_text_model_get_nth_object_bounds (text->model, i, &start_pos, &end_pos);

			attr->start_index = g_utf8_offset_to_pointer (text->text, start_pos) - text->text;
			attr->end_index = g_utf8_offset_to_pointer (text->text, end_pos) - text->text;

			pango_attr_list_insert (attrs, attr);
		}

		if (text->bold || text->strikeout)
			length = strlen (text->text);

		if (text->bold) {
			PangoAttribute *attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
			attr->start_index = 0;
			attr->end_index = length;

			pango_attr_list_insert_before (attrs, attr);
		}
		if (text->strikeout) {
			PangoAttribute *attr = pango_attr_strikethrough_new (TRUE);
			attr->start_index = 0;
			attr->end_index = length;

			pango_attr_list_insert_before (attrs, attr);
		}
	}
	pango_layout_set_attributes (text->layout, attrs);
	if (attrs)
		pango_attr_list_unref (attrs);
	calc_height (text);
}

static void
create_layout (EText *text)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (text);

	if (text->layout)
		return;

	text->layout = gtk_widget_create_pango_layout (GTK_WIDGET (item->canvas), text->text);
	if (text->line_wrap)
		pango_layout_set_width (text->layout, text->clip_width < 0 ? -1 : text->clip_width * PANGO_SCALE);
	reset_layout_attrs (text);
}

static void
reset_layout (EText *text)
{
	if (text->layout == NULL) {
		create_layout (text);
	}
	else {
		pango_layout_set_text (text->layout, text->text, -1);
		reset_layout_attrs (text);
	}

	if (!text->button_down) {
		PangoRectangle strong_pos, weak_pos;
		char *offs = g_utf8_offset_to_pointer (text->text, text->selection_start);

		pango_layout_get_cursor_pos (text->layout, offs - text->text, &strong_pos, &weak_pos);

		if (strong_pos.x != weak_pos.x ||
		    strong_pos.y != weak_pos.y ||
		    strong_pos.width != weak_pos.width ||
		    strong_pos.height != weak_pos.height)
			show_pango_rectangle (text, weak_pos);

		show_pango_rectangle (text, strong_pos);
	}
}

static void
e_text_text_model_changed (ETextModel *model, EText *text)
{
	gint model_len = e_text_model_get_text_length (model);
	text->text = e_text_model_get_text(model);

	/* Make sure our selection doesn't extend past the bounds of our text. */
	text->selection_start = CLAMP (text->selection_start, 0, model_len);
	text->selection_end   = CLAMP (text->selection_end,   0, model_len);

	text->needs_reset_layout = 1;
	text->needs_split_into_lines = 1;
	text->needs_redraw = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM(text));
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (text));

	g_signal_emit (text, e_text_signals[E_TEXT_CHANGED], 0);
}

static void
e_text_text_model_reposition (ETextModel *model, ETextModelReposFn fn, gpointer repos_data, gpointer user_data)
{
	EText *text = E_TEXT (user_data);
	gint model_len = e_text_model_get_text_length (model);

	text->selection_start = fn (text->selection_start, repos_data);
	text->selection_end   = fn (text->selection_end,   repos_data);

	/* Our repos function should make sure we don't overrun the buffer, but it never
	   hurts to be paranoid. */
	text->selection_start = CLAMP (text->selection_start, 0, model_len);
	text->selection_end   = CLAMP (text->selection_end,   0, model_len);

	if (text->selection_start > text->selection_end) {
		gint tmp = text->selection_start;
		text->selection_start = text->selection_end;
		text->selection_end = tmp;
	}
}

static void
get_bounds (EText *text, double *px1, double *py1, double *px2, double *py2)
{
	GnomeCanvasItem *item;
	double wx, wy, clip_width, clip_height;

	item = GNOME_CANVAS_ITEM (text);

	/* Get canvas pixel coordinates for text position */

	wx = 0;
	wy = 0;
	gnome_canvas_item_i2w (item, &wx, &wy);
	gnome_canvas_w2c (item->canvas, wx + text->xofs, wy + text->yofs, &text->cx, &text->cy);
	gnome_canvas_w2c (item->canvas, wx, wy, &text->clip_cx, &text->clip_cy);

	if (text->clip_width < 0)
		clip_width = text->width;
	else
		clip_width = text->clip_width;

	if ( text->clip_height < 0 )
		clip_height = text->height;
	else
		clip_height = text->clip_height;

	/* Get canvas pixel coordinates for clip rectangle position */
	text->clip_cwidth = clip_width * item->canvas->pixels_per_unit;
	text->clip_cheight = clip_height * item->canvas->pixels_per_unit;

	/* Anchor text */

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		text->cx -= text->width / 2;
		text->clip_cx -= text->clip_cwidth / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		text->cx -= text->width;
		text->clip_cx -= text->clip_cwidth;
		break;
	}

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		text->cy -= text->height / 2;
		text->clip_cy -= text->clip_cheight / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		text->cy -= text->height;
		text->clip_cy -= text->clip_cheight;
		break;
	}

	text->text_cx = text->cx;
	text->text_cy = text->cy;

	if (text->draw_borders) {
		text->text_cx += BORDER_INDENT;
		text->text_cy += BORDER_INDENT;
	}

	/* Bounds */

	if (text->clip) {
		*px1 = text->clip_cx;
		*py1 = text->clip_cy;
		*px2 = text->clip_cx + text->clip_cwidth;
		*py2 = text->clip_cy + text->clip_cheight;
	} else {
		*px1 = text->cx;
		*py1 = text->cy;
		*px2 = text->cx + text->width;
		*py2 = text->cy + text->height;
	}
}

static void
calc_height (EText *text)
{
	GnomeCanvasItem *item;
	int old_height;
	int old_width;
	int width = 0;
	int height = 0;

	item = GNOME_CANVAS_ITEM (text);

	/* Calculate text dimensions */

	old_height = text->height;
	old_width = text->width;

	if (text->layout)
		pango_layout_get_pixel_size (text->layout, &width, &height);

	text->height = height;
	text->width = width;

	if (old_height != text->height || old_width != text->width)
		e_canvas_item_request_parent_reflow(item);
}

static void
calc_ellipsis (EText *text)
{
#warning "AIEEEE FIX ME.  a pango layout per calc_ellipsis sucks"
	int width;
	PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas),
							      text->ellipsis ? text->ellipsis : "...");
	pango_layout_get_size (layout, &width, NULL);

	text->ellipsis_width = width;

	g_object_unref (layout);
}

static void
split_into_lines (EText *text)
{
	text->num_lines = pango_layout_get_line_count (text->layout);
}

/* Convenience function to set the text's GC's foreground color */
static void
set_text_gc_foreground (EText *text)
{
	if (!text->gc)
		return;

	gdk_gc_set_foreground (text->gc, &text->color);
}

/* Sets the stipple pattern for the text */
static void
set_stipple (EText *text, GdkBitmap *stipple, int reconfigure)
{
	if (text->stipple && !reconfigure)
		gdk_bitmap_unref (text->stipple);

	text->stipple = stipple;
	if (stipple && !reconfigure)
		gdk_bitmap_ref (stipple);

	if (text->gc) {
		if (stipple) {
			gdk_gc_set_stipple (text->gc, stipple);
			gdk_gc_set_fill (text->gc, GDK_STIPPLED);
		} else
			gdk_gc_set_fill (text->gc, GDK_SOLID);
	}
}

/* Set_arg handler for the text item */
static void
e_text_set_property (GObject *object,
                    guint prop_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	EText *text;
	GdkColor color = { 0, 0, 0, 0, };
	GdkColor *pcolor;
	gboolean color_changed;
	int have_pixel;
	
	gboolean needs_update = 0;
	gboolean needs_reflow = 0;

	item = GNOME_CANVAS_ITEM (object);
	text = E_TEXT (object);

	color_changed = FALSE;
	have_pixel = FALSE;

	switch (prop_id) {
	case PROP_MODEL:

		if ( text->model_changed_signal_id )
			g_signal_handler_disconnect (text->model,
					     text->model_changed_signal_id);
		
		if ( text->model_repos_signal_id )
			g_signal_handler_disconnect (text->model,
						     text->model_repos_signal_id);

		g_object_unref (text->model);
		text->model = E_TEXT_MODEL (g_value_get_object (value));
		g_object_ref (text->model);

		text->model_changed_signal_id =
			g_signal_connect (text->model,
					  "changed",
					  G_CALLBACK (e_text_text_model_changed),
					  text);

		text->model_repos_signal_id =
			g_signal_connect (text->model,
					  "reposition",
					  G_CALLBACK (e_text_text_model_reposition),
					  text);

		text->text = e_text_model_get_text(text->model);
		g_signal_emit (text, e_text_signals[E_TEXT_CHANGED], 0);

		text->needs_split_into_lines = 1;
		needs_reflow = 1;
		break;

	case PROP_EVENT_PROCESSOR:
		if ( text->tep && text->tep_command_id )
			g_signal_handler_disconnect(text->tep,
						    text->tep_command_id);
		if ( text->tep ) {
			g_object_unref(text->tep);
		}
		text->tep = E_TEXT_EVENT_PROCESSOR(g_value_get_object (value));
		g_object_ref(text->tep);
		text->tep_command_id = 
			g_signal_connect(text->tep,
					 "command",
					 G_CALLBACK(e_text_command),
					 text);
		if (!text->allow_newlines)
			g_object_set (text->tep,
				      "allow_newlines", FALSE,
				      NULL);
		break;

	case PROP_TEXT:
		e_text_model_set_text(text->model, g_value_get_string (value));
		break;

	case PROP_BOLD:
		text->bold = g_value_get_boolean (value);

		text->needs_redraw = 1;
		text->needs_recalc_bounds = 1;
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_height = 1;
		}
		needs_update = 1;
		needs_reflow = 1;
		break;

	case PROP_STRIKEOUT:
		text->strikeout = g_value_get_boolean (value);
		text->needs_redraw = 1;
		needs_update = 1;
		break;

	case PROP_ANCHOR:
		text->anchor = g_value_get_enum (value);
		text->needs_recalc_bounds = 1;
		needs_update = 1;
		break;

	case PROP_JUSTIFICATION:
		text->justification = g_value_get_enum (value);
		text->needs_redraw = 1;
		needs_update = 1;
		break;

	case PROP_CLIP_WIDTH:
		text->clip_width = fabs (g_value_get_double (value));
		calc_ellipsis (text);
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_height = 1;
		}
		needs_reflow = 1;
		break;

	case PROP_CLIP_HEIGHT:
		text->clip_height = fabs (g_value_get_double (value));
		text->needs_recalc_bounds = 1;
		/* toshok: kind of a hack - set needs_reset_layout
		   here so when something about the style/them
		   changes, we redraw the text at the proper size/with
		   the proper font. */
		text->needs_reset_layout = 1;
		needs_reflow = 1;
		break;

	case PROP_CLIP:
		text->clip = g_value_get_boolean (value);
		calc_ellipsis (text);
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_height = 1;
		}
		needs_reflow = 1;
		break;

	case PROP_FILL_CLIP_RECTANGLE:
		text->fill_clip_rectangle = g_value_get_boolean (value);
		needs_update = 1;
		break;

	case PROP_X_OFFSET:
		text->xofs = g_value_get_double (value);
		text->needs_recalc_bounds = 1;
		needs_update = 1;
		break;

	case PROP_Y_OFFSET:
		text->yofs = g_value_get_double (value);
		text->needs_recalc_bounds = 1;
		needs_update = 1;
		break;

        case PROP_FILL_COLOR:
		if (g_value_get_string (value))
			gdk_color_parse (g_value_get_string (value), &color);

		text->rgba = ((color.red & 0xff00) << 16 |
			      (color.green & 0xff00) << 8 |
			      (color.blue & 0xff00) |
			      0xff);
		color_changed = TRUE;
		break;

	case PROP_FILL_COLOR_GDK:
		pcolor = g_value_get_boxed (value);
		if (pcolor) {
			color = *pcolor;
		}

		text->rgba = ((color.red & 0xff00) << 16 |
			      (color.green & 0xff00) << 8 |
			      (color.blue & 0xff00) |
			      0xff);
		color_changed = TRUE;
		break;

        case PROP_FILL_COLOR_RGBA:
		text->rgba = g_value_get_uint (value);
		color.red = ((text->rgba >> 24) & 0xff) * 0x101;
		color.green = ((text->rgba >> 16) & 0xff) * 0x101;
		color.blue = ((text->rgba >> 8) & 0xff) * 0x101;
		color_changed = TRUE;
		break;

	case PROP_FILL_STIPPLE:
		set_stipple (text, g_value_get_object (value), FALSE);
		text->needs_redraw = 1;
		needs_update = 1;
		break;

	case PROP_EDITABLE:
		text->editable = g_value_get_boolean (value);
		text->needs_redraw = 1;
		needs_update = 1;
		break;

	case PROP_USE_ELLIPSIS:
		text->use_ellipsis = g_value_get_boolean (value);
		needs_reflow = 1;
		break;

	case PROP_ELLIPSIS:
		if (text->ellipsis)
			g_free (text->ellipsis);

		text->ellipsis = g_strdup (g_value_get_string (value));
		calc_ellipsis (text);
		needs_reflow = 1;
		break;

	case PROP_LINE_WRAP:
		text->line_wrap = g_value_get_boolean (value);
		if (text->line_wrap) {
			if (text->layout) {
				pango_layout_set_width (text->layout, text->width < 0 ? -1 : text->width * PANGO_SCALE);
			}
		}
		text->needs_split_into_lines = 1;
		needs_reflow = 1;
		break;
		
	case PROP_BREAK_CHARACTERS:
		if ( text->break_characters ) {
			g_free(text->break_characters);
			text->break_characters = NULL;
		}
		if ( g_value_get_string (value) )
			text->break_characters = g_strdup( g_value_get_string (value) );
		text->needs_split_into_lines = 1;
		needs_reflow = 1;
		break;

	case PROP_MAX_LINES:
		text->max_lines = g_value_get_int (value);
		text->needs_split_into_lines = 1;
		needs_reflow = 1;
		break;

	case PROP_WIDTH:
		text->clip_width = fabs (g_value_get_double (value));
		calc_ellipsis (text);
		if ( text->line_wrap ) {
			if (text->layout) {
				pango_layout_set_width (text->layout, text->width < 0 ? -1 : text->width * PANGO_SCALE);
			}
			text->needs_split_into_lines = 1;
		}
		else {
			text->needs_calc_height = 1;
		}
		needs_reflow = 1;
		break;
		
	case PROP_DRAW_BORDERS:
		if (text->draw_borders != g_value_get_boolean (value)) {
			text->draw_borders = g_value_get_boolean (value);
			text->needs_calc_height = 1;
			text->needs_redraw = 1;
			needs_reflow = 1;
			needs_update = 1;
		}
		break;

	case PROP_DRAW_BACKGROUND:
		if (text->draw_background != g_value_get_boolean (value)) {
			text->draw_background = g_value_get_boolean (value);
			text->needs_redraw = 1;
		}
		break;

	case PROP_DRAW_BUTTON:
		if (text->draw_button !=  g_value_get_boolean (value)) {
			text->draw_button = g_value_get_boolean (value);
			text->needs_redraw = 1;
		}
		break;

	case PROP_ALLOW_NEWLINES:
		text->allow_newlines = g_value_get_boolean (value);
		_get_tep(text);
		g_object_set (text->tep,
			      "allow_newlines", g_value_get_boolean (value),
			      NULL);
		break;

	case PROP_CURSOR_POS: {
		ETextEventProcessorCommand command;

		command.action = E_TEP_MOVE;
		command.position = E_TEP_VALUE;
		command.value = g_value_get_int (value);
		command.time = GDK_CURRENT_TIME;
		e_text_command (text->tep, &command, text);
		break;
	}

	case PROP_IM_CONTEXT:
		if (text->im_context)
			g_object_unref (text->im_context);

		text->im_context = g_value_get_object (value);
		if (text->im_context)
			g_object_ref (text->im_context);

		text->need_im_reset = FALSE;
		break;

	case PROP_HANDLE_POPUP:
		text->handle_popup = g_value_get_boolean (value);
		break;

	default:
		return;
	}

	if (color_changed) {
               GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (item->canvas));

	       text->color = color;
               gdk_rgb_find_color (colormap, &text->color);

	       if (!item->canvas->aa)
		       set_text_gc_foreground (text);

	       text->needs_redraw = 1;
	       needs_update = 1;
	}

	if ( needs_reflow )
		e_canvas_item_request_reflow (item);
	if ( needs_update )
		gnome_canvas_item_request_update (item);
}

/* Get_arg handler for the text item */
static void
e_text_get_property (GObject *object,
                    guint prop_id,
                    GValue *value,
                    GParamSpec *pspec)
{
	EText *text;

	text = E_TEXT (object);

	switch (prop_id) {
	case PROP_MODEL:
		g_value_set_object (value, text->model);
		break;

	case PROP_EVENT_PROCESSOR:
		_get_tep(text);
		g_value_set_object (value, text->tep);
		break;

	case PROP_TEXT:
		g_value_set_string (value, g_strdup (text->text));
		break;

	case PROP_BOLD:
		g_value_set_boolean (value, text->bold);
		break;

	case PROP_STRIKEOUT:
		g_value_set_boolean (value, text->strikeout);
		break;

	case PROP_ANCHOR:
		g_value_set_enum (value, text->anchor);
		break;

	case PROP_JUSTIFICATION:
		g_value_set_enum (value, text->justification);
		break;

	case PROP_CLIP_WIDTH:
		g_value_set_double (value, text->clip_width);
		break;

	case PROP_CLIP_HEIGHT:
		g_value_set_double (value, text->clip_height);
		break;

	case PROP_CLIP:
		g_value_set_boolean (value, text->clip);
		break;

	case PROP_FILL_CLIP_RECTANGLE:
		g_value_set_boolean (value, text->fill_clip_rectangle);
		break;

	case PROP_X_OFFSET:
		g_value_set_double (value, text->xofs);
		break;

	case PROP_Y_OFFSET:
		g_value_set_double (value, text->yofs);
		break;

	case PROP_FILL_COLOR_GDK:
		g_value_set_boxed (value, &text->color);
		break;

	case PROP_FILL_COLOR_RGBA:
		g_value_set_uint (value, text->rgba);
		break;

	case PROP_FILL_STIPPLE:
		g_value_set_object (value, text->stipple);
		break;

	case PROP_TEXT_WIDTH:
		g_value_set_double (value, text->width / text->item.canvas->pixels_per_unit);
		break;

	case PROP_TEXT_HEIGHT:
		g_value_set_double (value, text->height / text->item.canvas->pixels_per_unit);
		break;
		
	case PROP_EDITABLE:
		g_value_set_boolean (value, text->editable);
		break;

	case PROP_USE_ELLIPSIS:
		g_value_set_boolean (value, text->use_ellipsis);
		break;

	case PROP_ELLIPSIS:
		g_value_set_string (value, g_strdup (text->ellipsis));
		break;

	case PROP_LINE_WRAP:
		g_value_set_boolean (value, text->line_wrap);
		break;
		
	case PROP_BREAK_CHARACTERS:
		g_value_set_string (value, g_strdup (text->break_characters));
		break;

	case PROP_MAX_LINES:
		g_value_set_int (value, text->max_lines);
		break;

	case PROP_WIDTH:
		g_value_set_double (value, text->clip_width);
		break;

	case PROP_HEIGHT:
		g_value_set_double (value, text->clip && text->clip_height != -1 ? text->clip_height : text->height / text->item.canvas->pixels_per_unit);
		break;

	case PROP_DRAW_BORDERS:
		g_value_set_boolean (value, text->draw_borders);
		break;

	case PROP_DRAW_BACKGROUND:
		g_value_set_boolean (value, text->draw_background);
		break;
		
	case PROP_DRAW_BUTTON:
		g_value_set_boolean (value, text->draw_button);
		break;
		
	case PROP_ALLOW_NEWLINES:
		g_value_set_boolean (value, text->allow_newlines);
		break;

	case PROP_CURSOR_POS:
		g_value_set_int (value, text->selection_start);
		break;

	case PROP_IM_CONTEXT:
		g_value_set_object (value, text->im_context);
		break;
		
	case PROP_HANDLE_POPUP:
		g_value_set_boolean (value, text->handle_popup);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* Update handler for the text item */
static void
e_text_reflow (GnomeCanvasItem *item, int flags)
{
	EText *text;

	text = E_TEXT (item);

	if (text->needs_reset_layout) {
		reset_layout (text);
		text->needs_reset_layout = 0;
		text->needs_calc_height = 1;
	}

	if (text->needs_split_into_lines) {
		split_into_lines (text);

		text->needs_split_into_lines = 0;
		text->needs_calc_height = 1;
	}

	if ( text->needs_calc_height ) {
		calc_height (text);
		gnome_canvas_item_request_update(item);
		text->needs_calc_height = 0;
		text->needs_recalc_bounds = 1;
	}
}

/* Update handler for the text item */
static void
e_text_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	EText *text;
	double x1, y1, x2, y2;

	text = E_TEXT (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	if ( text->needs_recalc_bounds
	     || (flags & GNOME_CANVAS_UPDATE_AFFINE)) {
		if (!item->canvas->aa) {
			set_text_gc_foreground (text);
			set_stipple (text, text->stipple, TRUE);
			get_bounds (text, &x1, &y1, &x2, &y2);
			if ( item->x1 != x1 ||
			     item->x2 != x2 ||
			     item->y1 != y1 ||
			     item->y2 != y2 ) {
				gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
				item->x1 = x1;
				item->y1 = y1;
				item->x2 = x2;
				item->y2 = y2;
				text->needs_redraw = 1;
				item->canvas->need_repick = TRUE;
			}
			if (!text->fill_clip_rectangle)
				item->canvas->need_repick = TRUE;
		}
		text->needs_recalc_bounds = 0;
	}
	if ( text->needs_redraw ) {
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
		text->needs_redraw = 0;
	}
}

/* Realize handler for the text item */
static void
e_text_realize (GnomeCanvasItem *item)
{
	EText *text;

	text = E_TEXT (item);

	if (parent_class->realize)
		(* parent_class->realize) (item);

	create_layout (text);

	text->gc = gdk_gc_new (item->canvas->layout.bin_window);
#ifndef NO_WARNINGS
#warning Color brokenness ...
#endif
#if 0
	gdk_color_context_query_color (item->canvas->cc, &text->color);
	gdk_gc_set_foreground (text->gc, &text->color);
#endif
	
	text->i_cursor = gdk_cursor_new (GDK_XTERM);
	text->default_cursor = gdk_cursor_new (GDK_LEFT_PTR);
}

/* Unrealize handler for the text item */
static void
e_text_unrealize (GnomeCanvasItem *item)
{
	EText *text;

	text = E_TEXT (item);

	gdk_gc_unref (text->gc);
	text->gc = NULL;

	gdk_cursor_destroy (text->i_cursor);
	text->i_cursor = NULL;
	gdk_cursor_destroy (text->default_cursor);
	text->default_cursor = NULL;

	if (parent_class->unrealize)
		(* parent_class->unrealize) (item);
}

static void
_get_tep(EText *text)
{
	if (!text->tep) {
		text->tep = e_text_event_processor_emacs_like_new();
		text->tep_command_id = 
			g_signal_connect(text->tep,
					 "command",
					 G_CALLBACK(e_text_command),
					 text);
	}
}

static void
draw_pango_rectangle (GdkDrawable *drawable, GdkGC *gc, int x1, int y1, PangoRectangle rect)
{
	int width = rect.width / PANGO_SCALE;
	int height = rect.height / PANGO_SCALE;
	if (width <= 0)
		width = 1;
	if (height <= 0)
		height = 1;
	gdk_draw_rectangle (drawable, gc, TRUE,
			    x1 + rect.x / PANGO_SCALE, y1 + rect.y / PANGO_SCALE, width, height);
}

static gboolean
show_pango_rectangle (EText *text, PangoRectangle rect)
{
	int x1 = rect.x / PANGO_SCALE;
	int x2 = (rect.x + rect.width) / PANGO_SCALE;

	int y1 = rect.y / PANGO_SCALE;
	int y2 = (rect.y + rect.height) / PANGO_SCALE;

	int new_xofs_edit = text->xofs_edit;
	int new_yofs_edit = text->yofs_edit;

	int clip_width, clip_height;

	clip_width = text->clip_width;
	if (clip_width >= 0 && text->draw_borders) {
		clip_width -= 6;
		if (clip_width < 0)
			clip_width = 0;
	}

	clip_height = text->clip_height;

	if (clip_height >= 0 && text->draw_borders) {
		clip_height -= 6;
		if (clip_height < 0)
			clip_height = 0;
	}

	if (x1 < new_xofs_edit)
		new_xofs_edit = x1;

	if (y1 < new_yofs_edit)
		new_yofs_edit = y1;

	if (clip_width >= 0) {
		if (2 + x2 - clip_width > new_xofs_edit)
			new_xofs_edit = 2 + x2 - clip_width;
	} else {
		new_xofs_edit = 0;
	}

	if (clip_height >= 0) {
		if (y2 - clip_height > new_yofs_edit)
			new_yofs_edit = y2 - clip_height;
	} else {
		new_yofs_edit = 0;
	}

	if (new_xofs_edit < 0)
		new_xofs_edit = 0;
	if (new_yofs_edit < 0)
		new_yofs_edit = 0;

	if (new_xofs_edit != text->xofs_edit ||
	    new_yofs_edit != text->yofs_edit) {
		text->xofs_edit = new_xofs_edit;
		text->yofs_edit = new_yofs_edit;
		return TRUE;
	}

	return FALSE;
}

/* Draw handler for the text item */
static void
e_text_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
	     int x, int y, int width, int height)
{
	EText *text;
	GdkRectangle rect, *clip_rect;
	int xpos, ypos;
	GdkGC *main_gc;
	GnomeCanvas *canvas;
	GtkWidget *widget;

	text = E_TEXT (item);
	canvas = GNOME_CANVAS_ITEM(text)->canvas;
	widget = GTK_WIDGET(canvas);

	if (text->draw_background || text->draw_button) {
		main_gc = widget->style->fg_gc[GTK_WIDGET_STATE (widget)];
	} else {
		main_gc = text->gc;
	}

	if (text->draw_borders || text->draw_background) {
		gdouble thisx = item->x1 - x;
		gdouble thisy = item->y1 - y;
		gdouble thiswidth, thisheight;
		GtkWidget *widget = GTK_WIDGET(item->canvas);

		g_object_get(text,
			     "width", &thiswidth,
			     "height", &thisheight,
			     NULL);
		
		if (text->draw_borders){

			gtk_paint_shadow (widget->style, drawable,
					  GTK_STATE_NORMAL, GTK_SHADOW_IN,
					  NULL, widget, "entry",
					  thisx, thisy, thiswidth, thisheight);

		}

		if (text->draw_background) {
			gtk_paint_flat_box (widget->style, drawable,
					    GTK_WIDGET_STATE(widget), GTK_SHADOW_NONE,
					    NULL, widget, "entry_bg", 
					    thisx + widget->style->xthickness,
					    thisy + widget->style->ythickness, 
					    thiswidth - widget->style->xthickness * 2,
					    thisheight - widget->style->ythickness * 2);
		}
	}
	if (text->draw_button) {
		GtkWidget *widget;
		int xoff = item->x1 - x;
		int yoff = item->y1 - y;

		widget = GTK_WIDGET (item->canvas);

		xoff -= widget->allocation.x;
		yoff -= widget->allocation.y;

		widget = widget->parent;

		while (widget && !GTK_IS_BUTTON(widget)) {
			if (!GTK_WIDGET_NO_WINDOW (widget)) {
				widget = NULL;
				break;
			}
			widget = widget->parent;
		}
		if (widget) {
			GtkButton *button = GTK_BUTTON (widget);
			GtkShadowType shadow_type;
			int thisx, thisy, thisheight, thiswidth;
			int default_spacing;
			GdkRectangle area;
			area.x = 0;
			area.y = 0;
			area.width = width;
			area.height = height;

#define DEFAULT_SPACING   7
#if 0
			default_spacing = gtk_style_get_prop_experimental (widget->style,
									   "GtkButton::default_spacing",
									   DEFAULT_SPACING);
#endif
			default_spacing = 7;

			thisx = 0;
			thisy = 0;
			thiswidth = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
			thisheight = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;

			if (GTK_WIDGET_HAS_DEFAULT (widget) &&
			    GTK_BUTTON (widget)->relief == GTK_RELIEF_NORMAL)
				{
					gtk_paint_box (widget->style, drawable,
						       GTK_STATE_NORMAL, GTK_SHADOW_IN,
						       &area, widget, "buttondefault",
						       thisx + xoff, thisy + yoff, thiswidth, thisheight);
				}

			if (GTK_WIDGET_CAN_DEFAULT (widget)) {
				thisx += widget->style->xthickness;
				thisy += widget->style->ythickness;
				thiswidth -= 2 * thisx + default_spacing;
				thisheight -= 2 * thisy + default_spacing;
				thisx += (1 + default_spacing) / 2;
				thisy += (1 + default_spacing) / 2;
			}

			if (GTK_WIDGET_HAS_FOCUS (widget)) {
				thisx += 1;
				thisy += 1;
				thiswidth -= 2;
				thisheight -= 2;
			}

			if (GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE)
				shadow_type = GTK_SHADOW_IN;
			else
				shadow_type = GTK_SHADOW_OUT;

			if ((button->relief != GTK_RELIEF_NONE) ||
			    ((GTK_WIDGET_STATE(widget) != GTK_STATE_NORMAL) &&
			     (GTK_WIDGET_STATE(widget) != GTK_STATE_INSENSITIVE)))
			gtk_paint_box (widget->style, drawable,
				       GTK_WIDGET_STATE (widget),
				       shadow_type, &area, widget, "button",
				       thisx + xoff, thisy + yoff, thiswidth, thisheight);

			if (GTK_WIDGET_HAS_FOCUS (widget)) {
				thisx -= 1;
				thisy -= 1;
				thiswidth += 2;
				thisheight += 2;

				gtk_paint_focus (widget->style, widget->window, GTK_WIDGET_STATE (widget),
						 &area, widget, "button",
						 thisx + xoff, thisy + yoff, thiswidth - 1, thisheight - 1);
			}
		}
	}


	insert_preedit_text (text);	

	if (!pango_layout_get_text (text->layout))
		return;

	if (text->stipple)
		gnome_canvas_set_stipple_origin (item->canvas, main_gc);

	xpos = text->text_cx;
	ypos = text->text_cy;

	xpos -= x;
	ypos -= y;

	clip_rect = NULL;
	if (text->clip) {
		rect.x = xpos;
		rect.y = ypos;
		rect.width = text->clip_cwidth;
		rect.height = text->clip_cheight;
		
		gdk_gc_set_clip_rectangle (main_gc, &rect);
		clip_rect = &rect;
	}

	if (text->editing) {
		xpos -= text->xofs_edit;
		ypos -= text->yofs_edit;
	}

	gdk_draw_layout (drawable, main_gc,
			 xpos, ypos,
			 text->layout);

	if (text->editing) {
		if (text->selection_start != text->selection_end) {
			PangoLayoutIter *iter;
			GdkRegion *clip_region = gdk_region_new ();
			GdkGC *selection_gc;
			GdkGC *text_gc;
			int start_index, end_index;

			start_index = MIN (text->selection_start, text->selection_end);
			end_index = MAX (text->selection_start, text->selection_end);

			/* convert these into byte indices */
			start_index = g_utf8_offset_to_pointer(text->text, start_index) - text->text;
			end_index = g_utf8_offset_to_pointer(text->text, end_index) - text->text;

			if (text->has_selection) {
				selection_gc = widget->style->base_gc [GTK_STATE_SELECTED];
				text_gc = widget->style->text_gc[GTK_STATE_SELECTED];
			} else {
				selection_gc = widget->style->base_gc [GTK_STATE_ACTIVE];
				text_gc = widget->style->text_gc[GTK_STATE_ACTIVE];
			}

			gdk_gc_set_clip_rectangle (selection_gc, clip_rect);

			iter = pango_layout_get_iter (text->layout);

			do {
				PangoLayoutLine *line = pango_layout_iter_get_line (iter);
				gint n_ranges, i;
				gint *ranges;
				int y0, y1;
				int s, e;

				if (start_index < line->start_index + line->length
				    && end_index > line->start_index) {
					
					if (start_index <= line->start_index)
						s = line->start_index;
					else
						s = start_index;

					if (end_index > line->start_index + line->length)
						e = line->start_index + line->length;
					else
						e = end_index;

					pango_layout_line_get_x_ranges (line, s, e, &ranges, &n_ranges);

					pango_layout_iter_get_line_yrange (iter, &y0, &y1);

					for (i=0; i < n_ranges; i++) {
						GdkRectangle sel_rect;

						sel_rect.x = xpos + PANGO_PIXELS (ranges[2*i]);
						sel_rect.y = ypos + PANGO_PIXELS (y0);
						sel_rect.width = (ranges[2*i + 1] - ranges[2*i]) / PANGO_SCALE;
						sel_rect.height = (y1 - y0 + PANGO_SCALE / 2) / PANGO_SCALE;

						gdk_draw_rectangle (drawable, selection_gc, TRUE,
								    sel_rect.x, sel_rect.y, sel_rect.width, sel_rect.height);

						gdk_region_union_with_rect (clip_region, &sel_rect);
					}
					g_free (ranges);
				}
			} while (pango_layout_iter_next_line (iter));

			pango_layout_iter_free (iter);

			if (clip_rect) {
				GdkRegion *rect_region = gdk_region_rectangle (clip_rect);
				gdk_region_intersect (clip_region, rect_region);
				gdk_region_destroy (rect_region);
			}

			gdk_gc_set_clip_region (text_gc, clip_region);
			gdk_draw_layout (drawable, text_gc, 
					 xpos, ypos,
					 text->layout);

			gdk_gc_set_clip_region (text_gc, NULL);
			gdk_gc_set_clip_region (selection_gc, NULL);

			gdk_region_destroy (clip_region);
		} else {
			if (text->show_cursor) {
				PangoRectangle strong_pos, weak_pos;
				char *offs = g_utf8_offset_to_pointer (text->text, text->selection_start);

				pango_layout_get_cursor_pos (text->layout, offs - text->text + text->preedit_len, &strong_pos, &weak_pos);
				draw_pango_rectangle (drawable, main_gc, xpos, ypos, strong_pos);
				if (strong_pos.x != weak_pos.x ||
				    strong_pos.y != weak_pos.y ||
				    strong_pos.width != weak_pos.width ||
				    strong_pos.height != weak_pos.height)
					draw_pango_rectangle (drawable, main_gc, xpos, ypos, weak_pos);
			}
		}
	}


	if (text->clip) {
		gdk_gc_set_clip_rectangle (main_gc, NULL);
	}
}

/* Point handler for the text item */
static double
e_text_point (GnomeCanvasItem *item, double x, double y,
	      int cx, int cy, GnomeCanvasItem **actual_item)
{
	EText *text;
	double clip_width;
	double clip_height;

	text = E_TEXT (item);

	*actual_item = item;

	/* The idea is to build bounding rectangles for each of the lines of
	 * text (clipped by the clipping rectangle, if it is activated) and see
	 * whether the point is inside any of these.  If it is, we are done.
	 * Otherwise, calculate the distance to the nearest rectangle.
	 */

	if (text->clip_width < 0)
		clip_width = text->width;
	else
		clip_width = text->clip_width;

	if ( text->clip_height < 0 )
		clip_height = text->height;
	else
		clip_height = text->clip_height;

	/* Get canvas pixel coordinates for clip rectangle position */
	clip_width = clip_width * item->canvas->pixels_per_unit;
	clip_height = clip_height * item->canvas->pixels_per_unit;

	if (cx < text->clip_cx ||
	    cx > text->clip_cx + clip_width ||
	    cy < text->clip_cy ||
	    cy > text->clip_cy + clip_height)
		return 1;

	if (text->fill_clip_rectangle)
		return 0;

	cx -= text->cx;

	if (pango_layout_xy_to_index (text->layout, cx, cy, NULL, NULL))
		return 0;

	return 1;
}

/* Bounds handler for the text item */
static void
e_text_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	EText *text;
	double width, height;

	text = E_TEXT (item);

	*x1 = 0;
	*y1 = 0;

	width = text->width;
	height = text->height;

	if (text->clip) {
		if (text->clip_width >= 0)
			width = text->clip_width;
		if ( text->clip_height >= 0 )
			height = text->clip_height;
	}

	width = width / item->canvas->pixels_per_unit;
	height = height / item->canvas->pixels_per_unit;

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		*x1 -= width / 2.0;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		*x1 -= width;
		break;
	}

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		*y1 -= height / 2.0;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		*y1 -= height;
		break;
	}

	*x2 = *x1 + width;
	*y2 = *y1 + height;
}

static gint
get_position_from_xy (EText *text, gint x, gint y)
{
	int index;
	int trailing;

	if (text->draw_borders) {
		x -= BORDER_INDENT;
		y -= BORDER_INDENT;
	}

	x -= text->xofs;
	y -= text->yofs;

	if (text->editing) {
		x += text->xofs_edit;
		y += text->yofs_edit;
	}

	x -= text->cx;
	y -= text->cy;

	pango_layout_xy_to_index (text->layout, x * PANGO_SCALE, y * PANGO_SCALE, &index, &trailing);

	return g_utf8_pointer_to_offset (text->text, text->text + index + trailing);
}

#define SCROLL_WAIT_TIME 30000

static gboolean
_blink_scroll_timeout (gpointer data)
{
	EText *text = E_TEXT(data);
	gulong current_time;
	gboolean scroll = FALSE;
	gboolean redraw = FALSE;
	
	g_timer_elapsed(text->timer, &current_time);

	if (text->scroll_start + SCROLL_WAIT_TIME > 1000000) {
		if (current_time > text->scroll_start - (1000000 - SCROLL_WAIT_TIME) &&
		    current_time < text->scroll_start)
			scroll = TRUE;
	} else {
		if (current_time > text->scroll_start + SCROLL_WAIT_TIME ||
		    current_time < text->scroll_start)
			scroll = TRUE;
	}
	if (scroll && text->button_down && text->clip) {
		int old_xofs_edit = text->xofs_edit;
		int old_yofs_edit = text->yofs_edit;

		if (text->clip_cwidth >= 0 &&
		    text->lastx - text->clip_cx > text->clip_cwidth &&
		    text->xofs_edit < text->width - text->clip_cwidth) {
			text->xofs_edit += 4;
			if (text->xofs_edit > text->width - text->clip_cwidth + 1)
				text->xofs_edit = text->width - text->clip_cwidth + 1;
		}
		if (text->lastx - text->clip_cx < 0 &&
		    text->xofs_edit > 0) {
			text->xofs_edit -= 4;
			if (text->xofs_edit < 0)
				text->xofs_edit = 0;
		}

		if (text->clip_cheight >= 0 &&
		    text->lasty - text->clip_cy > text->clip_cheight &&
		    text->yofs_edit < text->height - text->clip_cheight) {
			text->yofs_edit += 4;
			if (text->yofs_edit > text->height - text->clip_cheight + 1)
				text->yofs_edit = text->height - text->clip_cheight + 1;
		}
		if (text->lasty - text->clip_cy < 0 &&
		    text->yofs_edit > 0) {
			text->yofs_edit -= 4;
			if (text->yofs_edit < 0)
				text->yofs_edit = 0;
		}

		if (old_xofs_edit != text->xofs_edit ||
		    old_yofs_edit != text->yofs_edit) {
			ETextEventProcessorEvent e_tep_event;
			e_tep_event.type = GDK_MOTION_NOTIFY;
			e_tep_event.motion.state = text->last_state;
			e_tep_event.motion.time = 0;
			e_tep_event.motion.position = get_position_from_xy(text, text->lastx, text->lasty);
			_get_tep(text);
			e_text_event_processor_handle_event (text->tep,
							     &e_tep_event);
			text->scroll_start = current_time;
			redraw = TRUE;
		}
	}

	if (!((current_time / 500000) % 2)) {
		if (!text->show_cursor)
			redraw = TRUE;
		text->show_cursor = TRUE;
	} else {
		if (text->show_cursor)
			redraw = TRUE;
		text->show_cursor = FALSE;
	}
	if (redraw) {
		text->needs_redraw = 1;
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(text));
	}
	return TRUE;
}

static gboolean
tooltip_event(GtkWidget *tooltip, GdkEvent *event, EText *text)
{
	gint ret_val = FALSE;

	if (!text->model)
		return FALSE;

	switch (event->type) {
	case GDK_LEAVE_NOTIFY:
		e_canvas_hide_tooltip (E_CANVAS(GNOME_CANVAS_ITEM(text)->canvas));
		break;
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		if (event->type == GDK_BUTTON_RELEASE) {
			e_canvas_hide_tooltip (E_CANVAS(GNOME_CANVAS_ITEM(text)->canvas));
		}
		/* Forward events to the text item */
		g_signal_emit_by_name (text, "event", event,
				       &ret_val);
		if (!ret_val)
			gtk_propagate_event (GTK_WIDGET(GNOME_CANVAS_ITEM(text)->canvas), event);
		ret_val = TRUE;
	default:
		break;
	}
	return ret_val;
}

static void
tooltip_destroy(gpointer data, GObject *where_object_was)
{
	EText *text = data;
	text->tooltip_owner = FALSE;
	g_object_unref (text);
}

static gboolean
_do_tooltip (gpointer data)
{
#warning "need to sort out tooltip stuff."
	EText *text = E_TEXT (data);
	GtkWidget *canvas;
	int i;
	int max_width;
	gboolean cut_off;
	double i2c[6];
	ArtPoint origin = {0, 0};
	ArtPoint pixel_origin;
	int canvas_x, canvas_y;
	GnomeCanvasItem *tooltip_text;
	double tooltip_width;
	double tooltip_height;
	double tooltip_x;
	double tooltip_y;
#if 0
	double x1, x2, y1, y2;
#endif
	GnomeCanvasItem *rect;
	GtkWidget *tooltip_window;      /* GtkWindow for displaying the tooltip */

	text->tooltip_count = 0;

	if (E_CANVAS(GNOME_CANVAS_ITEM(text)->canvas)->tooltip_window || text->editing || !text->num_lines) {
		text->tooltip_timeout = 0;
		return FALSE;
	}

	cut_off = FALSE;
	for ( i = 0; i < text->num_lines; i++ ) {
		PangoLayoutLine *line = pango_layout_get_line (text->layout, i);
		PangoRectangle rect;

		pango_layout_line_get_pixel_extents (line, &rect, NULL);

		if (rect.width > text->clip_width) {
			cut_off = TRUE;
			break;
		}
	}
	if ( ! cut_off ) {
		text->tooltip_timeout = 0;
		return FALSE;
	}

	gnome_canvas_item_i2c_affine(GNOME_CANVAS_ITEM(text), i2c);
	art_affine_point (&pixel_origin, &origin, i2c);

	gdk_window_get_origin (GTK_WIDGET(GNOME_CANVAS_ITEM(text)->canvas)->window, &canvas_x, &canvas_y);
	pixel_origin.x += canvas_x;
	pixel_origin.y += canvas_y;
	pixel_origin.x -= (int) gtk_layout_get_hadjustment(GTK_LAYOUT(GNOME_CANVAS_ITEM(text)->canvas))->value;
	pixel_origin.y -= (int) gtk_layout_get_vadjustment(GTK_LAYOUT(GNOME_CANVAS_ITEM(text)->canvas))->value;

	tooltip_window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_container_set_border_width (GTK_CONTAINER (tooltip_window), 1);

	canvas = e_canvas_new ();

	gtk_container_add (GTK_CONTAINER (tooltip_window), canvas);
	
	/* Get the longest line length */
	pango_layout_get_size (text->layout, &max_width, NULL);

	rect = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
				      gnome_canvas_rect_get_type (),
				      "x1", (double) 0,
				      "y1", (double) 0,
				      "x2", (double) max_width + 4,
				      "y2", (double) text->height + 4,
				      NULL);

	tooltip_text = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
					      e_text_get_type (),
					      "anchor", GTK_ANCHOR_NW,
					      "bold", text->bold,
					      "strikeout", text->strikeout,
					      "text", text->text,
					      "editable", FALSE,
					      "clip_width", text->max_lines != 1 ? text->clip_width : max_width,
					      "clip_height", text->max_lines != 1 ? -1 : (double)text->height,
					      "clip", TRUE,
					      "line_wrap", text->line_wrap,
					      "justification", text->justification,
					      NULL);



	if (text->draw_borders)
		e_canvas_item_move_absolute(tooltip_text, 1 + BORDER_INDENT, 1 + BORDER_INDENT);
	else
		e_canvas_item_move_absolute(tooltip_text, 1, 1);

	create_layout (E_TEXT (tooltip_text));

	split_into_lines (E_TEXT(tooltip_text));
	calc_height (E_TEXT(tooltip_text));

	gnome_canvas_item_set (tooltip_text,
			       "clip_height", (double) E_TEXT(tooltip_text)->height,
			       "clip_width", (double) E_TEXT(tooltip_text)->width,
			       NULL);

	tooltip_width = E_TEXT(tooltip_text)->width;
	tooltip_height = E_TEXT(tooltip_text)->height;
	tooltip_x = 0;
	tooltip_y = 0;
	switch(E_TEXT(tooltip_text)->justification) {
	case GTK_JUSTIFY_CENTER:
		tooltip_x = - tooltip_width / 2;
		break;
	case GTK_JUSTIFY_RIGHT:
		tooltip_x = tooltip_width / 2;
		break;
	case GTK_JUSTIFY_FILL:
	case GTK_JUSTIFY_LEFT:
		tooltip_x = 0;
		break;
	}
	switch(text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		tooltip_y -= tooltip_height / 2.0;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		tooltip_y -= tooltip_height;
		break;
	}
	switch(E_TEXT(tooltip_text)->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		tooltip_x -= tooltip_width / 2.0;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		tooltip_x -= tooltip_width;
		break;
	}

	gnome_canvas_item_set(rect,
			      "x2", (double) tooltip_width + 4 + (text->draw_borders ? BORDER_INDENT * 2 : 0),
			      "y2", (double) tooltip_height + 4 + (text->draw_borders ? BORDER_INDENT * 2 : 0),
			      NULL);
	
	gtk_widget_show (canvas);
	gtk_widget_realize (tooltip_window);

	gtk_widget_set_usize (tooltip_window,
			      tooltip_width + 4 + (text->draw_borders ? BORDER_INDENT * 2 : 0),
			      tooltip_height + 4 + (text->draw_borders ? BORDER_INDENT * 2 : 0));
	gnome_canvas_set_scroll_region (GNOME_CANVAS(canvas), 0.0, 0.0,
					tooltip_width + (text->draw_borders ? BORDER_INDENT * 2 : 0), 
					(double)tooltip_height + (text->draw_borders ? BORDER_INDENT * 2 : 0));
	g_signal_connect (tooltip_window, "event",
			  G_CALLBACK(tooltip_event), text);
	g_object_weak_ref (G_OBJECT (tooltip_window),
			   tooltip_destroy, text);
	g_object_ref (text);

	e_canvas_popup_tooltip (E_CANVAS(GNOME_CANVAS_ITEM(text)->canvas),
				tooltip_window,
				pixel_origin.x - 2 + tooltip_x,
				pixel_origin.y - 2 + tooltip_y);
	text->tooltip_owner = TRUE;
	
	text->tooltip_timeout = 0;

	return FALSE;
}

static void
start_editing (EText *text)
{
	if (text->editing)
		return;

	g_free (text->revert);
	text->revert = g_strdup (text->text);

	text->editing = TRUE;
	if (text->pointer_in) {
		if (text->default_cursor_shown && (!text->draw_borders)) {
			gdk_window_set_cursor (GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas)->window, text->i_cursor);
			text->default_cursor_shown = FALSE;
		}
	}
	text->select_by_word = FALSE;
	text->xofs_edit = 0;
	text->yofs_edit = 0;
	if (text->timeout_id == 0)
		text->timeout_id = g_timeout_add(10, _blink_scroll_timeout, text);
	text->timer = g_timer_new();
	g_timer_elapsed(text->timer, &(text->scroll_start));
	g_timer_start(text->timer);
}

void
e_text_stop_editing (EText *text)
{
	if (!text->editing)
		return;

	g_free (text->revert);
	text->revert = NULL;

	text->editing = FALSE;
	if ( (!text->default_cursor_shown) && (!text->draw_borders) ) {
		gdk_window_set_cursor (GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas)->window, text->default_cursor);
		text->default_cursor_shown = TRUE;
	}
	if (text->timer) {
		g_timer_stop(text->timer);
		g_timer_destroy(text->timer);
		text->timer = NULL;
	}
}

void
e_text_cancel_editing (EText *text)
{
	if (text->revert)
		e_text_model_set_text(text->model, text->revert);
	e_text_stop_editing (text);
}

static gboolean
_click (gpointer data)
{
	*(gint *)data = 0;
	return FALSE;
}

static gint
e_text_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EText *text = E_TEXT(item);
	ETextEventProcessorEvent e_tep_event;

	static EText *save_text = NULL;

	gint return_val = 0;

	if (!text->model)
		return FALSE;

	e_tep_event.type = event->type;
	switch (event->type) {
	case GDK_FOCUS_CHANGE:
		if (text->editable) {
			GdkEventFocus *focus_event;
			focus_event = (GdkEventFocus *) event;
			if (focus_event->in) {

				/* Evil hack to disconnect the signal handlers for the previous Etext
				 * which was not disconnected because of being in preedit mode.
				 * In preedit mode the widget can go out of focus due to popups associated
				 * with preedit.,but still the callbacks need to be connected. 
				 * Here when a new text widget comes into focus we can disconnect the 
				 * old one.Shouldn't hurt much, as in worst case, save_text which should
				 * be disconnected will be overwritten and we will have signal 
			   	 * handlers connect to  multiple e-texts but with subsequent commit these 
				 * should go away.
				 */

				if (save_text && save_text->im_context) {
					g_signal_handlers_disconnect_matched (save_text->im_context,
									      G_SIGNAL_MATCH_DATA,
                                                                             0, 0, NULL,
                                                                             NULL, save_text);
                                       save_text->im_context_signals_registered = FALSE;
                                }

				if (text->im_context) {
					if (!text->im_context_signals_registered) {
						g_signal_connect (text->im_context, "commit",
								  G_CALLBACK (e_text_commit_cb), text);
						g_signal_connect (text->im_context, "preedit_changed",
								  G_CALLBACK (e_text_preedit_changed_cb), text);
						g_signal_connect (text->im_context, "retrieve_surrounding",
								  G_CALLBACK (e_text_retrieve_surrounding_cb), text);
						g_signal_connect (text->im_context, "delete_surrounding",
								  G_CALLBACK (e_text_delete_surrounding_cb), text);
						text->im_context_signals_registered = TRUE;
					}
				}
				start_editing (text);
				text->show_cursor = FALSE; /* so we'll redraw and the cursor will be shown */
			} else {
				/* Incase we are not disconnecting the signals 
				 * for text, we are saving text for 
				 * disconnecting in the next focus_in.
				 */
				if (!text->preedit_len && text->im_context) {
					g_signal_handlers_disconnect_matched (text->im_context,
									      G_SIGNAL_MATCH_DATA,
									      0, 0, NULL,
									      NULL, text);
					text->im_context_signals_registered = FALSE;
				} else {
					save_text = text;
				}

				e_text_stop_editing (text);
				if (text->timeout_id) {
					g_source_remove(text->timeout_id);
					text->timeout_id = 0;
				}
				if (text->show_cursor || text->draw_borders) {
					text->show_cursor = FALSE;
					text->needs_redraw = 1;
					gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(text));
				}
			}
			if ( text->line_wrap )
				text->needs_split_into_lines = 1;
			e_canvas_item_request_reflow (GNOME_CANVAS_ITEM(text));
		}
		return_val = 0;
		break;
	case GDK_KEY_PRESS: 

		/* Handle S-F10 key binding here. */

		if (event->key.keyval == GDK_F10 
		    && (event->key.state & GDK_SHIFT_MASK)
		    && text->handle_popup ){

			/* Simulate a GdkEventButton here, so that we can call e_text_do_popup directly */

			GdkEventButton *button = (GdkEventButton *) gdk_event_new (GDK_BUTTON_PRESS);
			button->time = event->key.time;
			button->button = 0;
			e_text_do_popup (text, button, 0);
			return TRUE;
		}

		/* Fall Through */

	case GDK_KEY_RELEASE:

		if (text->editing) {
			GdkEventKey key;
			gint ret;

			if (text->im_context && gtk_im_context_filter_keypress (text->im_context, (GdkEventKey*)event)) {
				text->need_im_reset = TRUE;
				return 1;
			}

			key = event->key;
			e_tep_event.key.time = key.time;
			e_tep_event.key.state = key.state;
			e_tep_event.key.keyval = key.keyval;

			/* This is probably ugly hack, but we have to handle UTF-8 input somehow */
#if 0
			e_tep_event.key.length = key.length;
			e_tep_event.key.string = key.string;
#else
			e_tep_event.key.string = e_utf8_from_gtk_event_key (GTK_WIDGET (item->canvas), key.keyval, key.string);
			if (e_tep_event.key.string != NULL) {
				e_tep_event.key.length = strlen (e_tep_event.key.string);
			} else {
				e_tep_event.key.length = 0;
			}
#endif
			_get_tep(text);
			ret = e_text_event_processor_handle_event (text->tep, &e_tep_event);

			if (event->type == GDK_KEY_PRESS)
				g_signal_emit (text, e_text_signals[E_TEXT_KEYPRESS], 0,
					       e_tep_event.key.keyval, e_tep_event.key.state);
			

			if (e_tep_event.key.string)
				g_free (e_tep_event.key.string);


			return ret;
		}
		break;
	case GDK_BUTTON_PRESS: /* Fall Through */
	case GDK_BUTTON_RELEASE:
		if (text->tooltip_timeout) {
			gtk_timeout_remove (text->tooltip_timeout);
			text->tooltip_timeout = 0;
		}
		e_canvas_hide_tooltip (E_CANVAS(GNOME_CANVAS_ITEM(text)->canvas));
#if 0
		if ((!text->editing) 
		    && text->editable 
		    && event->type == GDK_BUTTON_RELEASE
		    && event->button.button == 1) {
			GdkEventButton button = event->button;

			e_canvas_item_grab_focus (item, TRUE);

			e_tep_event.type = GDK_BUTTON_RELEASE;
			e_tep_event.button.time = button.time;
			e_tep_event.button.state = button.state;
			e_tep_event.button.button = button.button;
			e_tep_event.button.position = get_position_from_xy(text, button.x, button.y);
			_get_tep(text);
			return_val = e_text_event_processor_handle_event (text->tep,
									  &e_tep_event);
			e_tep_event.type = GDK_BUTTON_RELEASE;
		}
#else
		if ((!text->editing) 
		    && text->editable 
		    && (event->button.button == 1 ||
			event->button.button == 2)) {
			e_canvas_item_grab_focus (item, TRUE);
			start_editing (text);
		}
#endif

		/* We follow convention and emit popup events on right-clicks. */
		if (event->type == GDK_BUTTON_PRESS && event->button.button == 3) {
			if (text->handle_popup) {
				e_text_do_popup (text, &(event->button),
						 get_position_from_xy (text, event->button.x, event->button.y));
				return TRUE;
			}
			else {
				break;
			}
		}

		/* Create our own double and triple click events, 
		   as gnome-canvas doesn't forward them to us */
		if (event->type == GDK_BUTTON_PRESS) {
			if (text->dbl_timeout == 0 && 
			    text->tpl_timeout == 0) {
				text->dbl_timeout = gtk_timeout_add (200,
								     _click,
								     &(text->dbl_timeout));
			} else {
				if (text->tpl_timeout == 0) {
					e_tep_event.type = GDK_2BUTTON_PRESS;
					text->tpl_timeout = gtk_timeout_add (200, _click, &(text->tpl_timeout));
				} else {
					e_tep_event.type = GDK_3BUTTON_PRESS;
				}				     
			}
		}
		
		if (text->editing) {
			GdkEventButton button = event->button;
			e_tep_event.button.time = button.time;
			e_tep_event.button.state = button.state;
			e_tep_event.button.button = button.button;
			e_tep_event.button.position = get_position_from_xy(text, button.x, button.y);
			_get_tep(text);
			return_val = e_text_event_processor_handle_event (text->tep,
									  &e_tep_event);
			if (event->button.button == 1) {
				if (event->type == GDK_BUTTON_PRESS)
					text->button_down = TRUE;
				else
					text->button_down = FALSE;
			}
			text->lastx = button.x;
			text->lasty = button.y;
			text->last_state = button.state;
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (text->editing) {
			GdkEventMotion motion = event->motion;
			e_tep_event.motion.time = motion.time;
			e_tep_event.motion.state = motion.state;
			e_tep_event.motion.position = get_position_from_xy(text, motion.x, motion.y);
			_get_tep(text);
			return_val = e_text_event_processor_handle_event (text->tep,
								       &e_tep_event);
			text->lastx = motion.x;
			text->lasty = motion.y;
			text->last_state = motion.state;
		}
		break;
	case GDK_ENTER_NOTIFY:
		{
				if ( text->tooltip_count == 0 && text->clip) {
					if (!text->tooltip_timeout)
						text->tooltip_timeout = gtk_timeout_add (1000, _do_tooltip, text);
				}
				text->tooltip_count ++;
		}

		text->pointer_in = TRUE;
		if (text->editing || text->draw_borders) {
			if ( text->default_cursor_shown ) {
				gdk_window_set_cursor(GTK_WIDGET(item->canvas)->window, text->i_cursor);
				text->default_cursor_shown = FALSE;
			}
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (text->tooltip_count > 0)
			text->tooltip_count --;
		if ( text->tooltip_count == 0 && text->clip) {
			if ( text->tooltip_timeout ) {
				gtk_timeout_remove (text->tooltip_timeout);
				text->tooltip_timeout = 0;
			}
		}
		
		text->pointer_in = FALSE;
		if (text->editing || text->draw_borders) {
			if ( ! text->default_cursor_shown ) {
				gdk_window_set_cursor(GTK_WIDGET(item->canvas)->window, text->default_cursor);
				text->default_cursor_shown = TRUE;
			}
		}
		break;
	default:
		break;
	}
	if (return_val)
		return return_val;
	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->event)
		return GNOME_CANVAS_ITEM_CLASS(parent_class)->event(item, event);
	else
		return 0;
}

void
e_text_copy_clipboard (EText *text)
{
	gint selection_start_pos; 
	gint selection_end_pos;
	char *str;

	selection_start_pos = MIN (text->selection_start, text->selection_end);
	selection_end_pos = MAX (text->selection_start, text->selection_end);

	/* convert sel_start/sel_end to byte indices */
	selection_start_pos = g_utf8_offset_to_pointer (text->text, selection_start_pos) - text->text;
	selection_end_pos = g_utf8_offset_to_pointer (text->text, selection_end_pos) - text->text;

	str = g_strndup (text->text + selection_start_pos,
			 selection_end_pos - selection_start_pos);

	gtk_clipboard_set_text (
#ifdef GTK_2_2
				gtk_widget_get_clipboard (GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas),
							  GDK_SELECTION_CLIPBOARD),
#else
				gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
#endif
				str, -1);
	g_free (str);
}

void
e_text_delete_selection(EText *text)
{
	int sel_start, sel_end;

	sel_start = MIN(text->selection_start, text->selection_end);
	sel_end   = MAX(text->selection_start, text->selection_end);

	if (sel_start != sel_end)
		e_text_model_delete(text->model, sel_start, sel_end - sel_start);
}

void
e_text_cut_clipboard (EText *text)
{
	e_text_copy_clipboard (text);
	e_text_delete_selection (text);
}

void
e_text_paste_clipboard (EText *text)
{
	ETextEventProcessorCommand command;

	command.action = E_TEP_PASTE;
	command.position = E_TEP_SELECTION;
	command.string = "";
	command.value = 0;
	e_text_command(text->tep, &command, text);
}

void
e_text_select_all (EText *text)
{
	ETextEventProcessorCommand command;

	command.action = E_TEP_SELECT;
	command.position = E_TEP_SELECT_ALL;
	command.string = "";
	command.value = 0;
	e_text_command(text->tep, &command, text);
}


static void
primary_get_cb (GtkClipboard     *clipboard,
		GtkSelectionData *selection_data,
		guint             info,
		gpointer          data)
{
	EText *text = E_TEXT (data);
	int sel_start, sel_end;

	sel_start = MIN(text->selection_start, text->selection_end);
	sel_end   = MAX(text->selection_start, text->selection_end);

	/* convert sel_start/sel_end to byte indices */
	sel_start = g_utf8_offset_to_pointer (text->text, sel_start) - text->text;
	sel_end = g_utf8_offset_to_pointer (text->text, sel_end) - text->text;

	if (sel_start != sel_end) {
		gchar *str = g_strndup (text->text + sel_start,
					sel_end - sel_start);
		gtk_selection_data_set_text (selection_data, str, -1);
		g_free (str);
	}
}

static void
primary_clear_cb (GtkClipboard *clipboard,
		  gpointer      data)
{
#if notyet
	/* XXX */
	gtk_editable_select_region (GTK_EDITABLE (entry), entry->current_pos, entry->current_pos);
#endif
}

static void
e_text_update_primary_selection (EText *text)
{
	static const GtkTargetEntry targets[] = {
		{ "UTF8_STRING", 0, 0 },
		{ "UTF-8", 0, 0 },
		{ "STRING", 0, 0 },
		{ "TEXT",   0, 0 }, 
		{ "COMPOUND_TEXT", 0, 0 }
	};
	GtkClipboard *clipboard;

#ifdef GTK_2_2
	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas), GDK_SELECTION_PRIMARY);
#else
	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
#endif
  
	if (text->selection_start != text->selection_end) {
		if (!gtk_clipboard_set_with_owner (clipboard, targets, G_N_ELEMENTS (targets),
						   primary_get_cb, primary_clear_cb, G_OBJECT (text)))
			primary_clear_cb (clipboard, text);
	}
	else {
		if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (text))
			gtk_clipboard_clear (clipboard);
	}
}

static void
paste_received (GtkClipboard *clipboard,
		const gchar  *text,
		gpointer      data)
{
	EText *etext = E_TEXT (data);
      
	if (text && g_utf8_validate (text, strlen (text), NULL)) {
		if (etext->selection_end != etext->selection_start)
			e_text_delete_selection (etext);

		e_text_insert (etext, text);
	}

	g_object_unref (etext);
}

static void
e_text_paste (EText *text, GdkAtom selection)
{
	g_object_ref (text);
	gtk_clipboard_request_text (
#ifdef GTK_2_2

				    gtk_widget_get_clipboard (GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas),
							      selection),
#else
				    gtk_clipboard_get (selection),
#endif
				    paste_received, text);
}

typedef struct {
	EText *text;
	GdkEventButton *button;
	int position;
} PopupClosure;

static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
}

static void
popup_menu_placement_cb (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
	EText *text = E_TEXT(user_data);
	GnomeCanvasItem *item = &text->item;
	GnomeCanvas *parent = item->canvas;

	if (parent){
		gdk_window_get_origin (((GtkWidget*) parent)->window, x, y);
		*x += item->x1 + text->width / 2;
		*y += item->y1 + text->height / 2;
	}

	return;
}

static void
popup_targets_received (GtkClipboard     *clipboard,
			GtkSelectionData *data,
			gpointer          user_data)
{
	PopupClosure *closure = user_data;
	EText *text = closure->text;
	GdkEventButton *button = closure->button;
	int position = closure->position;
	GtkWidget *popup_menu = gtk_menu_new ();
	GtkWidget *menuitem, *submenu;

	g_free (closure);

	gtk_menu_attach_to_widget (GTK_MENU (popup_menu),
				   GTK_WIDGET(GNOME_CANVAS_ITEM (text)->canvas),
				   popup_menu_detach);

	/* cut menu item */
	menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_CUT, NULL);
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menuitem);
	g_signal_connect_swapped (menuitem, "activate",
				  G_CALLBACK (e_text_cut_clipboard), text);
	gtk_widget_set_sensitive (menuitem, text->editable && (text->selection_start != text->selection_end));

	/* copy menu item */
	menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, NULL);
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menuitem);
	g_signal_connect_swapped (menuitem, "activate",
				  G_CALLBACK (e_text_copy_clipboard), text);
	gtk_widget_set_sensitive (menuitem, text->selection_start != text->selection_end);

	/* paste menu item */
	menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_PASTE, NULL);
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menuitem);
	g_signal_connect_swapped (menuitem, "activate",
				  G_CALLBACK (e_text_paste_clipboard), text);
	gtk_widget_set_sensitive (menuitem, text->editable && gtk_selection_data_targets_include_text (data));

	menuitem = gtk_menu_item_new_with_label (_("Select All"));
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menuitem);
	g_signal_connect_swapped (menuitem, "activate",
				  G_CALLBACK (e_text_select_all), text);
	gtk_widget_set_sensitive (menuitem, strlen (text->text) > 0);

	menuitem = gtk_separator_menu_item_new ();
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menuitem);

	if (text->im_context && GTK_IS_IM_MULTICONTEXT (text->im_context)) {
		menuitem = gtk_menu_item_new_with_label (_("Input Methods"));
		gtk_widget_show (menuitem);
		submenu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
      
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menuitem);

		gtk_im_multicontext_append_menuitems (GTK_IM_MULTICONTEXT (text->im_context),
						      GTK_MENU_SHELL (submenu));
	}

      g_signal_emit (text,
		     e_text_signals[E_TEXT_POPULATE_POPUP],
		     0,
		     button, position,
		     popup_menu);

      /* If invoked by S-F10 key binding, button will be 0. */
      if (button->button == 0){
      	      gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			      popup_menu_placement_cb, (gpointer)text,
			      button->button, GDK_CURRENT_TIME);
      } else {
	      gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			      NULL, NULL,
			      button->button, button->time);
      }

      g_object_unref (text);
      gdk_event_free ((GdkEvent *)button);
}

static void
e_text_do_popup (EText *text, GdkEventButton *button, int position)
{
	PopupClosure *closure = g_new (PopupClosure, 1);

	closure->text = text;
	g_object_ref (closure->text);
	closure->button = (GdkEventButton *) gdk_event_copy ((GdkEvent *)button);
	closure->position = position;

	gtk_clipboard_request_contents (
#ifdef GTK_2_2

					gtk_widget_get_clipboard (GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas),
								  GDK_SELECTION_CLIPBOARD),
#else
					gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
#endif
					gdk_atom_intern ("TARGETS", FALSE),
					popup_targets_received,
					closure);
}

#if 0
static void
e_text_reset_im_context (EText *text)
{
	if (text->need_im_reset) {
		text->need_im_reset = 0;
		gtk_im_context_reset (text->im_context);
	}
}
#endif

/* fixme: */

static int
next_word (EText *text, int start)
{
	char *p = g_utf8_offset_to_pointer (text->text, start);
	int length;

	length = g_utf8_strlen (text->text, -1);

	if (start >= length) {
		return length;
	} else {
		p = g_utf8_next_char (p);
		start++;

		while (p && *p) {
			gunichar unival = g_utf8_get_char (p);
			if (g_unichar_isspace (unival)) {
				return start + 1;
			}
			else {
				p = g_utf8_next_char (p);
				start++;
			}
		}
	}
			
	return g_utf8_pointer_to_offset (text->text, p);
}

static int
find_offset_into_line (EText *text, int offset_into_text, char **start_of_line)
{
	char *p;

	p = g_utf8_offset_to_pointer (text->text, offset_into_text);

	if (p == text->text) {
		if (start_of_line)
			*start_of_line = (char*)text->text;
		return 0;
	}
	else {
		p = g_utf8_find_prev_char (text->text, p);
		
		while (p && p > text->text) {
			if (*p == '\n') {
				if (start_of_line)
					*start_of_line = p+1;
				return offset_into_text - g_utf8_pointer_to_offset (text->text, p + 1);
			}
			p = g_utf8_find_prev_char (text->text, p);
		}

		if (start_of_line)
			*start_of_line = (char*)text->text;
		return offset_into_text;
	}
}

static int
_get_position(EText *text, ETextEventProcessorCommand *command)
{
	int length, obj_num;
	gunichar unival;
	char *p = NULL;
	gint new_pos = 0;

	switch (command->position) {
		
	case E_TEP_VALUE:
		new_pos = command->value;
		break;

	case E_TEP_SELECTION:
		new_pos = text->selection_end;
		break;

	case E_TEP_START_OF_BUFFER:
		new_pos = 0;
		break;

	case E_TEP_END_OF_BUFFER:
		new_pos = strlen (text->text);
		break;

	case E_TEP_START_OF_LINE:

		if (text->selection_end >= 1) {
			
			p = g_utf8_offset_to_pointer (text->text, text->selection_end);
			if (p != text->text) {
				p = g_utf8_find_prev_char (text->text, p);
				while (p && p > text->text) {
					if (*p == '\n') {
						new_pos = g_utf8_pointer_to_offset (text->text, p) + 1;
						break;
					}
					p = g_utf8_find_prev_char (text->text, p);
				}
			}
		}

		break;

	case E_TEP_END_OF_LINE:
		new_pos = -1;
		length = g_utf8_strlen (text->text, -1);
		
		if (text->selection_end >= length) {
			new_pos = length;
		} else {

			p = g_utf8_offset_to_pointer (text->text, text->selection_end);

			while (p && *p) {
				if (*p == '\n') {
					new_pos = g_utf8_pointer_to_offset (text->text, p);
					p = NULL;
				} else 
					p = g_utf8_next_char (p);
			}
		}

		if (new_pos == -1)
			new_pos = g_utf8_pointer_to_offset (text->text, p);

		break;

	case E_TEP_FORWARD_CHARACTER:
		length = g_utf8_strlen (text->text, -1);

		if (text->selection_end >= length)
			new_pos = length;
		else
			new_pos = text->selection_end + 1;

		break;

	case E_TEP_BACKWARD_CHARACTER:
		new_pos = 0;
		if (text->selection_end >= 1) {
			new_pos = text->selection_end - 1;
		}

		break;

	case E_TEP_FORWARD_WORD:
		new_pos = next_word (text, text->selection_end);
		break;

	case E_TEP_BACKWARD_WORD:
		new_pos = 0;
		if (text->selection_end >= 1) {
			int pos = text->selection_end;

			p = g_utf8_find_prev_char (text->text, g_utf8_offset_to_pointer (text->text, text->selection_end));
			pos --;

			if (p != text->text) {
				p = g_utf8_find_prev_char (text->text, p);
				pos --;

				while (p && p > text->text) {
					unival = g_utf8_get_char (p);
					if (g_unichar_isspace (unival)) {
						new_pos = pos + 1;
						p = NULL;
					}
					else {
						p = g_utf8_find_prev_char (text->text, p);
						pos --;
					}
				}
			}
		}
					
		break;

	case E_TEP_FORWARD_LINE: {
		int offset_into_line;
		char *p;

		offset_into_line = find_offset_into_line (text, text->selection_end, NULL);
		if (offset_into_line == -1)
			return text->selection_end;

		/* now we search forward til we hit a \n, and then
		   offset_into_line more characters */
		p = g_utf8_offset_to_pointer (text->text, text->selection_end);
		while (p && *p) {
			if (*p == '\n')
				break;
			p = g_utf8_next_char (p);
		}
		if (p && *p == '\n') {
			/* now we loop forward offset_into_line
			   characters, or until we hit \n or \0 */

			p = g_utf8_next_char (p);
			while (offset_into_line > 0 && p && *p != '\n' && *p != '\0') {
				p = g_utf8_next_char (p);
				offset_into_line --;
			}
		}

		/* at this point, p points to the new location,
		   convert it to an offset and we're done */
		new_pos = g_utf8_pointer_to_offset (text->text, p);
		break;
	}
	case E_TEP_BACKWARD_LINE: {
		char *p;
		int offset_into_line = find_offset_into_line (text, text->selection_end, &p);

		if (offset_into_line == -1)
			return text->selection_end;

		/* p points to the first character on our line.  if we
		   have a \n before it, skip it and scan til we hit
		   the next one */
		if (p != text->text) {
			p = g_utf8_find_prev_char (text->text, p);
			if (*p == '\n') {
				p = g_utf8_find_prev_char (text->text, p);
				while (p > text->text) {
					if (*p == '\n') {
						p ++;
						break;
					}
					p = g_utf8_find_prev_char (text->text, p);
				}
			}
		}

		/* at this point 'p' points to the start of the
		   previous line, move forward 'offset_into_line'
		   times. */

		while (offset_into_line > 0 && p && *p != '\n' && *p != '\0') {
			p = g_utf8_next_char (p);
			offset_into_line --;
		}

		/* at this point, p points to the new location,
		   convert it to an offset and we're done */
		new_pos = g_utf8_pointer_to_offset (text->text, p);
		break;
	}
	case E_TEP_SELECT_WORD:
		/* This is a silly hack to cause double-clicking on an object
		   to activate that object.
		   (Normally, double click == select word, which is why this is here.) */
		
		obj_num = e_text_model_get_object_at_offset (text->model, text->selection_start);
		if (obj_num != -1) {
			e_text_model_activate_nth_object (text->model, obj_num);
			new_pos = text->selection_start;
			break;
		}

		if (text->selection_end < 1) {
			new_pos = 0;
			break;
		}

		p = g_utf8_offset_to_pointer (text->text, text->selection_end);

		p = g_utf8_find_prev_char (text->text, p);

		while (p && p > text->text) {
			unival = g_utf8_get_char (p);
			if (g_unichar_isspace (unival)) {
				p = g_utf8_next_char (p);
				break;
			}
			p = g_utf8_find_prev_char (text->text, p);
		}

		if (!p)
			text->selection_start = 0;
		else
			text->selection_start = g_utf8_pointer_to_offset (text->text, p);


		text->selection_start = e_text_model_validate_position (text->model, text->selection_start);
		
		length = g_utf8_strlen (text->text, -1);
		if (text->selection_end >= length) {
			new_pos = length;
			break;
		}

		p = g_utf8_offset_to_pointer (text->text, text->selection_end);
		while (p && *p) {
			unival = g_utf8_get_char (p);
			if (g_unichar_isspace (unival)) {
				new_pos =  g_utf8_pointer_to_offset (text->text, p);
				break;
			} else
				p = g_utf8_next_char (p);
		}

		if (!new_pos)
			new_pos = g_utf8_strlen (text->text, -1);

		return new_pos;

	case E_TEP_SELECT_ALL:
		text->selection_start = 0;
		new_pos = g_utf8_strlen (text->text, -1);
		break;

	case E_TEP_FORWARD_PARAGRAPH:
	case E_TEP_BACKWARD_PARAGRAPH:
		
	case E_TEP_FORWARD_PAGE:
	case E_TEP_BACKWARD_PAGE:
		new_pos = text->selection_end;
		break;

	default:
		new_pos = text->selection_end;
		break;
	}

	new_pos = e_text_model_validate_position (text->model, new_pos);

	return new_pos;
}

static void
e_text_insert(EText *text, const char *string)
{
	int len = strlen (string);

	if (len > 0) {
		int utf8len = 0;

		if (!text->allow_newlines) {
			const char *i;
			char *new_string = g_malloc (len + 1);
			char *j = new_string;

			for (i = string; *i; i = g_utf8_next_char(i)) {
				if (*i != '\n') {
					gunichar c;
					int charlen;

					c = g_utf8_get_char (i);
					charlen = g_unichar_to_utf8 (c, j);
					j += charlen;
					utf8len++;
				}
			}
			*j = 0;
			e_text_model_insert_length(text->model, text->selection_start, new_string, utf8len);
			g_free (new_string);
		}
		else {
			utf8len = g_utf8_strlen (string, -1);
			e_text_model_insert_length(text->model, text->selection_start, string, utf8len);
		}
	}
}

static void
capitalize (EText *text, int start, int end, ETextEventProcessorCaps type)
{
	gboolean first = TRUE;
	const char *p = g_utf8_offset_to_pointer (text->text, start);
	const char *text_end = g_utf8_offset_to_pointer (text->text, end);
	int utf8len = text_end - p;

	if (utf8len > 0) {
		char *new_text = g_new0 (char, utf8len * 6);
		char *output = new_text;

		while (p && *p && p < text_end) {
			gunichar unival = g_utf8_get_char (p);
			gunichar newval = unival;

			switch (type) {
			case E_TEP_CAPS_UPPER:
				newval = g_unichar_toupper (unival);
				break;
			case E_TEP_CAPS_LOWER:
				newval = g_unichar_tolower (unival);
				break;
			case E_TEP_CAPS_TITLE:
				if (g_unichar_isalpha (unival)) {
					if (first)
						newval = g_unichar_totitle (unival);
					else
						newval = g_unichar_tolower (unival);
					first = FALSE;
				} else {
					first = TRUE;
				}
				break;
			}
			g_unichar_to_utf8 (newval, output);
			output = g_utf8_next_char (output);

			p = g_utf8_next_char (p);
		}
		*output = 0;

		e_text_model_delete (text->model, start, utf8len);
		e_text_model_insert_length (text->model, start, new_text, utf8len);
		g_free (new_text);
	}
}

static void
e_text_command(ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data)
{
	EText *text = E_TEXT(data);
	gboolean scroll = TRUE;
	gboolean use_start = TRUE;

	switch (command->action) {
	case E_TEP_MOVE:
		text->selection_start = _get_position(text, command);
		text->selection_end = text->selection_start;
		if (text->timer) {
			g_timer_reset(text->timer);
		}

		use_start = TRUE;
		break;
	case E_TEP_SELECT:
		text->selection_start = e_text_model_validate_position (text->model, text->selection_start); /* paranoia */
		text->selection_end = _get_position(text, command);
		
		e_text_update_primary_selection (text);

		use_start = FALSE;

		break;
	case E_TEP_DELETE:
		if (text->selection_end == text->selection_start) {
			text->selection_end = _get_position(text, command);
		}
		e_text_delete_selection(text);
		if (text->timer) {
			g_timer_reset(text->timer);
		}

		use_start = FALSE;

		break;

	case E_TEP_INSERT:
		if (g_utf8_validate (command->string, command->value, NULL)) {
			if (text->selection_end != text->selection_start) {
				e_text_delete_selection(text);
			}
			e_text_insert(text, command->string);
			if (text->timer) {
				g_timer_reset(text->timer);
			}
		}
		break;
	case E_TEP_COPY:
		e_text_copy_clipboard (text);

		if (text->timer) {
			g_timer_reset(text->timer);
		}
		scroll = FALSE;
		break;
	case E_TEP_PASTE:
		e_text_paste (text, GDK_NONE);
		if (text->timer) {
			g_timer_reset(text->timer);
		}
		break;
	case E_TEP_GET_SELECTION:
		e_text_paste (text, GDK_SELECTION_PRIMARY);
		break;
	case E_TEP_ACTIVATE:
		g_signal_emit (text, e_text_signals[E_TEXT_ACTIVATE], 0);
		if (text->timer) {
			g_timer_reset(text->timer);
		}
		break;
	case E_TEP_SET_SELECT_BY_WORD:
		text->select_by_word = command->value;
		break;
	case E_TEP_GRAB:
		e_canvas_item_grab (E_CANVAS (GNOME_CANVAS_ITEM(text)->canvas),
				    GNOME_CANVAS_ITEM(text), 
				    GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
				    text->i_cursor,
				    command->time,
				    NULL,
				    NULL);
		scroll = FALSE;
		break;
	case E_TEP_UNGRAB:
		e_canvas_item_ungrab (E_CANVAS (GNOME_CANVAS_ITEM(text)->canvas),
				      GNOME_CANVAS_ITEM(text),
				      command->time);
		scroll = FALSE;
		break;
	case E_TEP_CAPS:
		if (text->selection_start == text->selection_end) {
			capitalize (text, text->selection_start, next_word (text, text->selection_start), command->value);
		} else {
			int selection_start = MIN (text->selection_start, text->selection_end);
			int selection_end = MAX (text->selection_start, text->selection_end);
			capitalize (text, selection_start, selection_end, command->value);
		}
		break;
	case E_TEP_NOP:
		scroll = FALSE;
		break;
	}

	/* it's possible to get here without ever having been realized
	   by our canvas (if the e-text started completely obscured.)
	   so let's create our layout object if we don't already have
	   one. */
	if (!text->layout)
		create_layout (text);

	if (scroll && !text->button_down) {
		/* XXX do we really need the @trailing logic here?  if
		   we don't we can scrap the loop and just use
		   pango_layout_index_to_pos */
		PangoLayoutLine *cur_line = NULL;
		int selection_index;
		PangoLayoutIter *iter = pango_layout_get_iter (text->layout);

		selection_index = use_start ? text->selection_start : text->selection_end;
		/* convert to a byte index */
		selection_index = g_utf8_offset_to_pointer (text->text, selection_index) - text->text;

		do {
			PangoLayoutLine *line = pango_layout_iter_get_line (iter);

			if (selection_index >= line->start_index && selection_index <= line->start_index + line->length) {
				/* found the line with the start of the selection */
				cur_line = line;
				break;
			}

		} while (pango_layout_iter_next_line (iter));

		if (cur_line) {
			int xpos, ypos;
			double clip_width, clip_height;
			gboolean trailing = FALSE;
			PangoRectangle pango_pos;

			if (selection_index > 0 && selection_index == cur_line->start_index + cur_line->length) {
				selection_index--;
				trailing = TRUE;
			}

			pango_layout_index_to_pos (text->layout, selection_index, &pango_pos);

			pango_pos.x = PANGO_PIXELS (pango_pos.x);
			pango_pos.y = PANGO_PIXELS (pango_pos.y);
			pango_pos.width = (pango_pos.width + PANGO_SCALE / 2) / PANGO_SCALE;
			pango_pos.height = (pango_pos.height + PANGO_SCALE / 2) / PANGO_SCALE;

			/* scroll for X */
			xpos = pango_pos.x; /* + (trailing ? 0 : pango_pos.width);*/

			if (xpos + 2 < text->xofs_edit) {
				text->xofs_edit = xpos;
			}
                                                                                                
			clip_width = text->clip_width;
			if (clip_width >= 0 && text->draw_borders) {
				clip_width -= 6;
				if (clip_width < 0)
					clip_width = 0;
			}
                                                                                                
			if (xpos + pango_pos.width - clip_width > text->xofs_edit) {
				text->xofs_edit = xpos + pango_pos.width - clip_width;
			}

			/* scroll for Y */
			if (pango_pos.y + 2 < text->yofs_edit) {
				ypos = pango_pos.y;
				text->yofs_edit = ypos;
			}
			else {
				ypos = pango_pos.y + pango_pos.height;
			}

			if ( text->clip_height < 0 )
				clip_height = text->height;
			else
				clip_height = text->clip_height;
                                                                                                
			if (clip_height >= 0 && text->draw_borders) {
				clip_height -= 6;
				if (clip_height < 0)
					clip_height = 0;
			}
                                                                                                
			if (ypos - clip_height > text->yofs_edit) {
				text->yofs_edit = ypos - clip_height;
			}

		}

		pango_layout_iter_free (iter);
	}

	text->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(text));
}


/* Class initialization function for the text item */
static void
e_text_class_init (ETextClass *klass)
{
	GObjectClass *gobject_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) klass;
	item_class = (GnomeCanvasItemClass *) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	gobject_class->dispose = e_text_dispose;
	gobject_class->set_property = e_text_set_property;
	gobject_class->get_property = e_text_get_property;

	item_class->update = e_text_update;
	item_class->realize = e_text_realize;
	item_class->unrealize = e_text_unrealize;
	item_class->draw = e_text_draw;
	item_class->point = e_text_point;
	item_class->bounds = e_text_bounds;
	item_class->event = e_text_event;

	klass->changed = NULL;
	klass->activate = NULL;

	e_text_signals[E_TEXT_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextClass, changed),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_text_signals[E_TEXT_ACTIVATE] =
		g_signal_new ("activate",
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextClass, activate),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_text_signals[E_TEXT_KEYPRESS] =
		g_signal_new ("keypress",
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextClass, keypress),
			      NULL, NULL,
			      e_util_marshal_NONE__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

	e_text_signals[E_TEXT_POPULATE_POPUP] =
		g_signal_new ("populate_popup",
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextClass, populate_popup),
			      NULL, NULL,
			      e_util_marshal_NONE__POINTER_INT_OBJECT,
			      G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_INT, GTK_TYPE_MENU);

	g_object_class_install_property (gobject_class, PROP_MODEL,
					 g_param_spec_object ("model",
							      _( "Model" ),
							      _( "Model" ),
							      E_TYPE_TEXT_MODEL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_EVENT_PROCESSOR,
					 g_param_spec_object ("event_processor",
							      _( "Event Processor" ),
							      _( "Event Processor" ),
							      E_TEXT_EVENT_PROCESSOR_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_TEXT,
					 g_param_spec_string ("text",
							      _( "Text" ),
							      _( "Text" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_BOLD,
					 g_param_spec_boolean ("bold",
							       _( "Bold" ),
							       _( "Bold" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_STRIKEOUT,
					 g_param_spec_boolean ("strikeout",
							       _( "Strikeout" ),
							       _( "Strikeout" ),
							       FALSE,
							       G_PARAM_READWRITE));
               
	g_object_class_install_property (gobject_class, PROP_ANCHOR,
					 g_param_spec_enum ("anchor",
							    _( "Anchor" ),
							    _( "Anchor" ),
							    GTK_TYPE_ANCHOR_TYPE, GTK_ANCHOR_CENTER,
							    G_PARAM_READWRITE));


	g_object_class_install_property (gobject_class, PROP_JUSTIFICATION,
					 g_param_spec_enum ("justification",
							    _( "Justification" ),
							    _( "Justification" ),
							    GTK_TYPE_JUSTIFICATION, GTK_JUSTIFY_LEFT,
							    G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CLIP_WIDTH,
					 g_param_spec_double ("clip_width",
							      _( "Clip Width" ),
							      _( "Clip Width" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CLIP_HEIGHT,
					 g_param_spec_double ("clip_height",
							      _( "Clip Height" ),
							      _( "Clip Height" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CLIP,
					 g_param_spec_boolean ("clip",
							       _( "Clip" ),
							       _( "Clip" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FILL_CLIP_RECTANGLE,
					 g_param_spec_boolean ("fill_clip_rectangle",
							       _( "Fill clip rectangle" ),
							       _( "Fill clip rectangle" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_X_OFFSET,
					 g_param_spec_double ("x_offset",
							      _( "X Offset" ),
							      _( "X Offset" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_Y_OFFSET,
					 g_param_spec_double ("y_offset",
							      _( "Y Offset" ),
							      _( "Y Offset" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FILL_COLOR,
					 g_param_spec_string ("fill_color",
							      _( "Fill color" ),
							      _( "Fill color" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FILL_COLOR_GDK,
					 g_param_spec_boxed ("fill_color_gdk",
							     _( "GDK fill color" ),
							     _( "GDK fill color" ),
							     GDK_TYPE_COLOR,
							     G_PARAM_READWRITE));


	g_object_class_install_property (gobject_class, PROP_FILL_COLOR_RGBA,
					 g_param_spec_uint ("fill_color_rgba",
							    _( "GDK fill color" ),
							    _( "GDK fill color" ),
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FILL_STIPPLE,
					 g_param_spec_object ("fill_stipple",
							      _( "Fill stipple" ),
							      _( "Fill stipple" ),
							      GDK_TYPE_DRAWABLE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_TEXT_WIDTH,
					 g_param_spec_double ("text_width",
							      _( "Text width" ),
							      _( "Text width" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_TEXT_HEIGHT,
					 g_param_spec_double ("text_height",
							      _( "Text height" ),
							      _( "Text height" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READABLE));


	g_object_class_install_property (gobject_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _( "Editable" ),
							       _( "Editable" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_USE_ELLIPSIS,
					 g_param_spec_boolean ("use_ellipsis",
							       _( "Use ellipsis" ),
							       _( "Use ellipsis" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_ELLIPSIS,
					 g_param_spec_string ("ellipsis",
							      _( "Ellipsis" ),
							      _( "Ellipsis" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_LINE_WRAP,
					 g_param_spec_boolean ("line_wrap",
							       _( "Line wrap" ),
							       _( "Line wrap" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_BREAK_CHARACTERS,
					 g_param_spec_string ("break_characters",
							      _( "Break characters" ),
							      _( "Break characters" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_MAX_LINES,
					 g_param_spec_int ("max_lines",
							   _( "Max lines" ),
							   _( "Max lines" ),
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_WIDTH,
					 g_param_spec_double ("width",
							      _( "Width" ),
							      _( "Width" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));


	g_object_class_install_property (gobject_class, PROP_HEIGHT,
					 g_param_spec_double ("height",
							      _( "Height" ),
							      _( "Height" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_DRAW_BORDERS,
					 g_param_spec_boolean ("draw_borders",
							       _( "Draw borders" ),
							       _( "Draw borders" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_ALLOW_NEWLINES,
					 g_param_spec_boolean ("allow_newlines",
							       _( "Allow newlines" ),
							       _( "Allow newlines" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_DRAW_BACKGROUND,
					 g_param_spec_boolean ("draw_background",
							       _( "Draw background" ),
							       _( "Draw background" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_DRAW_BUTTON,
					 g_param_spec_boolean ("draw_button",
							       _( "Draw button" ),
							       _( "Draw button" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CURSOR_POS,
					 g_param_spec_int ("cursor_pos",
							   _( "Cursor position" ),
							   _( "Cursor position" ),
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_IM_CONTEXT,
					 g_param_spec_object ("im_context",
							      _( "IM Context" ),
							      _( "IM Context" ),
							      GTK_TYPE_IM_CONTEXT,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_HANDLE_POPUP,
					 g_param_spec_boolean ("handle_popup",
							       _( "Handle Popup" ),
							       _( "Handle Popup" ),
							       FALSE,
							       G_PARAM_READWRITE));

	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

	gal_a11y_e_text_init ();
}

/* Object initialization function for the text item */
static void
e_text_init (EText *text)
{
	text->model                   = e_text_model_new ();
	text->text                    = e_text_model_get_text (text->model);
	text->preedit_len	      = 0;
	text->layout                  = NULL;

	text->revert                  = NULL;

	text->model_changed_signal_id = 
		g_signal_connect (text->model,
				  "changed",
				  G_CALLBACK (e_text_text_model_changed),
				  text);
	text->model_repos_signal_id   = 
		g_signal_connect (text->model,
				  "reposition",
				  G_CALLBACK (e_text_text_model_reposition),
				  text);

	text->anchor                  = GTK_ANCHOR_CENTER;
	text->justification           = GTK_JUSTIFY_LEFT;
	text->clip_width              = -1.0;
	text->clip_height             = -1.0;
	text->xofs                    = 0.0;
	text->yofs                    = 0.0;

	text->ellipsis                = NULL;
	text->use_ellipsis            = FALSE;
	text->ellipsis_width          = 0;

	text->editable                = FALSE;
	text->editing                 = FALSE;
	text->xofs_edit               = 0;
	text->yofs_edit               = 0;

	text->selection_start         = 0;
	text->selection_end           = 0;
	text->select_by_word          = FALSE;

	text->timeout_id              = 0;
	text->timer                   = NULL;

	text->lastx                   = 0;
	text->lasty                   = 0;
	text->last_state              = 0;

	text->scroll_start            = 0;
	text->show_cursor             = TRUE;
	text->button_down             = FALSE;

	text->tep                     = NULL;
	text->tep_command_id          = 0;

	text->has_selection           = FALSE;

	text->pointer_in              = FALSE;
	text->default_cursor_shown    = TRUE;
	text->line_wrap               = FALSE;
	text->break_characters        = NULL;
	text->max_lines               = -1;
	text->tooltip_timeout         = 0;
	text->tooltip_count           = 0;
	text->tooltip_owner           = FALSE;
	text->dbl_timeout             = 0;
	text->tpl_timeout             = 0;

	text->draw_background         = FALSE;
	text->draw_button             = FALSE;

	text->bold                    = FALSE;
	text->strikeout               = FALSE;

	text->allow_newlines          = TRUE;

	text->last_type_request       = -1;
	d(g_print ("Setting last_type_request to %d at line %d\n", text->last_type_request, __LINE__));
	text->last_time_request       = 0;
	text->queued_requests         = NULL;

	text->im_context              = NULL;
	text->need_im_reset           = FALSE;
	text->im_context_signals_registered = FALSE;

	text->handle_popup            = FALSE;


	e_canvas_item_set_reflow_callback(GNOME_CANVAS_ITEM(text), e_text_reflow);
}

/**
 * e_text_get_type:
 * @void: 
 * 
 * Registers the &EText class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &EText class.
 **/
E_MAKE_TYPE (e_text,
	     "EText",
	     EText,
	     e_text_class_init,
	     e_text_init,
	     PARENT_TYPE)



/* IM Context Callbacks */
static void
e_text_commit_cb (GtkIMContext *context,
		  const gchar  *str,
		  EText        *text)
{
	if (g_utf8_validate (str, strlen (str), NULL)) {
		if (text->selection_end != text->selection_start)
			e_text_delete_selection (text);
		e_text_insert (text, str);
		g_signal_emit (text, e_text_signals[E_TEXT_KEYPRESS], 0, 0, 0);
	}
}

static void
e_text_preedit_changed_cb (GtkIMContext *context,
				EText        *etext)
{
	gchar *preedit_string = NULL;

	gtk_im_context_get_preedit_string (context, &preedit_string, 
					NULL, NULL);

	etext->preedit_len = strlen (preedit_string);

	g_signal_emit (etext, e_text_signals[E_TEXT_KEYPRESS], 0, 0, 0);
}

static gboolean
e_text_retrieve_surrounding_cb (GtkIMContext *context,
				EText        *text)
{
	gtk_im_context_set_surrounding (context,
					text->text,
					strlen (text->text),
					g_utf8_offset_to_pointer (text->text, MIN (text->selection_start, text->selection_end)) - text->text);
	
	return TRUE;
}

static gboolean
e_text_delete_surrounding_cb   (GtkIMContext *context,
				gint          offset,
				gint          n_chars,
				EText        *text)
{
	e_text_model_delete (text->model,
		             MIN (text->selection_start, text->selection_end) + offset,
			     n_chars);

	return TRUE;
}
