/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-destination.c
 *
 * Copyright (C) 2001-2004 Ximian, Inc.
 *
 * Authors: Jon Trowbridge <trow@ximian.com>
 *          Chris Toshok <toshok@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

/*
  This file (and e-destination.h) should really be in
  e-d-s/addressbook/libebook.  but there are at present (at least) 2
  reasons why they can't be:

  1) e_utf8_casefold_collate (in eab-book-util.c).  we could just copy
  the implementation into this file, though.. we already have an
  implementation in the select-names code as well, yuck.

  and

  2) camel-internet-address.  fejj mentioned writing a stripped down
  version for our use here, which would work.  alternatively we can
  just keep this file in evolution/ until/unless camel moves into
  e-d-s.


  we should probably make most of the functions in this file a little
  stupider..  all the work extracting useful info from the
  EContact/raw text/etc should happen in e_destination_set_contact
  (and the other setters), not in a bunch of if's in the respective
  _get_*() functions.
*/

#include <config.h>
#include "e-destination.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <libebook/e-book.h>
#include "eab-marshal.h"
#include "eab-book-util.h"

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <camel/camel-internet-address.h>

#define d(x)

struct _EDestinationPrivate {
	gchar *raw;

	char *source_uid;

	EContact *contact;
	char *contact_uid;

	int email_num;

	char *name;
	char *email;
	char *addr;
	char *textrep;

	GList *list_dests;

	guint html_mail_override : 1;
	guint wants_html_mail : 1;

	guint show_addresses : 1;

	guint auto_recipient : 1;
};

static gboolean       e_destination_from_contact       (const EDestination *);
static xmlNodePtr     e_destination_xml_encode         (const EDestination *dest);
static gboolean       e_destination_xml_decode         (EDestination *dest, xmlNodePtr node);
static void           e_destination_clear              (EDestination *dest);

static GObjectClass *parent_class;

