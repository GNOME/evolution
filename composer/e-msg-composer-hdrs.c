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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>

#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktooltips.h>
#include <libgnomeui/gnome-uidefs.h>

#include "Composer.h"

#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktooltips.h>

#include <gal/e-text/e-entry.h>
#include <gal/widgets/e-unicode.h>

#include <camel/camel.h>
#include "evolution-folder-selector-button.h"
#include "e-msg-composer-hdrs.h"
#include "mail/mail-config.h"
#include "addressbook/backend/ebook/e-book-util.h"

extern EvolutionShellClient *global_shell_client;



/* Indexes in the GtkTable assigned to various items */

#define LINE_FROM    0
#define LINE_REPLYTO 1
#define LINE_TO      2
#define LINE_CC      3
#define LINE_BCC     4
#define LINE_POSTTO  5
#define LINE_SUBJECT 6


typedef struct {
	GtkWidget *label;
	GtkWidget *entry;
} EMsgComposerHdrPair;

struct _EMsgComposerHdrsPrivate {
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;
	
	/* ui component */
	BonoboUIComponent *uic;
	
	/* The tooltips.  */
	GtkTooltips *tooltips;
	
	GSList *from_options;
	
	/* Standard headers.  */
	EMsgComposerHdrPair from, reply_to, to, cc, bcc, post_to, subject;
};


static GtkTableClass *parent_class = NULL;

enum {
	SHOW_ADDRESS_DIALOG,
	SUBJECT_CHANGED,
	HDRS_CHANGED,
	FROM_CHANGED,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL];


static gboolean
setup_corba (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;
	CORBA_Environment ev;
	
	priv = hdrs->priv;
	
	g_assert (priv->corba_select_names == CORBA_OBJECT_NIL);
	
	CORBA_exception_init (&ev);
	
	priv->corba_select_names = bonobo_activation_activate_from_id (SELECT_NAMES_OAFIID, 0, NULL, &ev);
	
	/* OAF seems to be broken -- it can return a CORBA_OBJECT_NIL without
           raising an exception in `ev'.  */
	if (ev._major != CORBA_NO_EXCEPTION || priv->corba_select_names == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		return FALSE;
	}
	
	CORBA_exception_free (&ev);
	
	return TRUE;
}

typedef struct {
	EMsgComposerHdrs *hdrs;
	char *string;
} EMsgComposerHdrsAndString;

static void
e_msg_composer_hdrs_and_string_free (EMsgComposerHdrsAndString *emchas)
{
	if (emchas->hdrs)
		g_object_unref(emchas->hdrs);
	g_free (emchas->string);
}

static EMsgComposerHdrsAndString *
e_msg_composer_hdrs_and_string_create (EMsgComposerHdrs *hdrs, const char *string)
{
	EMsgComposerHdrsAndString *emchas;
	
	emchas = g_new (EMsgComposerHdrsAndString, 1);
	emchas->hdrs = hdrs;
	emchas->string = g_strdup (string);
	if (emchas->hdrs)
		g_object_ref(emchas->hdrs);
	
	return emchas;
}

static void
address_button_clicked_cb (GtkButton *button, gpointer data)
{
	EMsgComposerHdrsAndString *emchas;
	EMsgComposerHdrs *hdrs;
	EMsgComposerHdrsPrivate *priv;
	CORBA_Environment ev;
	
	emchas = data;
	hdrs = emchas->hdrs;
	priv = hdrs->priv;
	
	CORBA_exception_init (&ev);
	
	GNOME_Evolution_Addressbook_SelectNames_activateDialog (
		priv->corba_select_names, emchas->string, &ev);
	
	CORBA_exception_free (&ev);
}

static void
from_changed (GtkWidget *item, gpointer data)
{
	EMsgComposerHdrs *hdrs = E_MSG_COMPOSER_HDRS (data);
	const char *reply_to;
	
	hdrs->account = g_object_get_data(G_OBJECT(item), "account");
	
	/* we do this rather than calling e_msg_composer_hdrs_set_reply_to()
	   because we don't want to change the visibility of the header */
	reply_to = hdrs->account->id->reply_to;
	e_entry_set_text (E_ENTRY (hdrs->priv->reply_to.entry), reply_to ? reply_to : "");
	
	g_signal_emit(hdrs, signals [FROM_CHANGED], 0);
}

