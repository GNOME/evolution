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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <camel/camel.h>

#include <bonobo.h>

#include <liboaf/liboaf.h>

#include "Composer.h"

#include "e-msg-composer-hdrs.h"
#include <gal/e-text/e-entry.h>
#include <gal/widgets/e-unicode.h>

#include "mail/mail-config.h"


#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames"

struct _EMsgComposerHdrsPrivate {
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;

	/* Total number of headers that we have.  */
	guint num_hdrs;

	/* The tooltips.  */
	GtkTooltips *tooltips;
	
	GSList *from_options;
	
	/* Standard headers.  */
	GtkWidget *from_entry;
	GtkWidget *to_entry;
	GtkWidget *cc_entry;
	GtkWidget *bcc_entry;
	GtkWidget *subject_entry;
};


static GtkTableClass *parent_class = NULL;

enum {
	SHOW_ADDRESS_DIALOG,
	SUBJECT_CHANGED,
	LAST_SIGNAL
};

enum {
	HEADER_ADDRBOOK,
	HEADER_OPTIONMENU,
	HEADER_ENTRYBOX
};

static gint signals[LAST_SIGNAL];


static gboolean
setup_corba (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;
	CORBA_Environment ev;

	priv = hdrs->priv;

	g_assert (priv->corba_select_names == CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	priv->corba_select_names = oaf_activate_from_id (SELECT_NAMES_OAFID, 0, NULL, &ev);

	/* OAF seems to be broken -- it can return a CORBA_OBJECT_NIL without
           raising an exception in `ev'.  */
	if (ev._major != CORBA_NO_EXCEPTION || priv->corba_select_names == CORBA_OBJECT_NIL) {
		g_warning ("Cannot activate -- %s", SELECT_NAMES_OAFID);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}


static void
address_button_clicked_cb (GtkButton *button,
			   gpointer data)
{
	EMsgComposerHdrs *hdrs;
	EMsgComposerHdrsPrivate *priv;
	CORBA_Environment ev;

	hdrs = E_MSG_COMPOSER_HDRS (data);
	priv = hdrs->priv;

	CORBA_exception_init (&ev);

	/* FIXME: Section ID */
	GNOME_Evolution_Addressbook_SelectNames_activateDialog (priv->corba_select_names, "", &ev);

	CORBA_exception_free (&ev);
}

static void
from_changed (GtkWidget *item, gpointer data)
{
	EMsgComposerHdrs *hdrs = E_MSG_COMPOSER_HDRS (data);
	
	hdrs->account = gtk_object_get_data (GTK_OBJECT (item), "account");
}

static GtkWidget *
create_optionmenu (EMsgComposerHdrs *hdrs,
		   const char *name)
{
	GtkWidget *omenu, *menu, *first = NULL;
	int i = 0, history = 0;
	
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	
	if (!strcmp (name, _("From:"))) {
		const GSList *accounts;
		GtkWidget *item;
		
		accounts = mail_config_get_accounts ();
		while (accounts) {
			const MailConfigAccount *account;
			
			account = accounts->data;
			
			/* this should never ever fail */
			if (!account || !account->name || !account->id) {
				g_assert_not_reached ();
				continue;
			}
			
			item = gtk_menu_item_new_with_label (account->name);
			gtk_object_set_data (GTK_OBJECT (item), "account", account_copy (account));
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (from_changed), hdrs);
			
			if (account->default_account) {
				first = item;
				history = i;
			}
			
			/* this is so we can later set which one we want */
			hdrs->priv->from_options = g_slist_append (hdrs->priv->from_options, item);
			
			gtk_menu_append (GTK_MENU (menu), item);
			gtk_widget_show (item);
			
			accounts = accounts->next;
			i++;
		}
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	if (first) {
		gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), history);
		gtk_signal_emit_by_name (GTK_OBJECT (first), "activate", hdrs);
	}
	
	return omenu;
}