static void
e_destination_dispose (GObject *obj)
{
	EDestination *dest = E_DESTINATION (obj);

	if (dest->priv) {
		e_destination_clear (dest);
	  
		g_free (dest->priv);
		dest->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (obj);
}

static void
e_destination_class_init (EDestinationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->dispose = e_destination_dispose;
}

static void
e_destination_init (EDestination *dest)
{
	dest->priv = g_new0 (struct _EDestinationPrivate, 1);

	dest->priv->auto_recipient = FALSE;
}

GType
e_destination_get_type (void)
{
	static GType dest_type = 0;

	if (!dest_type) {
		GTypeInfo dest_info = {
			sizeof (EDestinationClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  e_destination_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EDestination),
			0,    /* n_preallocs */
			(GInstanceInitFunc) e_destination_init
		};

		dest_type = g_type_register_static (G_TYPE_OBJECT, "EDestination", &dest_info, 0);
	}

	return dest_type;
}

EDestination *
e_destination_new (void)
{
	return g_object_new (E_TYPE_DESTINATION, NULL);
}

EDestination *
e_destination_copy (const EDestination *dest)
{
	EDestination *new_dest;
	GList *iter;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	new_dest = e_destination_new ();

        new_dest->priv->source_uid         = g_strdup (dest->priv->source_uid);
        new_dest->priv->contact_uid        = g_strdup (dest->priv->contact_uid);
        new_dest->priv->name               = g_strdup (dest->priv->name);
        new_dest->priv->email              = g_strdup (dest->priv->email);
        new_dest->priv->addr               = g_strdup (dest->priv->addr);
        new_dest->priv->email_num          = dest->priv->email_num;

	if (dest->priv->contact)
		new_dest->priv->contact = g_object_ref (dest->priv->contact);

	new_dest->priv->html_mail_override = dest->priv->html_mail_override;
	new_dest->priv->wants_html_mail    = dest->priv->wants_html_mail;

	/* deep copy, recursively copy our children */
	for (iter = dest->priv->list_dests; iter != NULL; iter = g_list_next (iter)) {
		new_dest->priv->list_dests = g_list_append (new_dest->priv->list_dests,
							    e_destination_copy (E_DESTINATION (iter->data)));
	}

	/* XXX other settings? */

	return new_dest;
}

static void
e_destination_clear (EDestination *dest)
{
	g_free (dest->priv->source_uid);
	dest->priv->source_uid = NULL;

	g_free (dest->priv->contact_uid);
	dest->priv->contact_uid = NULL;

	g_free (dest->priv->raw);
	dest->priv->raw = NULL;
	
	g_free (dest->priv->name);
	dest->priv->name = NULL;
	
	g_free (dest->priv->email);
	dest->priv->email = NULL;
	
	g_free (dest->priv->addr);
	dest->priv->addr = NULL;
	
	g_free (dest->priv->textrep);
	dest->priv->textrep = NULL;

	if (dest->priv->contact) {
		g_object_unref (dest->priv->contact);
		dest->priv->contact = NULL;
	}
	dest->priv->email_num = -1;
	
	g_list_foreach (dest->priv->list_dests, (GFunc) g_object_unref, NULL);
	g_list_free (dest->priv->list_dests);
	dest->priv->list_dests = NULL;
}

static gboolean
nonempty (const char *s)
{
	gunichar c;
	if (s == NULL)
		return FALSE;
	while (*s) {
		c = g_utf8_get_char (s);
		if (!g_unichar_isspace (c))
			return TRUE;
		s = g_utf8_next_char (s);
	}
	return FALSE;
}

gboolean
e_destination_empty (const EDestination *dest)

{
	struct _EDestinationPrivate *p;

	g_return_val_if_fail (E_IS_DESTINATION (dest), TRUE);
	
	p = dest->priv;
	
	return !(p->contact != NULL
		 || (p->source_uid && *p->source_uid)
		 || (p->contact_uid && *p->contact_uid)
		 || (nonempty (p->raw))
		 || (nonempty (p->name))
		 || (nonempty (p->email))
		 || (nonempty (p->addr))
		 || (p->list_dests != NULL));
}

gboolean
e_destination_equal (const EDestination *a, const EDestination *b)
{
	const struct _EDestinationPrivate *pa, *pb;
	const char *na, *nb;
	
	g_return_val_if_fail (E_IS_DESTINATION (a), FALSE);
	g_return_val_if_fail (E_IS_DESTINATION (b), FALSE);

	if (a == b)
		return TRUE;
	
	pa = a->priv;
	pb = b->priv;
	
	/* Check equality of contacts. */
	if (pa->contact || pb->contact) {
		if (! (pa->contact && pb->contact))
			return FALSE;
		
		if (pa->contact == pb->contact || !strcmp (e_contact_get_const (pa->contact, E_CONTACT_UID),
							   e_contact_get_const (pb->contact, E_CONTACT_UID)))
			return TRUE;
		
		return FALSE;
	}
	
	/* Just in case name returns NULL */
	na = e_destination_get_name (a);
	nb = e_destination_get_name (b);
	if ((na || nb) && !(na && nb && ! e_utf8_casefold_collate (na, nb)))
		return FALSE;
	
	if (!g_ascii_strcasecmp (e_destination_get_email (a), e_destination_get_email (b)))
		return TRUE;
	else
		return FALSE;
}

void
e_destination_set_contact (EDestination *dest, EContact *contact, gint email_num)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (contact && E_IS_CONTACT (contact));

	if (dest->priv->contact != contact || dest->priv->email_num != email_num) {

		e_destination_clear (dest);

		dest->priv->contact = g_object_ref (contact);

		dest->priv->contact_uid = e_contact_get (dest->priv->contact, E_CONTACT_UID);

		dest->priv->email_num = email_num;

		/* handle the mailing list case */
		if (e_contact_get (dest->priv->contact, E_CONTACT_IS_LIST)) {
			GList *email = e_contact_get_attributes (dest->priv->contact, E_CONTACT_EMAIL);

			if (email) {
				GList *iter;

				for (iter = email; iter; iter = iter->next) {
					EVCardAttribute *attr = iter->data;
					GList *p;
					EDestination *list_dest = e_destination_new ();
					char *contact_uid = NULL;
					char *email_addr = NULL;
					char *name = NULL;
					int email_num = -1;
					gboolean html_pref = FALSE;

					for (p = e_vcard_attribute_get_params (attr); p; p = p->next) {
						EVCardAttributeParam *param = p->data;
						const char *param_name = e_vcard_attribute_param_get_name (param);
						if (!g_ascii_strcasecmp (param_name,
									 EVC_X_DEST_CONTACT_UID)) {
							GList *v = e_vcard_attribute_param_get_values (param);
							contact_uid = v ? g_strdup (v->data) : NULL;
						}
						else if (!g_ascii_strcasecmp (param_name,
									      EVC_X_DEST_EMAIL_NUM)) {
							GList *v = e_vcard_attribute_param_get_values (param);
							email_num = v ? atoi (v->data) : -1;
						}
						else if (!g_ascii_strcasecmp (param_name,
									      EVC_X_DEST_NAME)) {
							GList *v = e_vcard_attribute_param_get_values (param);
							name = v ? v->data : NULL;
						}
						else if (!g_ascii_strcasecmp (param_name,
									      EVC_X_DEST_EMAIL)) {
							GList *v = e_vcard_attribute_param_get_values (param);
							email_addr = v ? v->data : NULL;
						}
						else if (!g_ascii_strcasecmp (param_name,
									      EVC_X_DEST_HTML_MAIL)) {
							GList *v = e_vcard_attribute_param_get_values (param);
							html_pref = v ? !g_ascii_strcasecmp (v->data, "true") : FALSE;
						}
					}

					if (contact_uid) e_destination_set_contact_uid (list_dest, contact_uid, email_num);
					if (name) e_destination_set_name (list_dest, name);
					if (email_addr) e_destination_set_email (list_dest, email_addr);
					e_destination_set_html_mail_pref (list_dest, html_pref);

					dest->priv->list_dests = g_list_append (dest->priv->list_dests, list_dest);
				}

				g_list_foreach (email, (GFunc) e_vcard_attribute_free, NULL);
				g_list_free (email);
			}
		}
		else {
			/* handle the normal contact case */
			/* is there anything to do here? */
		}
	}
}

