/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer.c
 *
 * Copyright (C) 1999  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Ettore Perazzoli
 */

/*

   TODO

   - Somehow users should be able to see if any file(s) are attached even when
     the attachment bar is not shown.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>
#include <bonobo/bonobo-stream-memory.h>
#include "e-util/e-html-utils.h"

#include <glade/glade.h>

#include <camel/camel.h>

#include "e-msg-composer.h"
#include "e-msg-composer-address-dialog.h"
#include "e-msg-composer-attachment-bar.h"
#include "e-msg-composer-hdrs.h"

#ifdef USING_OAF
#define HTML_EDITOR_CONTROL_ID "OAFIID:control:html-editor:63c5499b-8b0c-475a-9948-81ec96a9662c"
#else
#define HTML_EDITOR_CONTROL_ID "control:html-editor"
#endif



#define DEFAULT_WIDTH 600
#define DEFAULT_HEIGHT 500


enum {
	SEND,
	POSTPONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GnomeAppClass *parent_class = NULL;


static GtkWidget *
create_editor (EMsgComposer *composer)
{
	GtkWidget *control;

	control = bonobo_widget_new_control (HTML_EDITOR_CONTROL_ID,
					     bonobo_object_corba_objref (BONOBO_OBJECT (composer->uih)));
	if (control == NULL) {
		g_error ("Cannot get `%s'.", HTML_EDITOR_CONTROL_ID);
		return NULL;
	}

	return control;
}


static void
free_string_list (GList *list)
{
	GList *p;

	if (list == NULL)
		return;

	for (p = list; p != NULL; p = p->next)
		g_free (p->data);

	g_list_free (list);
}

static char *
get_editor_text (BonoboWidget *editor)
{
	Bonobo_PersistStream persist;
	BonoboStream *stream;
	BonoboStreamMem *stream_mem;
	CORBA_Environment ev;
	char *text;

	CORBA_exception_init (&ev);
	persist = (Bonobo_PersistStream)
		bonobo_object_client_query_interface (
			bonobo_widget_get_server (editor),
			"IDL:Bonobo/PersistStream:1.0",
			&ev);
	g_assert (persist != CORBA_OBJECT_NIL);

	stream = bonobo_stream_mem_create (NULL, 0, FALSE, TRUE);
	Bonobo_PersistStream_save (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)), "text/html", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME. Some error message. */
		return NULL;
	}
	if (ev._major != CORBA_SYSTEM_EXCEPTION)
		CORBA_Object_release (persist, &ev);

	Bonobo_Unknown_unref (persist, &ev);
	CORBA_exception_free (&ev);

	stream_mem = BONOBO_STREAM_MEM (stream);
	text = g_malloc (stream_mem->pos + 1);
	memcpy (text, stream_mem->buffer, stream_mem->pos);
	text[stream_mem->pos] = 0;
	bonobo_object_unref (BONOBO_OBJECT(stream));
	return text;
}


/* This functions builds a CamelMimeMessage for the message that the user has
   composed in `composer'.  */
