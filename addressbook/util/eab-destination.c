/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * eab-destination.c
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
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

#include <config.h>
#include "eab-destination.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "ebook/e-book.h"
#include "eab-marshal.h"
#include "eab-book-util.h"
#include <gal/widgets/e-unicode.h>

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <camel/camel-internet-address.h>

#define d(x)

enum {
	CHANGED,
	CONTACT_LOADED,
	LAST_SIGNAL
};

guint eab_destination_signals[LAST_SIGNAL] = { 0 };

struct _EABDestinationPrivate {
	gchar *raw;

	gchar *book_uri;
	gchar *uid;
	EContact *contact;
	gint email_num;

	gchar *name;
	gchar *email;
	gchar *addr;
	gchar *textrep;

	GList *list_dests;

	guint html_mail_override : 1;
	guint wants_html_mail : 1;

	guint show_addresses : 1;

	guint contact_loaded : 1;
	guint cannot_load : 1;
	guint auto_recipient : 1;
	guint pending_contact_load;

	guint pending_change : 1;

	EBook *book;

	gint freeze_count;
};

static void eab_destination_clear_contact    (EABDestination *);
static void eab_destination_clear_strings (EABDestination *);

/* the following prototypes were in e-destination.h, but weren't used
   by anything in evolution...  let's make them private for now. */
static gboolean       eab_destination_is_valid           (const EABDestination *);
static void           eab_destination_set_contact_uid    (EABDestination *, const gchar *uid, gint email_num); 
static void           eab_destination_set_book_uri       (EABDestination *, const gchar *uri); 
static gboolean       eab_destination_from_contact       (const EABDestination *);
static const gchar   *eab_destination_get_book_uri       (const EABDestination *);
static const gchar   *eab_destination_get_contact_uid    (const EABDestination *);
static xmlNodePtr     eab_destination_xml_encode         (const EABDestination *dest);
static gboolean       eab_destination_xml_decode         (EABDestination *dest, xmlNodePtr node);

static GObjectClass *parent_class;

static void
eab_destination_dispose (GObject *obj)
{
	EABDestination *dest = EAB_DESTINATION (obj);

	if (dest->priv) {
		eab_destination_clear (dest);

		if (dest->priv->book)
			g_object_unref (dest->priv->book);

		g_free (dest->priv);
		dest->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (obj);
}

static void
eab_destination_class_init (EABDestinationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->dispose = eab_destination_dispose;

	eab_destination_signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABDestinationClass, changed),
			      NULL, NULL,
			      eab_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	eab_destination_signals[CONTACT_LOADED] =
		g_signal_new ("contact_loaded",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABDestinationClass, contact_loaded),
			      NULL, NULL,
			      eab_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

static void
eab_destination_init (EABDestination *dest)
{
	dest->priv = g_new0 (struct _EABDestinationPrivate, 1);

	dest->priv->cannot_load = FALSE;
	dest->priv->auto_recipient = FALSE;
	dest->priv->pending_contact_load = 0;
}

GType
eab_destination_get_type (void)
{
	static GType dest_type = 0;

	if (!dest_type) {
		GTypeInfo dest_info = {
			sizeof (EABDestinationClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  eab_destination_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EABDestination),
			0,    /* n_preallocs */
			(GInstanceInitFunc) eab_destination_init
		};

		dest_type = g_type_register_static (G_TYPE_OBJECT, "EABDestination", &dest_info, 0);
	}

	return dest_type;
}

EABDestination *
eab_destination_new (void)
{
	return g_object_new (EAB_TYPE_DESTINATION, NULL);
}

static void
eab_destination_freeze (EABDestination *dest)
{
	g_return_if_fail (EAB_IS_DESTINATION (dest));
	g_return_if_fail (dest->priv->freeze_count >= 0);
	
	dest->priv->freeze_count++;
}

static void
eab_destination_thaw (EABDestination *dest)
{
	g_return_if_fail (EAB_IS_DESTINATION (dest));
	g_return_if_fail (dest->priv->freeze_count > 0);
	
	dest->priv->freeze_count--;
	if (dest->priv->freeze_count == 0 && dest->priv->pending_change)
		eab_destination_changed (dest);
}

void
eab_destination_changed (EABDestination *dest)
{
	if (dest->priv->freeze_count == 0) {
		g_signal_emit (dest, eab_destination_signals[CHANGED], 0);
		dest->priv->pending_change = FALSE;
		dest->priv->cannot_load = FALSE;
	
	} else {
		dest->priv->pending_change = TRUE;
	}
}

EABDestination *
eab_destination_copy (const EABDestination *dest)
{
	EABDestination *new_dest;
	GList *iter;

	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);

	new_dest = eab_destination_new ();

        new_dest->priv->book_uri           = g_strdup (dest->priv->book_uri);
        new_dest->priv->uid                = g_strdup (dest->priv->uid);
        new_dest->priv->name               = g_strdup (dest->priv->name);
        new_dest->priv->email              = g_strdup (dest->priv->email);
        new_dest->priv->addr               = g_strdup (dest->priv->addr);
        new_dest->priv->email_num          = dest->priv->email_num;

	new_dest->priv->contact     = dest->priv->contact;
	if (new_dest->priv->contact)
		g_object_ref (new_dest->priv->contact);

	new_dest->priv->html_mail_override = dest->priv->html_mail_override;
	new_dest->priv->wants_html_mail    = dest->priv->wants_html_mail;

	for (iter = dest->priv->list_dests; iter != NULL; iter = g_list_next (iter)) {
		new_dest->priv->list_dests = g_list_append (new_dest->priv->list_dests,
							    eab_destination_copy (EAB_DESTINATION (iter->data)));
	}

	return new_dest;
}