void
e_destination_set_book (EDestination *dest, EBook *book)
{
	ESource *source;

	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (book && E_IS_BOOK (book));

	source = e_book_get_source (book);

	if (dest->priv->source_uid || strcmp (e_source_peek_uid (source), dest->priv->source_uid)) {
		e_destination_clear (dest);

		dest->priv->source_uid = g_strdup (e_source_peek_uid (source));
	}
}

void
e_destination_set_contact_uid (EDestination *dest, const char *uid, gint email_num)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (uid != NULL);
	
	if (dest->priv->contact_uid == NULL
	    || strcmp (dest->priv->contact_uid, uid)
	    || dest->priv->email_num != email_num) {
		
		g_free (dest->priv->contact_uid);
		dest->priv->contact_uid = g_strdup (uid);
		dest->priv->email_num = email_num;
		
		/* If we already have a contact, remove it unless it's uid matches the one
		   we just set. */
		if (dest->priv->contact && strcmp (uid,
						   e_contact_get_const (dest->priv->contact, E_CONTACT_UID))) {
			g_object_unref (dest->priv->contact);
			dest->priv->contact = NULL;
		}
	}
}

static void
e_destination_set_source_uid (EDestination *dest, const char *uid)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (uid != NULL);
	
	if (dest->priv->source_uid == NULL
	    || strcmp (dest->priv->source_uid, uid)) {

		g_free (dest->priv->source_uid);
		dest->priv->source_uid = g_strdup (uid);
	}
}

void
e_destination_set_name (EDestination *dest, const char *name)
{
	gboolean changed = FALSE;

	g_return_if_fail (E_IS_DESTINATION (dest));
	
	if (name == NULL) {
		if (dest->priv->name != NULL) {
			g_free (dest->priv->name);
			dest->priv->name = NULL;
			changed = TRUE;
		}
	} else if (dest->priv->name == NULL || strcmp (dest->priv->name, name)) {
		g_free (dest->priv->name);
		dest->priv->name = g_strdup (name);
		changed = TRUE;
	}
	
	if (changed) {
		g_free (dest->priv->addr);
		dest->priv->addr = NULL;
		g_free (dest->priv->textrep);
		dest->priv->textrep = NULL;
	}
}

void
e_destination_set_email (EDestination *dest, const char *email)
{
	gboolean changed = FALSE;

	g_return_if_fail (E_IS_DESTINATION (dest));
	
	if (email == NULL) {
		if (dest->priv->email != NULL) {
			g_free (dest->priv->addr);
			dest->priv->addr = NULL;
			changed = TRUE;
		}
	} else if (dest->priv->email == NULL || strcmp (dest->priv->email, email)) {
		g_free (dest->priv->email);
		dest->priv->email = g_strdup (email);
		changed = TRUE;
	}
	
	if (changed) {
		g_free (dest->priv->addr);
		dest->priv->addr = NULL;
		g_free (dest->priv->textrep);
		dest->priv->textrep = NULL;
	}
}

