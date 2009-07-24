/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gtkhtml/gtkhtml.h>

#include <camel/camel-i18n.h>
#include "mail-ops.h"
#include "mail-mt.h"
#include "em-format-html-print.h"
#include <e-util/e-print.h>

static gpointer parent_class = NULL;

static void
efhp_finalize (GObject *o)
{
	EMFormatHTMLPrint *efhp = (EMFormatHTMLPrint *)o;

	gtk_widget_destroy (efhp->window);
	if (efhp->source != NULL)
		g_object_unref (efhp->source);

	((GObjectClass *) parent_class)->finalize (o);
}

static void
efhp_class_init (GObjectClass *class)
{
	parent_class = g_type_class_peek_parent (class);
	class->finalize = efhp_finalize;
}

static void
efhp_init (GObject *o)
{
	EMFormatHTMLPrint *efhp = (EMFormatHTMLPrint *)o;
	GtkWidget *html = (GtkWidget *)efhp->parent.html;

	/* ?? */
	gtk_widget_set_name(html, "EvolutionMailPrintHTMLWidget");

	/* gtk widgets don't like to be realized outside top level widget
	   so we put new html widget into gtk window */
	efhp->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_add (GTK_CONTAINER (efhp->window), html);
	gtk_widget_realize (html);
	efhp->parent.show_icon = FALSE;
	((EMFormat *)efhp)->print = TRUE;
}

GType
em_format_html_print_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFormatHTMLPrintClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) efhp_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFormatHTMLPrint),
			0,     /* n_preallocs */
			(GInstanceInitFunc) efhp_init
		};

		type = g_type_register_static (
			em_format_html_get_type (), "EMFormatHTMLPrint",
			&type_info, 0);
	}

	return type;
}

EMFormatHTMLPrint *
em_format_html_print_new (EMFormatHTML *source, GtkPrintOperationAction action)
{
	EMFormatHTMLPrint *efhp;

	efhp = g_object_new (EM_TYPE_FORMAT_HTML_PRINT, NULL);
	if (source != NULL)
		efhp->source = g_object_ref (source);
	efhp->action = action;

	return efhp;
}

static gint
efhp_calc_footer_height (GtkHTML *html,
                         GtkPrintOperation *operation,
                         GtkPrintContext *context)
{
	PangoContext *pango_context;
	PangoFontDescription *desc;
	PangoFontMetrics *metrics;
	gint footer_height;

	pango_context = gtk_print_context_create_pango_context (context);
	desc = pango_font_description_from_string ("Sans Regular 10");

	metrics = pango_context_get_metrics (
		pango_context, desc, pango_language_get_default ());
	footer_height =
		pango_font_metrics_get_ascent (metrics) +
		pango_font_metrics_get_descent (metrics);
	pango_font_metrics_unref (metrics);

	pango_font_description_free (desc);
	g_object_unref (pango_context);

	return footer_height;
}

static void
efhp_draw_footer (GtkHTML *html,
                  GtkPrintOperation *operation,
                  GtkPrintContext *context,
                  gint page_nr,
                  PangoRectangle *rec)
{
	PangoFontDescription *desc;
	PangoLayout *layout;
	gdouble x, y;
	gint n_pages;
	gchar *text;
	cairo_t *cr;

	g_object_get (operation, "n-pages", &n_pages, NULL);
	text = g_strdup_printf (_("Page %d of %d"), page_nr + 1, n_pages);

	desc = pango_font_description_from_string ("Sans Regular 10");
	layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	pango_layout_set_font_description (layout, desc);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_width (layout, rec->width);

	x = pango_units_to_double (rec->x);
	y = pango_units_to_double (rec->y);

	cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);
	cairo_set_source_rgb (cr, .0, .0, .0);
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, layout);
	cairo_restore (cr);

	g_object_unref (layout);
	pango_font_description_free (desc);

	g_free (text);
}

static void
emfhp_complete (EMFormatHTMLPrint *efhp)
{
	GtkPrintOperation *operation;
	GError *error = NULL;

	operation = e_print_operation_new ();

	gtk_html_print_operation_run (
		efhp->parent.html, operation, efhp->action, NULL,
		(GtkHTMLPrintCalcHeight) NULL,
		(GtkHTMLPrintCalcHeight) efhp_calc_footer_height,
		(GtkHTMLPrintDrawFunc) NULL,
		(GtkHTMLPrintDrawFunc) efhp_draw_footer,
		NULL, &error);

	g_object_unref (operation);
}

static void
emfhp_got_message (CamelFolder *folder, const gchar *uid,
                   CamelMimeMessage *msg, gpointer data)
{
	EMFormatHTMLPrint *efhp = data;

	if (msg == NULL) {
		g_object_unref (efhp);
		return;
	}

	if (efhp->source != NULL)
		((EMFormatHTML *)efhp)->load_http =
			efhp->source->load_http_now;

	g_signal_connect (
		efhp, "complete", G_CALLBACK (emfhp_complete), efhp);
	em_format_format_clone (
		(EMFormat *) efhp, folder, uid, msg,
		(EMFormat *) efhp->source);
}

void
em_format_html_print_message (EMFormatHTMLPrint *efhp,
                              CamelFolder *folder,
                              const gchar *uid)
{
	g_object_ref (efhp);

	/* Wrap flags to display all entries by default.*/
	((EMFormatHTML *) efhp)->header_wrap_flags |= EM_FORMAT_HTML_HEADER_TO
						| EM_FORMAT_HTML_HEADER_CC
						| EM_FORMAT_HTML_HEADER_BCC;

	mail_get_message (
		folder, uid, emfhp_got_message, efhp, mail_msg_unordered_push);
}

void
em_format_html_print_raw_message (EMFormatHTMLPrint *efhp,
                                  CamelMimeMessage *msg)
{
	g_object_ref (efhp);

	emfhp_got_message (NULL, NULL, msg, efhp);
}