static void
eab_destination_clear_contact (EABDestination *dest)
{
	g_free (dest->priv->book_uri);
	dest->priv->book_uri = NULL;
	g_free (dest->priv->uid);
	dest->priv->uid = NULL;
	
	dest->priv->contact = NULL;
	dest->priv->email_num = -1;
	
	g_list_foreach (dest->priv->list_dests, (GFunc) g_object_unref, NULL);
	g_list_free (dest->priv->list_dests);
	dest->priv->list_dests = NULL;
	
	dest->priv->cannot_load = FALSE;
	
	eab_destination_cancel_contact_load (dest);
	
	eab_destination_changed (dest);
}

static void
eab_destination_clear_strings (EABDestination *dest)
{
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
	
	eab_destination_changed (dest);
}

void
eab_destination_clear (EABDestination *dest)
{
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	
	eab_destination_freeze (dest);
	
	eab_destination_clear_contact (dest);
	eab_destination_clear_strings (dest);
	
	eab_destination_thaw (dest);
}

static gboolean
nonempty (const gchar *s)
{
	gunichar c;
	while (*s) {
		c = g_utf8_get_char (s);
		if (!g_unichar_isspace (c))
			return TRUE;
		s = g_utf8_next_char (s);
	}
	return FALSE;
}

gboolean
eab_destination_is_empty (const EABDestination *dest)

{
	struct _EABDestinationPrivate *p;
	
	g_return_val_if_fail (EAB_IS_DESTINATION (dest), TRUE);
	
	p = dest->priv;
	
	return !(p->contact != NULL
		 || (p->book_uri && *p->book_uri)
		 || (p->uid && *p->uid)
		 || (p->raw && nonempty (p->raw))
		 || (p->name && nonempty (p->name))
		 || (p->email && nonempty (p->email))
		 || (p->addr && nonempty (p->addr))
		 || (p->list_dests != NULL));
}

gboolean
eab_destination_is_valid (const EABDestination *dest)
{
	const char *email;
	
	g_return_val_if_fail (EAB_IS_DESTINATION (dest), FALSE);
	
	if (eab_destination_from_contact (dest))
		return TRUE;
	
	email = eab_destination_get_email (dest);
	
	/* FIXME: if we really wanted to get fancy here, we could
           check to make sure that the address was valid according to
           rfc822's addr-spec grammar. */
	
	return email && *email && strchr (email, '@');
}