static CamelMimeMessage *
build_message (EMsgComposer *composer)
{
	CamelMimeMessage *new;
	char *text;
	int i;
	
	new = camel_mime_message_new ();

	e_msg_composer_hdrs_to_message (E_MSG_COMPOSER_HDRS (composer->hdrs),
					new);
	for (i = 0; i < composer->extra_hdr_names->len; i++) {
		camel_medium_add_header (CAMEL_MEDIUM (new),
					 composer->extra_hdr_names->pdata[i],
					 composer->extra_hdr_values->pdata[i]);
	}

	text = get_editor_text (BONOBO_WIDGET (composer->editor));

	if (e_msg_composer_attachment_bar_get_num_attachments (E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar))) {
		CamelMultipart *multipart = camel_multipart_new ();
		CamelMimePart *part;

		/* Generate a random boundary. */
		camel_multipart_set_boundary (multipart, NULL);

		part = camel_mime_part_new ();
		camel_mime_part_set_content (part, text,
					     strlen (text), "text/html");
		camel_multipart_add_part (multipart, part);
		gtk_object_unref (GTK_OBJECT (part));

		e_msg_composer_attachment_bar_to_multipart (E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar), multipart);

		camel_medium_set_content_object (CAMEL_MEDIUM (new),
						 CAMEL_DATA_WRAPPER (multipart));
		gtk_object_unref (GTK_OBJECT (multipart));
	} else {
		CamelDataWrapper *cdw;
		CamelStream *stream;

		stream = camel_stream_mem_new_with_buffer (text,
							   strlen (text));
		cdw = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (cdw, stream);
		gtk_object_unref (GTK_OBJECT (stream));
		camel_data_wrapper_set_mime_type (cdw, "text/html");

		camel_medium_set_content_object (CAMEL_MEDIUM (new),
						 CAMEL_DATA_WRAPPER (cdw));
		gtk_object_unref (GTK_OBJECT (cdw));
	}
	g_free (text);

	/* FIXME refcounting is most certainly wrong.  We want all the stuff to
           be destroyed when we unref() the message.  */

	return new;
}

static void
set_editor_text (BonoboWidget *editor, const char *text)
{
	Bonobo_PersistStream persist;
	BonoboStream *stream;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	persist = (Bonobo_PersistStream)
		bonobo_object_client_query_interface (
			bonobo_widget_get_server (editor),
			"IDL:Bonobo/PersistStream:1.0",
			&ev);
	g_assert (persist != CORBA_OBJECT_NIL);

	stream = bonobo_stream_mem_create ((char *)text, strlen (text),
					   TRUE, FALSE);
	Bonobo_PersistStream_load (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)), "text/html", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME. Some error message. */
		return;
	}
	if (ev._major != CORBA_SYSTEM_EXCEPTION)
		CORBA_Object_release (persist, &ev);

	Bonobo_Unknown_unref (persist, &ev);
	CORBA_exception_free (&ev);
	bonobo_object_unref (BONOBO_OBJECT(stream));
}


static void
show_attachments (EMsgComposer *composer,
		  gboolean show)
{
	if (show) {
		gtk_widget_show (composer->attachment_scrolled_window);
		gtk_widget_show (composer->attachment_bar);
	} else {
		gtk_widget_hide (composer->attachment_scrolled_window);
		gtk_widget_hide (composer->attachment_bar);
	}

	composer->attachment_bar_visible = show;

	/* Update the GUI.  */

#if 0
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM
		 (glade_xml_get_widget (composer->menubar_gui,
					"menu_view_attachments")),
		 show);
#endif

	/* XXX we should update the toggle toolbar item as well.  At
	   this point, it is not a toggle because Glade is broken.  */
}


/* Address dialog callbacks.  */

static void
address_dialog_destroy_cb (GtkWidget *widget,
			   gpointer data)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (data);
	composer->address_dialog = NULL;
}

static void
address_dialog_apply_cb (EMsgComposerAddressDialog *dialog,
			 gpointer data)
{
	EMsgComposerHdrs *hdrs;
	GList *list;

	hdrs = E_MSG_COMPOSER_HDRS (E_MSG_COMPOSER (data)->hdrs);

	list = e_msg_composer_address_dialog_get_to_list (dialog);
	e_msg_composer_hdrs_set_to (hdrs, list);

	list = e_msg_composer_address_dialog_get_cc_list (dialog);
	e_msg_composer_hdrs_set_cc (hdrs, list);

	list = e_msg_composer_address_dialog_get_bcc_list (dialog);
	e_msg_composer_hdrs_set_bcc (hdrs, list);
}


/* Message composer window callbacks.  */

static void
send_cb (GtkWidget *widget,
	 gpointer data)
{
	gtk_signal_emit (GTK_OBJECT (data), signals[SEND]);
}

