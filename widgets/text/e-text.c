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

#include "e-text.h"

#include <math.h>
#include <ctype.h>
#include <string.h>
#include <gdk/gdkx.h> /* for BlackPixel */
#include <gtk/gtkinvisible.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkselection.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktypebuiltins.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>

#include "gal/util/e-util.h"
#include "gal/util/e-i18n.h"
#include "gal/widgets/e-canvas.h"
#include "gal/widgets/e-canvas-utils.h"
#include "gal/widgets/e-unicode.h"
#include "gal/util/e-text-event-processor-emacs-like.h"
#include "gal/util/e-util.h"
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rgb_bitmap_affine.h>

#define BORDER_INDENT 4
#define d(x)

enum {
	E_TEXT_CHANGED,
	E_TEXT_ACTIVATE,
	E_TEXT_KEYPRESS,
	E_TEXT_POPUP,
	E_TEXT_STYLE_SET,
	E_TEXT_LAST_SIGNAL
};

static GQuark e_text_signals[E_TEXT_LAST_SIGNAL] = { 0 };



/* This defines a line of text */
struct line {
	const char *text;	/* Line's text, it is a pointer into the text->text string */
	int length;	/* Line's length IN BYTES */
	int width;	/* Line's width in pixels */
	int ellipsis_length;  /* Length before adding ellipsis */
};

/* Object argument IDs */
enum {
	PROP_0,
	PROP_MODEL,
	PROP_EVENT_PROCESSOR,
	PROP_TEXT,
	PROP_FONT,
        PROP_FONTSET,
	PROP_FONT_GDK,
	PROP_FONT_E,
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
	PROP_CURSOR_POS
};


enum {
	E_SELECTION_PRIMARY,
	E_SELECTION_CLIPBOARD
};
enum _TargetInfo {  
	TARGET_UTF8_STRING,
	TARGET_UTF8,
	TARGET_COMPOUND_TEXT,
	TARGET_STRING,
	TARGET_TEXT
};


static void e_text_command(ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data);

static void e_text_get_selection(EText *text, GdkAtom selection, guint32 time);
static void e_text_supply_selection (EText *text, guint time, GdkAtom selection, guchar *data, gint length);

static void e_text_text_model_changed(ETextModel *model, EText *text);
static void e_text_text_model_reposition (ETextModel *model, ETextModelReposFn fn, gpointer repos_data, gpointer data);

static void _get_tep(EText *text);

static GtkWidget *e_text_get_invisible(EText *text);
static void _selection_clear_event (GtkInvisible *invisible,
				    GdkEventSelection *event,
				    EText *text);
static void _selection_get (GtkInvisible *invisible,
			    GtkSelectionData *selection_data,
			    guint info,
			    guint time_stamp,
			    EText *text);
static void _selection_received (GtkInvisible *invisible,
				 GtkSelectionData *selection_data,
				 guint time,
				 EText *text);

#if 0
static ETextSuckFont *e_suck_font (GdkFont *font);
static void e_suck_font_free (ETextSuckFont *suckfont);
#endif

static void e_text_free_lines(EText *text);

static gint text_width_with_objects (ETextModel *model,
				     EFont *font, EFontStyle style,
				     const gchar *text, gint bytelen);

static void calc_height (EText *text);
static void calc_line_widths (EText *text);
static void split_into_lines (EText *text);

#define PARENT_TYPE GNOME_TYPE_CANVAS_ITEM
static GnomeCanvasItemClass *parent_class;
static GdkAtom clipboard_atom = GDK_NONE;



/* Destroy handler for the text item */


static void
e_text_style_set (EText *text, GtkStyle *previous_style)
{
	if ( text->line_wrap ) {
		text->needs_split_into_lines = 1;
	} else {
		text->needs_calc_line_widths = 1;
		text->needs_calc_height = 1;
	}
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (text));
}

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
	
	if (text->invisible)
		g_object_unref (text->invisible);
	text->invisible = NULL;

	g_free (text->lines);
	text->lines = NULL;

	g_free (text->primary_selection);
	text->primary_selection = NULL;

	g_free (text->clipboard_selection);
	text->clipboard_selection = NULL;

	g_free (text->revert);
	text->revert = NULL;

	if (text->font)
		e_font_unref (text->font);
	text->font = NULL;

#if 0
	if (text->suckfont)
		e_suck_font_free (text->suckfont);
#endif

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

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
e_text_text_model_changed (ETextModel *model, EText *text)
{
	gint model_len = e_text_model_get_text_length (model);
	text->text = e_text_model_get_text(model);
	e_text_free_lines(text);

	/* Make sure our selection doesn't extend past the bounds of our text. */
	text->selection_start = CLAMP (text->selection_start, 0, model_len);
	text->selection_end   = CLAMP (text->selection_end,   0, model_len);

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
get_bounds_item_relative (EText *text, double *px1, double *py1, double *px2, double *py2)
{
	GnomeCanvasItem *item;
	double x, y;
	double clip_x, clip_y;
	int old_height;

	item = GNOME_CANVAS_ITEM (text);
	
	x = 0;
	y = 0;

	clip_x = x;
	clip_y = y;

	/* Calculate text dimensions */

	old_height = text->height;

	if (text->text && text->font)
		text->height = (e_font_height (text->font)) * text->num_lines;
	else
		text->height = 0;

	if (old_height != text->height)
		e_canvas_item_request_parent_reflow(item);

	/* Anchor text */

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		x -= text->max_width / 2;
		if ( text->clip_width >= 0)
			clip_x -= text->clip_width / 2;
		else
			clip_x -= text->width / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		x -= text->max_width;
		if (text->clip_width >= 0)
			clip_x -= text->clip_width;
		else
			clip_x -= text->width;
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
		y -= text->height / 2;
		if ( text->clip_height >= 0 )
			clip_y -= text->clip_height / 2;
		else
			clip_y -= text->height / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		y -= text->height;
		if ( text->clip_height >= 0 )
			clip_y -= text->clip_height;
		else
			clip_y -= text->height;
		break;
	}

	/* Bounds */

	if (text->clip) {
		/* maybe do bbox intersection here? */
		*px1 = clip_x;
		*py1 = clip_y;
		if (text->clip_width >= 0)
			*px2 = clip_x + text->clip_width;
		else
			*px2 = clip_x + text->width;

		if ( text->clip_height >= 0 )
			*py2 = clip_y + text->clip_height;
		else
			*py2 = clip_y + text->height;
	} else {
		*px1 = x;
		*py1 = y;
		*px2 = x + text->max_width;
		*py2 = y + text->height;
	}
}

static void
get_bounds (EText *text, double *px1, double *py1, double *px2, double *py2)
{
	GnomeCanvasItem *item;
	double wx, wy, clip_width;

	item = GNOME_CANVAS_ITEM (text);

	/* Get canvas pixel coordinates for text position */

	wx = 0;
	wy = 0;
	gnome_canvas_item_i2w (item, &wx, &wy);
	gnome_canvas_w2c (item->canvas, wx + text->xofs, wy + text->yofs, &text->cx, &text->cy);

	if (text->clip_width < 0)
		clip_width = text->max_width;
	else
		clip_width = text->clip_width;

	/* Get canvas pixel coordinates for clip rectangle position */
	gnome_canvas_w2c (item->canvas, wx, wy, &text->clip_cx, &text->clip_cy);
	text->clip_cwidth = clip_width * item->canvas->pixels_per_unit;
	if ( text->clip_height >= 0 )
		text->clip_cheight = text->clip_height * item->canvas->pixels_per_unit;
	else
		text->clip_cheight = text->height * item->canvas->pixels_per_unit;

	/* Anchor text */

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		text->cx -= text->max_width / 2;
		text->clip_cx -= text->clip_cwidth / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		text->cx -= text->max_width;
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

	/* Bounds */

	if (text->clip) {
		*px1 = text->clip_cx;
		*py1 = text->clip_cy;
		*px2 = text->clip_cx + text->clip_cwidth;
		*py2 = text->clip_cy + text->clip_cheight;
	} else {
		*px1 = text->cx;
		*py1 = text->cy;
		*px2 = text->cx + text->max_width;
		*py2 = text->cy + text->height;
	}
}

static void
calc_height (EText *text)
{
	GnomeCanvasItem *item;
	int old_height;

	item = GNOME_CANVAS_ITEM (text);

	/* Calculate text dimensions */

	old_height = text->height;

	if (text->text && text->font)
		text->height = e_font_height (text->font) * text->num_lines;
	else
		text->height = 0;

	if (old_height != text->height)
		e_canvas_item_request_parent_reflow(item);
}

static void
calc_ellipsis (EText *text)
{
	if (text->font)
		text->ellipsis_width = 
			e_font_utf8_text_width (text->font, text->style,
						text->ellipsis ? text->ellipsis : "...",
						text->ellipsis ? strlen (text->ellipsis) : 3);
}

