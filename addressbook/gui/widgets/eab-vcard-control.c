/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * eab-vcard-control.c
 *
 * Copyright (C) 1999, 2000, 2001, 2002, 2003, Ximian, Inc.
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
 * Authors:
 *    Chris Lahey <clahey@ximian.com>
 *    Chris Toshok <toshok@ximian.com>
 */

#include <config.h>
#include <string.h>

#include <gtk/gtk.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-persist.h>
#include <bonobo/bonobo-persist-stream.h>
#include <bonobo/bonobo-stream-client.h>
#include <gal/util/e-util.h>

#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include <addressbook/gui/component/addressbook.h>
#include <addressbook/gui/widgets/eab-contact-display.h>
#include <addressbook/util/eab-book-util.h>

#include "eab-vcard-control.h"
#include "eab-contact-merging.h"

typedef struct {
	EABContactDisplay *display;
	GList *card_list;
	GtkWidget *label;
	EABContactDisplayRenderMode render_mode;
} EABVCardControl;

#define VCARD_CONTROL_ID "OAFIID:GNOME_Evolution_Addressbook_VCard_Control:" BASE_VERSION

/*
 * Bonobo::PersistStream
 *
 * These two functions implement the Bonobo::PersistStream load and
 * save methods which allow data to be loaded into and out of the
 * BonoboObject.
 */
static char *
stream_read (Bonobo_Stream stream)
{
	Bonobo_Stream_iobuf *buffer;
	CORBA_Environment    ev;
	char *data = NULL;
	gint length = 0;

	CORBA_exception_init (&ev);
	do {
#define READ_CHUNK_SIZE 65536
		Bonobo_Stream_read (stream, READ_CHUNK_SIZE,
				    &buffer, &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			CORBA_exception_free (&ev);
			return NULL;
		}

		if (buffer->_length <= 0)
			break;

		data = g_realloc (data, length + buffer->_length + 1);

		memcpy (data + length, buffer->_buffer, buffer->_length);

		length += buffer->_length;

		CORBA_free (buffer);
	} while (1);

	CORBA_free (buffer);
	CORBA_exception_free (&ev);

	if (data)
		data[length] = '\0';
	else
		data = g_strdup("");

	return data;
} /* stream_read */

/*
 * This function implements the Bonobo::PersistStream:load method.
 */
static void
pstream_load (BonoboPersistStream *ps, const Bonobo_Stream stream,
	      Bonobo_Persist_ContentType type, void *data,
	      CORBA_Environment *ev)
{
	GList *list;
	char *vcard;
	EABVCardControl *vcard_control = data;

	if (type && g_ascii_strcasecmp (type, "text/vCard") != 0 &&	    
	    g_ascii_strcasecmp (type, "text/x-vCard") != 0) {	    
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	if ((vcard = stream_read (stream)) == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_FileNotFound, NULL);
		return;
	}

	e_free_object_list (vcard_control->card_list);
	list = eab_contact_list_from_string (vcard);
	g_free(vcard);
	vcard_control->card_list = list;
	if (list) {
		eab_contact_display_render (vcard_control->display, E_CONTACT (list->data),
					    vcard_control->render_mode);
	}
	if (list && list->next) {
		char *message;
		int length = g_list_length (list) - 1;
		message = g_strdup_printf (ngettext("and one other contact.", 
						    "and %d other contacts.", length), 
					   length);
		gtk_label_set_text (GTK_LABEL (vcard_control->label), message);
		g_free (message);
		gtk_widget_show (vcard_control->label);
	} else {
		gtk_widget_hide (vcard_control->label);
	}
} /* pstream_load */

/*
 * This function implements the Bonobo::PersistStream:save method.
 */