static void
exit_dialog_cb (int reply, gpointer data)
{
	if (reply == 0)
		gtk_widget_destroy (GTK_WIDGET (data));
}

static void
exit_cb (GtkWidget *widget, gpointer data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (data);
	GtkWindow *parent =
		GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (data),
						     GTK_TYPE_WINDOW));

	gnome_ok_cancel_dialog_parented ("Discard this message?",
					 exit_dialog_cb, composer, parent);
}
	

static void
menu_view_attachments_activate_cb (GtkWidget *widget,
				   gpointer data)
{
	e_msg_composer_show_attachments (E_MSG_COMPOSER (data),
					 GTK_CHECK_MENU_ITEM (widget)->active);
}

static void
toolbar_view_attachments_clicked_cb (GtkWidget *widget,
				      gpointer data)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (data);

	e_msg_composer_show_attachments (composer,
					 ! composer->attachment_bar_visible);
}

static void
add_attachment_cb (GtkWidget *widget,
		   gpointer data)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (data);

	e_msg_composer_attachment_bar_attach
		(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
		 NULL);
}

/* Create the address dialog if not created already.  */
static void
setup_address_dialog (EMsgComposer *composer)
{
	EMsgComposerAddressDialog *dialog;
	EMsgComposerHdrs *hdrs;
	GList *list;

	if (composer->address_dialog != NULL)
		return;

	composer->address_dialog = e_msg_composer_address_dialog_new ();
	dialog = E_MSG_COMPOSER_ADDRESS_DIALOG (composer->address_dialog);
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);

	gtk_signal_connect (GTK_OBJECT (dialog),
			    "destroy", address_dialog_destroy_cb, composer);
	gtk_signal_connect (GTK_OBJECT (dialog),
			    "apply", address_dialog_apply_cb, composer);

	list = e_msg_composer_hdrs_get_to (hdrs);
	e_msg_composer_address_dialog_set_to_list (dialog, list);

	list = e_msg_composer_hdrs_get_cc (hdrs);
	e_msg_composer_address_dialog_set_cc_list (dialog, list);

	list = e_msg_composer_hdrs_get_bcc (hdrs);
	e_msg_composer_address_dialog_set_bcc_list (dialog, list);
}

static void
address_dialog_cb (GtkWidget *widget,
		   gpointer data)
{
	EMsgComposer *composer;

	/* FIXME maybe we should hide the dialog on Cancel/OK instead of
           destroying it.  */

	composer = E_MSG_COMPOSER (data);

	setup_address_dialog (composer);

	gtk_widget_show (composer->address_dialog);
	gdk_window_show (composer->address_dialog->window);
}

static void
attachment_bar_changed_cb (EMsgComposerAttachmentBar *bar,
			   gpointer data)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);

	if (e_msg_composer_attachment_bar_get_num_attachments (bar) > 0)
		e_msg_composer_show_attachments (composer, TRUE);
	else
		e_msg_composer_show_attachments (composer, FALSE);
}


/* Menu bar implementation.  */

