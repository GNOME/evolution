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

#ifdef _HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <glade/glade.h>
#include <camel/camel.h>

#include "e-msg-composer.h"
#include "e-msg-composer-address-dialog.h"
#include "e-msg-composer-attachment-bar.h"
#include "e-msg-composer-hdrs.h"


#define DEFAULT_WIDTH 600
#define DEFAULT_HEIGHT 500


enum {
	SEND,
	POSTPONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GnomeAppClass *parent_class = NULL;


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

/* This functions builds a CamelMimeMessage for the message that the user has
   composed in `composer'.  */
static CamelMimeMessage *
build_message (EMsgComposer *composer)
{
	CamelMimeMessage *new;
	CamelMimeBodyPart *body_part;
	CamelMultipart *multipart;
	gchar *text;

	new = camel_mime_message_new_with_session (NULL);

	e_msg_composer_hdrs_to_message (E_MSG_COMPOSER_HDRS (composer->hdrs),
					new);

	multipart = camel_multipart_new ();
	body_part = camel_mime_body_part_new ();

	text = gtk_editable_get_chars (GTK_EDITABLE (composer->text), 0, -1);
	camel_mime_part_set_text (CAMEL_MIME_PART (body_part), text);
	camel_multipart_add_part (multipart, body_part);

	e_msg_composer_attachment_bar_to_multipart
		(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
		 multipart);

	camel_medium_set_content_object (CAMEL_MEDIUM (new),
					 CAMEL_DATA_WRAPPER (multipart));

	/* FIXME refcounting is most certainly wrong.  We want all the stuff to
           be destroyed when we unref() the message.  */

	return new;
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
attachment_bar_changed (EMsgComposerAttachmentBar *bar,
			gpointer data)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);

	if (e_msg_composer_attachment_bar_get_num_attachments (bar) > 0)
		e_msg_composer_show_attachments (composer, TRUE);
	else
		e_msg_composer_show_attachments (composer, FALSE);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (object);

	if (composer->address_dialog != NULL)
		gtk_widget_destroy (composer->address_dialog);

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
	composer->hdrs = NULL;

	composer->text = NULL;
	composer->text_scrolled_window = NULL;

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

	gtk_window_set_default_size (GTK_WINDOW (composer),
				     DEFAULT_WIDTH, DEFAULT_HEIGHT);

	gnome_app_construct (GNOME_APP (composer), "e-msg-composer",
			     "Compose a message");

	vbox = gtk_vbox_new (FALSE, 0);

	composer->hdrs = e_msg_composer_hdrs_new ();
	gtk_box_pack_start (GTK_BOX (vbox), composer->hdrs, FALSE, TRUE, 0);
	gtk_widget_show (composer->hdrs);

	/* GtkText for message body editing, wrapped into a
	   GtkScrolledWindow.  */

	composer->text_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW (composer->text_scrolled_window),
		 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	composer->text = gtk_text_new (NULL, NULL);
	gtk_text_set_word_wrap (GTK_TEXT (composer->text), FALSE);
	gtk_text_set_editable (GTK_TEXT (composer->text), TRUE);
	gtk_container_add (GTK_CONTAINER (composer->text_scrolled_window),
			   composer->text);
	gtk_widget_show (composer->text);
	gtk_box_pack_start (GTK_BOX (vbox), composer->text_scrolled_window,
			    TRUE, TRUE, 0);
	gtk_widget_show (composer->text_scrolled_window);

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

	gnome_app_set_contents (GNOME_APP (composer), vbox);
	gtk_widget_show (vbox);

	e_msg_composer_show_attachments (composer, FALSE);
}

/**
 * e_msg_composer_new:
 *
 * Create a new message composer widget.
 * 
 * Return value: A pointer to the newly created widget
 **/
GtkWidget *
e_msg_composer_new (void)
{
	GtkWidget *new;
 
	new = gtk_type_new (e_msg_composer_get_type ());
	e_msg_composer_construct (E_MSG_COMPOSER (new));

	return new;
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