static GtkWidget *
create_from_optionmenu (EMsgComposerHdrs *hdrs)
{
	GtkWidget *omenu, *menu, *first = NULL;
	const GSList *accounts, *a;
	const MailConfigAccount *account;
	GPtrArray *addresses;
	GtkWidget *item, *hbox;
	int i = 0, history = 0, m, matches;
	int default_account;
	
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	
	default_account = mail_config_get_default_account_num ();
	
	/* Make list of account email addresses */
	addresses = g_ptr_array_new ();
	accounts = mail_config_get_accounts ();
	for (a = accounts; a; a = a->next) {
		account = a->data;
		if (account->id->address)
			g_ptr_array_add (addresses, account->id->address);
	}

	while (accounts) {
		char *label;
		
		account = accounts->data;
		
		/* this should never ever fail */
		if (!account || !account->name || !account->id) {
			g_assert_not_reached ();
			continue;
		}
		
		if (account->id->address && *account->id->address) {
			/* If the account has a unique email address, just
			 * show that. Otherwise include the account name.
			 */
			for (m = matches = 0; m < addresses->len; m++) {
				if (!strcmp (account->id->address, addresses->pdata[m]))
					matches++;
			}
			
			if (matches > 1)
				label = g_strdup_printf ("%s <%s> (%s)", account->id->name,
							 account->id->address, account->name);
			else
				label = g_strdup_printf ("%s <%s>", account->id->name, account->id->address);
			
			item = gtk_menu_item_new_with_label (label);
			g_free (label);
			
			g_object_set_data(G_OBJECT(item), "account", account_copy (account));
			g_signal_connect(G_OBJECT (item), "activate",
					    G_CALLBACK (from_changed), hdrs);
			
			if (i == default_account) {
				first = item;
				history = i;
			}
			
			/* this is so we can later set which one we want */
			hdrs->priv->from_options = g_slist_append (hdrs->priv->from_options, item);
			
			gtk_menu_append (GTK_MENU (menu), item);
			gtk_widget_show (item);
			i++;
		}
		
		accounts = accounts->next;
	}
	g_ptr_array_free (addresses, TRUE);
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	
	if (first) {
		gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), history);
		g_signal_emit_by_name(first, "activate", hdrs);
	}
	
	hbox = gtk_hbox_new (FALSE, 3);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), omenu);
	gtk_widget_show (omenu);
	gtk_widget_show (hbox);
	
	g_object_set_data(G_OBJECT(hbox), "from_menu", omenu);
	
	return hbox;
}

static void
addressbook_entry_changed (BonoboListener    *listener,
			   const char        *event_name,
			   const CORBA_any   *arg,
			   CORBA_Environment *ev,
			   gpointer           user_data)
{
	EMsgComposerHdrs *hdrs = E_MSG_COMPOSER_HDRS (user_data);
	
	g_signal_emit(hdrs, signals[HDRS_CHANGED], 0);
}