static gboolean
e_destination_from_contact (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	return dest->priv->contact != NULL || dest->priv->source_uid != NULL || dest->priv->contact_uid != NULL;
}

gboolean
e_destination_is_auto_recipient (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	
	return dest->priv->auto_recipient;
}

void
e_destination_set_auto_recipient (EDestination *dest, gboolean value)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	
	dest->priv->auto_recipient = value;
}

EContact *
e_destination_get_contact (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	return dest->priv->contact;
}

const char *
e_destination_get_contact_uid (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	return dest->priv->contact_uid;
}

const char *
e_destination_get_source_uid (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	return dest->priv->source_uid;
}

gint
e_destination_get_email_num (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), -1);
	
	if (dest->priv->contact == NULL && (dest->priv->source_uid == NULL || dest->priv->contact_uid == NULL))
		return -1;
	
	return dest->priv->email_num;
}

const char *
e_destination_get_name (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->name == NULL) {
		if (priv->contact != NULL) {
			priv->name = e_contact_get (priv->contact, E_CONTACT_FULL_NAME);
			
			if (priv->name == NULL || *priv->name == '\0') {
				g_free (priv->name);
				priv->name = e_contact_get (priv->contact, E_CONTACT_FILE_AS);
			}
			
			if (priv->name == NULL || *priv->name == '\0') {
				g_free (priv->name);
				if (e_contact_get (priv->contact, E_CONTACT_IS_LIST))
					priv->name = g_strdup (_("Unnamed List"));
				else
					priv->name = g_strdup (e_destination_get_email (dest));
			}
		}
		else if (priv->raw != NULL) {
			CamelInternetAddress *addr = camel_internet_address_new ();

			if (camel_address_unformat (CAMEL_ADDRESS (addr), priv->raw)) {
				const char *camel_name = NULL;
				
				camel_internet_address_get (addr, 0, &camel_name, NULL);
				priv->name = g_strdup (camel_name);
			}
			
			camel_object_unref (CAMEL_OBJECT (addr));
		}
	}
	
	return priv->name;
}

const char *
e_destination_get_email (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->email == NULL) {
		if (priv->contact != NULL) {
			/* Pull the address out of the card. */
			GList *email = e_contact_get (priv->contact, E_CONTACT_EMAIL);
			if (email) {
				char *e = g_list_nth_data (email, priv->email_num);

				if (e)
					priv->email = g_strdup (e);
			} 
			if (email) {
				g_list_foreach (email, (GFunc)g_free, NULL);
				g_list_free (email);
			}
			
		} else if (priv->raw != NULL) {
			CamelInternetAddress *addr = camel_internet_address_new ();
			
			if (camel_address_unformat (CAMEL_ADDRESS (addr), priv->raw)) {
				const char *camel_email = NULL;
				camel_internet_address_get (addr, 0, NULL, &camel_email);
				priv->email = g_strdup (camel_email);
			}
			
			camel_object_unref (CAMEL_OBJECT (addr));
		} 
		
		/* Force e-mail to be non-null... */
		if (priv->email == NULL) {
			priv->email = g_strdup ("");
		}
	}
	
	return priv->email;
}