static GnomeUIInfo file_tree[] = {
	GNOMEUIINFO_MENU_OPEN_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_SAVE_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_SAVE_AS_ITEM (NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("Save in _folder..."), N_("Save the message in a specified folder"),
			       NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Send"), N_("Send the message"),
				send_cb, GNOME_STOCK_MENU_MAIL_SND),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_EXIT_ITEM (exit_cb, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_tree[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("View _attachments"), N_("View/hide attachments"),
				menu_view_attachments_activate_cb, GNOME_STOCK_MENU_ATTACH),
	GNOMEUIINFO_END
};

static GnomeUIInfo menubar_info[] = {
	GNOMEUIINFO_MENU_FILE_TREE (file_tree),
	GNOMEUIINFO_MENU_VIEW_TREE (view_tree),
	GNOMEUIINFO_END
};

static void
create_menubar (EMsgComposer *composer)
{
	BonoboUIHandler *uih;
	BonoboUIHandlerMenuItem *list;

	uih = composer->uih;

	bonobo_ui_handler_create_menubar (uih);

	list = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (menubar_info, composer);
	bonobo_ui_handler_menu_add_list (uih, "/", list);
}


/* Toolbar implementation.  */

static GnomeUIInfo toolbar_info[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Send"), N_("Send this message"), send_cb, GNOME_STOCK_PIXMAP_MAIL_SND),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Cut"), N_("Cut selected region into the clipboard"), NULL, GNOME_STOCK_PIXMAP_CUT),
	GNOMEUIINFO_ITEM_STOCK (N_("Copy"), N_("Copy selected region into the clipboard"), NULL, GNOME_STOCK_PIXMAP_COPY),
	GNOMEUIINFO_ITEM_STOCK (N_("Paste"), N_("Paste selected region into the clipboard"), NULL, GNOME_STOCK_PIXMAP_PASTE),
	GNOMEUIINFO_ITEM_STOCK (N_("Undo"), N_("Undo last operation"), NULL, GNOME_STOCK_PIXMAP_UNDO),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Attach"), N_("Attach a file"), add_attachment_cb, GNOME_STOCK_PIXMAP_ATTACH),
	GNOMEUIINFO_END
};

static void
create_toolbar (EMsgComposer *composer)
{
	BonoboUIHandler *uih;
	BonoboUIHandlerToolbarItem *list;

	uih = composer->uih;

	bonobo_ui_handler_create_toolbar (uih, "Toolbar");

	list = bonobo_ui_handler_toolbar_parse_uiinfo_list_with_data (toolbar_info, composer);
	bonobo_ui_handler_toolbar_add_list (uih, "/Toolbar", list);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (object);

	bonobo_object_unref (BONOBO_OBJECT (composer->uih));

	/* FIXME?  I assume the Bonobo widget will get destroyed
           normally?  */

	if (composer->address_dialog != NULL)
		gtk_widget_destroy (composer->address_dialog);
	if (composer->hdrs != NULL)
		gtk_widget_destroy (composer->hdrs);

	if (composer->extra_hdr_names) {
		int i;

		for (i = 0; i < composer->extra_hdr_names->len; i++) {
			g_free (composer->extra_hdr_names->pdata[i]);
			g_free (composer->extra_hdr_values->pdata[i]);
		}
		g_ptr_array_free (composer->extra_hdr_names, TRUE);
		g_ptr_array_free (composer->extra_hdr_values, TRUE);
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EMsgComposerClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	object_class->destroy = destroy;

	parent_class = gtk_type_class (gnome_app_get_type ());

	signals[SEND] =
		gtk_signal_new ("send",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, send),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	signals[POSTPONE] =
		gtk_signal_new ("postpone",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, postpone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposer *composer)
{
	composer->uih = NULL;

	composer->hdrs = NULL;
	composer->extra_hdr_names = g_ptr_array_new ();
	composer->extra_hdr_values = g_ptr_array_new ();

	composer->editor = NULL;

	composer->address_dialog = NULL;

	composer->attachment_bar = NULL;
	composer->attachment_scrolled_window = NULL;
}


GtkType
e_msg_composer_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposer",
			sizeof (EMsgComposer),
			sizeof (EMsgComposerClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gnome_app_get_type (), &info);
	}

	return type;
}


/**
 * e_msg_composer_construct:
 * @composer: A message composer widget
 * 
 * Construct @composer.
 **/