static void
pstream_save (BonoboPersistStream *ps, const Bonobo_Stream stream,
	      Bonobo_Persist_ContentType type, void *data,
	      CORBA_Environment *ev)
{
	EABVCardControl *vcard_control = data;
	char             *vcard;
	int               length;

	if (type && g_ascii_strcasecmp (type, "text/vCard") != 0 &&	    
	    g_ascii_strcasecmp (type, "text/x-vCard") != 0) {	    
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	vcard = eab_contact_list_to_string (vcard_control->card_list);
	length = strlen (vcard);
	bonobo_stream_client_write (stream, vcard, length, ev);
	g_free (vcard);
} /* pstream_save */

static Bonobo_Persist_ContentTypeList *
pstream_get_content_types (BonoboPersistStream *ps, void *closure,
			   CORBA_Environment *ev)
{
	return bonobo_persist_generate_content_types (2, "text/vCard", "text/x-vCard");
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	GList *list = closure;
	if (status == E_BOOK_ERROR_OK) {
		GList *p;
		for (p = list; p; p = p->next) {
			/* XXX argh, more passing of NULL's for callbacks */
			eab_merging_book_add_contact (book, E_CONTACT (p->data), NULL, NULL);
		}
	}
	if (book)
		g_object_unref (book);
	e_free_object_list (list);
}

static void
save_in_addressbook(GtkWidget *button, gpointer data)
{
	EABVCardControl *vcard_control = data;
	GList *list, *p;

	list = g_list_copy (vcard_control->card_list);

	for (p = list; p; p = p->next)
		g_object_ref (p->data);

	addressbook_load_default_book (book_open_cb, list);
}

static void
toggle_full_vcard(GtkWidget *button, gpointer data)
{
	EABVCardControl *vcard_control = data;
	char *label;

	if (!vcard_control->card_list)
		return;

	if (vcard_control->render_mode == EAB_CONTACT_DISPLAY_RENDER_NORMAL) {
		vcard_control->render_mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;
		label = _("Show Full VCard");
	}
	else {
		vcard_control->render_mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;
		label = _("Show Compact VCard");
	}

	gtk_button_set_label (GTK_BUTTON (button), label);
	eab_contact_display_render (vcard_control->display, E_CONTACT (vcard_control->card_list->data),
				    vcard_control->render_mode);
}

static void
free_struct (gpointer data, GObject *where_object_was)
{
	EABVCardControl *vcard_control = data;
	e_free_object_list (vcard_control->card_list);
	g_free (vcard_control);
}

BonoboControl *
eab_vcard_control_new (void)
{
	BonoboControl       *control;
	BonoboPersistStream *stream;
	GtkWidget	    *display;
	GtkWidget           *button1, *button2;
	GtkWidget           *bbox;
	GtkWidget           *vbox;

	EABVCardControl    *vcard_control = g_new (EABVCardControl, 1);

	printf ("inside eab_vcard_control_new\n");

	vcard_control->card_list = NULL;
	vcard_control->display = NULL;
	vcard_control->label = NULL;

	vcard_control->render_mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;

	/* Create the control. */

	display = eab_contact_display_new ();
	vcard_control->display = EAB_CONTACT_DISPLAY (display);

	bbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (bbox), 12);

	button1 = gtk_button_new_with_label(_("Show Full VCard"));
	g_signal_connect (button1, "clicked",
			  G_CALLBACK (toggle_full_vcard), vcard_control);
	gtk_box_pack_start (GTK_BOX (bbox), button1, FALSE, FALSE, 0);

	button2 = gtk_button_new_with_label(_("Save in addressbook"));
	g_signal_connect (button2, "clicked",
			  G_CALLBACK (save_in_addressbook), vcard_control);
	gtk_box_pack_start (GTK_BOX (bbox), button2, FALSE, FALSE, 0);

	/* This is intentionally not shown. */
	vcard_control->label = gtk_label_new ("");

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), display, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), vcard_control->label, TRUE, TRUE, 0);
	gtk_widget_show_all (bbox);
	gtk_widget_show (display);
	gtk_widget_show (vbox);

	control = bonobo_control_new (vbox);

	g_object_weak_ref (G_OBJECT (control), free_struct, vcard_control);

	stream = bonobo_persist_stream_new (pstream_load, pstream_save,
					    pstream_get_content_types,
					    VCARD_CONTROL_ID,
					    vcard_control);

	if (stream == NULL) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (control),
				    BONOBO_OBJECT (stream));

	return control;
}