const char *
e_destination_get_address (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->addr == NULL) {
		CamelInternetAddress *addr = camel_internet_address_new ();
		
		if (e_destination_is_evolution_list (dest)) {
			GList *iter = dest->priv->list_dests;
			
			while (iter) {
				EDestination *list_dest = E_DESTINATION (iter->data);
				
				if (!e_destination_empty (list_dest)) {
					const char *name, *email;
					name = e_destination_get_name (list_dest);
					email = e_destination_get_email (list_dest);

					if (nonempty (name) && nonempty (email))
						camel_internet_address_add (addr, name, email);
					else if (nonempty (email))
						camel_address_decode (CAMEL_ADDRESS (addr), email);
					else /* this case loses i suppose, but there's
						nothing we can do here */
						camel_address_decode (CAMEL_ADDRESS (addr), name);
				}
				iter = g_list_next (iter);
			}
			
			priv->addr = camel_address_encode (CAMEL_ADDRESS (addr));
		} else if (priv->raw) {

			if (camel_address_unformat (CAMEL_ADDRESS (addr), priv->raw)) {
				priv->addr = camel_address_encode (CAMEL_ADDRESS (addr));
			}
		} else {
			const char *name, *email;
			name = e_destination_get_name (dest);
			email = e_destination_get_email (dest);

			if (nonempty (name) && nonempty (email))
				camel_internet_address_add (addr, name, email);
			else if (nonempty (email))
				camel_address_decode (CAMEL_ADDRESS (addr), email);
			else /* this case loses i suppose, but there's
				nothing we can do here */
				camel_address_decode (CAMEL_ADDRESS (addr), name);
			
			priv->addr = camel_address_encode (CAMEL_ADDRESS (addr));
		}
		
		camel_object_unref (CAMEL_OBJECT (addr));
	}
	
	return priv->addr;
}

void
e_destination_set_raw (EDestination *dest, const char *raw)
{
	g_return_if_fail (E_IS_DESTINATION (dest));
	g_return_if_fail (raw != NULL);
	
	if (dest->priv->raw == NULL || strcmp (dest->priv->raw, raw)) {
			
		e_destination_clear (dest);

		dest->priv->raw = g_strdup (raw);
	}
}

const char *
e_destination_get_textrep (const EDestination *dest, gboolean include_email)
{
	const char *name, *email;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	if (dest->priv->raw)
		return dest->priv->raw;
	
	name  = e_destination_get_name (dest);
	email = e_destination_get_email (dest);
	
	if (e_destination_from_contact (dest) && name != NULL && (!include_email || !email || !*email))
		return name;
	
	/* Make sure that our address gets quoted properly */
	if (name && email && dest->priv->textrep == NULL) {
		CamelInternetAddress *addr = camel_internet_address_new ();
		
		camel_internet_address_add (addr, name, email);
		g_free (dest->priv->textrep);
		dest->priv->textrep = camel_address_format (CAMEL_ADDRESS (addr));
		camel_object_unref (CAMEL_OBJECT (addr));
	}
	
	if (dest->priv->textrep != NULL)
		return dest->priv->textrep;

	if (email)
		return email;
	
	return "";
}

gboolean
e_destination_is_evolution_list (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);

	return dest->priv->list_dests != NULL;
}

gboolean
e_destination_list_show_addresses (const EDestination *dest)
{
	g_return_val_if_fail (E_IS_DESTINATION (dest), FALSE);
	
	if (dest->priv->contact != NULL)
		return GPOINTER_TO_UINT (e_contact_get (dest->priv->contact, E_CONTACT_LIST_SHOW_ADDRESSES));
	
	return dest->priv->show_addresses;
}

gboolean
e_destination_get_html_mail_pref (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	
	if (dest->priv->html_mail_override || dest->priv->contact == NULL)
		return dest->priv->wants_html_mail;
	
	return e_contact_get (dest->priv->contact, E_CONTACT_WANTS_HTML) ? TRUE : FALSE;
}

void
e_destination_set_html_mail_pref (EDestination *dest, gboolean x)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	
	dest->priv->html_mail_override = TRUE;
	if (dest->priv->wants_html_mail != x) {
		dest->priv->wants_html_mail = x;
	}
}

/*
 * Destination import/export
 */

char *
e_destination_get_textrepv (EDestination **destv)
{
	int i, j, len = 0;
	char **strv;
	char *str;

	g_return_val_if_fail (destv, NULL);
	
	/* Q: Please tell me this is only for assertion
           reasons. If this is considered to be ok behavior then you
           shouldn't use g_return's. Just a reminder ;-) 
	   
	   A: Yes, this is just an assertion.  (Though it does find the
	   length of the vector in the process...)
	*/
	while (destv[len]) {
		g_return_val_if_fail (E_IS_DESTINATION (destv[len]), NULL);
		len++;
	}
	
	strv = g_new0 (char *, len + 1);
	for (i = 0, j = 0; destv[i]; i++) {
		if (!e_destination_empty (destv[i])) {
			const char *addr = e_destination_get_address (destv[i]);
			strv[j++] = addr ? (char *) addr : "";
		}
	}
	
	str = g_strjoinv (", ", strv);
	
	g_free (strv);
	
	return str;
}