gboolean
eab_destination_equal (const EABDestination *a, const EABDestination *b)
{
	const struct _EABDestinationPrivate *pa, *pb;
	const char *na, *nb;
	
	g_return_val_if_fail (EAB_IS_DESTINATION (a), FALSE);
	g_return_val_if_fail (EAB_IS_DESTINATION (b), FALSE);
	
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
	na = eab_destination_get_name (a);
	nb = eab_destination_get_name (b);
	if ((na || nb) && !(na && nb && ! e_utf8_casefold_collate (na, nb)))
		return FALSE;
	
	if (!g_ascii_strcasecmp (eab_destination_get_email (a), eab_destination_get_email (b)))
		return TRUE;
	else
		return FALSE;
}

void
eab_destination_set_contact (EABDestination *dest, EContact *contact, gint email_num)
{
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	g_return_if_fail (contact && E_IS_CONTACT (contact));
	
	if (dest->priv->contact != contact || dest->priv->email_num != email_num) {
		/* We have to freeze/thaw around these operations so that the 'changed'
		   signals don't cause the EABDestination's internal state to be altered
		   before we can finish setting ->contact && ->email_num. */
		eab_destination_freeze (dest);
		eab_destination_clear (dest);
		
		dest->priv->contact = contact;
		g_object_ref (dest->priv->contact);
		
		dest->priv->email_num = email_num;
		
		eab_destination_changed (dest);
		eab_destination_thaw (dest);
	}
}

static void
eab_destination_set_book_uri (EABDestination *dest, const gchar *uri)
{
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	g_return_if_fail (uri != NULL);
	
	if (dest->priv->book_uri == NULL || strcmp (dest->priv->book_uri, uri)) {
		g_free (dest->priv->book_uri);
		dest->priv->book_uri = g_strdup (uri);
		
		eab_destination_changed (dest);
	}
}

void
eab_destination_set_contact_uid (EABDestination *dest, const gchar *uid, gint email_num)
{
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	g_return_if_fail (uid != NULL);
	
	if (dest->priv->uid == NULL
	    || strcmp (dest->priv->uid, uid)
	    || dest->priv->email_num != email_num) {
		
		g_free (dest->priv->uid);
		dest->priv->uid = g_strdup (uid);
		dest->priv->email_num = email_num;
		
		/* If we already have a contact, remove it unless it's uid matches the one
		   we just set. */
		if (dest->priv->contact && strcmp (uid,
						   e_contact_get_const (dest->priv->contact, E_CONTACT_UID))) {
			g_object_unref (dest->priv->contact);
			dest->priv->contact = NULL;
		}
		
		eab_destination_changed (dest);
	}
}

void
eab_destination_set_name (EABDestination *dest, const gchar *name)
{
	gboolean changed = FALSE;
	
	g_return_if_fail (EAB_IS_DESTINATION (dest));
	
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
		eab_destination_changed (dest);
	}
}

void
eab_destination_set_email (EABDestination *dest, const gchar *email)
{
	gboolean changed = FALSE;
	
	g_return_if_fail (EAB_IS_DESTINATION (dest));
	
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
		eab_destination_changed (dest);
	}
}

void
eab_destination_set_html_mail_pref (EABDestination *dest, gboolean x)
{
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	
	dest->priv->html_mail_override = TRUE;
	if (dest->priv->wants_html_mail != x) {
		dest->priv->wants_html_mail = x;
		eab_destination_changed (dest);
	}
}

gboolean
eab_destination_contains_contact (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), FALSE);
	return dest->priv->contact != NULL;
}

gboolean
eab_destination_from_contact (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), FALSE);
	return dest->priv->contact != NULL || dest->priv->book_uri != NULL || dest->priv->uid != NULL;
}

gboolean
eab_destination_is_auto_recipient (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), FALSE);
	
	return dest->priv->auto_recipient;
}

void
eab_destination_set_auto_recipient (EABDestination *dest, gboolean value)
{
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	
	dest->priv->auto_recipient = value;
}

typedef struct _UseContact UseContact;
struct _UseContact {
	EABDestination *dest;
	EABDestinationContactCallback cb;
	gpointer closure;
};