/* Calculates the line widths (in pixels) of the text's splitted lines */
static void
calc_line_widths (EText *text)
{
	struct line *lines;
	int i;
	gdouble clip_width;
	const gchar *p;

	lines = text->lines;
	text->max_width = 0;

	clip_width = text->clip_width;
	if (clip_width >= 0 && text->draw_borders) {
		clip_width -= 6;
		if (clip_width < 0)
			clip_width = 0;
	}


	if (!lines)
		return;

	for (i = 0; i < text->num_lines; i++) {
		if (lines->length != 0) {
			if (text->font) {
				lines->width = text_width_with_objects (text->model,
									text->font, text->style,
									lines->text, lines->length);
				lines->ellipsis_length = 0;
			} else {
				lines->width = 0;
			}
			
			if (text->clip && 
			    text->use_ellipsis &&
			    ! text->editing &&
			    lines->width > clip_width &&
			    clip_width >= 0) {
				if (text->font) {
					lines->ellipsis_length = 0;
					for (p = lines->text;
					     p && *p && g_unichar_validate (g_utf8_get_char (p)) && (p - lines->text) < lines->length;
					     p = g_utf8_next_char (p)) {
						gint text_width = text_width_with_objects (text->model,
											   text->font, text->style,
											   lines->text, p - lines->text);
						if (clip_width >= text_width + text->ellipsis_width)
							lines->ellipsis_length = p - lines->text;
						else
							break;
					}
				}
				else
					lines->ellipsis_length = 0;
				lines->width = text_width_with_objects (text->model,
									text->font, text->style,
									lines->text, lines->ellipsis_length) + 
					text->ellipsis_width;
			} else
				lines->ellipsis_length = lines->length;

			if (lines->width > text->max_width)
				text->max_width = lines->width;
		}

		lines++;
	}
}

static void
e_text_free_lines(EText *text)
{
	if (text->lines)
		g_free (text->lines);

	text->lines = NULL;
	text->num_lines = 0;
}

static gint
text_width_with_objects (ETextModel *model,
			 EFont *font, EFontStyle style,
			 const gchar *text, gint numbytes)
{
	return text && *text ? e_font_utf8_text_width (font, style, text, numbytes) : 0;
}

static void
text_draw_with_objects (ETextModel *model, 
			GdkDrawable *drawable,
			EFont *font, EFontStyle style,
			GdkGC *gc,
			gint x, gint y,
			const gchar *text, gint numbytes)
{
	const gchar *c;

	if (text == NULL)
		return;
	
	if (text == NULL)
		return;
	
	while (*text && numbytes > 0) {
		gint obj_num = -1;
		
		c = text;

		while (*c
		       && (obj_num = e_text_model_get_object_at_pointer (model, c)) == -1
		       && numbytes > 0) {
			++c;
			--numbytes;
		}

		e_font_draw_utf8_text (drawable, font, style, gc, x, y, text, c-text);
		x += e_font_utf8_text_width (font, style, text, c-text);
		
		if (obj_num != -1 && numbytes > 0) {
			gint len;
			gint start_x = x;

			e_text_model_get_nth_object (model, obj_num, &len);

			if (len > numbytes)
				len = numbytes;
			e_font_draw_utf8_text (drawable, font, style, gc, x, y, c, len);
			x += e_font_utf8_text_width (font, style, c, len);

			/* We underline our objects. */
			gdk_draw_line (drawable, gc, start_x, y+1, x, y+1);

			c += len;
			numbytes -= len;
		}
		
		text = c;
	}
}

typedef void (*LineSplitterFn) (int line_num, const char *start, int length, gpointer user_data);

#define IS_BREAK_CHAR(break_chars, c) (g_unichar_isspace (c) || ((break_chars) && g_utf8_strchr ((break_chars), -1, (c))))

static gint
line_splitter (ETextModel *model, EFont *font, EFontStyle style,
	       const char *break_characters,
	       gboolean wrap_lines, double clip_width, double clip_height,
	       gint max_lines, LineSplitterFn split_cb, gpointer user_data)
{
	const char *curr;
	const char *text;
	const char *linestart;
	const char *last_breakpoint;
	gint line_count = 0;

	gunichar unival;

	if (max_lines < 1)
		max_lines = G_MAXINT;
	if (clip_height != -1)
		max_lines = CLAMP (max_lines, 1, clip_height / e_font_height (font));
	
	text = e_text_model_get_text (model);
	linestart = NULL;
	last_breakpoint = text;

	for (curr = text; curr && *curr && line_count < max_lines; curr = g_utf8_next_char (curr)) {

		unival = g_utf8_get_char (curr);

		if (linestart == NULL)
			linestart = curr;

		if (unival == '\n') { /* We always break on newline */

			if (split_cb)
				split_cb (line_count, linestart, curr - linestart, user_data);
			++line_count;
			linestart = NULL;

		} else if (wrap_lines) {
			
			if (clip_width < text_width_with_objects (model, font, style, linestart, curr - linestart)
			    && last_breakpoint > linestart) {

				/* Don't use break point if we are on the last usable line */
				if (split_cb && line_count < max_lines - 1)
					split_cb (line_count, linestart, last_breakpoint - linestart, user_data);
				else if (split_cb)
					split_cb (line_count, linestart, strlen (linestart), user_data);

				++line_count;
				linestart = NULL;
				curr = last_breakpoint;

			} else if (IS_BREAK_CHAR (break_characters, unival)
				   && e_text_model_get_object_at_pointer (model, curr) == -1) { /* don't break mid-object */
				last_breakpoint = curr;
			}
		}
	}

	/* Handle any leftover text. */
	if (linestart) {
		if (split_cb)
			split_cb (line_count, linestart, strlen (linestart), user_data);
		++line_count;

	}

	if (line_count == 0) {
		if (split_cb)
			split_cb (0, text, strlen (text), user_data);
		line_count ++;
	}
	
	return line_count;
}

static void
line_split_cb (int line_num, const char *start, int length, gpointer user_data)
{
	EText *text = user_data;
	struct line *line = &((struct line *)text->lines)[line_num];

	line->text = start;
	line->length = length;
}

