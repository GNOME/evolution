/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-about-box.c
 *
 * Copyright (C) 2001, 2002, 2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-about-box.h"

#include <gal/util/e-util.h>

#include <gtk/gtkeventbox.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


#define PARENT_TYPE gtk_event_box_get_type ()
static GtkEventBoxClass *parent_class = NULL;

/* must be in utf8, the weird breaking of escaped strings
   is so the hex escape strings dont swallow too many chars */
static const char *text[] = {
	"",
	"Evolution " VERSION,
	"Copyright 1999 - 2003 Ximian, Inc.",
	"",
	N_("Brought to you by"),
	"",
	"Darin Adler",
	"Arturo Espinosa Aldama",
	"H\xC3\xA9" "ctor Garc\xC3\xAD" "a Alvarez",
	"Jesus Bravo Alvarez",
	"Seth Alves",
	"Marius Andreiana",
	"Sean Atkinson",
	"Szabolcs BAN",
	"Timur Bakeyev",
	"Martin Baulig",
	"Frank Belew",
	"Dan Berger",
	"Jacob Berkman",
	"Matt Bissiri",
	"Jonathan Blandford",
	"Richard Boulton",
	"Robert Brady",
	"Kevin Breit",
	"Martha Burke",
	"Dave Camp",
	"Ian Campbell",
	"Anders Carlsson",
	"Damon Chaplin",
	"Abel Cheung",
	"Zbigniew Chyla",
	"Clifford R. Conover",
	"Sam Creasey",
	"Frederic Crozat",
	"Wayne Davis",
	"Rodney Dawes",
	"Jos Dehaes",
	"Fatih Demir",
	"Arik Devens",
	"Anna Marie Dirks",
	"Bob Doan",
	"Radek Doul\xC3\xADk",
	"Edd Dumbill",
	"Larry Ewing",
	"Gilbert Fang",
	"Nuno Ferreira",
	"Valek Filippov",
	"Nat Friedman",
	"Sean Gao",
	"Jeff Garzik",
	"Nike Gerdts",
	"Grzegorz Goawski",
	"Jody Goldberg",
	"Mark Gordon",
	"Kenny Graunke",
	"Alex Graveley",
	"Bertrand Guiheneuf",
	"Jean-Noel Guiheneuf",
	"Mikael Hallendal",
	"Raja R Harinath",
	"Heath Harrelson",
	"Taylor Hayward",
	"Jon K Hellan",
	"Martin Hicks",
	"Iain Holmes",
	"Max Horn",
	"Greg Hudson",
	"Richard Hult",
	"Andreas Hyden",
	"Miguel de Icaza",
	"Hans Petter Jansson",
	"Jack Jia",
	"Wang Jian",
	"Sanshao Jiang",
	"Benjamin Kahn",
	"Yanko Kaneti",
	"Lauris Kaplinski",
	"Jeremy Katz",
	"Mike Kestner",
	"Christian Kreibich",
	"Nicholas J Kreucher",
	"Ronald Kuetemeier",
	"Tuomas Kuosmanen",
	"Mathieu Lacage",
	"Christopher J. Lahey",
	"Miles Lane",
	"Jason Leach",
	"Elliot Lee",
	"Ji Lee",
	"Timothy Lee",
	"T\xC3\xB5" "ivo Leedj\xC3\xA4" "rv",
	"Richard Li",
	"Matthew Loper",
	"Duarte Loreto",
	"Harry Lu",
	"Michael MacDonald",
	"Duncan Mak",
	"Kjartan Maraas",
	"Garardo Marin",
	"Matt Martin",
	"Carlos Perell\xC3\xB3" " Mar\xC3\xAD" "n",
	"Dietmar Maurer",
	"Mike McEwan",
	"Alastair McKinstry",
	"Michael Meeks",
	"Federico Mena",
	"Christophe Merlet",
	"Michael M. Morrison",
	"Rodrigo Moya",
	"Steve Murphy",
	"Yukihiro Nakai",
	"Martin Norb\xC3\xA4" "ck",
	"Tomas Ogren",
	"Eskil Heyn Olsen",
	"Sergey Panov",
	"Gediminas Paulauskas",
	"Jesse Pavel",
	"Havoc Pennington",
	"Ettore Perazzoli",
	"Petta Pietikainen",
	"Herbert V. Riedel",
	"Ariel Rios",
	"JP Rosevear",
	"Cody Russell",
	"Changwoo Ryu",
	"Pablo Saratxaga",
	"Carsten Schaar",
	"Joe Shaw",
	"Timo Sirainen",
	"Craig Small",
	"Maciej Stachowiak",
	"Jeffrey Stedfast",
	"Jakub Steiner",
	"Russell Steinthal",
	"Vadim Strizhevsky",
	"Yuri Syrota",
	"Jason Tackaberry",
	"Peter Teichman",
	"Chris Toshok",
	"Tom Tromey",
	"Jon Trowbridge",
	"Andrew T. Veliath",
	"Gustavo Maciel Dias Vieira",
	"Luis Villa",
	"Stanislav Visnovsky",
	"Aaron Weber",
	"Dave West",
	"Peter Williams",
	"Matt Wilson",
	"Matthew Wilson",
	"Dan Winship",
	"Jeremy Wise",
	"Leon Zhang",
	"Philip Zhao",
	"Jukka Zitting",
	"Michael Zucchi"
};
#define NUM_TEXT_LINES (sizeof (text) / sizeof (*text))

