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
#include <libgnomeprint/gnome-print-job.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <gtkhtml/gtkhtml.h>
#include <gtk/gtkwindow.h>

#include <camel/camel-i18n.h>
#include "mail-ops.h"
#include "mail-mt.h"
#include "em-format-html-print.h"
#include <gtk/gtk.h>
#include <e-util/e-print.h>

static void efhp_builtin_init(EMFormatHTMLPrintClass *efhc);

static EMFormatHTMLClass *efhp_parent;

static void
efhp_init(GObject *o)
{
	EMFormatHTMLPrint *efhp = (EMFormatHTMLPrint *)o;
	GtkWidget *html = (GtkWidget *)efhp->formathtml.html;

	/* ?? */
	gtk_widget_set_name(html, "EvolutionMailPrintHTMLWidget");

	/* gtk widgets don't like to be realized outside top level widget
	   so we put new html widget into gtk window */
	efhp->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_add((GtkContainer *)efhp->window, html);
	gtk_widget_realize(html);
	efhp->formathtml.show_icon = FALSE;
}

static void
efhp_finalise(GObject *o)
{
	EMFormatHTMLPrint *efhp = (EMFormatHTMLPrint *)o;

	gtk_widget_destroy(efhp->window);
	if (efhp->settings)
		g_object_unref(efhp->settings);
	if (efhp->source)
		g_object_unref(efhp->source);

	((GObjectClass *)efhp_parent)->finalize(o);
}

static void
efhp_base_init(EMFormatHTMLPrintClass *efhpklass)
{
	efhp_builtin_init(efhpklass);
}

static void
efhp_class_init(GObjectClass *klass)
{
	klass->finalize = efhp_finalise;
}

GType
em_format_html_print_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMFormatHTMLPrintClass),
			(GBaseInitFunc)efhp_base_init, NULL,
			(GClassInitFunc)efhp_class_init,
			NULL, NULL,
			sizeof(EMFormatHTMLPrint), 0,
			(GInstanceInitFunc)efhp_init
		};
		efhp_parent = g_type_class_ref(em_format_html_get_type());
		type = g_type_register_static(em_format_html_get_type(), "EMFormatHTMLPrint", &info, 0);
	}

	return type;
}

EMFormatHTMLPrint *em_format_html_print_new(void)
{
	EMFormatHTMLPrint *efhp;

	efhp = g_object_new(em_format_html_print_get_type(), 0);

	return efhp;
}

struct footer_info {
	GnomeFont *local_font;
	gint page_num, pages;
};
typedef struct MailDraw MailDraw;

struct MailDraw {
	EMFormatHTMLPrint *efhp;
	struct footer_info info;
	void *data;
	gint res;
	gdouble line;
};

static void mail_draw_page(GtkPrintOperation *print, GtkPrintContext *context, gint page_nr, MailDraw *mdi);
static void
efhp_footer_cb(GtkHTML *html, GtkPrintContext *print_context, double x, double y, double width, double height, void *data)
{
	struct footer_info *info = data;
	cairo_t *cr;
     
	/* do we want anything nicer here, like who its from, etc? */
	if (info->local_font) {
		char *text = g_strdup_printf (_("Page %d of %d"), info->page_num, info->pages);
		/*gdouble tw = gnome_font_get_width_string (info->local_font, text);*/
		/* FIXME: work out how to measure this */
		gdouble tw = strlen(text) * 8;
		cr = gtk_print_context_get_cairo_context (print_context);
		cairo_save (cr);
		cairo_set_source_rgb (cr, .0, .0, .0);
		cairo_move_to (cr, x + width - tw, y - gnome_font_get_ascender(info->local_font));
		cairo_set_font_face (cr, NULL);
		cairo_set_font_size (cr, 6);
		cairo_show_text (cr, text);
		cairo_restore(cr);
		g_free(text); 
		info->page_num++;   
	}
}

