/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* msg-composer-hdrs.c
 *
 * Copyright (C) 1999 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>

#include <glib/gi18n.h>
#include <libedataserverui/e-name-selector.h>

#include "Composer.h"

#include <camel/camel.h>
#include <camel/camel-store.h>
#include <camel/camel-session.h>
#include "e-msg-composer-hdrs.h"
#include "mail/mail-config.h"
#include "mail/mail-session.h"
#include "e-account-combo-box.h"
#include "e-signature-combo-box.h"

#include "e-composer-header.h"
#include "e-composer-from-header.h"
#include "e-composer-name-header.h"
#include "e-composer-post-header.h"
#include "e-composer-text-header.h"

#define E_MSG_COMPOSER_HDRS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MSG_COMPOSER_HDRS, EMsgComposerHdrsPrivate))

/* Headers, listed in the order that they should appear in the table. */
enum {
	HEADER_FROM,
	HEADER_REPLY_TO,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC,
	HEADER_POST_TO,
	HEADER_SUBJECT,
	NUM_HEADERS
};

enum {
	SUBJECT_CHANGED,
	HDRS_CHANGED,
	FROM_CHANGED,
	SIGNATURE_CHANGED,
	LAST_SIGNAL
};

struct _EMsgComposerHdrsPrivate {
	ENameSelector *name_selector;

	/* ui component */
	BonoboUIComponent *uic;

	EComposerHeader *headers[NUM_HEADERS];
	GtkWidget *signature_combo_box;
};

static gpointer parent_class;
static guint signal_ids[LAST_SIGNAL];

static void
hdrs_changed (EMsgComposerHdrs *hdrs)
{
	g_signal_emit (hdrs, signal_ids[HDRS_CHANGED], 0);
}

static void
from_changed (EComposerFromHeader *from_header, EMsgComposerHdrs *hdrs)
{
	EComposerHeader *header;
	EAccount *account;

	account = e_composer_from_header_get_active (from_header);

	header = hdrs->priv->headers[HEADER_POST_TO];
	e_composer_post_header_set_account (
		E_COMPOSER_POST_HEADER (header), account);

	/* we do this rather than calling e_msg_composer_hdrs_set_reply_to()
	   because we don't want to change the visibility of the header */
	header = hdrs->priv->headers[HEADER_REPLY_TO];
	e_composer_text_header_set_text (
		E_COMPOSER_TEXT_HEADER (header), account->id->reply_to);

	g_signal_emit (hdrs, signal_ids[FROM_CHANGED], 0);
	hdrs_changed (hdrs);
}

static void
signature_changed (EMsgComposerHdrs *hdrs)
{
	g_signal_emit (hdrs, signal_ids[SIGNATURE_CHANGED], 0);
}

static void
subject_changed (EMsgComposerHdrs *hdrs)
{
	g_signal_emit (hdrs, signal_ids[SUBJECT_CHANGED], 0,
		       e_msg_composer_hdrs_get_subject (hdrs));
	hdrs_changed (hdrs);
}

static void
headers_set_visibility (EMsgComposerHdrs *h, int visible_flags)
{
	EMsgComposerHdrsPrivate *p = h->priv;

	/* To is always visible if we're not doing Post-To */
	if (!(h->visible_mask & E_MSG_COMPOSER_VISIBLE_POSTTO))
		visible_flags |= E_MSG_COMPOSER_VISIBLE_TO;
	else
		visible_flags |= E_MSG_COMPOSER_VISIBLE_POSTTO;

	e_composer_header_set_visible (
		p->headers[HEADER_FROM],
		visible_flags & E_MSG_COMPOSER_VISIBLE_FROM);
	e_composer_header_set_visible (
		p->headers[HEADER_REPLY_TO],
		visible_flags & E_MSG_COMPOSER_VISIBLE_REPLYTO);
	e_composer_header_set_visible (
		p->headers[HEADER_TO],
		visible_flags & E_MSG_COMPOSER_VISIBLE_TO);
	e_composer_header_set_visible (
		p->headers[HEADER_CC],
		visible_flags & E_MSG_COMPOSER_VISIBLE_CC);
	e_composer_header_set_visible (
		p->headers[HEADER_BCC],
		visible_flags & E_MSG_COMPOSER_VISIBLE_BCC);
	e_composer_header_set_visible (
		p->headers[HEADER_POST_TO],
		visible_flags & E_MSG_COMPOSER_VISIBLE_POSTTO);
	e_composer_header_set_visible (
		p->headers[HEADER_SUBJECT],
		visible_flags & E_MSG_COMPOSER_VISIBLE_SUBJECT);
}