static void
split_into_lines (EText *text)
{
	/* Free old array of lines */
	e_text_free_lines (text);

	/* First, count the number of lines */
	text->num_lines = line_splitter (text->model, text->font, text->style,
					 text->break_characters,
					 text->line_wrap, text->clip_width, text->clip_height,
					 -1, NULL, NULL);

	/* Allocate our array of lines */
	text->lines = g_new0 (struct line, text->num_lines);

	text->num_lines = line_splitter (text->model, text->font, text->style,
					 text->break_characters,
					 text->line_wrap, text->clip_width, text->clip_height,
					 text->num_lines, line_split_cb, text);
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

		e_text_free_lines(text);

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
			g_signal_connect (text->tep,
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

	case PROP_FONT:
		if (text->font)
  			e_font_unref (text->font);

		text->font = e_font_from_gdk_name (g_value_get_string (value));
		
		calc_ellipsis (text);
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_line_widths = 1;
			text->needs_calc_height = 1;
		}
		needs_reflow = 1;
		break;

	case PROP_FONTSET:
		if (text->font)
  			e_font_unref (text->font); 

		text->font = e_font_from_gdk_name (g_value_get_string (value));

		calc_ellipsis (text);
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_line_widths = 1;
			text->needs_calc_height = 1;
		}
		needs_reflow = 1;
		break;

	case PROP_FONT_GDK:
		/* Ref the font in case it was the font that is stored
		   in the e-font */
		gdk_font_ref (g_value_get_boxed (value));
		if (text->font)
  			e_font_unref (text->font); 

		text->font = e_font_from_gdk_font (g_value_get_boxed (value));

		calc_ellipsis (text);
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_line_widths = 1;
			text->needs_calc_height = 1;
		}
		needs_reflow = 1;
		break;

	case PROP_FONT_E:
		if (text->font)
			e_font_unref (text->font);

		text->font = g_value_get_pointer (value);
		e_font_ref (text->font);

		calc_ellipsis (text);
		if (text->line_wrap)
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_line_widths = 1;
			text->needs_calc_height = 1;
		}
		needs_reflow = 1;
		break;

	case PROP_BOLD:
		text->bold = g_value_get_boolean (value);
		text->style = text->bold ? E_FONT_BOLD : E_FONT_PLAIN;

		text->needs_redraw = 1;
		text->needs_recalc_bounds = 1;
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_line_widths = 1;
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
			text->needs_calc_line_widths = 1;
			text->needs_calc_height = 1;
		}
		needs_reflow = 1;
		break;

	case PROP_CLIP_HEIGHT:
		text->clip_height = fabs (g_value_get_double (value));
		text->needs_recalc_bounds = 1;
		needs_reflow = 1;
		break;

	case PROP_CLIP:
		text->clip = g_value_get_boolean (value);
		calc_ellipsis (text);
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_line_widths = 1;
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
		pcolor = g_value_get_pointer (value);
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
		text->needs_calc_line_widths = 1;
		needs_reflow = 1;
		break;

	case PROP_ELLIPSIS:
		if (text->ellipsis)
			g_free (text->ellipsis);

		text->ellipsis = g_strdup (g_value_get_string (value));
		calc_ellipsis (text);
		text->needs_calc_line_widths = 1;
		needs_reflow = 1;
		break;

	case PROP_LINE_WRAP:
		text->line_wrap = g_value_get_boolean (value);
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
		if ( text->line_wrap )
			text->needs_split_into_lines = 1;
		else {
			text->needs_calc_line_widths = 1;
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
		if (text->draw_button != g_value_get_boolean (value)) {
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
		
	default:
		return;
	}

	if (color_changed) {
#ifndef NO_WARNINGS
#warning Color stuff ...
#endif
#if 0
		if (GNOME_CANVAS_ITEM_REALIZED & GTK_OBJECT_FLAGS(item))
			gdk_color_context_query_color (item->canvas->cc, &color);
#endif

		text->color = color;

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

	case PROP_FONT_E:
		g_value_set_pointer (value, text->font);
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
		g_value_set_pointer (value, gdk_color_copy (&text->color));
		break;

	case PROP_FILL_COLOR_RGBA:
		g_value_set_uint (value, text->rgba);
		break;

	case PROP_FILL_STIPPLE:
		g_value_set_object (value, text->stipple);
		break;

	case PROP_TEXT_WIDTH:
		g_value_set_double (value, text->max_width / text->item.canvas->pixels_per_unit);
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

	if (text->needs_split_into_lines) {
		split_into_lines (text);

		text->needs_split_into_lines = 0;
		text->needs_calc_line_widths = 1;
		text->needs_calc_height = 1;
	}

	if ( text->needs_calc_line_widths ) {
		int x;
		int i;
		struct line *lines;
		gdouble clip_width;
		calc_line_widths (text);
		text->needs_calc_line_widths = 0;
		text->needs_redraw = 1;

		lines = text->lines;
		if ( !lines )
			return;

		for (lines = text->lines, i = 0; i < text->num_lines ; i++, lines ++) {
			if ((lines->text - text->text) > text->selection_end) {
				break;
			}
		}
		lines --;
		i--;
		x = text_width_with_objects (text->model,
					     text->font, text->style,
					     lines->text,
					     text->selection_end - (lines->text - text->text));

		if (x < text->xofs_edit) {
			text->xofs_edit = x;
		}

		clip_width = text->clip_width;
		if (clip_width >= 0 && text->draw_borders) {
			clip_width -= 6;
			if (clip_width < 0)
				clip_width = 0;
		}

		if (2 + x - clip_width > text->xofs_edit) {
			text->xofs_edit = 2 + x - clip_width;
		}

		if (e_font_height (text->font) * i < text->yofs_edit)
			text->yofs_edit = e_font_height (text->font) * i;

		if ( text->needs_calc_height ) {
			calc_height (text);
			gnome_canvas_item_request_update(item);
			text->needs_calc_height = 0;
			text->needs_recalc_bounds = 1;
		}

		if (e_font_height (text->font) * (i + 1) -
		     (text->clip_height != -1 ? text->clip_height : text->height) > text->yofs_edit)
			text->yofs_edit = e_font_height (text->font) * (i + 1) -
				(text->clip_height != -1 ? text->clip_height : text->height);

		gnome_canvas_item_request_update (item);
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
	ArtDRect i_bbox, c_bbox;
	int i;

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
		} else {
			/* aa rendering */
			for (i = 0; i < 6; i++)
				text->affine[i] = affine[i];
			get_bounds_item_relative (text, &i_bbox.x0, &i_bbox.y0, &i_bbox.x1, &i_bbox.y1);
			art_drect_affine_transform (&c_bbox, &i_bbox, affine);
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
	if (text->font == NULL) {
		gdk_font_ref (gtk_style_get_font (GTK_WIDGET (item->canvas)->style));
		text->font = e_font_from_gdk_font (gtk_style_get_font (GTK_WIDGET (item->canvas)->style));
	}
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

/* Calculates the x position of the specified line of text, based on the text's justification */
static double
get_line_xpos_item_relative (EText *text, struct line *line)
{
	double x;

	x = 0;

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		x -= text->max_width / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		x -= text->max_width;
		break;
	}

	switch (text->justification) {
	case GTK_JUSTIFY_RIGHT:
		x += text->max_width - line->width;
		break;

	case GTK_JUSTIFY_CENTER:
		x += (text->max_width - line->width) * 0.5;
		break;

	default:
		if (text->draw_borders)
			x += BORDER_INDENT;

		/* For GTK_JUSTIFY_LEFT, we don't have to do anything.  We do not support
		 * GTK_JUSTIFY_FILL, yet.
		 */
		break;
	}
	
	return x;
}

#if 0
/* Calculates the y position of the first line of text. */
static double
get_line_ypos_item_relative (EText *text)
{
	double y;

	y = 0;

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		y -= text->height / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		y -= text->height;
		break;
	}

	return y;
}
#endif

/* Calculates the x position of the specified line of text, based on the text's justification */
static int
get_line_xpos (EText *text, struct line *line)
{
	int x;

	x = text->cx;

	switch (text->justification) {
	case GTK_JUSTIFY_RIGHT:
		x += text->max_width - line->width;
		break;

	case GTK_JUSTIFY_CENTER:
		x += (text->max_width - line->width) / 2;
		break;

	default:
		if (text->draw_borders)
			x += BORDER_INDENT;
		/* For GTK_JUSTIFY_LEFT, we don't have to do anything.  We do not support
		 * GTK_JUSTIFY_FILL, yet.
		 */
		break;
	}


	return x;
}

static void
_get_tep(EText *text)
{
	if (!text->tep) {
		text->tep = e_text_event_processor_emacs_like_new();
		g_object_ref (text->tep);
		gtk_object_sink (GTK_OBJECT (text->tep));
		text->tep_command_id = 
			g_signal_connect (text->tep,
					  "command",
					  G_CALLBACK(e_text_command),
					  (gpointer) text);
	}
}

/* Draw handler for the text item */
static void
e_text_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			int x, int y, int width, int height)
{
	EText *text;
	GdkRectangle rect, *clip_rect;
	struct line *lines;
	int i;
	int xpos, ypos;
	int start_char, end_char;
	int sel_start, sel_end;
	GdkRectangle sel_rect;
	GdkGC *fg_gc, *main_gc;
	GnomeCanvas *canvas;
	GtkWidget *widget;

	text = E_TEXT (item);
	canvas = GNOME_CANVAS_ITEM(text)->canvas;
	widget = GTK_WIDGET(canvas);

	fg_gc = widget->style->fg_gc[text->has_selection ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE];
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

			if (text->editing) {
				thisx += 1;
				thisy += 1;
				thiswidth -= 2;
				thisheight -= 2;
			}

			gtk_paint_shadow (widget->style, drawable,
					  GTK_STATE_NORMAL, GTK_SHADOW_IN,
					  NULL, widget, "entry",
					  thisx, thisy, thiswidth, thisheight);
		
			if (text->editing) {
				thisx -= 1;
				thisy -= 1;
				thiswidth += 2;
				thisheight += 2;
				/*
				 * Chris: I am here "filling in" for the additions
				 * and substractions done in the previous if (text->editing).
				 * but you might have other plans for this.  Please enlighten
				 * me as to whether it should be:
				 * thiswidth + 2 or thiswidth + 1.
				 */
				gtk_paint_focus (widget->style, drawable, GTK_STATE_NORMAL,
						 NULL, widget, "entry",
						 thisx, thisy, thiswidth - 1, thisheight - 1);
			}
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

	if (!text->text || !text->font)
		return;

	lines = text->lines;
	if ( !lines ) {
		text->needs_split_into_lines = 1;
		e_canvas_item_request_reflow (item);
		return;
	}

	clip_rect = NULL;
	if (text->clip) {
		rect.x = text->clip_cx - x;
		rect.y = text->clip_cy - y;
		rect.width = text->clip_cwidth;
		rect.height = text->clip_cheight;
		
		gdk_gc_set_clip_rectangle (main_gc, &rect);
		gdk_gc_set_clip_rectangle (fg_gc, &rect);
		clip_rect = &rect;
	}
	ypos = text->cy + e_font_ascent (text->font);
	if (text->draw_borders)
		ypos += BORDER_INDENT;

	if (text->editing)
		ypos -= text->yofs_edit;

	if (text->stipple)
		gnome_canvas_set_stipple_origin (item->canvas, main_gc);

	for (i = 0; i < text->num_lines; i++) {

		xpos = get_line_xpos (text, lines);
		if (text->editing) {
			xpos -= text->xofs_edit;
			start_char = lines->text - text->text;
			end_char = start_char + lines->length;
			sel_start = text->selection_start;
			sel_end = text->selection_end;
			if (sel_start > sel_end ) {
				sel_start ^= sel_end;
				sel_end ^= sel_start;
				sel_start ^= sel_end;
			}
			if ( sel_start < start_char )
				sel_start = start_char;
			if ( sel_end > end_char )
				sel_end = end_char;
			if ( sel_start < sel_end ) {
				sel_rect.x = xpos - x + text_width_with_objects (text->model,
										 text->font, text->style,
										 lines->text,
										 sel_start - start_char);
				sel_rect.y = ypos - y - e_font_ascent (text->font);
				sel_rect.width = text_width_with_objects (text->model,
									  text->font, text->style,
									  lines->text + sel_start - start_char,
									  sel_end - sel_start);
				sel_rect.height = e_font_height (text->font);
				gtk_paint_flat_box(GTK_WIDGET(item->canvas)->style,
						   drawable,
						   text->has_selection ?
						   GTK_STATE_SELECTED :
						   GTK_STATE_ACTIVE,
						   GTK_SHADOW_NONE,
						   clip_rect,
						   GTK_WIDGET(item->canvas),
						   "text",
						   sel_rect.x,
						   sel_rect.y,
						   sel_rect.width,
						   sel_rect.height);
				text_draw_with_objects (text->model,
							drawable,
							text->font, text->style,
							main_gc,
							xpos - x,
							ypos - y,
							lines->text,
							sel_start - start_char);
				text_draw_with_objects (text->model,
							drawable,
							text->font, text->style,
							fg_gc,
							xpos - x + text_width_with_objects (text->model,
											    text->font, text->style,
											    lines->text,
											    sel_start - start_char),
							ypos - y,
							lines->text + sel_start - start_char,
							sel_end - sel_start);
				text_draw_with_objects (text->model,
							drawable,
							text->font, text->style,
							main_gc,
							xpos - x + text_width_with_objects (text->model,
											    text->font, text->style,
											    lines->text,
											    sel_end - start_char),
							ypos - y,
							lines->text + sel_end - start_char,
							end_char - sel_end);
			} else {
				text_draw_with_objects (text->model,
							drawable,
							text->font, text->style,
							main_gc,
							xpos - x,
							ypos - y,
							lines->text,
							lines->length);
			}
			if (text->selection_start == text->selection_end &&
			    text->selection_start >= start_char &&
			    text->selection_start <= end_char &&
			    text->show_cursor) {
				gdk_draw_rectangle (drawable,
						    main_gc,
						    TRUE,
						    xpos - x + text_width_with_objects (text->model,
											text->font, text->style,
											lines->text,
											sel_start - start_char),
						    ypos - y - e_font_ascent (text->font),
						    1,
						    e_font_height (text->font));
			}
		} else {
			if (text->clip && text->use_ellipsis && lines->ellipsis_length < lines->length) {
				text_draw_with_objects (text->model,
							drawable,
							text->font, text->style,
							main_gc,
							xpos - x,
							ypos - y,
							lines->text,
							lines->ellipsis_length);
				e_font_draw_utf8_text (drawable,
						       text->font, text->style,
						       main_gc,
						       xpos - x + lines->width - text->ellipsis_width,
						       ypos - y,
						       text->ellipsis ? text->ellipsis : "...",
						       text->ellipsis ? strlen (text->ellipsis) : 3);
			} else {
				text_draw_with_objects (text->model,
							drawable,
							text->font, text->style,
							main_gc,
							xpos - x,
							ypos - y,
							lines->text,
							lines->length);
			}
		}

		if (text->strikeout)
			gdk_draw_rectangle (drawable,
					    main_gc,
					    TRUE,
					    xpos - x,
					    ypos - y - e_font_ascent (text->font) / 2,
					    lines->width, 1);
		ypos += e_font_height (text->font);
		lines++;
	}

	if (text->clip) {
		gdk_gc_set_clip_rectangle (main_gc, NULL);
		gdk_gc_set_clip_rectangle (fg_gc, NULL);
	}
}