void
e_msg_composer_construct (EMsgComposer *composer)
{
	GtkWidget *vbox;

	g_return_if_fail (gtk_main_level () > 0);

	gtk_window_set_default_size (GTK_WINDOW (composer),
				     DEFAULT_WIDTH, DEFAULT_HEIGHT);

	gnome_app_construct (GNOME_APP (composer), "e-msg-composer",
			     "Compose a message");

	composer->uih = bonobo_ui_handler_new ();
	bonobo_ui_handler_set_app (composer->uih, GNOME_APP (composer));

	vbox = gtk_vbox_new (FALSE, 0);

	composer->hdrs = e_msg_composer_hdrs_new ();
	gtk_box_pack_start (GTK_BOX (vbox), composer->hdrs, FALSE, TRUE, 0);
	gtk_widget_show (composer->hdrs);

	/* Editor component.  */

	create_menubar (composer);
	create_toolbar (composer);
	composer->editor = create_editor (composer);

	gtk_widget_show (composer->editor);
	gtk_box_pack_start (GTK_BOX (vbox), composer->editor, TRUE, TRUE, 0);
	gtk_widget_show (composer->editor);

	/* Attachment editor, wrapped into a GtkScrolledWindow.  We don't
           show it for now.  */

	composer->attachment_scrolled_window = gtk_scrolled_window_new (NULL,
									NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW (composer->attachment_scrolled_window),
		 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	composer->attachment_bar = e_msg_composer_attachment_bar_new (NULL);
	GTK_WIDGET_SET_FLAGS (composer->attachment_bar, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (composer->attachment_scrolled_window),
			   composer->attachment_bar);
	gtk_box_pack_start (GTK_BOX (vbox),
			    composer->attachment_scrolled_window,
			    FALSE, TRUE, GNOME_PAD_SMALL);

	gtk_signal_connect (GTK_OBJECT (composer->attachment_bar), "changed",
			    GTK_SIGNAL_FUNC (attachment_bar_changed_cb), composer);

	gnome_app_set_contents (GNOME_APP (composer), vbox);
	gtk_widget_show (vbox);

	e_msg_composer_show_attachments (composer, FALSE);
}

/**
 * e_msg_composer_new:
 *
 * Create a new message composer widget.  This function must be called
 * within the GTK+ main loop, or it will fail.
 * 
 * Return value: A pointer to the newly created widget
 **/
GtkWidget *
e_msg_composer_new (void)
{
	GtkWidget *new;

	g_return_val_if_fail (gtk_main_level () > 0, NULL);
 
	new = gtk_type_new (e_msg_composer_get_type ());
	e_msg_composer_construct (E_MSG_COMPOSER (new));

	return new;
}


static GList *
add_recipients (GList *list, const char *recips, gboolean decode)
{
	int len;
	char *addr;

	while (*recips) {
		len = strcspn (recips, ",");
		if (len) {
			addr = g_strndup (recips, len);
			if (decode)
				camel_url_decode (addr);
			list = g_list_append (list, addr);
		}
		recips += len;
		if (*recips == ',')
			recips++;
	}

	return list;
}

static void
free_recipients (GList *list)
{
	GList *l;

	for (l = list; l; l = l->next)
		g_free (l->data);
	g_list_free (list);
}

/**
 * e_msg_composer_new_from_url:
 * @url: a mailto URL
 *
 * Create a new message composer widget, and fill in fields as
 * defined by the provided URL.
 **/
GtkWidget *
e_msg_composer_new_from_url (const char *url)
{
	EMsgComposer *composer;
	EMsgComposerHdrs *hdrs;
	GList *to = NULL, *cc = NULL, *bcc = NULL;
	char *subject = NULL, *body = NULL;
	const char *p, *header;
	int len, clen;
	char *content;

	g_return_val_if_fail (strncasecmp (url, "mailto:", 7) == 0, NULL);

	/* Parse recipients (everything after ':' until '?' or eos. */
	p = url + 7;
	len = strcspn (p, "?,");
	if (len) {
		content = g_strndup (p, len);
		to = add_recipients (to, content, TRUE);
		g_free (content);
	}

	p += len;
	if (*p == '?') {
		p++;

		while (*p) {
			len = strcspn (p, "=&");

			/* If it's malformed, give up. */
			if (p[len] != '=')
				break;

			header = p;
			p += len + 1;

			clen = strcspn (p, "&");
			content = g_strndup (p, clen);
			camel_url_decode (content);

			if (!strncasecmp (header, "to", len))
				to = add_recipients (to, content, FALSE);
			else if (!strncasecmp (header, "cc", len))
				cc = add_recipients (cc, content, FALSE);
			else if (!strncasecmp (header, "bcc", len))
				bcc = add_recipients (bcc, content, FALSE);
			else if (!strncasecmp (header, "subject", len))
				subject = g_strdup (content);
			else if (!strncasecmp (header, "body", len))
				body = g_strdup (content);

			g_free (content);
			p += clen;
			if (*p == '&') {
				p++;
				if (!strcmp (p, "amp;"))
					p += 4;
			}
		}
	}
		
	composer = E_MSG_COMPOSER (e_msg_composer_new ());
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	e_msg_composer_hdrs_set_to (hdrs, to);
	free_recipients (to);
	e_msg_composer_hdrs_set_cc (hdrs, cc);
	free_recipients (cc);
	e_msg_composer_hdrs_set_bcc (hdrs, bcc);
	free_recipients (bcc);
	if (subject) {
		e_msg_composer_hdrs_set_subject (hdrs, subject);
		g_free (subject);
	}

	if (body) {
		char *htmlbody = e_text_to_html (body, E_TEXT_TO_HTML_PRE);
		set_editor_text (BONOBO_WIDGET (composer->editor), htmlbody);
		g_free (htmlbody);
	}

	return GTK_WIDGET (composer);
}



/**
 * e_msg_composer_show_attachments:
 * @composer: A message composer widget
 * @show: A boolean specifying whether the attachment bar should be shown or
 * not
 * 
 * If @show is %FALSE, hide the attachment bar.  Otherwise, show it.
 **/
void
e_msg_composer_show_attachments (EMsgComposer *composer,
				 gboolean show)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	show_attachments (composer, show);
}