static GtkWidget *
create_addressbook_entry (EMsgComposerHdrs *hdrs,
			  const char *name)
{
	EMsgComposerHdrsPrivate *priv;
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;
	Bonobo_Control corba_control;
	GtkWidget *control_widget;
	CORBA_Environment ev;

	priv = hdrs->priv;
	corba_select_names = priv->corba_select_names;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_addSection (corba_select_names, name, name, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	corba_control = GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (corba_select_names, name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	control_widget = bonobo_widget_new_control_from_objref (corba_control, CORBA_OBJECT_NIL);

	return control_widget;
}

static void
entry_changed (GtkWidget *entry, EMsgComposerHdrs *hdrs)
{
	gchar *tmp;
	gchar *subject;

	/* Mark the headers as changed */
	hdrs->has_changed = TRUE;

	tmp = e_msg_composer_hdrs_get_subject (hdrs);
	subject = e_utf8_to_gtk_string (GTK_WIDGET (entry), tmp);

	gtk_signal_emit (GTK_OBJECT (hdrs), signals[SUBJECT_CHANGED], subject);
	g_free (tmp);
}

static GtkWidget *
add_header (EMsgComposerHdrs *hdrs,
	    const gchar *name,
	    const gchar *tip,
	    const gchar *tip_private,
	    int type)
{
	EMsgComposerHdrsPrivate *priv;
	GtkWidget *label;
	GtkWidget *entry;
	guint pad;
	
	priv = hdrs->priv;
	
	if (type == HEADER_ADDRBOOK) {
		label = gtk_button_new_with_label (name);
		GTK_OBJECT_UNSET_FLAGS (label, GTK_CAN_FOCUS);
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
	
	switch (type) {
	case HEADER_ADDRBOOK:
		entry = create_addressbook_entry (hdrs, name);
		break;
	case HEADER_OPTIONMENU:
		entry = create_optionmenu (hdrs, name);
		break;
	case HEADER_ENTRYBOX:
	default:
		entry = e_entry_new ();
		gtk_object_set (GTK_OBJECT(entry),
				"editable", TRUE,
				"use_ellipsis", TRUE,
				"allow_newlines", FALSE,
				NULL);
		gtk_signal_connect (GTK_OBJECT (entry), "changed",
				    GTK_SIGNAL_FUNC (entry_changed), hdrs);
	}

	if (entry != NULL) {
		gtk_widget_show (entry);
		
		gtk_table_attach (GTK_TABLE (hdrs), entry,
				  1, 2, priv->num_hdrs, priv->num_hdrs + 1,
				  GTK_FILL | GTK_EXPAND, 0,
				  2, 2);
		
		gtk_tooltips_set_tip (hdrs->priv->tooltips, entry, tip, tip_private);
	}
	
	priv->num_hdrs++;
	
	return entry;
}

static void
setup_headers (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;

	priv = hdrs->priv;
	
	priv->from_entry = add_header
		(hdrs, _("From:"), 
		 _("Enter the identity you wish to send this message from"),
		 NULL,
		 HEADER_OPTIONMENU);
	priv->to_entry = add_header
		(hdrs, _("To:"), 
		 _("Enter the recipients of the message"),
		 NULL,
		 HEADER_ADDRBOOK);
	priv->cc_entry = add_header
		(hdrs, _("Cc:"),
		 _("Enter the addresses that will receive a carbon copy of "
		   "the message"),
		 NULL,
		 HEADER_ADDRBOOK);
	priv->bcc_entry = add_header
		(hdrs, _("Bcc:"),
		 _("Enter the addresses that will receive a carbon copy of "
		   "the message without appearing in the recipient list of "
		   "the message."),
		 NULL,
		 HEADER_ADDRBOOK);
	priv->subject_entry = add_header
		(hdrs, _("Subject:"),
		 _("Enter the subject of the mail"),
		 NULL,
		 HEADER_ENTRYBOX);
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

	if (priv->corba_select_names != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (priv->corba_select_names, &ev);
		CORBA_exception_free (&ev);
	}

	gtk_object_destroy (GTK_OBJECT (priv->tooltips));
	
	l = priv->from_options;
	while (l) {
		MailConfigAccount *account;
		GtkWidget *item = l->data;
		
		account = gtk_object_get_data (GTK_OBJECT (item), "account");
		account_destroy (account);
		
		l = l->next;
	}
	g_slist_free (priv->from_options);
	
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

	signals[SUBJECT_CHANGED] =
		gtk_signal_new ("subject_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerHdrsClass,
						   subject_changed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE,
				1, GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;

	priv = g_new (EMsgComposerHdrsPrivate, 1);

	priv->corba_select_names = CORBA_OBJECT_NIL;
	
	priv->from_options  = NULL;
	
	priv->from_entry    = NULL;
	priv->to_entry      = NULL;
	priv->cc_entry      = NULL;
	priv->bcc_entry     = NULL;
	priv->subject_entry = NULL;
	
	priv->tooltips = gtk_tooltips_new ();

	priv->num_hdrs = 0;
	
	hdrs->priv = priv;
	
	hdrs->account = NULL;
	
	hdrs->has_changed = FALSE;
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
	EMsgComposerHdrsPrivate *priv;
	
	new = gtk_type_new (e_msg_composer_hdrs_get_type ());
	priv = new->priv;
	
	if (! setup_corba (new)) {
		gtk_widget_destroy (GTK_WIDGET (new));
		return NULL;
	}
	
	setup_headers (new);

	return GTK_WIDGET (new);
}


static void
set_recipients (CamelMimeMessage *msg, GtkWidget *entry_widget, const gchar *type)
{
	CamelInternetAddress *addr;
	char *string;
	
	bonobo_widget_get_property (BONOBO_WIDGET (entry_widget), "text", &string, NULL);
	
	addr = camel_internet_address_new ();
	camel_address_unformat (CAMEL_ADDRESS (addr), string);
	
	/* TODO: In here, we could cross-reference the names with an alias book
	   or address book, it should be sufficient for unformat to do the parsing too */
	
	camel_mime_message_set_recipients (msg, type, addr);
	
	camel_object_unref (CAMEL_OBJECT (addr));
	g_free (string);
}

void
e_msg_composer_hdrs_to_message (EMsgComposerHdrs *hdrs,
				CamelMimeMessage *msg)
{
	gchar *subject;
	CamelInternetAddress *from;
	
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (msg != NULL);
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));
	
	subject = e_msg_composer_hdrs_get_subject (hdrs);
	camel_mime_message_set_subject (msg, subject);
	g_free (subject);
	
	from = e_msg_composer_hdrs_get_from (hdrs);
	camel_mime_message_set_from (msg, from);
	camel_object_unref (CAMEL_OBJECT (from));
	
	set_recipients (msg, hdrs->priv->to_entry, CAMEL_RECIPIENT_TYPE_TO);
	set_recipients (msg, hdrs->priv->cc_entry, CAMEL_RECIPIENT_TYPE_CC);
	set_recipients (msg, hdrs->priv->bcc_entry, CAMEL_RECIPIENT_TYPE_BCC);
}


static void
set_entry (BonoboWidget *bonobo_widget,
	   const GList *list)
{
	GString *string;
	const GList *p;
	
	string = g_string_new ("");
	for (p = list; p != NULL; p = p->next) {
		if (string->str[0] != '\0')
			g_string_append (string, ", ");
		g_string_append (string, p->data);
	}
	
	bonobo_widget_set_property (BONOBO_WIDGET (bonobo_widget), "text", string->str, NULL);
	
	g_string_free (string, TRUE);
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
	
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	omenu = GTK_OPTION_MENU (hdrs->priv->from_entry);
	
	/* find the item that represents the account and activate it */
	l = hdrs->priv->from_options;
	while (l) {
		MailConfigAccount *account;
		item = l->data;
		
		account = gtk_object_get_data (GTK_OBJECT (item), "account");
		if (!strcmp (account_name, account->name)) {
			gtk_option_menu_set_history (omenu, i);
			gtk_signal_emit_by_name (GTK_OBJECT (item), "activate", hdrs);
			return;
		}
		
		l = l->next;
		i++;
	}
}

void
e_msg_composer_hdrs_set_to (EMsgComposerHdrs *hdrs,
			    const GList *to_list)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	set_entry (BONOBO_WIDGET (hdrs->priv->to_entry), to_list);
}

void
e_msg_composer_hdrs_set_cc (EMsgComposerHdrs *hdrs,
			    const GList *cc_list)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	set_entry (BONOBO_WIDGET (hdrs->priv->cc_entry), cc_list);
}