/* Render handler for the text item */
static void
e_text_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
#if 0
	EText *text;
	guint32 fg_color;
	double xpos, ypos;
	struct line *lines;
	int i, j;
	double affine[6];
	int dx, dy;
	ArtPoint start_i, start_c;

	text = E_TEXT (item);

	if (!text->text || !text->font || !text->suckfont)
		return;

	suckfont = text->suckfont;

	fg_color = text->rgba;

        gnome_canvas_buf_ensure_buf (buf);

	lines = text->lines;
	if ( !lines )
		return;

	start_i.y = get_line_ypos_item_relative (text);

	art_affine_scale (affine, item->canvas->pixels_per_unit, item->canvas->pixels_per_unit);
	for (i = 0; i < 6; i++)
		affine[i] = text->affine[i];

	for (i = 0; i < text->num_lines; i++) {
		if (lines->length != 0) {
			start_i.x = get_line_xpos_item_relative (text, lines);
			art_affine_point (&start_c, &start_i, text->affine);
			xpos = start_c.x;
			ypos = start_c.y;

			for (j = 0; j < lines->length; j++) {
				ETextSuckChar *ch;

				ch = &suckfont->chars[(unsigned char)((lines->text)[j])];

				affine[4] = xpos;
				affine[5] = ypos;
				art_rgb_bitmap_affine (
					buf->buf,
					buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1,
					buf->buf_rowstride,
					suckfont->bitmap + (ch->bitmap_offset >> 3),
					ch->width,
					suckfont->bitmap_height,
					suckfont->bitmap_width >> 3,
					fg_color,
					affine,
					ART_FILTER_NEAREST, NULL);

				dx = ch->left_sb + ch->width + ch->right_sb;
				xpos += dx * affine[0];
				ypos += dx * affine[1];
			}
		}

		dy = text->font->ascent + text->font->descent;
		start_i.y += dy;
		lines++;
	}

	buf->is_bg = 0;
#endif
}

