/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
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
	parent_class = g_type_class_ref(EM_TYPE_FORMAT_HTML_PRINT);
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

struct footer_info {
	PangoLayout *layout;
	gint page_num, pages;
};

static void
efhp_footer_cb (GtkHTML *html, GtkPrintContext *context, gdouble x, gdouble y,
                gdouble width, gdouble height, gpointer data)
{
	struct footer_info *info = data;
	gchar *footer_text;
	cairo_t *cr;

	footer_text = g_strdup_printf (_("Page %d of %d"),
		info->page_num++, info->pages);

	pango_layout_set_text (info->layout, footer_text, -1);
	pango_layout_set_width (info->layout, pango_units_from_double (width));

	cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);
	cairo_set_source_rgb (cr, .0, .0, .0);
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, info->layout);
	cairo_restore (cr);

	g_free (footer_text);
}

static void 
mail_draw_page (GtkPrintOperation *print, GtkPrintContext *context,
                gint page_nr, EMFormatHTMLPrint *efhp)
{
	GtkHTML *html = efhp->parent.html;
	PangoFontDescription *desc;
	PangoFontMetrics *metrics;
	struct footer_info info;
	gdouble footer_height;

	desc = pango_font_description_from_string ("Sans Regular 10");

	info.layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_alignment (info.layout, PANGO_ALIGN_CENTER);
	pango_layout_set_font_description (info.layout, desc);

	metrics = pango_context_get_metrics (
		pango_layout_get_context (info.layout),
		desc, pango_language_get_default ());
	footer_height = pango_units_to_double (
		pango_font_metrics_get_ascent (metrics) +
		pango_font_metrics_get_descent (metrics));
	pango_font_metrics_unref (metrics);

	pango_font_description_free (desc);

	info.page_num = 1;
	info.pages = gtk_html_print_page_get_pages_num (
		html, context, 0.0, footer_height);

	gtk_html_print_page_with_header_footer (
		html, context, 0.0, footer_height,
		NULL, efhp_footer_cb, &info);
}

static void
emfhp_complete (EMFormatHTMLPrint *efhp)
{
	GtkPrintOperation *operation;

	operation = e_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1); 

	g_signal_connect (
		operation, "draw-page",
		G_CALLBACK (mail_draw_page), efhp);

	gtk_print_operation_run (operation, efhp->action, NULL, NULL); 

	g_object_unref (operation);
}

static void
emfhp_got_message (CamelFolder *folder, const char *uid,
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
                              const char *uid)
{
	g_object_ref (efhp);

	mail_get_message (
		folder, uid, emfhp_got_message, efhp, mail_thread_new);
}

void
em_format_html_print_raw_message (EMFormatHTMLPrint *efhp,
                                  CamelMimeMessage *msg)
{
	g_object_ref (efhp);

	emfhp_got_message (NULL, NULL, msg, efhp);
}