static GtkWidget *
create_addressbook_entry (EMsgComposerHdrs *hdrs, const char *name)
{
	EMsgComposerHdrsPrivate *priv;
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;
	Bonobo_Control corba_control;
	GtkWidget *control_widget;
	CORBA_Environment ev;
	BonoboControlFrame *cf;
	Bonobo_PropertyBag pb = CORBA_OBJECT_NIL;
	
	priv = hdrs->priv;
	corba_select_names = priv->corba_select_names;
	
	CORBA_exception_init (&ev);
	
	GNOME_Evolution_Addressbook_SelectNames_addSection (
		corba_select_names, name, name, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	corba_control =
		GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (
			corba_select_names, name, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	control_widget = bonobo_widget_new_control_from_objref (
		corba_control, CORBA_OBJECT_NIL);
	
	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (control_widget));
	pb = bonobo_control_frame_get_control_property_bag (cf, NULL);
	
	bonobo_event_source_client_add_listener (
		pb, addressbook_entry_changed,
		"Bonobo/Property:change:entry_changed",
		NULL, hdrs);
	
	return control_widget;
}

static EMsgComposerHdrPair 
header_new_recipient (EMsgComposerHdrs *hdrs, const char *name, const char *tip)
{
	EMsgComposerHdrsPrivate *priv;
	EMsgComposerHdrPair ret;
	
	priv = hdrs->priv;
	
	ret.label = gtk_button_new_with_label (name);
	GTK_OBJECT_UNSET_FLAGS (ret.label, GTK_CAN_FOCUS);
	gtk_signal_connect_full (
		GTK_OBJECT (ret.label), "clicked",
		G_CALLBACK (address_button_clicked_cb), NULL,
		e_msg_composer_hdrs_and_string_create(hdrs, name),
		(GtkDestroyNotify) e_msg_composer_hdrs_and_string_free,
		FALSE, FALSE);
	
	gtk_tooltips_set_tip (
		hdrs->priv->tooltips, ret.label,
		_("Click here for the address book"),
		NULL);
	
	ret.entry = create_addressbook_entry (hdrs, name);
	
	return ret;
}

static void
entry_changed (GtkWidget *entry, EMsgComposerHdrs *hdrs)
{
	char *subject, *tmp;
	
	tmp = e_msg_composer_hdrs_get_subject (hdrs);
	g_signal_emit(hdrs, signals[SUBJECT_CHANGED], 0, subject);
	g_free (tmp);
	g_signal_emit(hdrs, signals[HDRS_CHANGED], 0);
}

static void
create_headers (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv = hdrs->priv;
	static const char *posting_types[] = { "mail/*", NULL };
	
	/*
	 * Reply-To:
	 *
	 * Create this before we call create_from_optionmenu,
	 * because that causes from_changed to be called, which
	 * expects the reply_to fields to be initialized.
	 */
	priv->reply_to.label = gtk_label_new (_("Reply-To:"));
	priv->reply_to.entry = e_entry_new ();
	gtk_object_set (GTK_OBJECT (priv->reply_to.entry),
			"editable", TRUE,
			"use_ellipsis", TRUE,
			"allow_newlines", FALSE,
			NULL);
	
	/*
	 * From
	 */
	priv->from.label = gtk_label_new (_("From:"));
	priv->from.entry = create_from_optionmenu (hdrs);
	
	/*
	 * Subject
	 */
	priv->subject.label = gtk_label_new (_("Subject:"));
	priv->subject.entry = e_entry_new ();
	gtk_object_set (GTK_OBJECT (priv->subject.entry),
			"editable", TRUE,
			"use_ellipsis", TRUE,
			"allow_newlines", FALSE,
			NULL);
	g_signal_connect(priv->subject.entry, "changed",
			 G_CALLBACK (entry_changed), hdrs);
	
	/*
	 * To, CC, and Bcc
	 */
	priv->to = header_new_recipient (
		hdrs, _("To:"),
		_("Enter the recipients of the message"));
	
	priv->cc = header_new_recipient (
		hdrs, _("Cc:"),
		_("Enter the addresses that will receive a carbon copy of the message"));
	
	priv->bcc = header_new_recipient (
		hdrs, _("Bcc:"),
		 _("Enter the addresses that will receive a carbon copy of "
		   "the message without appearing in the recipient list of "
		   "the message."));
	
	/*
	 * Post-To
	 */
	priv->post_to.label = gtk_label_new (_("Post To:"));
	priv->post_to.entry = evolution_folder_selector_button_new (
		global_shell_client, _("Posting destination"), NULL,
		posting_types);
}

static void
attach_couple (EMsgComposerHdrs *hdrs, EMsgComposerHdrPair *pair, int line)
{
	int pad;
	
	if (GTK_IS_LABEL (pair->label))
		pad = GNOME_PAD;
	else
		pad = 2;
	
	gtk_table_attach (GTK_TABLE (hdrs),
			  pair->label, 0, 1,
			  line, line + 1,
			  GTK_FILL, GTK_FILL, pad, pad);
	
	gtk_table_attach (GTK_TABLE (hdrs),
			  pair->entry, 1, 2,
			  line, line + 1,
			  GTK_FILL | GTK_EXPAND, 0, 2, 2);
}

static void
attach_headers (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *p = hdrs->priv;
	
	attach_couple (hdrs, &p->from, LINE_FROM);
	attach_couple (hdrs, &p->reply_to, LINE_REPLYTO);
	attach_couple (hdrs, &p->to, LINE_TO);
	attach_couple (hdrs, &p->cc, LINE_CC);
	attach_couple (hdrs, &p->bcc, LINE_BCC);
	attach_couple (hdrs, &p->post_to, LINE_POSTTO);
	attach_couple (hdrs, &p->subject, LINE_SUBJECT);
}

static void
set_pair_visibility (EMsgComposerHdrs *h, EMsgComposerHdrPair *pair, int visible)
{
	if (visible & h->visible_mask) {
		gtk_widget_show (pair->label);
		gtk_widget_show (pair->entry);
	} else {
		gtk_widget_hide (pair->label);
		gtk_widget_hide (pair->entry);
	}
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
	
	set_pair_visibility (h, &p->from, visible_flags & E_MSG_COMPOSER_VISIBLE_FROM);
	set_pair_visibility (h, &p->reply_to, visible_flags & E_MSG_COMPOSER_VISIBLE_REPLYTO);
	set_pair_visibility (h, &p->to, visible_flags & E_MSG_COMPOSER_VISIBLE_TO);
	set_pair_visibility (h, &p->cc, visible_flags & E_MSG_COMPOSER_VISIBLE_CC);
	set_pair_visibility (h, &p->bcc, visible_flags & E_MSG_COMPOSER_VISIBLE_BCC);
	set_pair_visibility (h, &p->post_to, visible_flags & E_MSG_COMPOSER_VISIBLE_POSTTO);
	set_pair_visibility (h, &p->subject, visible_flags & E_MSG_COMPOSER_VISIBLE_SUBJECT);
}

static void
headers_set_sensitivity (EMsgComposerHdrs *h)
{
	bonobo_ui_component_set_prop (
		h->priv->uic, "/commands/ViewFrom", "sensitive",
		h->visible_mask & E_MSG_COMPOSER_VISIBLE_FROM ? "1" : "0", NULL);
	
	bonobo_ui_component_set_prop (
		h->priv->uic, "/commands/ViewReplyTo", "sensitive",
		h->visible_mask & E_MSG_COMPOSER_VISIBLE_REPLYTO ? "1" : "0", NULL);
	
	bonobo_ui_component_set_prop (
		h->priv->uic, "/commands/ViewCC", "sensitive",
		h->visible_mask & E_MSG_COMPOSER_VISIBLE_CC ? "1" : "0", NULL);
	
	bonobo_ui_component_set_prop (
		h->priv->uic, "/commands/ViewBCC", "sensitive",
		h->visible_mask & E_MSG_COMPOSER_VISIBLE_BCC ? "1" : "0", NULL);
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

static void
setup_headers (EMsgComposerHdrs *hdrs, int visible_flags)
{
	create_headers (hdrs);
	attach_headers (hdrs);
	
	headers_set_sensitivity (hdrs);
	headers_set_visibility (hdrs, visible_flags);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposerHdrs *hdrs;
	EMsgComposerHdrsPrivate *priv;
	GSList *l;
	
	hdrs = E_MSG_COMPOSER_HDRS (object);
	priv = hdrs->priv;
	
	if (priv) {
		if (priv->corba_select_names != CORBA_OBJECT_NIL) {
			CORBA_Environment ev;
			CORBA_exception_init (&ev);
			bonobo_object_release_unref (priv->corba_select_names, &ev);
			CORBA_exception_free (&ev);
			priv->corba_select_names = CORBA_OBJECT_NIL;
		}

		if (priv->tooltips) {
			gtk_object_destroy (GTK_OBJECT (priv->tooltips));
			g_object_unref(priv->tooltips);
			priv->tooltips = NULL;
		}

		l = priv->from_options;
		while (l) {
			MailConfigAccount *account;
			GtkWidget *item = l->data;
			
			account = g_object_get_data(G_OBJECT(item), "account");
			account_destroy (account);
			
			l = l->next;
		}
		g_slist_free (priv->from_options);
		priv->from_options = NULL;
		
		g_free (priv);
		hdrs->priv = NULL;
	}
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EMsgComposerHdrsClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;
	
	parent_class = g_type_class_ref(gtk_table_get_type ());
	
	signals[SHOW_ADDRESS_DIALOG] =
		g_signal_new ("show_address_dialog",
			      E_TYPE_MSG_COMPOSER_HDRS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(EMsgComposerHdrsClass, show_address_dialog),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[SUBJECT_CHANGED] =
		g_signal_new ("subject_changed",
			      E_TYPE_MSG_COMPOSER_HDRS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(EMsgComposerHdrsClass, subject_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
	
	signals[HDRS_CHANGED] =
		g_signal_new ("hdrs_changed",
			      E_TYPE_MSG_COMPOSER_HDRS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(EMsgComposerHdrsClass, hdrs_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[FROM_CHANGED] =
		g_signal_new ("from_changed",
			      E_TYPE_MSG_COMPOSER_HDRS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(EMsgComposerHdrsClass, from_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
init (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;
	
	priv = g_new0 (EMsgComposerHdrsPrivate, 1);
	
	priv->tooltips = gtk_tooltips_new ();
	g_object_ref(priv->tooltips);
	gtk_object_sink((GtkObject *)priv->tooltips);

	hdrs->priv = priv;
}


GType
e_msg_composer_hdrs_get_type (void)
{
	static GType type = 0;
	
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (EMsgComposerHdrsClass),
			NULL,
			NULL,
			(GClassInitFunc) class_init,
			NULL,
			NULL,
			sizeof (EMsgComposerHdrs),
			0,
			(GInstanceInitFunc) init,
		};
		
		type = g_type_register_static(gtk_table_get_type(), "EMsgComposerHdrs", &info, 0);
	}
	
	return type;
}

GtkWidget *
e_msg_composer_hdrs_new (BonoboUIComponent *uic, int visible_mask, int visible_flags)
{
	EMsgComposerHdrs *new;
	EMsgComposerHdrsPrivate *priv;
	
	new = g_object_new (e_msg_composer_hdrs_get_type (), NULL);
	priv = new->priv;
	priv->uic = uic;
	
	if (!setup_corba (new)) {
		g_object_unref(new);
		return NULL;
	}
	
	new->visible_mask = visible_mask;
	
	setup_headers (new, visible_flags);
	
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
	
	camel_object_unref(to_addr);
	camel_object_unref(cc_addr);
	camel_object_unref(bcc_addr);
}

static void
e_msg_composer_hdrs_to_message_internal (EMsgComposerHdrs *hdrs,
					 CamelMimeMessage *msg,
					 gboolean redirect)
{
	EDestination **to_destv, **cc_destv, **bcc_destv;
	CamelInternetAddress *addr;
	char *subject, *header;
	
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));
	
	subject = e_msg_composer_hdrs_get_subject (hdrs);
	camel_mime_message_set_subject (msg, subject);
	g_free (subject);
	
	addr = e_msg_composer_hdrs_get_from (hdrs);
	if (redirect) {
		header = camel_address_encode (CAMEL_ADDRESS (addr));
		camel_medium_set_header (CAMEL_MEDIUM (msg), "Resent-From", header);
		g_free (header);
	} else {
		camel_mime_message_set_from (msg, addr);
	}
	camel_object_unref(addr);
	
 	addr = e_msg_composer_hdrs_get_reply_to (hdrs);
	if (addr) {
		camel_mime_message_set_reply_to (msg, addr);
		camel_object_unref(addr);
	}
	
	if (hdrs->visible_mask & E_MSG_COMPOSER_VISIBLE_MASK_RECIPIENTS) {
		to_destv  = e_msg_composer_hdrs_get_to (hdrs);
		cc_destv  = e_msg_composer_hdrs_get_cc (hdrs);
		bcc_destv = e_msg_composer_hdrs_get_bcc (hdrs);
		
		/* Attach destinations to the message. */
		
		set_recipients_from_destv (msg, to_destv, cc_destv, bcc_destv, redirect);
		
		e_destination_freev (to_destv);
		e_destination_freev (cc_destv);
		e_destination_freev (bcc_destv);
	}
	
	if (hdrs->visible_mask & E_MSG_COMPOSER_VISIBLE_POSTTO) {
		header = e_msg_composer_hdrs_get_post_to (hdrs);
		camel_medium_set_header (CAMEL_MEDIUM (msg), "X-Evolution-PostTo", header);
		g_free (header);
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


/* FIXME: yea, this could be better... but it's doubtful it'll be used much */
void
e_msg_composer_hdrs_set_from_account (EMsgComposerHdrs *hdrs,
				      const char *account_name)
{
	GtkOptionMenu *omenu;
	GtkWidget *item;
	GSList *l;
	int i = 0;
	int default_account = 0;
	
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	omenu = GTK_OPTION_MENU (e_msg_composer_hdrs_get_from_omenu (hdrs));
	
	if (account_name)
		default_account = -1;
	else
		default_account = mail_config_get_default_account_num ();
	
	/* find the item that represents the account and activate it */
	l = hdrs->priv->from_options;
	while (l) {
		MailConfigAccount *account;
		item = l->data;
		
		account = g_object_get_data(G_OBJECT(item), "account");
		if (account_name) {
			if (account->name && !strcmp (account_name, account->name)) {
				/* set the correct optionlist item */
				gtk_option_menu_set_history (omenu, i);
				g_signal_emit_by_name (G_OBJECT (item), "activate", hdrs);
				
				return;
			}
		} else if (i == default_account) {
			/* set the default optionlist item */
			gtk_option_menu_set_history (omenu, i);
			g_signal_emit_by_name (G_OBJECT (item), "activate", hdrs);
			
			return;
		}
		
		l = l->next;
		i++;
	}
}

void
e_msg_composer_hdrs_set_reply_to (EMsgComposerHdrs *hdrs,
				  const char *reply_to)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	e_entry_set_text (E_ENTRY (hdrs->priv->reply_to.entry), reply_to ? reply_to : "");
	
	if (reply_to && *reply_to)
		set_pair_visibility (hdrs, &hdrs->priv->cc, TRUE);
}

void
e_msg_composer_hdrs_set_to (EMsgComposerHdrs *hdrs,
			    EDestination **to_destv)
{
	char *str;
	
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	str = e_destination_exportv (to_destv);
	bonobo_widget_set_property (BONOBO_WIDGET (hdrs->priv->to.entry), "destinations", TC_CORBA_string, str, NULL); 
	g_free (str);
}

void
e_msg_composer_hdrs_set_cc (EMsgComposerHdrs *hdrs,
			    EDestination **cc_destv)
{
	char *str;
	
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	str = e_destination_exportv (cc_destv);
	bonobo_widget_set_property (BONOBO_WIDGET (hdrs->priv->cc.entry), "destinations", TC_CORBA_string, str, NULL);
	if (str && *str)
		set_pair_visibility (hdrs, &hdrs->priv->cc, TRUE);
	g_free (str);
}

void
e_msg_composer_hdrs_set_bcc (EMsgComposerHdrs *hdrs,
			     EDestination **bcc_destv)
{
	char *str;
	
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	str = e_destination_exportv (bcc_destv);
	bonobo_widget_set_property (BONOBO_WIDGET (hdrs->priv->bcc.entry), "destinations", TC_CORBA_string, str, NULL); 
	if (str && *str)
		set_pair_visibility (hdrs, &hdrs->priv->bcc, TRUE);
	g_free (str);
}

void
e_msg_composer_hdrs_set_post_to (EMsgComposerHdrs *hdrs,
				 const char *post_to)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (post_to != NULL);
	
	evolution_folder_selector_button_set_uri (EVOLUTION_FOLDER_SELECTOR_BUTTON (hdrs->priv->post_to.entry), post_to);
}

void
e_msg_composer_hdrs_set_subject (EMsgComposerHdrs *hdrs,
				 const char *subject)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (subject != NULL);
	
	gtk_object_set (GTK_OBJECT (hdrs->priv->subject.entry),
			"text", subject,
			NULL);
}


CamelInternetAddress *
e_msg_composer_hdrs_get_from (EMsgComposerHdrs *hdrs)
{
	const MailConfigAccount *account;
	CamelInternetAddress *addr;
	
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	account = hdrs->account;
	if (!account || !account->id) {
		/* FIXME: perhaps we should try the default account? */
		return NULL;
	}
	
	addr = camel_internet_address_new ();
	camel_internet_address_add (addr, account->id->name, account->id->address);
	
	return addr;
}

CamelInternetAddress *
e_msg_composer_hdrs_get_reply_to (EMsgComposerHdrs *hdrs)
{
	CamelInternetAddress *addr;
	const char *reply_to;
	
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	reply_to = e_entry_get_text (E_ENTRY (hdrs->priv->reply_to.entry));
	
	if (!reply_to || *reply_to == '\0')
		return NULL;
	
	addr = camel_internet_address_new ();
	if (camel_address_unformat (CAMEL_ADDRESS (addr), reply_to) == -1) {
		camel_object_unref (CAMEL_OBJECT (addr));
		return NULL;
	}
	
	return addr;
}

EDestination **
e_msg_composer_hdrs_get_to (EMsgComposerHdrs *hdrs)
{
	char *str = NULL;
	EDestination **destv = NULL;
	
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	bonobo_widget_get_property (BONOBO_WIDGET (hdrs->priv->to.entry), "destinations", TC_CORBA_string, &str, NULL); 
	
	if (str != NULL) {
		destv = e_destination_importv (str);
		g_free (str);
	}
	
	return destv;
}

EDestination **
e_msg_composer_hdrs_get_cc (EMsgComposerHdrs *hdrs)
{
	char *str = NULL;
	EDestination **destv = NULL;
	
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	bonobo_widget_get_property (BONOBO_WIDGET (hdrs->priv->cc.entry), "destinations", TC_CORBA_string, &str, NULL); 
	
	if (str != NULL) {
		destv = e_destination_importv (str);
		g_free (str);
	}
	
	return destv;
}

EDestination **
e_msg_composer_hdrs_get_bcc (EMsgComposerHdrs *hdrs)
{
	char *str = NULL;
	EDestination **destv = NULL;
	
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	bonobo_widget_get_property (BONOBO_WIDGET (hdrs->priv->bcc.entry), "destinations", TC_CORBA_string, &str, NULL); 
	
	if (str != NULL) {
		destv = e_destination_importv (str);
		g_free (str);
	}
	
	return destv;
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
	
	g_assert (j == n);
	recip_destv[j] = NULL;
	
	g_free (to_destv);
	g_free (cc_destv);
	g_free (bcc_destv);
	
	return recip_destv;
}

char *
e_msg_composer_hdrs_get_post_to (EMsgComposerHdrs *hdrs)
{
	GNOME_Evolution_Folder *folder;

	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	folder = evolution_folder_selector_button_get_folder (EVOLUTION_FOLDER_SELECTOR_BUTTON (hdrs->priv->post_to.entry));
	return folder ? g_strdup (folder->physicalUri) : NULL;
}

char *
e_msg_composer_hdrs_get_subject (EMsgComposerHdrs *hdrs)
{
	char *subject;
	
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	gtk_object_get (GTK_OBJECT (hdrs->priv->subject.entry),
			"text", &subject, NULL);
	
	return subject;
}


GtkWidget *
e_msg_composer_hdrs_get_reply_to_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->reply_to.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_to_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->to.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_cc_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->cc.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_bcc_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->bcc.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_post_to_label (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->post_to.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_subject_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->subject.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_from_hbox (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->from.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_from_omenu (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return GTK_WIDGET (g_object_get_data(G_OBJECT(hdrs->priv->from.entry), "from_menu"));
}
