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

#include <bonobo.h>

#include <liboaf/liboaf.h>

#include "Evolution-Addressbook-SelectNames.h"

#include "e-msg-composer-address-entry.h"
#include "e-msg-composer-hdrs.h"
#include "widgets/e-text/e-entry.h"

#include "mail/mail-config.h"


#define SELECT_NAMES_OAFID "OAFIID:addressbook:select-names:39301deb-174b-40d1-8a6e-5edc300f7b61"

struct _EMsgComposerHdrsPrivate {
	Evolution_Addressbook_SelectNames corba_select_names;

	/* Total number of headers that we have.  */
	guint num_hdrs;

	/* The tooltips.  */
	GtkTooltips *tooltips;

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
	LAST_SIGNAL
};

enum {
	HEADER_ADDRBOOK,
	HEADER_COMBOBOX,
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

	/* FIXME section.  */
	Evolution_Addressbook_SelectNames_activate_dialog (priv->corba_select_names, "", &ev);

	CORBA_exception_free (&ev);
}

static GtkWidget *
create_dropdown_entry (EMsgComposerHdrs *hdrs,
		       const char *name)
{
	GtkWidget *combo;
	GList *values = NULL;
	
	combo = gtk_combo_new ();
	gtk_combo_set_use_arrows (GTK_COMBO (combo), TRUE);
	gtk_combo_set_case_sensitive (GTK_COMBO (combo), FALSE);
	if (!strcmp (name, _("From:"))) {
		CamelInternetAddress *ciaddr;
		GSList *ids, *stmp;
		GList *tmp;
		MailConfigIdentity *id;
		char *val;
		
		ids = mail_config_get_identities ();
		stmp = ids;
		while (stmp) {
			char *address;
			
			id = stmp->data;
			g_assert (id);			
			g_assert (id->name);
			g_assert (id->address);
			
			ciaddr = camel_internet_address_new ();
			camel_internet_address_add (ciaddr, id->name, id->address);
			address = camel_address_encode (CAMEL_ADDRESS (ciaddr));
			values = g_list_append (values, address);
			stmp = stmp->next;
		}
		
		gtk_combo_set_popdown_strings (GTK_COMBO (combo), values);

		tmp = values;
		while (tmp) {
			g_free (tmp->data);
			tmp = tmp->next;
		}
		g_list_free (values);

		id = mail_config_get_default_identity ();
		g_assert (id);			
		g_assert (id->name);
		g_assert (id->address);
		
		ciaddr = camel_internet_address_new ();
		camel_internet_address_add (ciaddr, id->name, id->address);
		val = camel_address_encode (CAMEL_ADDRESS (ciaddr));
		
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry), val);
		g_free (val);
	}
	
	return combo;
}

static GtkWidget *
create_addressbook_entry (EMsgComposerHdrs *hdrs,
			  const char *name)
{
	EMsgComposerHdrsPrivate *priv;
	Evolution_Addressbook_SelectNames corba_select_names;
	Bonobo_Control corba_control;
	GtkWidget *control_widget;
	CORBA_Environment ev;

	priv = hdrs->priv;
	corba_select_names = priv->corba_select_names;

	CORBA_exception_init (&ev);

	Evolution_Addressbook_SelectNames_add_section (corba_select_names, name, name, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	corba_control = Evolution_Addressbook_SelectNames_get_entry_for_section (corba_select_names, name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	control_widget = bonobo_widget_new_control_from_objref (corba_control, CORBA_OBJECT_NIL);

	return control_widget;
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
		GTK_OBJECT_UNSET_FLAGS(label, GTK_CAN_FOCUS);
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
	case HEADER_COMBOBOX:
		entry = create_dropdown_entry (hdrs, name);
		break;
	default:
		entry = e_entry_new ();
		gtk_object_set(GTK_OBJECT(entry),
			       "editable", TRUE,
			       "use_ellipsis", TRUE,
			       NULL);
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
		 _("Enter the identitiy you wish to send this message from"),
		 NULL,
		 HEADER_COMBOBOX);
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

	hdrs = E_MSG_COMPOSER_HDRS (object);
	priv = hdrs->priv;

	if (priv->corba_select_names != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (priv->corba_select_names, &ev);
		CORBA_exception_free (&ev);
	}

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

	priv->corba_select_names = CORBA_OBJECT_NIL;

	priv->from_entry    = NULL;
	priv->to_entry      = NULL;
	priv->cc_entry      = NULL;
	priv->bcc_entry     = NULL;
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


static GList *
decode_addresses (const char *s)
{
	const char *p, *oldp;
	gboolean in_quotes;
	GList *list;

	g_print ("Decoding addresses -- %s\n", s ? s : "(null)");

	if (s == NULL)
		return NULL;

	in_quotes = FALSE;
	list = NULL;

	p = s;
	oldp = s;

	while (1) {
		if (*p == '"') {
			in_quotes = ! in_quotes;
			p++;
		} else if ((! in_quotes && *p == ',') || *p == 0) {
			if (p != oldp) {
				char *new_addr;

				new_addr = g_strndup (oldp, p - oldp);
				new_addr = g_strstrip (new_addr);
				if (*new_addr != '\0')
					list = g_list_prepend (list, new_addr);
				else
					g_free (new_addr);
			}

			while (*p == ',' || *p == ' ' || *p == '\t')
				p++;

			if (*p == 0)
				break;

			oldp = p;
		} else {
			p++;
		}
	}

	return g_list_reverse (list);
}

static void
set_recipients (CamelMimeMessage *msg,
		GtkWidget *entry_widget,
		const gchar *type)
{
	GList *list;
	GList *p;
	struct _header_address *addr;
	char *s;

	bonobo_widget_get_property (BONOBO_WIDGET (entry_widget), "text", &s, NULL);

	list = decode_addresses (s);

	g_free (s);

	/* FIXME leak?  */

	for (p = list; p != NULL; p = p->next) {
		addr = header_address_decode (p->data);
		camel_mime_message_add_recipient (msg, type, addr->name,
						  addr->v.addr);
		header_address_unref (addr);
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

	gtk_object_get(GTK_OBJECT(hdrs->priv->subject_entry),
		       "text", &s,
		       NULL);
	camel_mime_message_set_subject (msg, g_strdup (s));

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

	string = g_string_new (NULL);
	for (p = list; p != NULL; p = p->next) {
		if (string->str[0] != '\0')
			g_string_append (string, ", ");
		g_string_append (string, p->data);
	}

	bonobo_widget_set_property (BONOBO_WIDGET (bonobo_widget), "text", string->str, NULL);

	g_string_free (string, TRUE);
}

void
e_msg_composer_hdrs_set_from (EMsgComposerHdrs *hdrs,
			      const char *from)
{
	GtkEntry *entry;
	
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	entry = GTK_ENTRY (GTK_COMBO (hdrs->priv->from_entry)->entry);
	gtk_entry_set_text (entry, from);
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

	gtk_object_set(GTK_OBJECT(hdrs->priv->subject_entry),
		       "text", subject,
		       NULL);
}


char *
e_msg_composer_hdrs_get_from (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (hdrs->priv->from_entry)->entry));
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

const char *
e_msg_composer_hdrs_get_subject (EMsgComposerHdrs *hdrs)
{
	gchar *subject;

	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	gtk_object_get(GTK_OBJECT(hdrs->priv->subject_entry),
		       "text", &subject,
		       NULL);
	
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