struct _EShellAboutBoxPrivate {
	GdkPixmap *pixmap;
	GdkPixmap *text_background_pixmap;
	GdkGC *clipped_gc;
	int text_y_offset;
	int timeout_id;
	const gchar **permuted_text;
};


#define ANIMATION_DELAY 40

#define WIDTH  400
#define HEIGHT 200

#define TEXT_Y_OFFSET 57
#define TEXT_X_OFFSET 60
#define TEXT_WIDTH    (WIDTH - 2 * TEXT_X_OFFSET)
#define TEXT_HEIGHT   90

#define IMAGE_PATH  EVOLUTION_IMAGES "/about-box.png"



static void
permute_names (EShellAboutBox *about_box)
{
	EShellAboutBoxPrivate *priv = about_box->priv;
	gint i, j;

	srandom (time (NULL));
	
	for (i = 6; i < NUM_TEXT_LINES-1; ++i) {
		const gchar *tmp;
		j = i + random () % (NUM_TEXT_LINES - i);
		if (i != j) {
			tmp = priv->permuted_text[i];
			priv->permuted_text[i] = priv->permuted_text[j];
			priv->permuted_text[j] = tmp;
		}
	}
}

/* The callback.  */

static int
timeout_callback (void *data)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;
	GdkRectangle redraw_rect;
	GtkWidget *widget;
	PangoContext *context;
	PangoFontMetrics *metrics;
	PangoLayout *layout;
	int line_height;
	int first_line;
	int y;
	int i;

	about_box = E_SHELL_ABOUT_BOX (data);
	priv = about_box->priv;

	widget = GTK_WIDGET (about_box);

	context = gtk_widget_get_pango_context (widget);
	metrics = pango_context_get_metrics (context, gtk_widget_get_style (GTK_WIDGET (about_box))->font_desc,
					     pango_context_get_language (context));
	line_height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics)
				    + pango_font_metrics_get_descent (metrics));
	pango_font_metrics_unref (metrics);

	if (priv->text_y_offset < TEXT_HEIGHT) {
		y = TEXT_Y_OFFSET + (TEXT_HEIGHT - priv->text_y_offset);
		first_line = 0;
	} else {
		y = TEXT_Y_OFFSET - ((priv->text_y_offset - TEXT_HEIGHT) % line_height);
		first_line = (priv->text_y_offset - TEXT_HEIGHT) / line_height;
	}

	gdk_draw_pixmap (priv->pixmap, priv->clipped_gc, priv->text_background_pixmap,
			 0, 0,
			 TEXT_X_OFFSET, TEXT_Y_OFFSET, TEXT_WIDTH, TEXT_HEIGHT);

	layout = pango_layout_new (context);

	for (i = 0; i < TEXT_HEIGHT / line_height + 3; i ++) {
		const char *line;
		int width;
		int x;

		if (first_line + i >= NUM_TEXT_LINES)
			break;

		if (*priv->permuted_text[first_line + i] == '\0')
			line = "";
		else
			line = _(priv->permuted_text[first_line + i]);

		pango_layout_set_text (layout, line, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);
		x = TEXT_X_OFFSET + (TEXT_WIDTH - width) / 2;
		gdk_draw_layout (priv->pixmap, priv->clipped_gc, x, y, layout);

		y += line_height;
	}

	redraw_rect.x      = TEXT_X_OFFSET;
	redraw_rect.y      = TEXT_Y_OFFSET;
	redraw_rect.width  = TEXT_WIDTH;
	redraw_rect.height = TEXT_HEIGHT;
	gdk_window_invalidate_rect (widget->window, &redraw_rect, FALSE);
	gdk_window_process_updates (widget->window, FALSE);

	priv->text_y_offset ++;
	if (priv->text_y_offset > line_height * NUM_TEXT_LINES + TEXT_HEIGHT) {
		priv->text_y_offset = 0;
		permute_names (about_box);
	}

	g_object_unref (layout);

	return TRUE;
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;

	about_box = E_SHELL_ABOUT_BOX (object);
	priv = about_box->priv;

	if (priv->pixmap != NULL) {
		gdk_pixmap_unref (priv->pixmap);
		priv->pixmap = NULL;
	}

	if (priv->text_background_pixmap != NULL) {
		gdk_pixmap_unref (priv->text_background_pixmap);
		priv->text_background_pixmap = NULL;
	}

	if (priv->clipped_gc != NULL) {
		gdk_gc_unref (priv->clipped_gc);
		priv->clipped_gc = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;

	about_box = E_SHELL_ABOUT_BOX (object);
	priv = about_box->priv;

	if (priv->timeout_id != -1)
		g_source_remove (priv->timeout_id);

	g_free (priv->permuted_text);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* GtkWidget methods.  */

static void
impl_size_request (GtkWidget *widget,
		   GtkRequisition *requisition)
{
	requisition->width = WIDTH;
	requisition->height = HEIGHT;
}

static void
impl_realize (GtkWidget *widget)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;
	GdkPixbuf *background_pixbuf;
	GdkRectangle clip_rectangle;

	(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	about_box = E_SHELL_ABOUT_BOX (widget);
	priv = about_box->priv;

	background_pixbuf = gdk_pixbuf_new_from_file (IMAGE_PATH, NULL);
	g_assert (background_pixbuf != NULL);
	g_assert (gdk_pixbuf_get_width (background_pixbuf) == WIDTH);
	g_assert (gdk_pixbuf_get_height (background_pixbuf) == HEIGHT);

	g_assert (priv->pixmap == NULL);
	priv->pixmap = gdk_pixmap_new (widget->window, WIDTH, HEIGHT, -1);

	gdk_pixbuf_render_to_drawable (background_pixbuf, priv->pixmap, widget->style->black_gc,
				       0, 0, 0, 0, WIDTH, HEIGHT,
				       GDK_RGB_DITHER_MAX, 0, 0);

	g_assert (priv->clipped_gc == NULL);
	priv->clipped_gc = gdk_gc_new (widget->window);
	gdk_gc_copy (priv->clipped_gc, widget->style->black_gc);

	clip_rectangle.x      = TEXT_X_OFFSET;
	clip_rectangle.y      = TEXT_Y_OFFSET;
	clip_rectangle.width  = TEXT_WIDTH;
	clip_rectangle.height = TEXT_HEIGHT;
	gdk_gc_set_clip_rectangle (priv->clipped_gc, & clip_rectangle);

	priv->text_background_pixmap = gdk_pixmap_new (widget->window, clip_rectangle.width, clip_rectangle.height, -1);
	gdk_pixbuf_render_to_drawable (background_pixbuf, priv->text_background_pixmap, widget->style->black_gc,
				       TEXT_X_OFFSET, TEXT_Y_OFFSET,
				       0, 0, TEXT_WIDTH, TEXT_HEIGHT,
				       GDK_RGB_DITHER_MAX, 0, 0);

	g_assert (priv->timeout_id == -1);
	priv->timeout_id = g_timeout_add (ANIMATION_DELAY, timeout_callback, about_box);

	g_object_unref (background_pixbuf);
}

static void
impl_unrealize (GtkWidget *widget)
{
	EShellAboutBox *about_box;
	EShellAboutBoxPrivate *priv;

	about_box = E_SHELL_ABOUT_BOX (widget);
	priv = about_box->priv;

	(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

	g_assert (priv->clipped_gc != NULL);
	gdk_gc_unref (priv->clipped_gc);
	priv->clipped_gc = NULL;

	g_assert (priv->pixmap != NULL);
	gdk_pixmap_unref (priv->pixmap);
	priv->pixmap = NULL;

	if (priv->timeout_id != -1) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = -1;
	}
}

static int
impl_expose_event (GtkWidget *widget,
		   GdkEventExpose *event)
{
	EShellAboutBoxPrivate *priv;

	if (! GTK_WIDGET_DRAWABLE (widget))
		return FALSE;

	priv = E_SHELL_ABOUT_BOX (widget)->priv;

	gdk_draw_pixmap (widget->window, widget->style->black_gc,
			 priv->pixmap,
			 event->area.x, event->area.y,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);

	return TRUE;
}


static void
class_init (GObjectClass *object_class)
{
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_ref(PARENT_TYPE);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	widget_class = GTK_WIDGET_CLASS (object_class);
	widget_class->size_request = impl_size_request;
	widget_class->realize      = impl_realize;
	widget_class->unrealize    = impl_unrealize;
	widget_class->expose_event = impl_expose_event;
}

static void
init (EShellAboutBox *shell_about_box)
{
	EShellAboutBoxPrivate *priv;
	gint i;

	priv = g_new (EShellAboutBoxPrivate, 1);
	priv->pixmap                 = NULL;
	priv->text_background_pixmap = NULL;
	priv->clipped_gc             = NULL;
	priv->timeout_id             = -1;
	priv->text_y_offset          = 0;

	priv->permuted_text = g_new (const gchar *, NUM_TEXT_LINES);
	for (i = 0; i < NUM_TEXT_LINES; ++i) {
		priv->permuted_text[i] = text[i];
	}

	shell_about_box->priv = priv;

	permute_names (shell_about_box);
}


GtkWidget *
e_shell_about_box_new (void)
{
	EShellAboutBox *about_box;

	about_box = g_object_new (e_shell_about_box_get_type (), NULL);

	return GTK_WIDGET (about_box);
}


E_MAKE_TYPE (e_shell_about_box, "EShellAboutBox", EShellAboutBox, class_init, init, GTK_TYPE_EVENT_BOX)