static void
use_contact_cb (EContact *contact, gpointer closure)
{
	UseContact *uc = (UseContact *) closure;
	
	if (contact != NULL && uc->dest->priv->contact == NULL) {
		uc->dest->priv->contact = contact;
		g_object_ref (uc->dest->priv->contact);
		eab_destination_changed (uc->dest);
	}
	
	if (uc->cb) {
		uc->cb (uc->dest, uc->dest->priv->contact, uc->closure);
	}
	
	/* We held a copy of the destination during the callback. */
	g_object_unref (uc->dest);
	g_free (uc);
}

void
eab_destination_use_contact (EABDestination *dest, EABDestinationContactCallback cb, gpointer closure)
{
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	
	if (dest->priv->contact != NULL) {
		if (cb)
			cb (dest, dest->priv->contact, closure);
	} else if (dest->priv->book_uri != NULL && dest->priv->uid != NULL) {
		UseContact *uc = g_new (UseContact, 1);
		
		uc->dest = dest;
		/* Hold a reference to the destination during the callback. */
		g_object_ref (uc->dest);
		uc->cb = cb;
		uc->closure = closure;
#if notyet
		e_contact_load_uri (dest->priv->book_uri, dest->priv->uid, use_contact_cb, uc);
#endif
	} else {
		if (cb)
			cb (dest, NULL, closure);
	}
}

EContact *
eab_destination_get_contact (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	return dest->priv->contact;
}

const gchar *
eab_destination_get_contact_uid (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	if (dest->priv->uid)
		return dest->priv->uid;
	
	if (dest->priv->contact)
		return e_contact_get_const (dest->priv->contact, E_CONTACT_UID);
	
	return NULL;
}

const gchar *
eab_destination_get_book_uri (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	return dest->priv->book_uri;
}

gint
eab_destination_get_email_num (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), -1);
	
	if (dest->priv->contact == NULL && (dest->priv->book_uri == NULL || dest->priv->uid == NULL))
		return -1;
	
	return dest->priv->email_num;
}

const gchar *
eab_destination_get_name (const EABDestination *dest)
{
	struct _EABDestinationPrivate *priv;
	
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EABDestinationPrivate *)dest->priv; /* cast out const */
	
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
					priv->name = g_strdup (eab_destination_get_email (dest));
			}
		} else if (priv->raw != NULL) {
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

const gchar *
eab_destination_get_email (const EABDestination *dest)
{
	struct _EABDestinationPrivate *priv;
	
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EABDestinationPrivate *)dest->priv; /* cast out const */
	
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
				const gchar *camel_email = NULL;
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

const gchar *
eab_destination_get_address (const EABDestination *dest)
{
	struct _EABDestinationPrivate *priv;
	
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EABDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->addr == NULL) {
		CamelInternetAddress *addr = camel_internet_address_new ();
		
		if (eab_destination_is_evolution_list (dest)) {
			GList *iter = dest->priv->list_dests;
			
			while (iter) {
				EABDestination *list_dest = EAB_DESTINATION (iter->data);
				
				if (!eab_destination_is_empty (list_dest)) {
					camel_internet_address_add (addr, 
								    eab_destination_get_name (list_dest),
								    eab_destination_get_email (list_dest));
				}
				iter = g_list_next (iter);
			}
			
			priv->addr = camel_address_encode (CAMEL_ADDRESS (addr));
		} else if (priv->raw) {

			if (camel_address_unformat (CAMEL_ADDRESS (addr), priv->raw)) {
				priv->addr = camel_address_encode (CAMEL_ADDRESS (addr));
			}
		} else {
			camel_internet_address_add (addr,
						    eab_destination_get_name (dest),
						    eab_destination_get_email (dest));
			
			priv->addr = camel_address_encode (CAMEL_ADDRESS (addr));
		}
		
		camel_object_unref (CAMEL_OBJECT (addr));
	}
	
	return priv->addr;
}

void
eab_destination_set_raw (EABDestination *dest, const gchar *raw)
{
	g_return_if_fail (EAB_IS_DESTINATION (dest));
	g_return_if_fail (raw != NULL);
	
	if (dest->priv->raw == NULL || strcmp (dest->priv->raw, raw)) {
		eab_destination_freeze (dest);
		
		eab_destination_clear (dest);
		dest->priv->raw = g_strdup (raw);
		eab_destination_changed (dest);
		
		eab_destination_thaw (dest);
	}
}

