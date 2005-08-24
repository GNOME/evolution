/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Vivek Jain <jvivek@novell.com>
 *
 * Copyright 2004 Novell, Inc. (www.novell.com)
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <gtkhtml/gtkhtml.h>
#include "mail/em-format-html-display.h"
#include "mail/em-format-html-print.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include "mail/em-menu.h"
#include "mail/em-utils.h"
#include "e-util/e-print.h"
#include "e-util/e-dialog-utils.h"
#include "composer/e-msg-composer.h"

void org_gnome_compose_print_message (EPlugin *ep, EMMenuTargetWidget *t);

struct _print_data {
	GnomePrintConfig *config;
	CamelMimeMessage *msg;
	int preview;
};

static void
print_response (GtkWidget *w, int resp, struct _print_data *data)
{
	EMFormatHTMLPrint *print;

	switch (resp) {
	case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
		data->preview = TRUE;
	case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
		print = em_format_html_print_new();
	   	em_format_html_print_raw_message(print, data->config, data->msg, data->preview);
		g_object_unref(print);
		break;
	}

	if (w)
		gtk_widget_destroy(w);
	
	e_print_save_config (data->config);
	g_object_unref(data->config);
	g_free(data);

}

void  org_gnome_print_message (EPlugin *ep, EMMenuTargetWidget *t);

void 
org_gnome_print_message (EPlugin *ep, EMMenuTargetWidget *t)
{

       	EMsgComposer *composer = (EMsgComposer *)t->target.widget;
	struct _print_data *data;
	GtkDialog *dialog;
	
	data = g_malloc0(sizeof(*data));
	data->config = e_print_load_config ();
	data->preview = 0;
	
	data->msg = e_msg_composer_get_message (composer, 1);
	dialog = (GtkDialog *)e_print_get_dialog_with_config (_("Print Message"), GNOME_PRINT_DIALOG_COPIES, data->config);
	gtk_dialog_set_default_response(dialog, GNOME_PRINT_DIALOG_RESPONSE_PRINT);
	e_dialog_set_transient_for ((GtkWindow *) dialog, (GtkWidget *) composer);
	g_signal_connect(dialog, "response", G_CALLBACK(print_response), data);
	gtk_widget_show((GtkWidget *)dialog);
	
}

void org_gnome_print_preview (EPlugin *ep, EMMenuTargetWidget *t);

void
org_gnome_print_preview (EPlugin *ep, EMMenuTargetWidget *t)
{
  	EMsgComposer *composer = (EMsgComposer *)t->target.widget;
	struct _print_data *data;
	
	data = g_malloc0(sizeof(*data));
	data->config = e_print_load_config ();
	data->preview = 0;
	
	data->msg = e_msg_composer_get_message (composer, 1);

	print_response(NULL, GNOME_PRINT_DIALOG_RESPONSE_PREVIEW, data);
}


