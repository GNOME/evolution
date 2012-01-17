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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtkhtml/gtkhtml.h>

#include <e-util/e-print.h>

#include <libemail-utils/mail-mt.h>
#include <libemail-engine/mail-ops.h>

#include "em-format-html-print.h"

G_DEFINE_TYPE (
	EMFormatHTMLPrint,
	em_format_html_print,
	EM_TYPE_FORMAT_HTML)

static void
efhp_finalize (GObject *object)
{
	EMFormatHTMLPrint *efhp = (EMFormatHTMLPrint *) object;

	g_free (efhp->export_filename);
	efhp->export_filename = NULL;
	gtk_widget_destroy (efhp->window);
	if (efhp->source != NULL)
		g_object_unref (efhp->source);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_format_html_print_parent_class)->finalize (object);
}

static gboolean
efhp_is_inline (EMFormat *emf,
                const gchar *part_id,
                CamelMimePart *mime_part,
                const EMFormatHandler *handle)
{
	/* When printing, inline any part that has a handler. */
	return (handle != NULL);
}

static void
em_format_html_print_class_init (EMFormatHTMLPrintClass *class)
{
	GObjectClass *object_class;
	EMFormatClass *format_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = efhp_finalize;

	format_class = EM_FORMAT_CLASS (class);
	format_class->is_inline = efhp_is_inline;
}

static void
em_format_html_print_init (EMFormatHTMLPrint *efhp)
{
	EWebView *web_view;

	web_view = em_format_html_get_web_view (EM_FORMAT_HTML (efhp));

	/* gtk widgets don't like to be realized outside top level widget
	 * so we put new html widget into gtk window */
	efhp->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_add (GTK_CONTAINER (efhp->window), GTK_WIDGET (web_view));
	gtk_widget_realize (GTK_WIDGET (web_view));
	efhp->parent.show_icon = FALSE;
	((EMFormat *) efhp)->print = TRUE;

	efhp->export_filename = NULL;
	efhp->async = TRUE;
}

EMFormatHTMLPrint *
em_format_html_print_new (EMFormatHTML *source,
                          GtkPrintOperationAction action)
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
	EWebView *web_view;
	GError *error = NULL;

	web_view = em_format_html_get_web_view (EM_FORMAT_HTML (efhp));

	operation = e_print_operation_new ();

	if (efhp->action == GTK_PRINT_OPERATION_ACTION_EXPORT)
		gtk_print_operation_set_export_filename (operation, efhp->export_filename);

	gtk_html_print_operation_run (
		GTK_HTML (web_view),
		operation, efhp->action, NULL,
		(GtkHTMLPrintCalcHeight) NULL,
		(GtkHTMLPrintCalcHeight) efhp_calc_footer_height,
		(GtkHTMLPrintDrawFunc) NULL,
		(GtkHTMLPrintDrawFunc) efhp_draw_footer,
		NULL, &error);

	g_object_unref (operation);
}

void
em_format_html_print_message (EMFormatHTMLPrint *efhp,
                              CamelMimeMessage *message,
                              CamelFolder *folder,
                              const gchar *message_uid)
{
	g_return_if_fail (EM_IS_FORMAT_HTML_PRINT (efhp));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	/* Wrap flags to display all entries by default.*/
	EM_FORMAT_HTML (efhp)->header_wrap_flags |=
		EM_FORMAT_HTML_HEADER_TO |
		EM_FORMAT_HTML_HEADER_CC |
		EM_FORMAT_HTML_HEADER_BCC;

	if (efhp->async) {
		g_signal_connect (
			efhp, "complete", G_CALLBACK (emfhp_complete), efhp);
		
		/* FIXME Not passing a GCancellable here. */
		em_format_format_clone (
			(EMFormat *) efhp, 
			folder, message_uid, message,
			(EMFormat *) efhp->source, NULL);
	} else {
		em_format_html_clone_sync (folder, message_uid, message, (EMFormatHTML *)efhp, (EMFormat *)efhp->source);
		emfhp_complete (efhp);
	}
}