/* Point handler for the text item */
static double
e_text_point (GnomeCanvasItem *item, double x, double y,
			 int cx, int cy, GnomeCanvasItem **actual_item)
{
	EText *text;
	int i;
	struct line *lines;
	int x1, y1, x2, y2;
	int font_height;
	int dx, dy;
	double dist, best;

	text = E_TEXT (item);

	*actual_item = item;

	/* The idea is to build bounding rectangles for each of the lines of
	 * text (clipped by the clipping rectangle, if it is activated) and see
	 * whether the point is inside any of these.  If it is, we are done.
	 * Otherwise, calculate the distance to the nearest rectangle.
	 */

	if (text->font)
		font_height = e_font_height (text->font);
	else
		font_height = 0;

	best = 1.0e36;

	lines = text->lines;

	if ( !lines ) {
		text->needs_split_into_lines = 1;
		e_canvas_item_request_reflow (item);
		return 1;
	}

	if (text->fill_clip_rectangle) {
		double clip_width;
		double clip_height;

		if (text->clip_width < 0)
			clip_width = text->max_width;
		else
			clip_width = text->clip_width;

		/* Get canvas pixel coordinates for clip rectangle position */
		clip_width = clip_width * item->canvas->pixels_per_unit;
		if ( text->clip_height >= 0 )
			clip_height = text->clip_height * item->canvas->pixels_per_unit;
		else
			clip_height = text->height * item->canvas->pixels_per_unit;

		if (cx >= text->clip_cx &&
		    cx <= text->clip_cx + clip_width &&
		    cy >= text->clip_cy &&
		    cy <= text->clip_cy + clip_height)
			return 0;
		else
			return 1;
	}

	for (i = 0; i < text->num_lines; i++) {
		/* Compute the coordinates of rectangle for the current line,
		 * clipping if appropriate.
		 */

		x1 = get_line_xpos (text, lines);
		y1 = text->cy + i * font_height;
		x2 = x1 + lines->width;
		y2 = y1 + font_height;

		if (text->clip) {
			if (x1 < text->clip_cx)
				x1 = text->clip_cx;

			if (y1 < text->clip_cy)
				y1 = text->clip_cy;
			
			if ( text->clip_width >= 0 ) {
				if (x2 > (text->clip_cx + text->clip_width))
					x2 = text->clip_cx + text->clip_width;
			}

			if ( text->clip_height >= 0 ) {
				if (y2 > (text->clip_cy + text->clip_height))
					y2 = text->clip_cy + text->clip_height;
			}

			if ((x1 >= x2) || (y1 >= y2))
				continue;
		}

		/* Calculate distance from point to rectangle */

		if (cx < x1)
			dx = x1 - cx;
		else if (cx >= x2)
			dx = cx - x2 + 1;
		else
			dx = 0;

		if (cy < y1)
			dy = y1 - cy;
		else if (cy >= y2)
			dy = cy - y2 + 1;
		else
			dy = 0;

		if ((dx == 0) && (dy == 0))
			return 0.0;

		dist = sqrt (dx * dx + dy * dy);
		if (dist < best)
			best = dist;

		/* Next! */

		lines++;
	}

	return best / item->canvas->pixels_per_unit;
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

	if (text->clip) {
		width = text->clip_width;
		if ( text->clip_height >= 0 )
			height = text->clip_height;
		else height = text->height;
	} else {
		width = text->max_width / item->canvas->pixels_per_unit;
		height = text->height / item->canvas->pixels_per_unit;
	}

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

static gboolean
_get_xy_from_position (EText *text, gint position, gint *xp, gint *yp)
{
	if (text->lines && (xp || yp)) {
		struct line *lines = NULL;
		int x, y;
		double xd, yd;
		int j;
		x = get_line_xpos_item_relative (text, lines);
		y = text->yofs;
		y -= text->yofs_edit;
		for (j = 0, lines = text->lines; j < text->num_lines; lines++, j++) {
			if (lines->text > text->text + position)
				break;
			y += e_font_height (text->font);
		}
		lines --;
		y -= e_font_descent (text->font);
		
		x += text_width_with_objects (text->model,
					      text->font, text->style,
					      lines->text,
					      position - (lines->text - text->text));
		x -= text->xofs_edit;

		xd = x;  yd = y;
		gnome_canvas_item_i2w (GNOME_CANVAS_ITEM(text), &xd, &yd);
		gnome_canvas_w2c (GNOME_CANVAS_ITEM(text)->canvas, xd, yd, &x, &y);

		if (xp)
			*xp = x;
		if (yp)
			*yp = y;

		return TRUE;
	}

	return FALSE;
}

static gint
_get_position_from_xy (EText *text, gint x, gint y)
{
	int i, j;
	int ypos = text->yofs;
	int xpos;
	double xd, yd;
	const char *p;
	gunichar unival;
	gint font_ht, adjust=0;
	struct line *lines;

	xd = x;  yd = y;
	gnome_canvas_c2w (GNOME_CANVAS_ITEM(text)->canvas, xd, yd, &xd, &yd);
	gnome_canvas_item_w2i (GNOME_CANVAS_ITEM(text), &xd, &yd);
	x = xd;  y = yd;

	y += text->yofs_edit;
	font_ht = e_font_height (text->font);

	if (text->draw_borders)
		ypos += BORDER_INDENT;

	switch (text->anchor) {
	case GTK_ANCHOR_WEST:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_EAST:
		y += (text->num_lines * font_ht)/2;
		break;
	case GTK_ANCHOR_SOUTH:
	case GTK_ANCHOR_SOUTH_EAST:
	case GTK_ANCHOR_SOUTH_WEST:
		y += text->num_lines * font_ht;
	default:
		/* Do nothing */
		break;
	}
		

	j = 0;
	while (y > ypos) {
		ypos += font_ht;
		j ++;
	}
	j--;
	if (j >= text->num_lines)
		j = text->num_lines - 1;
	if (j < 0)
		j = 0;
	i = 0;
	lines = text->lines;
	
	if ( !lines )
		return 0;

	lines += j;
	x += text->xofs_edit;
	xpos = get_line_xpos_item_relative (text, lines);

	for (i = 0, p = lines->text; p && i < lines->length; i++, p = e_unicode_get_utf8 (p, &unival)) {
		int charwidth;
		int step1, step2;

#if 0
		if (unival == '\1') {
			const gchar *obj_str = NULL; /*e_text_model_get_nth_object (text->model, object_num);*/
			charwidth = e_font_utf8_text_width (text->font, text->style, obj_str, strlen (obj_str));
			++object_num;

			step1 = charwidth;
			step2 = 0;
			adjust = -1;

		} else {
#endif
			charwidth = e_font_utf8_char_width (text->font, text->style, (gchar *) p);
			
			step1 = charwidth / 2;
			step2 = (charwidth + 1) / 2;
			adjust = 0;
#if 0
		}
#endif

		xpos += step1;
		if (xpos > x) {
			break;
		}
		xpos += step2;
	}

	if (!p) return 0;
	
	return MAX (p - text->text + adjust, 0);
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
	if (scroll && text->button_down) {
		if (text->lastx - text->clip_cx > text->clip_cwidth &&
		    text->xofs_edit < text->max_width - text->clip_cwidth) {
			text->xofs_edit += 4;
			if (text->xofs_edit > text->max_width - text->clip_cwidth + 1)
				text->xofs_edit = text->max_width - text->clip_cwidth + 1;
			redraw = TRUE;
		}
		if (text->lastx - text->clip_cx < 0 &&
		    text->xofs_edit > 0) {
			text->xofs_edit -= 4;
			if (text->xofs_edit < 0)
				text->xofs_edit = 0;
			redraw = TRUE;
		}

		if (text->lasty - text->clip_cy > text->clip_cheight &&
		    text->yofs_edit < text->height - text->clip_cheight) {
			text->yofs_edit += 4;
			if (text->yofs_edit > text->height - text->clip_cheight + 1)
				text->yofs_edit = text->height - text->clip_cheight + 1;
			redraw = TRUE;
		}
		if (text->lasty - text->clip_cy < 0 &&
		    text->yofs_edit > 0) {
			text->yofs_edit -= 4;
			if (text->yofs_edit < 0)
				text->yofs_edit = 0;
			redraw = TRUE;
		}

		if (redraw) {
			ETextEventProcessorEvent e_tep_event;
			e_tep_event.type = GDK_MOTION_NOTIFY;
			e_tep_event.motion.state = text->last_state;
			e_tep_event.motion.time = 0;
			e_tep_event.motion.position = _get_position_from_xy(text, text->lastx, text->lasty);
			_get_tep(text);
			e_text_event_processor_handle_event (text->tep,
							     &e_tep_event);
			text->scroll_start = current_time;
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
	EText *text = E_TEXT (data);
	struct line *lines;
	GtkWidget *canvas;
	int i;
	gdouble max_width;
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

	lines = text->lines;

	if (E_CANVAS(GNOME_CANVAS_ITEM(text)->canvas)->tooltip_window || text->editing || (!lines)) {
		text->tooltip_timeout = 0;
		return FALSE;
	}

	cut_off = FALSE;
	for ( lines = text->lines, i = 0; i < text->num_lines; lines++, i++ ) {
		if (lines->length > lines->ellipsis_length) {
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
	max_width = 0.0;
	for (lines = text->lines, i = 0; i < text->num_lines; lines++, i++) {
		gdouble line_width;

		line_width = text_width_with_objects (text->model, text->font, text->style, lines->text, lines->length);
		max_width = MAX (max_width, line_width);
	}

	rect = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
				      gnome_canvas_rect_get_type (),
				      "x1", (double) 0,
				      "y1", (double) 0,
				      "x2", (double) max_width + 4,
				      "y2", (double) text->height + 4,
				      "fill_color", "light gray",
				      NULL);

	tooltip_text = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
					      e_text_get_type (),
					      "anchor", GTK_ANCHOR_NW,
					      "bold", text->bold,
					      "strikeout", text->strikeout,
					      "font_e", text->font,
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


	split_into_lines (E_TEXT(tooltip_text));
	calc_height (E_TEXT(tooltip_text));
	calc_line_widths (E_TEXT(tooltip_text));
	gnome_canvas_item_set (tooltip_text,
			       "clip_height", (double) E_TEXT(tooltip_text)->height,
			       "clip_width", (double) E_TEXT(tooltip_text)->max_width,
			       NULL);
	tooltip_width = E_TEXT(tooltip_text)->max_width;
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
	
	gtk_widget_set_usize (tooltip_window,
			      tooltip_width + 4 + (text->draw_borders ? BORDER_INDENT * 2 : 0),
			      tooltip_height + 4 + (text->draw_borders ? BORDER_INDENT * 2 : 0));
	gnome_canvas_set_scroll_region (GNOME_CANVAS(canvas), 0.0, 0.0,
					tooltip_width + (text->draw_borders ? BORDER_INDENT * 2 : 0), 
					(double)tooltip_height + (text->draw_borders ? BORDER_INDENT * 2 : 0));
	gtk_widget_show (canvas);
	gtk_widget_realize (tooltip_window);
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
				start_editing (text);
			} else {
				e_text_stop_editing (text);
				if (text->timeout_id) {
					g_source_remove(text->timeout_id);
					text->timeout_id = 0;
				}
			}
			if ( text->line_wrap )
				text->needs_split_into_lines = 1;
			else
				text->needs_calc_line_widths = 1;
			e_canvas_item_request_reflow (GNOME_CANVAS_ITEM(text));
		}
		return_val = 0;
		break;
	case GDK_KEY_PRESS: /* Fall Through */
	case GDK_KEY_RELEASE:
		if (text->editing) {
			GdkEventKey key = event->key;
			gint ret;

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
		else
			return 0;
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
			e_tep_event.button.position = _get_position_from_xy(text, button.x, button.y);
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
			g_signal_emit (text,
				       e_text_signals[E_TEXT_POPUP], 0,
				       &(event->button),
				       _get_position_from_xy (text, event->button.x, event->button.y));

			break;
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
			e_tep_event.button.position = _get_position_from_xy(text, button.x, button.y);
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
			e_tep_event.motion.position = _get_position_from_xy(text, motion.x, motion.y);
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
#if 0
			GdkEventCrossing *crossing = (GdkEventCrossing *) event;
			double x1, y1, x2, y2;
			split_into_lines (text);
			calc_height (text);
			calc_line_widths (text);
			get_bounds (text, &x1, &y1, &x2, &y2);
			if (crossing->x >= x1 &&
			    crossing->y >= y1 &&
			    crossing->x <= x2 &&
			    crossing->y <= y2) {
#endif
				if ( text->tooltip_count == 0 && text->clip) {
					if (!text->tooltip_timeout)
						text->tooltip_timeout = gtk_timeout_add (1000, _do_tooltip, text);
				}
				text->tooltip_count ++;
#if 0
			}
#endif
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

/* fixme: */

static int
next_word (EText *text, int start)
{
	char *p;
	int length;

	length = strlen (text->text);

	if (start >= length) {
		return length;
	} else {
		p = g_utf8_next_char (text->text + start);

		while (p && *p && g_unichar_validate (g_utf8_get_char (p))) {
			gunichar unival = g_utf8_get_char (p);
			if (g_unichar_isspace (unival)) {
				return p - text->text;
			} else 
				p = g_utf8_next_char (p);
		}
	}
			
	return p - text->text;
}

static int
_get_position(EText *text, ETextEventProcessorCommand *command)
{
	int length, obj_num;
	int x, y;
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

		new_pos = 0;
		
		if (text->selection_end >= 1) {
			
			p = g_utf8_find_prev_char (text->text, text->text + text->selection_end);
			if (p != text->text) {
				p = g_utf8_find_prev_char (text->text, p);

				while (p && p > text->text && !new_pos) {
					if (*p == '\n')
						new_pos = p - text->text + 1;
					p = g_utf8_find_prev_char (text->text, p);
				}
			}
		}

		break;

	case E_TEP_END_OF_LINE:
		new_pos = -1;
		length = strlen (text->text);
		
		if (text->selection_end >= length) {
			new_pos = length;
		} else {

			p = g_utf8_next_char (text->text + text->selection_end);

			while (p && *p && g_unichar_validate (g_utf8_get_char (p))) {
				if (*p == '\n') {
					new_pos = p - text->text;
					p = NULL;
				} else 
					p = g_utf8_next_char (p);
			}
		}

		if (new_pos == -1)
			new_pos = p - text->text;

		break;

	case E_TEP_FORWARD_CHARACTER:
		length = strlen (text->text);

		if (text->selection_end >= length) {
			new_pos = length;
		} else {
			p = g_utf8_next_char (text->text + text->selection_end);
			new_pos = p - text->text;
		}

		break;

	case E_TEP_BACKWARD_CHARACTER:
		new_pos = 0;
		if (text->selection_end >= 1) {
			p = g_utf8_find_prev_char (text->text, text->text + text->selection_end);

			if (p != NULL)
				new_pos = p - text->text;
		}

		break;

	case E_TEP_FORWARD_WORD:
		new_pos = next_word (text, text->selection_end);
		break;

	case E_TEP_BACKWARD_WORD:
		new_pos = 0;
		if (text->selection_end >= 1) {
			p = g_utf8_find_prev_char (text->text, text->text + text->selection_end);
			if (p != text->text) {
				p = g_utf8_find_prev_char (text->text, p);

				while (p && p > text->text && g_unichar_validate (g_utf8_get_char (p))) {
					unival = g_utf8_get_char (p);
					if (g_unichar_isspace (unival)) {
						new_pos = g_utf8_next_char (p) - text->text; 
						p = NULL;
					} else
						p = g_utf8_find_prev_char (text->text, p);
				}
			}
		}
					
		break;

	case E_TEP_FORWARD_LINE:
		if (_get_xy_from_position(text, text->selection_end, &x, &y)) {
			y += e_font_height (text->font);
			new_pos = _get_position_from_xy(text, x, y);
		}
		break;

	case E_TEP_BACKWARD_LINE:
		if (_get_xy_from_position(text, text->selection_end, &x, &y)) {
			y -= e_font_height (text->font);
			new_pos = _get_position_from_xy(text, x, y);
		}
		break;

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

		p = g_utf8_find_prev_char (text->text, text->text + text->selection_end);
		if (p == text->text) {
			new_pos = 0;
			break;
		}
		p = g_utf8_find_prev_char (text->text, p);

		while (p && p > text->text && g_unichar_validate (g_utf8_get_char (p))) {
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
			text->selection_start = p - text->text;


		text->selection_start = e_text_model_validate_position (text->model, text->selection_start);
		
		length = strlen (text->text);
		if (text->selection_end >= length) {
			new_pos = length;
			break;
		}

		p = g_utf8_next_char (text->text + text->selection_end);

		while (p && *p && g_unichar_validate (g_utf8_get_char (p))) {
			unival = g_utf8_get_char (p);
			if (g_unichar_isspace (unival)) {
				new_pos =  p - text->text;
				p = NULL;
			} else
				p = g_utf8_next_char (p);
		}

		if (p)
			new_pos = p - text->text;

		return new_pos;

	case E_TEP_SELECT_ALL:
		text->selection_start = 0;
		new_pos = strlen (text->text);
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
_delete_selection(EText *text)
{
	if ( text->selection_start < text->selection_end ) {
		e_text_model_delete(text->model, text->selection_start, text->selection_end - text->selection_start);
#if 0
		text->selection_end = text->selection_start;
#endif
	} else {
		e_text_model_delete(text->model, text->selection_end, text->selection_start - text->selection_end);
#if 0
		text->selection_start = text->selection_end;
#endif
	}
}

static void
_insert(EText *text, char *string, int value)
{
	if (value > 0) {
		if (!text->allow_newlines) {
			char *i;
			for (i = string; *i; i++) {
				if (*i == '\n') {
					char *new_string = g_malloc (strlen (string) + 1);
					char *j = new_string;
					for (i = string; *i; i++) {
						if (*i != '\n')
							*(j++) = *i;
					}
					*j = 0;
					e_text_model_insert_length(text->model, text->selection_start, new_string, j - new_string);
					g_free (new_string);
					return;
				}
			}
		}
		e_text_model_insert_length(text->model, text->selection_start, string, value);
		
#if 0
		text->selection_start += value;
		text->selection_end = text->selection_start;
#endif
	}
}

static void
capitalize (EText *text, int start, int end, ETextEventProcessorCaps type)
{
	gboolean first = TRUE;
	const char *p = text->text + start;
	const char *text_end = text->text + end;
	char *new_text = g_new0 (char, g_utf8_strlen (text->text + start, start - end) * 6);
	char *output = new_text;

	while (p && *p && p < text_end && g_unichar_validate (g_utf8_get_char (p))) {
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

	e_text_model_delete (text->model, start, end - start);
	e_text_model_insert (text->model, start, new_text);
	g_free (new_text);
}

static void
e_text_command(ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data)
{
	EText *text = E_TEXT(data);
	int sel_start, sel_end;
	gboolean scroll = TRUE;

	switch (command->action) {
	case E_TEP_MOVE:
		text->selection_start = _get_position(text, command);
		text->selection_end = text->selection_start;
		if (text->timer) {
			g_timer_reset(text->timer);
		}

		break;
	case E_TEP_SELECT:
		text->selection_start = e_text_model_validate_position (text->model, text->selection_start); /* paranoia */
		text->selection_end = _get_position(text, command);
		
		sel_start = MIN(text->selection_start, text->selection_end);
		sel_end   = MAX(text->selection_start, text->selection_end);

		sel_start = e_text_model_validate_position (text->model, sel_start);
		
		if (sel_start != sel_end) {
			e_text_supply_selection (text, command->time, GDK_SELECTION_PRIMARY,
						 (guchar *) text->text + sel_start, sel_end - sel_start);
		} else if (text->timer) {
			g_timer_reset(text->timer);
		}

		break;
	case E_TEP_DELETE:
		if (text->selection_end == text->selection_start) {
			text->selection_end = _get_position(text, command);
		}
		_delete_selection(text);
		if (text->timer) {
			g_timer_reset(text->timer);
		}
		break;

	case E_TEP_INSERT:
		if (text->selection_end != text->selection_start) {
			_delete_selection(text);
		}
		_insert(text, command->string, command->value);
		if (text->timer) {
			g_timer_reset(text->timer);
		}
		break;
	case E_TEP_COPY:
		sel_start = MIN(text->selection_start, text->selection_end);
		sel_end = MAX(text->selection_start, text->selection_end);
		if (sel_start != sel_end) {
			e_text_supply_selection (text, command->time, clipboard_atom,
						 (guchar *) text->text + sel_start, sel_end - sel_start);
		}
		if (text->timer) {
			g_timer_reset(text->timer);
		}
		scroll = FALSE;
		break;
	case E_TEP_PASTE:
		e_text_get_selection (text, clipboard_atom, command->time);
		if (text->timer) {
			g_timer_reset(text->timer);
		}
		break;
	case E_TEP_GET_SELECTION:
		e_text_get_selection (text, GDK_SELECTION_PRIMARY, command->time);
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
			int selection_end = text->selection_start + text->selection_end - selection_start; /* Slightly faster than MAX */
			capitalize (text, selection_start, selection_end, command->value);
		}
		break;
	case E_TEP_NOP:
		scroll = FALSE;
		break;
	}

	if (scroll && !text->button_down) {
		int x;
		int i;
		struct line *lines = text->lines;
		gdouble clip_width;
		if ( !lines )
			return;

		for (lines = text->lines, i = 0; i < text->num_lines ; i++, lines ++) {
			if ((lines->text - text->text) > text->selection_end) {
				break;
			}
		}
		lines --;
		i --;
		x = text_width_with_objects (text->model,
					     text->font, text->style,
					     lines->text, 
					     text->selection_end - (lines->text - text->text));
		

		if (x < text->xofs_edit) {
			text->xofs_edit = x;
		}

		clip_width = text->clip_width;
		if (clip_width >= 0 && text->draw_borders) {
			clip_width -= 6;
			if (clip_width < 0)
				clip_width = 0;
		}

		if (2 + x - clip_width > text->xofs_edit) {
			text->xofs_edit = 2 + x - clip_width;
		}
		
		if (e_font_height (text->font) * i < text->yofs_edit)
			text->yofs_edit = e_font_height (text->font) * i;
		
		if (e_font_height (text->font) * (i + 1) - (text->clip_height != -1 ? text->clip_height : text->height) > text->yofs_edit)
			text->yofs_edit = e_font_height (text->font) * (i + 1) - (text->clip_height != -1 ? text->clip_height : text->height);
	}

	text->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(text));
}

static void 
_invisible_destroy (gpointer data, GObject *where_object_was)
{
	EText *text = data;
	text->invisible = NULL;
}

static GtkWidget *
e_text_get_invisible(EText *text)
{	
	GtkWidget *invisible;
	if (text->invisible) {
		invisible = text->invisible;
	} else {
		static const GtkTargetEntry targets[] = {
			{ "UTF8_STRING", 0, TARGET_UTF8_STRING },
			{ "UTF-8", 0, TARGET_UTF8 },
			{ "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
			{ "STRING", 0, TARGET_STRING },
			{ "TEXT",   0, TARGET_TEXT }
		};
		static const gint n_targets = sizeof(targets) / sizeof(targets[0]);

		invisible = gtk_invisible_new();
		text->invisible = invisible;
		
		gtk_selection_add_targets (invisible,
					   GDK_SELECTION_PRIMARY,
					   targets, n_targets);
		gtk_selection_add_targets (invisible,
					   clipboard_atom,
					   targets, n_targets);
		
		g_signal_connect (invisible, "selection_get",
				  G_CALLBACK (_selection_get), 
				  text);
		g_signal_connect (invisible, "selection_clear_event",
				  G_CALLBACK (_selection_clear_event),
				  text);
		g_signal_connect (invisible, "selection_received",
				  G_CALLBACK (_selection_received),
				  text);
		
		g_object_weak_ref (G_OBJECT (invisible),
				   _invisible_destroy, text);
	}
	return invisible;
}

static void
_selection_clear_event (GtkInvisible *invisible,
			GdkEventSelection *event,
			EText *text)
{
	if (event->selection == GDK_SELECTION_PRIMARY) {
		g_free (text->primary_selection);
		text->primary_selection = NULL;
		text->primary_length = 0;

		text->has_selection = FALSE;
		text->needs_redraw = 1;
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(text));

	} else if (event->selection == clipboard_atom) {
		g_free (text->clipboard_selection);
		text->clipboard_selection = NULL;
		text->clipboard_length = 0;
	}
}

static void
_selection_get (GtkInvisible *invisible,
		GtkSelectionData *selection_data,
		guint info,
		guint time_stamp,
		EText *text)
{
	char *selection_string;
	int selection_length;
	if (selection_data->selection == GDK_SELECTION_PRIMARY) {
		selection_string = text->primary_selection;
		selection_length = text->primary_length;
	} else /* CLIPBOARD */ {
		selection_string = text->clipboard_selection;
		selection_length = text->clipboard_length;
	}

	if (selection_string != NULL) {
		if (info == TARGET_UTF8_STRING) {
			gtk_selection_data_set (selection_data,
						gdk_atom_intern ("UTF8_STRING", FALSE), 8,
						(const guchar *) selection_string,
						selection_length);
		} else if (info == TARGET_UTF8) {
			gtk_selection_data_set (selection_data,
						gdk_atom_intern ("UTF-8", FALSE), 8,
						(const guchar *) selection_string,
						selection_length);
		} else if (info == TARGET_STRING || info == TARGET_TEXT || info == TARGET_COMPOUND_TEXT) {
			gchar *localized_string;

			localized_string = e_utf8_to_gtk_string (GTK_WIDGET (GNOME_CANVAS_ITEM(text)->canvas),
								 selection_string);

			if (info == TARGET_STRING) {
				gtk_selection_data_set (selection_data,
							GDK_SELECTION_TYPE_STRING, 8,
							(const guchar *) localized_string, 
							strlen (localized_string));
			} else {
				guchar *text;
				GdkAtom encoding;
				gint format;
				gint new_length;

				gdk_string_to_compound_text (localized_string, 
							     &encoding, &format,
							     &text, &new_length);

				gtk_selection_data_set (selection_data,
							encoding, format,
							text, new_length);
				gdk_free_compound_text (text);
			}
			g_free (localized_string);
		}
	}
}

typedef struct {
	guint32 time;
	GdkAtom selection;
} SelectionAndTime;

static const char *formats[] = {"UTF8_STRING", "UTF-8", "STRING"};
#define E_STRING_ATOM 2
static const int format_count = sizeof (formats) / sizeof (formats[0]);
static GdkAtom atoms[sizeof (formats) / sizeof (formats[0])];
static int initialized = FALSE;

static inline void
init_atoms (void)
{
	int type;
	if (!initialized) {
		for (type = 0; type < format_count; type++) 
			atoms[type] = gdk_atom_intern (formats[type], FALSE);
		initialized = TRUE;
	}
}

static void
e_text_request_paste (EText *text)
{
	GdkAtom format_atom;
	GtkWidget *invisible;
	int type = text->last_type_request;

	init_atoms ();

	format_atom = GDK_NONE;

	while (format_atom == GDK_NONE) {
		type ++;

		if (type >= format_count) {
			if (text->queued_requests) {
				guint32 *new_time = text->queued_requests->data;
				text->queued_requests = g_list_remove_link (text->queued_requests, text->queued_requests); 
				text->last_time_request = *new_time;
				g_free (new_time);

				type = -1;
				continue;
			} else {
				text->last_type_request = -1;
				d(g_print ("Setting last_type_request to %d at line %d\n", text->last_type_request, __LINE__));
				text->last_time_request = 0;
				return;
			}
		}

		format_atom = atoms [type];
	}

	/* This must come before the gtk_selection_convert because sometimes _selection_received is called reentrantly. */

	text->last_type_request = type;
	d(g_print ("Setting last_type_request to %d at line %d\n", text->last_type_request, __LINE__));

	/* And request the format target for the required selection */
	invisible = e_text_get_invisible(text);
	gtk_selection_convert(invisible,
			      text->last_selection_request,
			      format_atom,
			      text->last_time_request);

	return;
}

static void
_selection_received (GtkInvisible *invisible,
		     GtkSelectionData *selection_data,
		     guint time,
		     EText *text)
{
	init_atoms ();
	if (selection_data->length < 0) {
		d(g_print ("Calling e_text_request_paste at line %d\n", __LINE__));
		e_text_request_paste (text);
		return;
	} else if (selection_data->type == atoms[E_STRING_ATOM]) {
		ETextEventProcessorCommand command;
		char *string;

		string = e_utf8_from_gtk_string_sized (GTK_WIDGET (GNOME_CANVAS_ITEM(text)->canvas),
						       selection_data->data,
						       selection_data->length);
		command.action = E_TEP_INSERT;
		command.position = E_TEP_SELECTION;
		command.string = string;
		command.value = strlen (string);
		command.time = time;
		e_text_command(text->tep, &command, text);
		g_free (string);
	} else {
		ETextEventProcessorCommand command;
		command.action = E_TEP_INSERT;
		command.position = E_TEP_SELECTION;
		command.string = selection_data->data;
		command.value = selection_data->length;
		command.time = time;
		e_text_command(text->tep, &command, text);
	}

	text->last_type_request = -1;
	d(g_print ("Setting last_type_request to %d at line %d\n", text->last_type_request, __LINE__));
	if (text->queued_requests) {
		SelectionAndTime *new_request = text->queued_requests->data;
		text->queued_requests = g_list_remove_link (text->queued_requests, text->queued_requests);
		text->last_time_request = new_request->time;
		text->last_selection_request = new_request->selection;
		g_free (new_request);
		d(g_print ("Calling e_text_request_paste at line %d\n", __LINE__));
		e_text_request_paste (text);
	}
}

static void
e_text_get_selection(EText *text, GdkAtom selection, guint32 time)
{
	if (text->last_type_request == -1) {
		text->last_time_request = time;
		text->last_selection_request = selection;
		d(g_print ("Calling e_text_request_paste at line %d\n", __LINE__));
		e_text_request_paste (text);
	} else {
		SelectionAndTime *new_request = g_new (SelectionAndTime, 1);
		new_request->time = time;
		new_request->selection = selection;
		/* FIXME: Queue the selection request type as well. */
		text->queued_requests = g_list_append (text->queued_requests, new_request);
	}
}

static void 
e_text_supply_selection (EText *text, guint time, GdkAtom selection, guchar *data, gint length)
{
	gboolean successful;
	GtkWidget *invisible;

	invisible = e_text_get_invisible(text);

	if (selection == GDK_SELECTION_PRIMARY ) {
		g_free (text->primary_selection);
		text->primary_selection = g_strndup(data, length);
		text->primary_length = length;
	} else if (selection == clipboard_atom) {
		g_free (text->clipboard_selection);
		text->clipboard_selection = g_strndup(data, length);
		text->clipboard_length = length;
	}

	successful = gtk_selection_owner_set (invisible,
					      selection,
					      time);
	
	if (selection == GDK_SELECTION_PRIMARY)
		text->has_selection = successful;
}

#if 0
static void
e_text_real_copy_clipboard (EText *text)
{
  guint32 time;
  gint selection_start_pos; 
  gint selection_end_pos;

  g_return_if_fail (text != NULL);
  g_return_if_fail (E_IS_TEXT (text));
  
  time = gtk_text_get_event_time (text);
  selection_start_pos = MIN (text->selection_start, text->selection_end);
  selection_end_pos = MAX (text->selection_start, text->selection_end);
 
  if (selection_start_pos != selection_end_pos)
    {
      if (gtk_selection_owner_set (GTK_WIDGET (text->canvas),
				   clipboard_atom,
				   time))
	      text->clipboard_text = "";
    }
}

static void
e_text_real_paste_clipboard (EText *text)
{
  guint32 time;

  g_return_if_fail (text != NULL);
  g_return_if_fail (E_IS_TEXT (text));
  
  time = e_text_get_event_time (text);
  if (text->editable)
	  gtk_selection_convert (GTK_WIDGET(text->widget),
				 clipboard_atom,
				 gdk_atom_intern ("COMPOUND_TEXT", FALSE), time);
}
#endif


#if 0
/* Routines for sucking fonts from the X server */

static ETextSuckFont *
e_suck_font (GdkFont *font)
{
	ETextSuckFont *suckfont;
	int i;
	int x, y;
	char text[1];
	int lbearing, rbearing, ch_width, ascent, descent;
	GdkPixmap *pixmap;
	GdkColor black, white;
	GdkImage *image;
	GdkGC *gc;
	guchar *line;
	int width, height;
	int black_pixel, pixel;

	if (!font)
		return NULL;

	suckfont = g_new (ETextSuckFont, 1);

	height = font->ascent + font->descent;
	x = 0;
	for (i = 0; i < 256; i++) {
		text[0] = i;
		gdk_text_extents (font, text, 1,
				  &lbearing, &rbearing, &ch_width, &ascent, &descent);
		suckfont->chars[i].left_sb = lbearing;
		suckfont->chars[i].right_sb = ch_width - rbearing;
		suckfont->chars[i].width = rbearing - lbearing;
		suckfont->chars[i].ascent = ascent;
		suckfont->chars[i].descent = descent;
		suckfont->chars[i].bitmap_offset = x;
		x += (ch_width + 31) & -32;
	}

	width = x;

	suckfont->bitmap_width = width;
	suckfont->bitmap_height = height;
	suckfont->ascent = font->ascent;

	pixmap = gdk_pixmap_new (NULL, suckfont->bitmap_width,
				 suckfont->bitmap_height, 1);
	gc = gdk_gc_new (pixmap);
	gdk_gc_set_font (gc, font);

	black_pixel = BlackPixel (gdk_display, DefaultScreen (gdk_display));
	black.pixel = black_pixel;
	white.pixel = WhitePixel (gdk_display, DefaultScreen (gdk_display));
	gdk_gc_set_foreground (gc, &white);
	gdk_draw_rectangle (pixmap, gc, 1, 0, 0, width, height);

	gdk_gc_set_foreground (gc, &black);
	for (i = 0; i < 256; i++) {
		text[0] = i;
		gdk_draw_text (pixmap, font, gc,
			       suckfont->chars[i].bitmap_offset - suckfont->chars[i].left_sb,
			       font->ascent,
			       text, 1);
	}

	/* The handling of the image leaves me with distinct unease.  But this
	 * is more or less copied out of gimp/app/text_tool.c, so it _ought_ to
	 * work. -RLL
	 */

	image = gdk_image_get (pixmap, 0, 0, width, height);
	suckfont->bitmap = g_malloc0 ((width >> 3) * height);

	line = suckfont->bitmap;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			pixel = gdk_image_get_pixel (image, x, y);
			if (pixel == black_pixel)
				line[x >> 3] |= 128 >> (x & 7);
		}
		line += width >> 3;
	}

	gdk_image_destroy (image);

	/* free the pixmap */
	gdk_pixmap_unref (pixmap);

	/* free the gc */
	gdk_gc_destroy (gc);

	return suckfont;
}

static void
e_suck_font_free (ETextSuckFont *suckfont)
{
	g_free (suckfont->bitmap);
	g_free (suckfont);
}
#endif

/* Class initialization function for the text item */
static void
e_text_class_init (ETextClass *klass)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GObjectClass *) klass;
	item_class = (GnomeCanvasItemClass *) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	klass->changed = NULL;
	klass->activate = NULL;

	object_class->dispose = e_text_dispose;
	object_class->set_property = e_text_set_property;
	object_class->get_property = e_text_get_property;

	item_class->update = e_text_update;
	item_class->realize = e_text_realize;
	item_class->unrealize = e_text_unrealize;
	item_class->draw = e_text_draw;
	item_class->point = e_text_point;
	item_class->bounds = e_text_bounds;
	item_class->render = e_text_render;
	item_class->event = e_text_event;

	e_text_signals[E_TEXT_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextClass, changed),
			      NULL, NULL,
			      e_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_text_signals[E_TEXT_ACTIVATE] =
		g_signal_new ("activate",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextClass, activate),
			      NULL, NULL,
			      e_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_text_signals[E_TEXT_KEYPRESS] =
		g_signal_new ("keypress",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextClass, keypress),
			      NULL, NULL,
			      e_marshal_NONE__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

	e_text_signals[E_TEXT_POPUP] =
		g_signal_new ("popup",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextClass, popup),
			      NULL, NULL,
			      e_marshal_NONE__POINTER_INT,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);

	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model",
							      _( "Model" ),
							      _( "Model" ),
							      E_TYPE_TEXT_MODEL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EVENT_PROCESSOR,
					 g_param_spec_object ("event_processor",
							      _( "Event Processor" ),
							      _( "Event Processor" ),
							      E_TEXT_EVENT_PROCESSOR_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TEXT,
					 g_param_spec_string ("text",
							      _( "Text" ),
							      _( "Text" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FONT,
					 g_param_spec_string ("font",
							      _( "Font" ),
							      _( "Font" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FONTSET,
					 g_param_spec_string ("fontset",
							      _( "Fontset" ),
							      _( "Fontset" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FONT_GDK,
					 g_param_spec_boxed ("font_gdk",
							     _( "GDKFont" ),
							     _( "GDKFont" ),
							     GDK_TYPE_FONT,
							     G_PARAM_WRITABLE));
					 
	g_object_class_install_property (object_class, PROP_FONT_E,
					 g_param_spec_pointer ("font_e",
							       _( "EFont" ),
							       _( "EFont" ),
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_BOLD,
					 g_param_spec_boolean ("bold",
							       _( "Bold" ),
							       _( "Bold" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_STRIKEOUT,
					 g_param_spec_boolean ("strikeout",
							       _( "Strikeout" ),
							       _( "Strikeout" ),
							       FALSE,
							       G_PARAM_READWRITE));
		
	g_object_class_install_property (object_class, PROP_ANCHOR,
					 g_param_spec_enum ("anchor",
							    _( "Anchor" ),
							    _( "Anchor" ),
							    GTK_TYPE_ANCHOR_TYPE, GTK_ANCHOR_CENTER,
							    G_PARAM_READWRITE));


	g_object_class_install_property (object_class, PROP_JUSTIFICATION,
					 g_param_spec_enum ("justification",
							    _( "Justification" ),
							    _( "Justification" ),
							    GTK_TYPE_JUSTIFICATION, GTK_JUSTIFY_LEFT,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CLIP_WIDTH,
					 g_param_spec_double ("clip_width",
							      _( "Clip Width" ),
							      _( "Clip Width" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CLIP_HEIGHT,
					 g_param_spec_double ("clip_height",
							      _( "Clip Height" ),
							      _( "Clip Height" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CLIP,
					 g_param_spec_boolean ("clip",
							       _( "Clip" ),
							       _( "Clip" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FILL_CLIP_RECTANGLE,
					 g_param_spec_boolean ("fill_clip_rectangle",
							       _( "Fill clip rectangle" ),
							       _( "Fill clip rectangle" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_X_OFFSET,
					 g_param_spec_double ("x_offset",
							      _( "X Offset" ),
							      _( "X Offset" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_Y_OFFSET,
					 g_param_spec_double ("y_offset",
							      _( "Y Offset" ),
							      _( "Y Offset" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FILL_COLOR,
					 g_param_spec_string ("fill_color",
							      _( "Fill color" ),
							      _( "Fill color" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FILL_COLOR_GDK,
					 g_param_spec_pointer ("fill_color_gdk",
							       _( "GDK fill color" ),
							       _( "GDK fill color" ),
							       G_PARAM_READWRITE));


	g_object_class_install_property (object_class, PROP_FILL_COLOR_RGBA,
					 g_param_spec_uint ("fill_color_rgba",
							    _( "GDK fill color" ),
							    _( "GDK fill color" ),
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FILL_STIPPLE,
					 g_param_spec_object ("fill_stipple",
							      _( "Fill stipple" ),
							      _( "FIll stipple" ),
							      GDK_TYPE_WINDOW,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TEXT_WIDTH,
					 g_param_spec_double ("text_width",
							      _( "Text width" ),
							      _( "Text width" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_TEXT_HEIGHT,
					 g_param_spec_double ("text_height",
							      _( "Text height" ),
							      _( "Text height" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READABLE));


	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _( "Editable" ),
							       _( "Editable" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_USE_ELLIPSIS,
					 g_param_spec_boolean ("use_ellipsis",
							       _( "Use ellipsis" ),
							       _( "Use ellipsis" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ELLIPSIS,
					 g_param_spec_string ("ellipsis",
							      _( "Ellipsis" ),
							      _( "Ellipsis" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_LINE_WRAP,
					 g_param_spec_boolean ("line_wrap",
							       _( "Line wrap" ),
							       _( "Line wrap" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_BREAK_CHARACTERS,
					 g_param_spec_string ("break_characters",
							      _( "Break characters" ),
							      _( "Break characters" ),
							      NULL,
							      G_PARAM_READWRITE));


	g_object_class_install_property (object_class, PROP_MAX_LINES,
					 g_param_spec_int ("max_lines",
							   _( "Max lines" ),
							   _( "Max lines" ),
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_WIDTH,
					 g_param_spec_double ("width",
							      _( "Width" ),
							      _( "Width" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));


	g_object_class_install_property (object_class, PROP_HEIGHT,
					 g_param_spec_double ("height",
							      _( "Height" ),
							      _( "Height" ),
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_DRAW_BORDERS,
					 g_param_spec_boolean ("draw_borders",
							       _( "Draw borders" ),
							       _( "Draw borders" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ALLOW_NEWLINES,
					 g_param_spec_boolean ("allow_newlines",
							       _( "Allow newlines" ),
							       _( "Allow newlines" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_DRAW_BACKGROUND,
					 g_param_spec_boolean ("draw_background",
							       _( "Draw background" ),
							       _( "Draw background" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_DRAW_BUTTON,
					 g_param_spec_boolean ("draw_button",
							       _( "Draw button" ),
							       _( "Draw button" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CURSOR_POS,
					 g_param_spec_int ("cursor_pos",
							   _( "Cursor position" ),
							   _( "Cursor position" ),
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
}

/* Object initialization function for the text item */
static void
e_text_init (EText *text)
{
	text->model                   = e_text_model_new ();
	text->text                    = e_text_model_get_text (text->model);

	text->revert                  = NULL;

	g_object_ref (text->model);
	gtk_object_sink (GTK_OBJECT (text->model));

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

	text->invisible               = NULL;
	text->primary_selection       = NULL;
	text->primary_length          = 0;
	text->clipboard_selection     = NULL;
	text->clipboard_length        = 0;

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

	text->style                   = E_FONT_PLAIN;
	text->allow_newlines          = TRUE;

	text->last_type_request       = -1;
	d(g_print ("Setting last_type_request to %d at line %d\n", text->last_type_request, __LINE__));
	text->last_time_request       = 0;
	text->queued_requests         = NULL;

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