/* perform preview, or print */
/* returns GNOME_PRINT_OK on success */
static void
emfhp_complete(EMFormatHTMLPrint *efhp, void *data)
{
	GtkPaperSize *paper_size;
	GtkPrintOperation *print;
	GtkPrintSettings *settings;
	GtkPageSetup *page_setup;
	struct MailDraw *mdi;
	struct footer_info info;

	page_setup = gtk_page_setup_new ();
	paper_size = gtk_paper_size_new ("iso_a4");/*FIXME paper size hardcoded */
	print = gtk_print_operation_new ();
	gtk_page_setup_set_paper_size (page_setup, paper_size);	 

	settings = e_print_load_settings ();
	gtk_print_operation_set_print_settings (print, settings);
	/* running the dialog */
	gtk_print_operation_set_default_page_setup (print, page_setup);
	gtk_print_operation_set_n_pages (print, 1); 

	/*initialise the struct */
	mdi = g_new0 (MailDraw, 1);
	mdi->efhp = efhp;
	mdi->line = 0.0;
	mdi->res = GNOME_PRINT_OK;
	mdi->info = info;
			
        /* connect */
	g_signal_connect (print,"draw_page", G_CALLBACK (mail_draw_page), mdi);
	if (!efhp->preview)
		gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, NULL, NULL); 
	else
		gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PREVIEW, NULL, NULL);
	settings = gtk_print_operation_get_print_settings (print);
	e_print_save_settings (settings);
	g_object_unref (print);
}

static void 
mail_draw_page (GtkPrintOperation *print, GtkPrintContext *context, gint page_nr, MailDraw *mdi)
{
	mdi->info.local_font = gnome_font_find_closest("Sans Regular", 10.0);
	if (mdi->info.local_font) {
		mdi->line = gnome_font_get_ascender(mdi->info.local_font) - gnome_font_get_descender(mdi->info.local_font);                
		mdi->info.page_num = 1;
		mdi->info.pages = gtk_html_print_page_get_pages_num(mdi->efhp->formathtml.html, context, 0.0, mdi->line );
		gtk_html_print_page_with_header_footer(mdi->efhp->formathtml.html,
						  context, 
						  0.0, 
					          mdi->line,
					          NULL,
						  efhp_footer_cb, 
						  &(mdi->info));
	} else {
		gtk_html_print_page (mdi->efhp->formathtml.html, context);
	}
}

int em_format_html_print_print(EMFormatHTMLPrint *efhp, EMFormatHTML *source, struct GtkPrintSettings *settings, int preview)
{
	EMFormat *emfs = (EMFormat *)source;

	efhp->settings = settings;
	if (settings)
		g_object_ref(settings);

	efhp->preview = preview;
	((EMFormatHTML *)efhp)->load_http = source->load_http_now;

	g_signal_connect(efhp, "complete", G_CALLBACK(emfhp_complete), efhp);

	g_object_ref(efhp);
	em_format_format_clone((EMFormat *)efhp, emfs->folder, emfs->uid, emfs->message, (EMFormat *)source);

	return 0;		/* damn async ... */
}

static void
emfhp_got_message(struct _CamelFolder *folder, const char *uid, struct _CamelMimeMessage *msg, void *data)
{
	EMFormatHTMLPrint *efhp = data;

	if (msg) {
		if (efhp->source)
			((EMFormatHTML *)efhp)->load_http = efhp->source->load_http_now;
		g_signal_connect(efhp, "complete", G_CALLBACK(emfhp_complete), efhp);
		em_format_format_clone((EMFormat *)efhp, folder, uid, msg, (EMFormat *)efhp->source);
	} else {
		g_object_unref(efhp);
	}
}

int em_format_html_print_message(EMFormatHTMLPrint *efhp, EMFormatHTML *source, struct GtkPrintSettings *settings, struct _CamelFolder *folder, const char *uid, int preview)
{
	efhp->settings = settings;

	if (settings)
		g_object_ref(settings);
	efhp->preview = preview;
	efhp->source = source;
	if (source)
		g_object_ref(source);
	g_object_ref(efhp);

	mail_get_message(folder, uid, emfhp_got_message, efhp, mail_thread_new);

	return 0;		/* damn async ... */
}

int em_format_html_print_raw_message(EMFormatHTMLPrint *efhp, GtkPrintSettings *settings, struct _CamelMimeMessage *msg, int preview)
{
	efhp->settings = settings;

	if (settings)
		g_object_ref(settings);
	efhp->source = NULL;
	efhp->preview = preview;
	g_object_ref(efhp);

	emfhp_got_message(NULL, NULL, msg, efhp);

	return 0;
}

/* ********************************************************************** */

/* if only ... but i doubt this is possible with gnome print/gtkhtml */
static EMFormatHandler type_builtin_table[] = {
	/*{ "application/postscript", (EMFormatFunc)efhp_application_postscript },*/
};

static void
efhp_builtin_init(EMFormatHTMLPrintClass *efhc)
{
	int i;

	for (i=0;i<sizeof(type_builtin_table)/sizeof(type_builtin_table[0]);i++)
		em_format_class_add_handler((EMFormatClass *)efhc, &type_builtin_table[i]);
}