void
e_msg_composer_hdrs_set_bcc (EMsgComposerHdrs *hdrs,
			     const GList *bcc_list)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	set_entry (BONOBO_WIDGET (hdrs->priv->bcc_entry), bcc_list);
}

void
e_msg_composer_hdrs_set_subject (EMsgComposerHdrs *hdrs,
				 const char *subject)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (subject != NULL);

	gtk_object_set (GTK_OBJECT (hdrs->priv->subject_entry),
			"text", subject,
			NULL);
}


CamelInternetAddress *
e_msg_composer_hdrs_get_from (EMsgComposerHdrs *hdrs)
{
	const MailConfigAccount *account;
	CamelInternetAddress *addr;
	
	g_return_val_if_fail (hdrs != NULL, NULL);
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

/* FIXME this is currently unused and broken.  */
GList *
e_msg_composer_hdrs_get_to (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	g_assert_not_reached ();
	
	return NULL;
}

/* FIXME this is currently unused and broken.  */
GList *
e_msg_composer_hdrs_get_cc (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	g_assert_not_reached ();
	
	return NULL;
}

/* FIXME this is currently unused and broken.  */
GList *
e_msg_composer_hdrs_get_bcc (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	g_assert_not_reached ();
	
	return NULL;
}

char *
e_msg_composer_hdrs_get_subject (EMsgComposerHdrs *hdrs)
{
	gchar *subject;
	
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	gtk_object_get (GTK_OBJECT (hdrs->priv->subject_entry),
			"text", &subject, NULL);
	
	return subject;
}


GtkWidget *
e_msg_composer_hdrs_get_to_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->to_entry;
}

GtkWidget *
e_msg_composer_hdrs_get_cc_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->cc_entry;
}

GtkWidget *
e_msg_composer_hdrs_get_bcc_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->bcc_entry;
}

GtkWidget *
e_msg_composer_hdrs_get_subject_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->subject_entry;
}