const gchar *
eab_destination_get_textrep (const EABDestination *dest, gboolean include_email)
{
	const char *name, *email;
	
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	if (dest->priv->raw)
		return dest->priv->raw;
	
	name  = eab_destination_get_name (dest);
	email = eab_destination_get_email (dest);
	
	if (eab_destination_from_contact (dest) && name != NULL && (!include_email || !email || !*email))
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
eab_destination_is_evolution_list (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), FALSE);
	
	if (dest->priv->list_dests == NULL
	    && dest->priv->contact != NULL
	    && e_contact_get (dest->priv->contact, E_CONTACT_IS_LIST)) {
		GList *email = e_contact_get (dest->priv->contact, E_CONTACT_EMAIL);
		if (email) {
			GList *iter;
			for (iter = email; iter; iter = iter->next) {
				EABDestination *list_dest = eab_destination_import ((char *) iter->data);

				if (list_dest)
					dest->priv->list_dests = g_list_append (dest->priv->list_dests, list_dest);
			}
		}
	}
	
	return dest->priv->list_dests != NULL;
}

gboolean
eab_destination_list_show_addresses (const EABDestination *dest)
{
	g_return_val_if_fail (EAB_IS_DESTINATION (dest), FALSE);
	
	if (dest->priv->contact != NULL)
		return GPOINTER_TO_UINT (e_contact_get (dest->priv->contact, E_CONTACT_LIST_SHOW_ADDRESSES));
	
	return dest->priv->show_addresses;
}

gboolean
eab_destination_get_html_mail_pref (const EABDestination *dest)
{
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), FALSE);
	
	if (dest->priv->html_mail_override || dest->priv->contact == NULL)
		return dest->priv->wants_html_mail;
	
	return e_contact_get (dest->priv->contact, E_CONTACT_WANTS_HTML) ? TRUE : FALSE;
}

static void
set_book (EABDestination *dest, EBook *book)
{
	if (dest->priv->book && dest->priv->book != book) {
		g_object_unref (dest->priv->book);
	}
		
	dest->priv->book = book;
	
	if (book)
		g_object_ref (book);
}

static void
name_and_email_cb (EBook *book, EBookStatus status, GList *contacts, gpointer closure)
{
	EABDestination *dest = EAB_DESTINATION (closure);
	
	if (status == E_BOOK_ERROR_OK && g_list_length ((GList *) contacts) == 1) {
		EContact *contact = E_CONTACT (contacts->data);
		const char *email = eab_destination_get_email (dest);
		int email_num = 0;
		
#if notyet
		if (eab_destination_is_valid (dest) && email && *email) {
			email_num = e_contact_email_find_number (contact, eab_destination_get_email (dest));
		}
#endif
		
		if (email_num >= 0) {
			const char *book_uri;
			
			book_uri = e_book_get_uri (book);
			
			dest->priv->contact_loaded = TRUE;
			eab_destination_set_contact (dest, contact, email_num);
			eab_destination_set_book_uri (dest, book_uri);
			g_signal_emit (dest, eab_destination_signals[CONTACT_LOADED], 0);
		}
	}
	
	if (!dest->priv->contact_loaded)
		dest->priv->cannot_load = TRUE;
	
	g_object_unref (dest); /* drop the reference held by the query */
}


static void
nickname_cb (EBook *book, EBookStatus status, GList *contacts, gpointer closure)
{
	EABDestination *dest = EAB_DESTINATION (closure);
	
	if (status == E_BOOK_ERROR_OK) {
		if (g_list_length ((GList *) contacts) == 1) {
			const char *book_uri;
			
			book_uri = e_book_get_uri (book);
			
			dest->priv->contact_loaded = TRUE;
			eab_destination_set_contact (dest, E_CONTACT (contacts->data), 0); /* Uses primary e-mail by default. */
			eab_destination_set_book_uri (dest, book_uri);
			g_signal_emit (dest, eab_destination_signals[CONTACT_LOADED], 0);
			
			g_object_unref (dest); /* drop the reference held by the query */
			
		} else {
			/* We can only end up here if we don't look at all like an e-mail address, so
			   we do a name-only query on the textrep */
			
			eab_name_and_email_query (book,
						  eab_destination_get_textrep (dest, FALSE),
						  NULL,
						  name_and_email_cb,
						  dest);
		}
	} else {
		/* Something went wrong with the query: drop our ref to the destination and return. */
		g_object_unref (dest);
	}
}