xmlNodePtr
e_destination_xml_encode (const EDestination *dest)
{
	xmlNodePtr dest_node;
	const char *str;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	dest_node = xmlNewNode (NULL, "destination");
	
	str = e_destination_get_name (dest);
	if (str)
		xmlNewTextChild (dest_node, NULL, "name", str);
	
	if (!e_destination_is_evolution_list (dest)) {
		str = e_destination_get_email (dest);
		if (str)
			xmlNewTextChild (dest_node, NULL, "email", str);
	} else {
		GList *iter = dest->priv->list_dests;
		
		while (iter) {
			EDestination *list_dest = E_DESTINATION (iter->data);
			xmlNodePtr list_node = xmlNewNode (NULL, "list_entry");

			str = e_destination_get_name (list_dest);
			if (str) {
				char *escaped = xmlEncodeEntitiesReentrant (NULL, str);
				xmlNewTextChild (list_node, NULL, "name", escaped);
				xmlFree (escaped);
			}
			
			str = e_destination_get_email (list_dest);
			if (str) {
				char *escaped = xmlEncodeEntitiesReentrant (NULL, str);
				xmlNewTextChild (list_node, NULL, "email", str);
				xmlFree (escaped);
			}
			
			xmlAddChild (dest_node, list_node);
			
			iter = g_list_next (iter);
		}
		
		xmlNewProp (dest_node, "is_list", "yes");
		xmlNewProp (dest_node, "show_addresses", 
			    e_destination_list_show_addresses (dest) ? "yes" : "no");
	}
	
	str = e_destination_get_source_uid (dest);
	if (str) {
		char *escaped = xmlEncodeEntitiesReentrant (NULL, str);
		xmlNewTextChild (dest_node, NULL, "source_uid", str);
		xmlFree (escaped);
	}
	
	str = e_destination_get_contact_uid (dest);
	if (str) {
		char buf[16];
		
		xmlNodePtr uri_node = xmlNewTextChild (dest_node, NULL, "card_uid", str);
		g_snprintf (buf, 16, "%d", e_destination_get_email_num (dest));
		xmlNewProp (uri_node, "email_num", buf);
	}
	
	xmlNewProp (dest_node, "html_mail", e_destination_get_html_mail_pref (dest) ? "yes" : "no");
	
	xmlNewProp (dest_node, "auto_recipient",
		    e_destination_is_auto_recipient (dest) ? "yes" : "no");
	
	return dest_node;
}

