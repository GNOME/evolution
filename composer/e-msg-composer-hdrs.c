/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* msg-composer-hdrs.c
 *
 * Copyright (C) 1999 Helix Code, Inc.
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

#ifdef _HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <camel/camel.h>

#include "e-msg-composer-address-entry.h"
#include "e-msg-composer-hdrs.h"


struct _EMsgComposerHdrsPrivate {
	/* Total number of headers that we have.  */
	guint num_hdrs;

	/* The tooltips.  */
	GtkTooltips *tooltips;

	/* Standard headers.  */
	GtkWidget *to_entry;
	GtkWidget *cc_entry;
	GtkWidget *bcc_entry;
	GtkWidget *subject_entry;
};


static GtkTableClass *parent_class = NULL;

enum {
	SHOW_ADDRESS_DIALOG,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];


static void
address_button_clicked_cb (GtkButton *button,
			   gpointer data)
{
	gtk_signal_emit (GTK_OBJECT (data), signals[SHOW_ADDRESS_DIALOG]);
}

static GtkWidget *
add_header (EMsgComposerHdrs *hdrs,
	    const gchar *name,
	    const gchar *tip,
	    const gchar *tip_private,
	    gboolean addrbook_button)
{
	EMsgComposerHdrsPrivate *priv;
	GtkWidget *label;
	GtkWidget *entry;
	guint pad;

	priv = hdrs->priv;

	if (addrbook_button) {
		label = gtk_button_new_with_label (name);
		gtk_signal_connect (GTK_OBJECT (label), "clicked",
				    GTK_SIGNAL_FUNC (address_button_clicked_cb),
				    hdrs);
		pad = 2;
		gtk_tooltips_set_tip (hdrs->priv->tooltips, label,
				      _("Click here for the address book"),
				      NULL);
	} else {
		label = gtk_label_new (name);
		pad = GNOME_PAD;
	}

	gtk_table_attach (GTK_TABLE (hdrs), label,
			  0, 1, priv->num_hdrs, priv->num_hdrs + 1,
			  GTK_FILL, GTK_FILL,
			  pad, pad);
	gtk_widget_show (label);

	entry = e_msg_composer_address_entry_new ();
	gtk_table_attach (GTK_TABLE (hdrs), entry,
			  1, 2, priv->num_hdrs, priv->num_hdrs + 1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  2, 2);
	gtk_widget_show (entry);

	gtk_tooltips_set_tip (hdrs->priv->tooltips, entry, tip, tip_private);

	priv->num_hdrs++;

	return entry;
}

static void
setup_headers (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;

	priv = hdrs->priv;

	priv->to_entry = add_header
		(hdrs, _("To:"), 
		 _("Enter the recipients of the message"),
		 NULL,
		 TRUE);
	priv->cc_entry = add_header
		(hdrs, _("Cc:"),
		 _("Enter the addresses that will receive a carbon copy of "
		   "the message"),
		 NULL,
		 TRUE);
	priv->bcc_entry = add_header
		(hdrs, _("Bcc:"),
		 _("Enter the addresses that will receive a carbon copy of "
		   "the message without appearing in the recipient list of "
		   "the message."),
		 NULL,
		 TRUE);
	priv->subject_entry = add_header
		(hdrs, _("Subject:"),
		 _("Enter the subject of the mail"),
		 NULL,
		 FALSE);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposerHdrs *hdrs;
	EMsgComposerHdrsPrivate *priv;

	hdrs = E_MSG_COMPOSER_HDRS (object);
	priv = hdrs->priv;

	gtk_object_destroy (GTK_OBJECT (priv->tooltips));

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EMsgComposerHdrsClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_table_get_type ());

	signals[SHOW_ADDRESS_DIALOG] =
		gtk_signal_new ("show_address_dialog",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerHdrsClass,
						   show_address_dialog),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;

	priv = g_new (EMsgComposerHdrsPrivate, 1);

	priv->to_entry = NULL;
	priv->cc_entry = NULL;
	priv->bcc_entry = NULL;
	priv->subject_entry = NULL;

	priv->tooltips = gtk_tooltips_new ();

	priv->num_hdrs = 0;

	hdrs->priv = priv;
}


