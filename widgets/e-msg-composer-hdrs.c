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

	/* Standard headers.  */
	GtkWidget *to_entry;
	GtkWidget *cc_entry;
	GtkWidget *bcc_entry;
	GtkWidget *subject_entry;
};


static GtkTableClass *parent_class = NULL;


static GtkWidget *
add_address_header (EMsgComposerHdrs *hdrs,
		    const gchar *name)
{
	EMsgComposerHdrsPrivate *priv;
	GtkWidget *label;
	GtkWidget *entry;

	priv = hdrs->priv;

	label = gtk_label_new (name);
	gtk_table_attach (GTK_TABLE (hdrs), label,
			  0, 1, priv->num_hdrs, priv->num_hdrs + 1,
			  GTK_FILL, GTK_FILL,
			  GNOME_PAD, GNOME_PAD);
	gtk_widget_show (label);

	entry = e_msg_composer_address_entry_new ();
	gtk_table_attach (GTK_TABLE (hdrs), entry,
			  1, 2, priv->num_hdrs, priv->num_hdrs + 1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  GNOME_PAD, GNOME_PAD);
	gtk_widget_show (entry);

	priv->num_hdrs++;

	return entry;
}

static void
setup_headers (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;

	priv = hdrs->priv;

	priv->subject_entry = add_address_header (hdrs, _("Subject:"));
	priv->to_entry = add_address_header (hdrs, _("To:"));
	priv->cc_entry = add_address_header (hdrs, _("Cc:"));
	priv->bcc_entry = add_address_header (hdrs, _("Bcc:"));
}


static void
class_init (EMsgComposerHdrsClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gtk_table_get_type ());
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
	g_return_if_fail (IS_CAMEL_MIME_MESSAGE (msg));

	s = gtk_entry_get_text (GTK_ENTRY (hdrs->priv->subject_entry));
	camel_mime_message_set_subject (msg, g_strdup (s));

	set_recipients (msg, hdrs->priv->to_entry, RECIPIENT_TYPE_TO);
	set_recipients (msg, hdrs->priv->cc_entry, RECIPIENT_TYPE_CC);
	set_recipients (msg, hdrs->priv->bcc_entry, RECIPIENT_TYPE_BCC);
}