gboolean
e_destination_xml_decode (EDestination *dest, xmlNodePtr node)
{
	char *name = NULL, *email = NULL, *source_uid = NULL, *card_uid = NULL;
	gboolean is_list = FALSE, show_addr = FALSE, auto_recip = FALSE;
	gboolean html_mail = FALSE;
	GList *list_dests = NULL;
	int email_num = -1;
	char *tmp;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);
	
	if (strcmp (node->name, "destination"))
		return FALSE;
	
	tmp = xmlGetProp (node, "html_mail");
	if (tmp) {
		html_mail = !strcmp (tmp, "yes");
		xmlFree (tmp);
	}
	
	tmp = xmlGetProp (node, "is_list");
	if (tmp) {
		is_list = !strcmp (tmp, "yes");
		xmlFree (tmp);
	}
	
	tmp = xmlGetProp (node, "show_addresses");
	if (tmp) {
		show_addr = !strcmp (tmp, "yes");
		xmlFree (tmp);
	}
	
	tmp = xmlGetProp (node, "auto_recipient");
	if (tmp) {
		auto_recip = !strcmp (tmp, "yes");
		xmlFree (tmp);
	}
	
	node = node->xmlChildrenNode;
	while (node) {
		if (!strcmp (node->name, "name")) {
			tmp = xmlNodeGetContent (node);
			g_free (name);
			name = g_strdup (tmp);
			xmlFree (tmp);
		} else if (!is_list && !strcmp (node->name, "email")) {
			tmp = xmlNodeGetContent (node);
			g_free (email);
			email = g_strdup (tmp);
			xmlFree (tmp);
		} else if (is_list && !strcmp (node->name, "list_entry")) {
			xmlNodePtr subnode = node->xmlChildrenNode;
			char *list_name = NULL, *list_email = NULL;
			
			while (subnode) {
				if (!strcmp (subnode->name, "name")) {
					tmp = xmlNodeGetContent (subnode);
					g_free (list_name);
					list_name = g_strdup (tmp);
					xmlFree (tmp);
				} else if (!strcmp (subnode->name, "email")) {
					tmp = xmlNodeGetContent (subnode);
					g_free (list_email);
					list_email = g_strdup (tmp);
					xmlFree (tmp);
				}
				
				subnode = subnode->next;
			}
			
			if (list_name || list_email) {
				EDestination *list_dest = e_destination_new ();
				
				if (list_name)
					e_destination_set_name (list_dest, list_name);
				if (list_email)
					e_destination_set_email (list_dest, list_email);
				
				g_free (list_name);
				g_free (list_email);
				
				list_dests = g_list_append (list_dests, list_dest);
			}
		} else if (!strcmp (node->name, "source_uid")) {
			tmp = xmlNodeGetContent (node);
			g_free (source_uid);
			source_uid = g_strdup (tmp);
			xmlFree (tmp);
		} else if (!strcmp (node->name, "card_uid")) {
			tmp = xmlNodeGetContent (node);
			g_free (card_uid);
			card_uid = g_strdup (tmp);
			xmlFree (tmp);
			
			tmp = xmlGetProp (node, "email_num");
			email_num = atoi (tmp);
			xmlFree (tmp);
		}
		
		node = node->next;
	}
	
	e_destination_clear (dest);
	
	if (name) {
		e_destination_set_name (dest, name);
		g_free (name);
	}
	if (email) {
		e_destination_set_email (dest, email);
		g_free (email);
	}
	if (source_uid) {
		e_destination_set_source_uid (dest, source_uid);
		g_free (source_uid);
	}
	if (card_uid) {
		e_destination_set_contact_uid (dest, card_uid, email_num);
		g_free (card_uid);
	}
	if (list_dests)
		dest->priv->list_dests = list_dests;
	
	dest->priv->html_mail_override = TRUE;
	dest->priv->wants_html_mail = html_mail;
	
	dest->priv->show_addresses = show_addr;
	
	dest->priv->auto_recipient = auto_recip;
	
	return TRUE;
}

static char *
null_terminate_and_remove_extra_whitespace (xmlChar *xml_in, gint size)
{
	gboolean skip_white = FALSE;
	char *xml, *r, *w;

	if (xml_in == NULL || size <= 0) 
		return NULL;
	
	xml = g_strndup (xml_in, size);
	r = w = xml;
	
	while (*r) {
		if (*r == '\n' || *r == '\r') {
			skip_white = TRUE;
		} else {
			gunichar c = g_utf8_get_char (r);
			gboolean is_space = g_unichar_isspace (c);
			
			*w = *r;
			
			if (!(skip_white && is_space))
				w++;
			if (!is_space)
				skip_white = FALSE;
		}
		r = g_utf8_next_char (r);
	}
	
	*w = '\0';
	
	return xml;
}

char *
e_destination_export (const EDestination *dest)
{
	xmlNodePtr dest_node;
	xmlDocPtr dest_doc;
	xmlChar *buffer = NULL;
	int size = -1;
	char *str;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	dest_node = e_destination_xml_encode (dest);
	if (dest_node == NULL)
		return NULL;
	
	dest_doc = xmlNewDoc (XML_DEFAULT_VERSION);
	xmlDocSetRootElement (dest_doc, dest_node);
	
	xmlDocDumpMemory (dest_doc, &buffer, &size);
	xmlFreeDoc (dest_doc);
	
	str = null_terminate_and_remove_extra_whitespace (buffer, size);
	xmlFree (buffer);
	
	return str;
}

EDestination *
e_destination_import (const char *str)
{
	EDestination *dest = NULL;
	xmlDocPtr dest_doc;

	if (!(str && *str))
		return NULL;
	
	dest_doc = xmlParseMemory ((char *) str, strlen (str));
	if (dest_doc && dest_doc->xmlRootNode) {
		dest = e_destination_new ();
		if (! e_destination_xml_decode (dest, dest_doc->xmlRootNode)) {
			g_object_unref (dest);
			dest = NULL;
		}
	}
	xmlFreeDoc (dest_doc);
	
	return dest;
}