/**
 * e_msg_composer_set_headers:
 * @composer: a composer object
 * @to: the values for the "To" header
 * @cc: the values for the "Cc" header
 * @bcc: the values for the "Bcc" header
 * @subject: the value for the "Subject" header
 *
 * Sets the headers in the composer to the given values.
 **/
void 
e_msg_composer_set_headers (EMsgComposer *composer, const GList *to,
			    const GList *cc, const GList *bcc,
			    const char *subject)
{
	EMsgComposerHdrs *hdrs;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);

	e_msg_composer_hdrs_set_to (hdrs, to);
	e_msg_composer_hdrs_set_cc (hdrs, cc);
	e_msg_composer_hdrs_set_bcc (hdrs, bcc);
	e_msg_composer_hdrs_set_subject (hdrs, subject);
}


/**
 * e_msg_composer_set_body_text:
 * @composer: a composer object
 * @text: the HTML text to initialize the editor with
 *
 * Loads the given HTML text into the editor.
 **/
void
e_msg_composer_set_body_text (EMsgComposer *composer, const char *text)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	set_editor_text (BONOBO_WIDGET (composer->editor), text);
}


/**
 * e_msg_composer_add_header:
 * @composer: a composer object
 * @name: the header name
 * @value: the header value
 *
 * Adds a header with @name and @value to the message. This header
 * may not be displayed by the composer, but will be included in
 * the message it outputs.
 **/
void
e_msg_composer_add_header (EMsgComposer *composer, const char *name,
			   const char *value)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	g_ptr_array_add (composer->extra_hdr_names, g_strdup (name));
	g_ptr_array_add (composer->extra_hdr_values, g_strdup (value));
}


/**
 * e_msg_composer_get_message:
 * @composer: A message composer widget
 * 
 * Retrieve the message edited by the user as a CamelMimeMessage.  The
 * CamelMimeMessage object is created on the fly; subsequent calls to this
 * function will always create new objects from scratch.
 * 
 * Return value: A pointer to the new CamelMimeMessage object
 **/
CamelMimeMessage *
e_msg_composer_get_message (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return build_message (composer);
}