static void
launch_load_contact_query (EABDestination *dest)
{
	if (! eab_destination_is_valid (dest)) {
		/* If it doesn't look like an e-mail address, see if it is a nickname. */
		eab_nickname_query (dest->priv->book,
				    eab_destination_get_textrep (dest, FALSE),
				    nickname_cb,
				    dest);

	} else {
		eab_name_and_email_query (dest->priv->book,
					  eab_destination_get_name (dest),
					  eab_destination_get_email (dest),
					  name_and_email_cb,
					  dest);
	}
}

void
eab_destination_load_contact (EABDestination *dest, EBook *book)
{
	g_return_if_fail (EAB_IS_DESTINATION (dest));
	g_return_if_fail (book == NULL || E_IS_BOOK (book));
	
	if (eab_destination_is_evolution_list (dest))
		return;
	
	if (eab_destination_contains_contact (dest))
		return;
	
	if (dest->priv->cannot_load)
		return;
	
	eab_destination_cancel_contact_load (dest);
	
	set_book (dest, book);
	
	/* Handle the case of an EABDestination containing a contact URL */
	if (eab_destination_contains_contact (dest)) {
		eab_destination_use_contact (dest, NULL, NULL);
		return;
	}
	
	/* We hold a reference to ourselves until our query is complete. */
	g_object_ref (dest);
	launch_load_contact_query (dest);
}

static int
do_load_delayed (gpointer ptr)
{
	EABDestination *dest = EAB_DESTINATION (ptr);
	
	eab_destination_load_contact (dest, dest->priv->book);
	return FALSE;
}

void
eab_destination_load_contact_delayed (EABDestination *dest, EBook *book, gint delay)
{
	g_return_if_fail (EAB_IS_DESTINATION (dest));
	g_return_if_fail (book == NULL || E_IS_BOOK (book));
	
	if (delay < 0)
		delay = 500;
	
	eab_destination_cancel_contact_load (dest);
	
	set_book (dest, book);
	
	dest->priv->pending_contact_load = g_timeout_add (delay, do_load_delayed, dest);
}

void
eab_destination_cancel_contact_load (EABDestination *dest)
{
	g_return_if_fail (EAB_IS_DESTINATION (dest));
	
	if (dest->priv->pending_contact_load) {
		g_source_remove (dest->priv->pending_contact_load);
		dest->priv->pending_contact_load = 0;
	}
}

gboolean
eab_destination_unload_contact (EABDestination *dest)
{
	char *email;
	
	g_return_val_if_fail (EAB_IS_DESTINATION (dest), FALSE);
	
	if (!eab_destination_contains_contact (dest))
		return FALSE;
	
	email = g_strdup (eab_destination_get_email (dest));
	
	if (email == NULL)
		return FALSE;
	
	eab_destination_freeze (dest);
	eab_destination_clear (dest);
	eab_destination_set_raw (dest, email);
	g_free (email);
	eab_destination_thaw (dest);
	
	return TRUE;
}

/*
 * Destination import/export
 */

gchar *
eab_destination_get_address_textv (EABDestination **destv)
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
		g_return_val_if_fail (EAB_IS_DESTINATION (destv[len]), NULL);
		len++;
	}
	
	strv = g_new0 (char *, len + 1);
	for (i = 0, j = 0; destv[i]; i++) {
		if (!eab_destination_is_empty (destv[i])) {
			const char *addr = eab_destination_get_address (destv[i]);
			strv[j++] = addr ? (char *) addr : "";
		}
	}
	
	str = g_strjoinv (", ", strv);
	
	g_free (strv);
	
	return str;
}