static void
headers_set_sensitivity (EMsgComposerHdrs *h)
{
	/* these ones are always on */
	bonobo_ui_component_set_prop (
		h->priv->uic, "/commands/ViewTo", "sensitive",
		h->visible_mask & E_MSG_COMPOSER_VISIBLE_TO ? "0" : "1", NULL);

	bonobo_ui_component_set_prop (
		h->priv->uic, "/commands/ViewPostTo", "sensitive",
		h->visible_mask & E_MSG_COMPOSER_VISIBLE_POSTTO ? "0" : "1", NULL);
}

void
e_msg_composer_hdrs_set_visible_mask (EMsgComposerHdrs *hdrs, int visible_mask)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	hdrs->visible_mask = visible_mask;
	headers_set_sensitivity (hdrs);
}

void
e_msg_composer_hdrs_set_visible (EMsgComposerHdrs *hdrs, int visible_flags)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	headers_set_visibility (hdrs, visible_flags);
	gtk_widget_queue_resize (GTK_WIDGET (hdrs));
}

static GObject *
msg_composer_hdrs_constructor (GType type,
                               guint n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
	GObject *object;
	EMsgComposerHdrsPrivate *priv;
	GtkWidget *widget;
	guint rows, ii;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	priv = E_MSG_COMPOSER_HDRS_GET_PRIVATE (object);

	rows = G_N_ELEMENTS (priv->headers);
	gtk_table_resize (GTK_TABLE (object), rows, 4);
	gtk_table_set_row_spacings (GTK_TABLE (object), 0);
	gtk_table_set_col_spacings (GTK_TABLE (object), 6);

	/* Use "ypadding" instead of "row-spacing" because some rows may
	 * be invisible and we don't want spacing around them. */

	for (ii = 0; ii < rows; ii++) {
		gtk_table_attach (
			GTK_TABLE (object), priv->headers[ii]->title_widget,
			0, 1, ii, ii + 1, GTK_FILL, GTK_FILL, 0, 3);
		gtk_table_attach (
			GTK_TABLE (object), priv->headers[ii]->input_widget,
			1, 4, ii, ii + 1, GTK_FILL | GTK_EXPAND, 0, 0, 3);
	}

	/* Leave room in the "From" row for signature stuff. */
	gtk_container_child_set (
		GTK_CONTAINER (object),
		priv->headers[HEADER_FROM]->input_widget,
		"right-attach", 2, NULL);

	/* Now add the signature stuff. */
	widget = gtk_label_new_with_mnemonic (_("Si_gnature:"));
	gtk_table_attach (
		GTK_TABLE (object), widget,
		2, 3, HEADER_FROM, HEADER_FROM + 1, 0, 0, 0, 3);
	gtk_table_attach (
		GTK_TABLE (object), priv->signature_combo_box,
		3, 4, HEADER_FROM, HEADER_FROM + 1, 0, 0, 0, 3);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), priv->signature_combo_box);
	gtk_widget_show (widget);

	return object;
}