GtkType
e_msg_composer_hdrs_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposerHdrs",
			sizeof (EMsgComposerHdrs),
			sizeof (EMsgComposerHdrsClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gtk_table_get_type (), &info);
	}

	return type;
}

GtkWidget *
e_msg_composer_hdrs_new (void)
{
	EMsgComposerHdrs *new;

	new = gtk_type_new (e_msg_composer_hdrs_get_type ());

	setup_headers (E_MSG_COMPOSER_HDRS (new));

	return GTK_WIDGET (new);
}


static void
set_recipients (CamelMimeMessage *msg,
		GtkWidget *entry_widget,
		const gchar *type)
{
	EMsgComposerAddressEntry *entry;
	GList *list;
	GList *p;

	entry = E_MSG_COMPOSER_ADDRESS_ENTRY (entry_widget);
	list = e_msg_composer_address_entry_get_addresses (entry);

	/* FIXME leak?  */

	for (p = list; p != NULL; p = p->next) {
		printf ("Adding `%s:' header: %s\n", type, (gchar *) p->data);
		camel_mime_message_add_recipient (msg, type, (gchar *) p->data);
	}

	g_list_free (list);
}

void
e_msg_composer_hdrs_to_message (EMsgComposerHdrs *hdrs,
				CamelMimeMessage *msg)
{
	const gchar *s;

	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (msg != NULL);
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));

	s = gtk_entry_get_text (GTK_ENTRY (hdrs->priv->subject_entry));
	camel_mime_message_set_subject (msg, g_strdup (s));

	set_recipients (msg, hdrs->priv->to_entry, CAMEL_RECIPIENT_TYPE_TO);
	set_recipients (msg, hdrs->priv->cc_entry, CAMEL_RECIPIENT_TYPE_CC);
	set_recipients (msg, hdrs->priv->bcc_entry, CAMEL_RECIPIENT_TYPE_BCC);
}


void
e_msg_composer_hdrs_set_to (EMsgComposerHdrs *hdrs,
			    GList *to_list)
{
	EMsgComposerAddressEntry *entry;

	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	entry = E_MSG_COMPOSER_ADDRESS_ENTRY (hdrs->priv->to_entry);
	e_msg_composer_address_entry_set_list (entry, to_list);
}

void
e_msg_composer_hdrs_set_cc (EMsgComposerHdrs *hdrs,
			    GList *cc_list)
{
	EMsgComposerAddressEntry *entry;

	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	entry = E_MSG_COMPOSER_ADDRESS_ENTRY (hdrs->priv->cc_entry);
	e_msg_composer_address_entry_set_list (entry, cc_list);
}

void
e_msg_composer_hdrs_set_bcc (EMsgComposerHdrs *hdrs,
			     GList *bcc_list)
{
	EMsgComposerAddressEntry *entry;

	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	entry = E_MSG_COMPOSER_ADDRESS_ENTRY (hdrs->priv->bcc_entry);
	e_msg_composer_address_entry_set_list (entry, bcc_list);
}


GList *
e_msg_composer_hdrs_get_to (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return e_msg_composer_address_entry_get_addresses
		(E_MSG_COMPOSER_ADDRESS_ENTRY (hdrs->priv->to_entry));
}

GList *
e_msg_composer_hdrs_get_cc (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return e_msg_composer_address_entry_get_addresses
		(E_MSG_COMPOSER_ADDRESS_ENTRY (hdrs->priv->cc_entry));
}

GList *
e_msg_composer_hdrs_get_bcc (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return e_msg_composer_address_entry_get_addresses
		(E_MSG_COMPOSER_ADDRESS_ENTRY (hdrs->priv->bcc_entry));
}