xmlNodePtr
eab_destination_xml_encode (const EABDestination *dest)
{
	xmlNodePtr dest_node;
	const char *str;
	
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	dest_node = xmlNewNode (NULL, "destination");
	
	str = eab_destination_get_name (dest);
	if (str)
		xmlNewTextChild (dest_node, NULL, "name", str);
	
	if (!eab_destination_is_evolution_list (dest)) {
		str = eab_destination_get_email (dest);
		if (str)
			xmlNewTextChild (dest_node, NULL, "email", str);
	} else {
		GList *iter = dest->priv->list_dests;
		
		while (iter) {
			EABDestination *list_dest = EAB_DESTINATION (iter->data);
			xmlNodePtr list_node = xmlNewNode (NULL, "list_entry");
			
			str = eab_destination_get_name (list_dest);
			if (str)
				xmlNewTextChild (list_node, NULL, "name", str);
			
			str = eab_destination_get_email (list_dest);
			if (str)
				xmlNewTextChild (list_node, NULL, "email", str);
			
			xmlAddChild (dest_node, list_node);
			
			iter = g_list_next (iter);
		}
		
		xmlNewProp (dest_node, "is_list", "yes");
		xmlNewProp (dest_node, "show_addresses", 
			    eab_destination_list_show_addresses (dest) ? "yes" : "no");
	}
	
	str = eab_destination_get_book_uri (dest);
	if (str) {
		xmlNewTextChild (dest_node, NULL, "book_uri", str);
	}
	
	str = eab_destination_get_contact_uid (dest);
	if (str) {
		char buf[16];
		
		xmlNodePtr uri_node = xmlNewTextChild (dest_node, NULL, "card_uid", str);
		g_snprintf (buf, 16, "%d", eab_destination_get_email_num (dest));
		xmlNewProp (uri_node, "email_num", buf);
	}
	
	xmlNewProp (dest_node, "html_mail", eab_destination_get_html_mail_pref (dest) ? "yes" : "no");
	
	xmlNewProp (dest_node, "auto_recipient",
		    eab_destination_is_auto_recipient (dest) ? "yes" : "no");
	
	return dest_node;
}

gboolean
eab_destination_xml_decode (EABDestination *dest, xmlNodePtr node)
{
	char *name = NULL, *email = NULL, *book_uri = NULL, *card_uid = NULL;
	gboolean is_list = FALSE, show_addr = FALSE, auto_recip = FALSE;
	gboolean html_mail = FALSE;
	GList *list_dests = NULL;
	int email_num = -1;
	char *tmp;
	
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), FALSE);
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
				EABDestination *list_dest = eab_destination_new ();
				
				if (list_name)
					eab_destination_set_name (list_dest, list_name);
				if (list_email)
					eab_destination_set_email (list_dest, list_email);
				
				g_free (list_name);
				g_free (list_email);
				
				list_dests = g_list_append (list_dests, list_dest);
			}
		} else if (!strcmp (node->name, "book_uri")) {
			tmp = xmlNodeGetContent (node);
			g_free (book_uri);
			book_uri = g_strdup (tmp);
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
	
	eab_destination_freeze (dest);
	
	eab_destination_clear (dest);
	
	if (name) {
		eab_destination_set_name (dest, name);
		g_free (name);
	}
	if (email) {
		eab_destination_set_email (dest, email);
		g_free (email);
	}
	if (book_uri) {
		eab_destination_set_book_uri (dest, book_uri);
		g_free (book_uri);
	}
	if (card_uid) {
		eab_destination_set_contact_uid (dest, card_uid, email_num);
		g_free (card_uid);
	}
	if (list_dests)
		dest->priv->list_dests = list_dests;
	
	dest->priv->html_mail_override = TRUE;
	dest->priv->wants_html_mail = html_mail;
	
	dest->priv->show_addresses = show_addr;
	
	dest->priv->auto_recipient = auto_recip;
	
	eab_destination_thaw (dest);
	
	return TRUE;
}