static void
msg_composer_hdrs_dispose (GObject *object)
{
	EMsgComposerHdrsPrivate *priv;
	gint ii;

	priv = E_MSG_COMPOSER_HDRS_GET_PRIVATE (object);

	if (priv->name_selector != NULL) {
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	for (ii = 0; ii < G_N_ELEMENTS (priv->headers); ii++) {
		if (priv->headers[ii] != NULL) {
			g_object_unref (priv->headers[ii]);
			priv->headers[ii] = NULL;
		}
	}

	if (priv->signature_combo_box != NULL) {
		g_object_unref (priv->signature_combo_box);
		priv->signature_combo_box = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
msg_composer_hdrs_class_init (EMsgComposerHdrsClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMsgComposerHdrsPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = msg_composer_hdrs_constructor;
	object_class->dispose = msg_composer_hdrs_dispose;

	signal_ids[SUBJECT_CHANGED] =
		g_signal_new ("subject_changed",
			      E_TYPE_MSG_COMPOSER_HDRS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(EMsgComposerHdrsClass, subject_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	signal_ids[HDRS_CHANGED] =
		g_signal_new ("hdrs_changed",
			      E_TYPE_MSG_COMPOSER_HDRS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(EMsgComposerHdrsClass, hdrs_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signal_ids[FROM_CHANGED] =
		g_signal_new ("from_changed",
			      E_TYPE_MSG_COMPOSER_HDRS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(EMsgComposerHdrsClass, from_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signal_ids[SIGNATURE_CHANGED] =
		g_signal_new ("signature_changed",
			      E_TYPE_MSG_COMPOSER_HDRS,
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
msg_composer_hdrs_init (EMsgComposerHdrs *hdrs)
{
	EComposerHeader *header;
	ENameSelector *name_selector;
	GtkWidget *widget;

	hdrs->priv = E_MSG_COMPOSER_HDRS_GET_PRIVATE (hdrs);

	name_selector = e_name_selector_new ();

	hdrs->priv->name_selector = name_selector;

	header = e_composer_from_header_new (_("Fr_om:"));
	g_signal_connect (
		header, "changed",
		G_CALLBACK (from_changed), hdrs);
	hdrs->priv->headers[HEADER_FROM] = header;

	header = e_composer_text_header_new_label (_("_Reply-To:"));
	g_signal_connect_swapped (
		header, "changed",
		G_CALLBACK (hdrs_changed), hdrs);
	hdrs->priv->headers[HEADER_REPLY_TO] = header;

	header = e_composer_name_header_new (_("_To:"), name_selector);
	e_composer_header_set_input_tooltip (
		header, _("Enter the recipients of the message"));
	g_signal_connect_swapped (
		header, "changed",
		G_CALLBACK (hdrs_changed), hdrs);
	hdrs->priv->headers[HEADER_TO] = header;

	header = e_composer_name_header_new (_("_Cc:"), name_selector);
	e_composer_header_set_input_tooltip (
		header, _("Enter the addresses that will receive a "
		"carbon copy of the message"));
	g_signal_connect_swapped (
		header, "changed",
		G_CALLBACK (hdrs_changed), hdrs);
	hdrs->priv->headers[HEADER_CC] = header;

	header = e_composer_name_header_new (_("_Bcc:"), name_selector);
	e_composer_header_set_input_tooltip (
		header, _("Enter the addresses that will receive a "
		"carbon copy of the message without appearing in the "
		"recipient list of the message"));
	g_signal_connect_swapped (
		header, "changed",
		G_CALLBACK (hdrs_changed), hdrs);
	hdrs->priv->headers[HEADER_BCC] = header;

	header = e_composer_post_header_new (_("_Post To:"));
	g_signal_connect_swapped (
		header, "changed",
		G_CALLBACK (hdrs_changed), hdrs);
	hdrs->priv->headers[HEADER_POST_TO] = header;

	header = e_composer_text_header_new_label (_("S_ubject:"));
	g_signal_connect_swapped (
		header, "changed",
		G_CALLBACK (subject_changed), hdrs);
	hdrs->priv->headers[HEADER_SUBJECT] = header;

	/* Do this after all the headers are initialized. */
	e_composer_from_header_set_account_list (
		E_COMPOSER_FROM_HEADER (hdrs->priv->headers[HEADER_FROM]),
		mail_config_get_accounts ());

	widget = e_signature_combo_box_new ();
	e_signature_combo_box_set_signature_list (
		E_SIGNATURE_COMBO_BOX (widget),
		mail_config_get_signatures ());
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (signature_changed), hdrs);
	g_signal_connect_swapped (
		widget, "refreshed",
		G_CALLBACK (signature_changed), hdrs);
	hdrs->priv->signature_combo_box = g_object_ref_sink (widget);
	gtk_widget_show (widget);
}

GType
e_msg_composer_hdrs_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMsgComposerHdrsClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) msg_composer_hdrs_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMsgComposerHdrs),
			0,     /* n_preallocs */
			(GInstanceInitFunc) msg_composer_hdrs_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_TABLE, "EMsgComposerHdrs", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_msg_composer_hdrs_new (BonoboUIComponent *uic, int visible_mask, int visible_flags)
{
	EMsgComposerHdrs *new;
	EMsgComposerHdrsPrivate *priv;

	new = g_object_new (E_TYPE_MSG_COMPOSER_HDRS, NULL);
	priv = new->priv;
	priv->uic = uic;

	g_object_ref_sink (new);

	new->visible_mask = visible_mask;

	headers_set_sensitivity (new);
	headers_set_visibility (new, visible_flags);

	return GTK_WIDGET (new);
}

static void
set_recipients_from_destv (CamelMimeMessage *msg,
			   EDestination **to_destv,
			   EDestination **cc_destv,
			   EDestination **bcc_destv,
			   gboolean redirect)
{
	CamelInternetAddress *to_addr;
	CamelInternetAddress *cc_addr;
	CamelInternetAddress *bcc_addr;
	CamelInternetAddress *target;
	const char *text_addr, *header;
	gboolean seen_hidden_list = FALSE;
	int i;

	to_addr  = camel_internet_address_new ();
	cc_addr  = camel_internet_address_new ();
	bcc_addr = camel_internet_address_new ();

	if (to_destv) {
		for (i = 0; to_destv[i] != NULL; ++i) {
			text_addr = e_destination_get_address (to_destv[i]);

			if (text_addr && *text_addr) {
				target = to_addr;
				if (e_destination_is_evolution_list (to_destv[i])
				    && !e_destination_list_show_addresses (to_destv[i])) {
					target = bcc_addr;
					seen_hidden_list = TRUE;
				}

				camel_address_decode (CAMEL_ADDRESS (target), text_addr);
			}
		}
	}

	if (cc_destv) {
		for (i = 0; cc_destv[i] != NULL; ++i) {
			text_addr = e_destination_get_address (cc_destv[i]);
			if (text_addr && *text_addr) {
				target = cc_addr;
				if (e_destination_is_evolution_list (cc_destv[i])
				    && !e_destination_list_show_addresses (cc_destv[i])) {
					target = bcc_addr;
					seen_hidden_list = TRUE;
				}

				camel_address_decode (CAMEL_ADDRESS (target), text_addr);
			}
		}
	}

	if (bcc_destv) {
		for (i = 0; bcc_destv[i] != NULL; ++i) {
			text_addr = e_destination_get_address (bcc_destv[i]);
			if (text_addr && *text_addr) {
				camel_address_decode (CAMEL_ADDRESS (bcc_addr), text_addr);
			}
		}
	}

	header = redirect ? CAMEL_RECIPIENT_TYPE_RESENT_TO : CAMEL_RECIPIENT_TYPE_TO;
	if (camel_address_length (CAMEL_ADDRESS (to_addr)) > 0) {
		camel_mime_message_set_recipients (msg, header, to_addr);
	} else if (seen_hidden_list) {
		camel_medium_set_header (CAMEL_MEDIUM (msg), header, "Undisclosed-Recipient:;");
	}

	header = redirect ? CAMEL_RECIPIENT_TYPE_RESENT_CC : CAMEL_RECIPIENT_TYPE_CC;
	if (camel_address_length (CAMEL_ADDRESS (cc_addr)) > 0) {
		camel_mime_message_set_recipients (msg, header, cc_addr);
	}

	header = redirect ? CAMEL_RECIPIENT_TYPE_RESENT_BCC : CAMEL_RECIPIENT_TYPE_BCC;
	if (camel_address_length (CAMEL_ADDRESS (bcc_addr)) > 0) {
		camel_mime_message_set_recipients (msg, header, bcc_addr);
	}

	camel_object_unref (to_addr);
	camel_object_unref (cc_addr);
	camel_object_unref (bcc_addr);
}

static void
e_msg_composer_hdrs_to_message_internal (EMsgComposerHdrs *hdrs,
					 CamelMimeMessage *msg,
					 gboolean redirect)
{
	EDestination **to_destv, **cc_destv, **bcc_destv;
	CamelInternetAddress *addr;
	const char *subject;
	gboolean visible;
	char *header;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));

	subject = e_msg_composer_hdrs_get_subject (hdrs);
	camel_mime_message_set_subject (msg, subject);

	addr = e_msg_composer_hdrs_get_from (hdrs);
	if (redirect) {
		header = camel_address_encode (CAMEL_ADDRESS (addr));
		camel_medium_set_header (CAMEL_MEDIUM (msg), "Resent-From", header);
		g_free (header);
	} else {
		camel_mime_message_set_from (msg, addr);
	}
	camel_object_unref (addr);

 	addr = e_msg_composer_hdrs_get_reply_to (hdrs);
	if (addr) {
		camel_mime_message_set_reply_to (msg, addr);
		camel_object_unref (addr);
	}

	visible =
		e_composer_header_get_visible (
			hdrs->priv->headers[HEADER_TO]) ||
		e_composer_header_get_visible (
			hdrs->priv->headers[HEADER_CC]) ||
		e_composer_header_get_visible (
			hdrs->priv->headers[HEADER_BCC]);

	if (visible) {
		to_destv  = e_msg_composer_hdrs_get_to (hdrs);
		cc_destv  = e_msg_composer_hdrs_get_cc (hdrs);
		bcc_destv = e_msg_composer_hdrs_get_bcc (hdrs);

		/* Attach destinations to the message. */

		set_recipients_from_destv (msg, to_destv, cc_destv, bcc_destv, redirect);

		e_destination_freev (to_destv);
		e_destination_freev (cc_destv);
		e_destination_freev (bcc_destv);
	}

	visible = e_composer_header_get_visible (
		hdrs->priv->headers[HEADER_POST_TO]);

	if (visible) {
		GList *post, *l;

		camel_medium_remove_header((CamelMedium *)msg, "X-Evolution-PostTo");
		post = e_msg_composer_hdrs_get_post_to(hdrs);
		for (l=post;l;l=g_list_next(l)) {
			camel_medium_add_header((CamelMedium *)msg, "X-Evolution-PostTo", l->data);
			g_free(l->data);
		}
		g_list_free(post);
	}
}


void
e_msg_composer_hdrs_to_message (EMsgComposerHdrs *hdrs,
				CamelMimeMessage *msg)
{
	e_msg_composer_hdrs_to_message_internal (hdrs, msg, FALSE);
}


void
e_msg_composer_hdrs_to_redirect (EMsgComposerHdrs *hdrs,
				 CamelMimeMessage *msg)
{
	e_msg_composer_hdrs_to_message_internal (hdrs, msg, TRUE);
}

EAccount *
e_msg_composer_hdrs_get_from_account (EMsgComposerHdrs *hdrs)
{
	EComposerFromHeader *header;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	header = E_COMPOSER_FROM_HEADER (hdrs->priv->headers[HEADER_FROM]);
	return e_composer_from_header_get_active (header);
}

gboolean
e_msg_composer_hdrs_set_from_account (EMsgComposerHdrs *hdrs,
				      const gchar *account_name)
{
	EComposerFromHeader *header;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), FALSE);

	header = E_COMPOSER_FROM_HEADER (hdrs->priv->headers[HEADER_FROM]);
	return e_composer_from_header_set_active_name (header, account_name);
}

ESignature *
e_msg_composer_hdrs_get_signature (EMsgComposerHdrs *hdrs)
{
	ESignatureComboBox *combo_box;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	combo_box = E_SIGNATURE_COMBO_BOX (hdrs->priv->signature_combo_box);
	return e_signature_combo_box_get_active (combo_box);
}

gboolean
e_msg_composer_hdrs_set_signature (EMsgComposerHdrs *hdrs,
                                   ESignature *signature)
{
	ESignatureComboBox *combo_box;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), FALSE);

	combo_box = E_SIGNATURE_COMBO_BOX (hdrs->priv->signature_combo_box);
	return e_signature_combo_box_set_active (combo_box, signature);
}

void
e_msg_composer_hdrs_set_reply_to (EMsgComposerHdrs *hdrs,
				  const gchar *text)
{
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	header = hdrs->priv->headers[HEADER_REPLY_TO];

	e_composer_text_header_set_text (
		E_COMPOSER_TEXT_HEADER (header), text);

	if (*text != '\0')
		e_composer_header_set_visible (header, TRUE);
}

void
e_msg_composer_hdrs_set_to (EMsgComposerHdrs *hdrs,
			    EDestination **to_destv)
{
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	header = hdrs->priv->headers[HEADER_TO];

	e_composer_name_header_set_destinations (
		E_COMPOSER_NAME_HEADER (header), to_destv);
}

void
e_msg_composer_hdrs_set_cc (EMsgComposerHdrs *hdrs,
			    EDestination **cc_destv)
{
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	header = hdrs->priv->headers[HEADER_CC];

	e_composer_name_header_set_destinations (
		E_COMPOSER_NAME_HEADER (header), cc_destv);

	if (cc_destv != NULL && *cc_destv != NULL)
		e_composer_header_set_visible (header, TRUE);
}

void
e_msg_composer_hdrs_set_bcc (EMsgComposerHdrs *hdrs,
			     EDestination **bcc_destv)
{
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	header = hdrs->priv->headers[HEADER_BCC];

	e_composer_name_header_set_destinations (
		E_COMPOSER_NAME_HEADER (header), bcc_destv);

	if (bcc_destv != NULL && *bcc_destv != NULL)
		e_composer_header_set_visible (header, TRUE);
}

void
e_msg_composer_hdrs_set_post_to (EMsgComposerHdrs *hdrs,
				 const char *post_to)
{
	GList *list;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (post_to != NULL);

	list = g_list_append (NULL, g_strdup (post_to));

	e_msg_composer_hdrs_set_post_to_list (hdrs, list);

	g_free (list->data);
	g_list_free (list);
}

void
e_msg_composer_hdrs_set_post_to_list (EMsgComposerHdrs *hdrs, GList *urls)
{
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	header = hdrs->priv->headers[HEADER_POST_TO];

	e_composer_post_header_set_folders (
		E_COMPOSER_POST_HEADER (header), urls);
}

void
e_msg_composer_hdrs_set_post_to_base (EMsgComposerHdrs *hdrs,
                                      const gchar *base,
                                      const gchar *post_to)
{
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	header = hdrs->priv->headers[HEADER_POST_TO];

	e_composer_post_header_set_folders_base (
		E_COMPOSER_POST_HEADER (header), base, post_to);
}

void
e_msg_composer_hdrs_set_subject (EMsgComposerHdrs *hdrs,
				 const gchar *subject)
{
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (subject != NULL);

	header = hdrs->priv->headers[HEADER_SUBJECT];

	e_composer_text_header_set_text (
		E_COMPOSER_TEXT_HEADER (header), subject);
}


CamelInternetAddress *
e_msg_composer_hdrs_get_from (EMsgComposerHdrs *hdrs)
{
	EComposerHeader *header;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	header = hdrs->priv->headers[HEADER_FROM];

	return e_composer_from_header_get_active_address (
		E_COMPOSER_FROM_HEADER (header));
}

CamelInternetAddress *
e_msg_composer_hdrs_get_reply_to (EMsgComposerHdrs *hdrs)
{
	CamelInternetAddress *addr;
	EComposerHeader *header;
	const gchar *text;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	header = hdrs->priv->headers[HEADER_REPLY_TO];

	text = e_composer_text_header_get_text (
		E_COMPOSER_TEXT_HEADER (header));

	if (text == NULL || *text == '\0')
		return NULL;

	addr = camel_internet_address_new ();
	if (camel_address_unformat (CAMEL_ADDRESS (addr), text) == -1) {
		camel_object_unref (CAMEL_OBJECT (addr));
		return NULL;
	}

	return addr;
}

EDestination **
e_msg_composer_hdrs_get_to (EMsgComposerHdrs *hdrs)
{
	EComposerNameHeader *header;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	header = E_COMPOSER_NAME_HEADER (hdrs->priv->headers[HEADER_TO]);
	return e_composer_name_header_get_destinations (header);
}

EDestination **
e_msg_composer_hdrs_get_cc (EMsgComposerHdrs *hdrs)
{
	EComposerNameHeader *header;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	header = E_COMPOSER_NAME_HEADER (hdrs->priv->headers[HEADER_CC]);
	return e_composer_name_header_get_destinations (header);
}

EDestination **
e_msg_composer_hdrs_get_bcc (EMsgComposerHdrs *hdrs)
{
	EComposerNameHeader *header;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	header = E_COMPOSER_NAME_HEADER (hdrs->priv->headers[HEADER_BCC]);
	return e_composer_name_header_get_destinations (header);
}

EDestination **
e_msg_composer_hdrs_get_recipients (EMsgComposerHdrs *hdrs)
{
	EDestination **to_destv;
	EDestination **cc_destv;
	EDestination **bcc_destv;
	EDestination **recip_destv;
	int i, j, n;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	to_destv  = e_msg_composer_hdrs_get_to (hdrs);
	cc_destv  = e_msg_composer_hdrs_get_cc (hdrs);
	bcc_destv = e_msg_composer_hdrs_get_bcc (hdrs);

	n = 0;

	for (i = 0; to_destv && to_destv[i] != NULL; i++, n++);
	for (i = 0; cc_destv && cc_destv[i] != NULL; i++, n++);
	for (i = 0; bcc_destv && bcc_destv[i] != NULL; i++, n++);

	if (n == 0)
		return NULL;

	recip_destv = g_new (EDestination *, n + 1);

	j = 0;

	for (i = 0; to_destv && to_destv[i] != NULL; i++, j++)
		recip_destv[j] = to_destv[i];
	for (i = 0; cc_destv && cc_destv[i] != NULL; i++, j++)
		recip_destv[j] = cc_destv[i];
	for (i = 0; bcc_destv && bcc_destv[i] != NULL; i++, j++)
		recip_destv[j] = bcc_destv[i];

	if (j != n) {
		g_warning ("j!=n \n");
	}
	recip_destv[j] = NULL;

	g_free (to_destv);
	g_free (cc_destv);
	g_free (bcc_destv);

	return recip_destv;
}


GList *
e_msg_composer_hdrs_get_post_to (EMsgComposerHdrs *hdrs)
{
	EComposerHeader *header;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	header = hdrs->priv->headers[HEADER_POST_TO];

	return e_composer_post_header_get_folders (
		E_COMPOSER_POST_HEADER (header));
}


const gchar *
e_msg_composer_hdrs_get_subject (EMsgComposerHdrs *hdrs)
{
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	widget = e_msg_composer_hdrs_get_subject_entry (hdrs);

	return gtk_entry_get_text (GTK_ENTRY (widget));
}


GtkWidget *
e_msg_composer_hdrs_get_reply_to_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->headers[HEADER_REPLY_TO]->input_widget;
}

GtkWidget *
e_msg_composer_hdrs_get_to_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->headers[HEADER_TO]->input_widget;
}

GtkWidget *
e_msg_composer_hdrs_get_cc_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->headers[HEADER_CC]->input_widget;
}

GtkWidget *
e_msg_composer_hdrs_get_bcc_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->headers[HEADER_BCC]->input_widget;
}

GtkWidget *
e_msg_composer_hdrs_get_post_to_label (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->headers[HEADER_POST_TO]->input_widget;
}

GtkWidget *
e_msg_composer_hdrs_get_subject_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->headers[HEADER_SUBJECT]->input_widget;
}

GtkWidget *
e_msg_composer_hdrs_get_from_hbox (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->headers[HEADER_FROM]->input_widget;
}