char *
e_destination_exportv (EDestination **destv)
{
	xmlDocPtr destv_doc;
	xmlNodePtr destv_node;
	xmlChar *buffer = NULL;
	int i, size = -1;
	char *str;

	if (destv == NULL || *destv == NULL)
		return NULL;
	
	destv_doc  = xmlNewDoc (XML_DEFAULT_VERSION);
	destv_node = xmlNewNode (NULL, "destinations");
	xmlDocSetRootElement (destv_doc, destv_node);
	
	for (i = 0; destv[i]; i++) {
		if (! e_destination_empty (destv[i])) {
			xmlNodePtr dest_node = e_destination_xml_encode (destv[i]);
			if (dest_node)
				xmlAddChild (destv_node, dest_node);
		}
	}
	
	xmlDocDumpMemory (destv_doc, &buffer, &size);
	xmlFreeDoc (destv_doc);
	
	str = null_terminate_and_remove_extra_whitespace (buffer, size);
	xmlFree (buffer);
	
	return str;
}

EDestination **
e_destination_importv (const char *str)
{
	GPtrArray *dest_array = NULL;
	xmlDocPtr destv_doc;
	xmlNodePtr node;
	EDestination **destv = NULL;
	
	if (!(str && *str))
		return NULL;
	
	destv_doc = xmlParseMemory ((char *)str, strlen (str));
	if (destv_doc == NULL)
		return NULL;
	
	node = destv_doc->xmlRootNode;
	
	if (strcmp (node->name, "destinations"))
		goto finished;
	
	node = node->xmlChildrenNode;
	
	dest_array = g_ptr_array_new ();
	
	while (node) {
		EDestination *dest;
		
		dest = e_destination_new ();
		if (e_destination_xml_decode (dest, node) && !e_destination_empty (dest)) {
			g_ptr_array_add (dest_array, dest);
		} else {
			g_object_unref (dest);
		}
		
		node = node->next;
	}
	
	/* we need destv to be NULL terminated */
	g_ptr_array_add (dest_array, NULL);
	
	destv = (EDestination **) dest_array->pdata;
	g_ptr_array_free (dest_array, FALSE);
	
 finished:
	xmlFreeDoc (destv_doc);
	
	return destv;
}

void
e_destination_freev (EDestination **destv)
{
	int i;
	
	if (destv) {
		for (i = 0; destv[i] != NULL; ++i) {
			g_object_unref (destv[i]);
		}
		g_free (destv);
	}

}

void
e_destination_export_to_vcard_attribute (EDestination *dest, EVCardAttribute *attr)
{
	e_vcard_attribute_remove_values (attr);
	e_vcard_attribute_remove_params (attr);

	if (e_destination_get_contact_uid (dest))
		e_vcard_attribute_add_param_with_value (attr,
							e_vcard_attribute_param_new (EVC_X_DEST_CONTACT_UID),
							e_destination_get_contact_uid (dest));
	if (e_destination_get_source_uid (dest))
		e_vcard_attribute_add_param_with_value (attr,
							e_vcard_attribute_param_new (EVC_X_DEST_SOURCE_UID),
							e_destination_get_source_uid (dest));
	if (-1 != e_destination_get_email_num (dest)) {
		char buf[10];
		g_snprintf (buf, sizeof (buf), "%d", e_destination_get_email_num (dest));
		e_vcard_attribute_add_param_with_value (attr,
							e_vcard_attribute_param_new (EVC_X_DEST_EMAIL_NUM),
							buf);
	}
	if (e_destination_get_name (dest))
		e_vcard_attribute_add_param_with_value (attr,
							e_vcard_attribute_param_new (EVC_X_DEST_NAME),
							e_destination_get_name (dest));
	if (e_destination_get_email (dest))
		e_vcard_attribute_add_param_with_value (attr,
							e_vcard_attribute_param_new (EVC_X_DEST_EMAIL),
							e_destination_get_email (dest));
	e_vcard_attribute_add_param_with_value (attr,
						e_vcard_attribute_param_new (EVC_X_DEST_HTML_MAIL),
						e_destination_get_html_mail_pref (dest) ? "TRUE" : "FALSE");

	if (e_destination_get_address (dest))
		e_vcard_attribute_add_value (attr, e_destination_get_address (dest));
}