/* FIXME: Make utf-8 safe */
static gchar *
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
			gboolean is_space = isspace (*r);
			
			*w = *r;
			
			if (!(skip_white && is_space))
				w++;
			if (!is_space)
				skip_white = FALSE;
		}
		r++;
	}
	
	*w = '\0';
	
	return xml;
}

gchar *
eab_destination_export (const EABDestination *dest)
{
	xmlNodePtr dest_node;
	xmlDocPtr dest_doc;
	xmlChar *buffer = NULL;
	int size = -1;
	char *str;
	
	g_return_val_if_fail (dest && EAB_IS_DESTINATION (dest), NULL);
	
	dest_node = eab_destination_xml_encode (dest);
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

EABDestination *
eab_destination_import (const gchar *str)
{
	EABDestination *dest = NULL;
	xmlDocPtr dest_doc;
	
	if (!(str && *str))
		return NULL;
	
	dest_doc = xmlParseMemory ((char *) str, strlen (str));
	if (dest_doc && dest_doc->xmlRootNode) {
		dest = eab_destination_new ();
		if (! eab_destination_xml_decode (dest, dest_doc->xmlRootNode)) {
			g_object_unref (dest);
			dest = NULL;
		}
	}
	xmlFreeDoc (dest_doc);
	
	return dest;
}

gchar *
eab_destination_exportv (EABDestination **destv)
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
		if (! eab_destination_is_empty (destv[i])) {
			xmlNodePtr dest_node = eab_destination_xml_encode (destv[i]);
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

EABDestination **
eab_destination_importv (const gchar *str)
{
	GPtrArray *dest_array = NULL;
	xmlDocPtr destv_doc;
	xmlNodePtr node;
	EABDestination **destv = NULL;
	
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
		EABDestination *dest;
		
		dest = eab_destination_new ();
		if (eab_destination_xml_decode (dest, node) && !eab_destination_is_empty (dest)) {
			g_ptr_array_add (dest_array, dest);
		} else {
			g_object_unref (dest);
		}
		
		node = node->next;
	}
	
	/* we need destv to be NULL terminated */
	g_ptr_array_add (dest_array, NULL);
	
	destv = (EABDestination **) dest_array->pdata;
	g_ptr_array_free (dest_array, FALSE);
	
 finished:
	xmlFreeDoc (destv_doc);
	
	return destv;
}

EABDestination **
eab_destination_list_to_vector_sized (GList *list, int n)
{
	EABDestination **destv;
	int i = 0;
	
	if (n == -1)
		n = g_list_length (list);
	
	if (n == 0)
		return NULL;
	
	destv = g_new (EABDestination *, n + 1);
	while (list != NULL && i < n) {
		destv[i] = EAB_DESTINATION (list->data);
		list->data = NULL;
		i++;
		list = g_list_next (list);
	}
	destv[i] = NULL;
	
	return destv;
}

EABDestination **
eab_destination_list_to_vector (GList *list)
{
	return eab_destination_list_to_vector_sized (list, -1);
}

void
eab_destination_freev (EABDestination **destv)
{
	int i;
	
	if (destv) {
		for (i = 0; destv[i] != NULL; ++i) {
			g_object_unref (destv[i]);
		}
		g_free (destv);
	}

}

#if notyet
static void
touch_cb (EBook *book, const gchar *addr, ECard *card, gpointer closure)
{
	if (book != NULL && card != NULL) {
		e_card_touch (card);
		d(g_message ("Use score for \"%s\" is now %f", addr, e_card_get_use_score (card)));
		e_book_commit_card (book, card, NULL, NULL);
	}
}
#endif

void
eab_destination_touch (EABDestination *dest)
{
#if notyet
	const char *email;
	
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	
	if (!eab_destination_is_auto_recipient (dest)) {
		email = eab_destination_get_email (dest);
		
		if (email)
			e_book_query_address_default (email, touch_cb, NULL);
	}
#endif
}

void
eab_destination_touchv (EABDestination **destv)
{
#if notyet
	int i;
	
	g_return_if_fail (destv != NULL);
	
	for (i = 0; destv[i] != NULL; ++i) {
		eab_destination_touch (destv[i]);
	}
#endif
}
