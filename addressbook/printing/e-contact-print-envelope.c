/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-print-envelope.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "addressbook/printing/e-contact-print-envelope.h"
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <time.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-job.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <e-util/e-print.h>

#define ENVELOPE_HEIGHT (72.0 * 4.0)
#define ENVELOPE_WIDTH (72.0 * 9.5)

typedef struct {
	int start;
	int length;
} EcpeLine;

static void
startset(void *pointer, EcpeLine **iterator)
{
	(*iterator)--;
	(*iterator)->start = GPOINTER_TO_INT(pointer);
}

static void
lengthset(void *pointer, EcpeLine **iterator)
{
	(*iterator)--;
	(*iterator)->length = GPOINTER_TO_INT(pointer);
}

static EcpeLine *
ecpe_break(char *address)
{
	int i;
	int length = 0;
	int laststart = 0;
	GList *startlist = NULL;
	GList *lengthlist = NULL;
	EcpeLine *ret_val;
	EcpeLine *iterator;

	for (i = 0; address[i]; i++) {
		if (address[i] == '\n') {
			startlist = g_list_prepend (startlist, GINT_TO_POINTER(laststart));
			lengthlist = g_list_prepend (lengthlist, GINT_TO_POINTER(i - laststart));
			length ++;
			laststart = i + 1;
		}
	}
	startlist = g_list_prepend (startlist, GINT_TO_POINTER(laststart));
	lengthlist = g_list_prepend (lengthlist, GINT_TO_POINTER(i - laststart));
	length ++;

	ret_val = g_new(EcpeLine, length + 1);

	iterator = ret_val + length;
	g_list_foreach(startlist, (GFunc) startset, &iterator);
	g_list_free(startlist);

	iterator = ret_val + length;
	g_list_foreach(lengthlist, (GFunc) lengthset, &iterator);
	g_list_free(lengthlist);

	ret_val[length].start = -1;
	ret_val[length].length = -1;

	return ret_val;
}

static void
ecpe_linelist_dimensions(GnomeFont *font, char *address, EcpeLine *linelist, double *widthp, double *heightp)
{
	double width = 0;
	int i;
	if (widthp) {
		for (i = 0; linelist[i].length != -1; i++) {
			width = MAX(width, gnome_font_get_width_utf8_sized (font, address + linelist[i].start, linelist[i].length));
		}
		*widthp = width;
	} else {
		for (i = 0; linelist[i].length != -1; i++)
			/* Intentionally empty */;
	}
	if (heightp) {
		*heightp = gnome_font_get_size(font) * i;
	}
}

static void
ecpe_linelist_print(GnomePrintContext *pc, GnomeFont *font, char *address, EcpeLine *linelist, double x, double y)
{
	int i;
	gnome_print_setfont(pc, font);
	for (i = 0; linelist[i].length != -1; i++) {
		gnome_print_moveto(pc, x, y + gnome_font_get_ascender(font));
		gnome_print_show_sized (pc, address + linelist[i].start, linelist[i].length);
		y -= gnome_font_get_size(font);
	}
}

static gint
e_contact_print_envelope_close(GnomeDialog *dialog, gpointer data)
{
	return FALSE;
}

static void
ecpe_print(GnomePrintContext *pc, EContact *contact, gboolean as_return)
{
	char *address;
	EcpeLine *linelist;
	double x;
	double y;
	GnomeFont *font;
	

	gnome_print_rotate(pc, 90);
	gnome_print_translate(pc, 72.0 * 11.0 - ENVELOPE_WIDTH, -72.0 * 8.5 + (72.0 * 8.5 - ENVELOPE_HEIGHT) / 2);

	address = e_contact_get(contact, E_CONTACT_ADDRESS_LABEL_WORK);
	linelist = ecpe_break(address);
	if (as_return)
		font = gnome_font_find ("Sans", 9);
	else
		font = gnome_font_find ("Sans", 12);
	ecpe_linelist_dimensions(font, address, linelist, NULL, &y);
	if (as_return) {
		x = 36;
		y = ENVELOPE_HEIGHT - 36;
	} else {
		x = ENVELOPE_WIDTH / 2;
		y = (ENVELOPE_HEIGHT - y) / 2;
	}
	ecpe_linelist_print(pc, font, address, linelist, x, y);
	g_object_unref(font);
	g_free(linelist);

	g_free(address);

	gnome_print_showpage(pc);
	gnome_print_context_close(pc);
}

static void
e_contact_print_envelope_button(GnomeDialog *dialog, gint button, gpointer data)
{
	GnomePrintJob *master;
	GnomePrintContext *pc;
	GnomePrintConfig *config;
	EContact *contact = NULL;
	GtkWidget *preview;

	contact = g_object_get_data(G_OBJECT(dialog), "contact");

	switch( button ) {
	case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
		config = gnome_print_dialog_get_config (GNOME_PRINT_DIALOG (dialog));
		master = gnome_print_job_new (config);
		pc = gnome_print_job_get_context( master );

		ecpe_print(pc, contact, FALSE);
		
		gnome_print_job_print(master);
		gnome_dialog_close(dialog);
		break;
	case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
		config = gnome_print_dialog_get_config (GNOME_PRINT_DIALOG (dialog));
		master = gnome_print_job_new (config);
		pc = gnome_print_job_get_context( master );

		ecpe_print(pc, contact, FALSE);
		
		preview = GTK_WIDGET(gnome_print_job_preview_new(master, "Print Preview"));
		gtk_widget_show_all(preview);
		break;
	case GNOME_PRINT_DIALOG_RESPONSE_CANCEL:
		g_object_unref(contact);
		gnome_dialog_close(dialog);
		break;
	}
}

GtkWidget *
e_contact_print_envelope_dialog_new(EContact *contact)
{
	GtkWidget *dialog;
	
	dialog = e_print_get_dialog (_("Print envelope"), GNOME_PRINT_DIALOG_COPIES);

	contact = e_contact_duplicate(contact);
	g_object_set_data(G_OBJECT(dialog), "contact", contact);
	g_signal_connect(dialog,
			 "clicked", G_CALLBACK(e_contact_print_envelope_button), NULL);
	g_signal_connect(dialog,
			 "close", G_CALLBACK(e_contact_print_envelope_close), NULL);
	return dialog;
}

/* FIXME: Print all the contacts selected. */
GtkWidget *
e_contact_print_envelope_list_dialog_new(GList *list)
{
	GtkWidget *dialog;
	EContact *contact;

	if (list == NULL)
		return NULL;

	dialog = e_print_get_dialog(_("Print envelope"), GNOME_PRINT_DIALOG_COPIES);

	contact = e_contact_duplicate(list->data);
	g_object_set_data(G_OBJECT(dialog), "contact", contact);
	g_signal_connect(dialog,
			 "clicked", G_CALLBACK(e_contact_print_envelope_button), NULL);
	g_signal_connect(dialog,
			 "close", G_CALLBACK(e_contact_print_envelope_close), NULL);
	return dialog;
}
