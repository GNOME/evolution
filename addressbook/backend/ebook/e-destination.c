/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-destination.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
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
#include "e-destination.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "e-book.h"
#include "e-book-marshal.h"
#include "e-book-util.h"
#include <gal/widgets/e-unicode.h>

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <camel/camel-internet-address.h>

#define d(x)

enum {
	CHANGED,
	CARDIFIED,
	LAST_SIGNAL
};

guint e_destination_signals[LAST_SIGNAL] = { 0 };

struct _EDestinationPrivate {
	gchar *raw;

	gchar *book_uri;
	gchar *card_uid;
	ECard *card;
	gint card_email_num;

	ECard *old_card;
	gint old_card_email_num;
	gchar *old_textrep;

	gchar *name;
	gchar *email;
	gchar *addr;
	gchar *textrep;

	GList *list_dests;

	guint html_mail_override : 1;
	guint wants_html_mail : 1;

	guint show_addresses : 1;

	guint has_been_cardified : 1;
	guint allow_cardify : 1;
	guint cannot_cardify : 1;
	guint auto_recipient : 1;
	guint pending_cardification;

	guint pending_change : 1;

	EBook *cardify_book;

	gint freeze_count;
};

static void e_destination_clear_card    (EDestination *);
static void e_destination_clear_strings (EDestination *);

static GObjectClass *parent_class;

static void
e_destination_dispose (GObject *obj)
{
	EDestination *dest = E_DESTINATION (obj);

	if (dest->priv) {
		e_destination_clear (dest);

		if (dest->priv->old_card)
			g_object_unref (dest->priv->old_card);
	
		if (dest->priv->cardify_book)
			g_object_unref (dest->priv->cardify_book);

		g_free (dest->priv->old_textrep);

		g_free (dest->priv);
		dest->priv = NULL;
	}
}

static void
e_destination_class_init (EDestinationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->dispose = e_destination_dispose;

	e_destination_signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EDestinationClass, changed),
			      NULL, NULL,
			      e_book_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_destination_signals[CARDIFIED] =
		g_signal_new ("cardified",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EDestinationClass, cardified),
			      NULL, NULL,
			      e_book_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

static void
e_destination_init (EDestination *dest)
{
	dest->priv = g_new0 (struct _EDestinationPrivate, 1);

	dest->priv->allow_cardify = TRUE;
	dest->priv->cannot_cardify = FALSE;
	dest->priv->auto_recipient = FALSE;
	dest->priv->pending_cardification = 0;
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

static void
e_destination_freeze (EDestination *dest)
{
	g_return_if_fail (E_IS_DESTINATION (dest));
	g_return_if_fail (dest->priv->freeze_count >= 0);
	
	dest->priv->freeze_count++;
}

static void
e_destination_thaw (EDestination *dest)
{
	g_return_if_fail (E_IS_DESTINATION (dest));
	g_return_if_fail (dest->priv->freeze_count > 0);
	
	dest->priv->freeze_count--;
	if (dest->priv->freeze_count == 0 && dest->priv->pending_change)
		e_destination_changed (dest);
}

void
e_destination_changed (EDestination *dest)
{
	if (dest->priv->freeze_count == 0) {
		g_signal_emit (dest, e_destination_signals[CHANGED], 0);
		dest->priv->pending_change = FALSE;
		dest->priv->cannot_cardify = FALSE;
	
	} else {
		dest->priv->pending_change = TRUE;
	}
}

EDestination *
e_destination_copy (const EDestination *dest)
{
	EDestination *new_dest;
	GList *iter;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	new_dest = e_destination_new ();

	new_dest->priv->book_uri           = g_strdup (dest->priv->book_uri);
	new_dest->priv->card_uid           = g_strdup (dest->priv->card_uid);
	new_dest->priv->name               = g_strdup (dest->priv->name);
	new_dest->priv->email              = g_strdup (dest->priv->email);
	new_dest->priv->addr               = g_strdup (dest->priv->addr);
	new_dest->priv->card_email_num     = dest->priv->card_email_num;
	new_dest->priv->old_card_email_num = dest->priv->old_card_email_num;
	new_dest->priv->old_textrep        = g_strdup (dest->priv->old_textrep);

	new_dest->priv->card     = dest->priv->card;
	if (new_dest->priv->card)
		g_object_ref (new_dest->priv->card);

	new_dest->priv->old_card = dest->priv->old_card;
	if (new_dest->priv->old_card)
		g_object_ref (new_dest->priv->old_card);

	new_dest->priv->html_mail_override = dest->priv->html_mail_override;
	new_dest->priv->wants_html_mail    = dest->priv->wants_html_mail;

	for (iter = dest->priv->list_dests; iter != NULL; iter = g_list_next (iter)) {
		new_dest->priv->list_dests = g_list_append (new_dest->priv->list_dests,
							    e_destination_copy (E_DESTINATION (iter->data)));
	}

	return new_dest;
}

static void
e_destination_clear_card (EDestination *dest)
{
	if (dest->priv->card) {
		if (dest->priv->old_card)
			g_object_unref (dest->priv->old_card);
		
		dest->priv->old_card = dest->priv->card;
		dest->priv->old_card_email_num = dest->priv->card_email_num;
		
		g_free (dest->priv->old_textrep);
		dest->priv->old_textrep = g_strdup (e_destination_get_textrep (dest, FALSE));
	}
	
	g_free (dest->priv->book_uri);
	dest->priv->book_uri = NULL;
	g_free (dest->priv->card_uid);
	dest->priv->card_uid = NULL;
	
	dest->priv->card = NULL;
	dest->priv->card_email_num = -1;
	
	g_list_foreach (dest->priv->list_dests, (GFunc) g_object_unref, NULL);
	g_list_free (dest->priv->list_dests);
	dest->priv->list_dests = NULL;
	
	dest->priv->allow_cardify = TRUE;
	dest->priv->cannot_cardify = FALSE;
	
	e_destination_cancel_cardify (dest);
	
	e_destination_changed (dest);
}

static void
e_destination_clear_strings (EDestination *dest)
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
	
	e_destination_changed (dest);
}

void
e_destination_clear (EDestination *dest)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	
	e_destination_freeze (dest);
	
	e_destination_clear_card (dest);
	e_destination_clear_strings (dest);
	
	e_destination_thaw (dest);
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
e_destination_is_empty (const EDestination *dest)

{
	struct _EDestinationPrivate *p;
	
	g_return_val_if_fail (E_IS_DESTINATION (dest), TRUE);
	
	p = dest->priv;
	
	return !(p->card != NULL
		 || (p->book_uri && *p->book_uri)
		 || (p->card_uid && *p->card_uid)
		 || (p->raw && nonempty (p->raw))
		 || (p->name && nonempty (p->name))
		 || (p->email && nonempty (p->email))
		 || (p->addr && nonempty (p->addr))
		 || (p->list_dests != NULL));
}

gboolean
e_destination_is_valid (const EDestination *dest)
{
	const char *email;
	
	g_return_val_if_fail (E_IS_DESTINATION (dest), FALSE);
	
	if (e_destination_from_card (dest))
		return TRUE;
	
	email = e_destination_get_email (dest);
	
	/* FIXME: if we really wanted to get fancy here, we could
           check to make sure that the address was valid according to
           rfc822's addr-spec grammar. */
	
	return email && *email && strchr (email, '@');
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
	
	/* Check equality of cards. */
	if (pa->card || pb->card) {
		if (! (pa->card && pb->card))
			return FALSE;
		
		if (pa->card == pb->card || !strcmp (e_card_get_id (pa->card), e_card_get_id (pb->card)))
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
e_destination_set_card (EDestination *dest, ECard *card, gint email_num)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (card && E_IS_CARD (card));
	
	if (dest->priv->card != card || dest->priv->card_email_num != email_num) {
		/* We have to freeze/thaw around these operations so that the 'changed'
		   signals don't cause the EDestination's internal state to be altered
		   before we can finish setting ->card && ->card_email_num. */
		e_destination_freeze (dest);
		e_destination_clear (dest);
		
		dest->priv->card = card;
		g_object_ref (dest->priv->card);
		
		dest->priv->card_email_num = email_num;
		
		e_destination_changed (dest);
		e_destination_thaw (dest);
	}
}

void
e_destination_set_book_uri (EDestination *dest, const gchar *uri)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (uri != NULL);
	
	if (dest->priv->book_uri == NULL || strcmp (dest->priv->book_uri, uri)) {
		g_free (dest->priv->book_uri);
		dest->priv->book_uri = g_strdup (uri);
		
		/* If we already have a card, remove it unless it's uri matches the one
		   we just set. */
		if (dest->priv->card) {
			EBook *book = e_card_get_book (dest->priv->card);
			if ((!book) || strcmp (uri, e_book_get_uri (book))) {
				g_object_unref (dest->priv->card);
				dest->priv->card = NULL;
			}
		}
		
		e_destination_changed (dest);
	}
}

void
e_destination_set_card_uid (EDestination *dest, const gchar *uid, gint email_num)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (uid != NULL);
	
	if (dest->priv->card_uid == NULL
	    || strcmp (dest->priv->card_uid, uid)
	    || dest->priv->card_email_num != email_num) {
		
		g_free (dest->priv->card_uid);
		dest->priv->card_uid = g_strdup (uid);
		dest->priv->card_email_num = email_num;
		
		/* If we already have a card, remove it unless it's uri matches the one
		   we just set. */
		if (dest->priv->card && strcmp (uid, e_card_get_id (dest->priv->card))) {
			g_object_unref (dest->priv->card);
			dest->priv->card = NULL;
		}
		
		e_destination_changed (dest);
	}
}

void
e_destination_set_name (EDestination *dest, const gchar *name)
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
		e_destination_changed (dest);
	}
}

void
e_destination_set_email (EDestination *dest, const gchar *email)
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
		e_destination_changed (dest);
	}
}

void
e_destination_set_html_mail_pref (EDestination *dest, gboolean x)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	
	dest->priv->html_mail_override = TRUE;
	if (dest->priv->wants_html_mail != x) {
		dest->priv->wants_html_mail = x;
		e_destination_changed (dest);
	}
}

gboolean
e_destination_contains_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	return dest->priv->card != NULL;
}

gboolean
e_destination_from_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	return dest->priv->card != NULL || dest->priv->book_uri != NULL || dest->priv->card_uid != NULL;
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

typedef struct _UseCard UseCard;
struct _UseCard {
	EDestination *dest;
	EDestinationCardCallback cb;
	gpointer closure;
};

static void
use_card_cb (ECard *card, gpointer closure)
{
	UseCard *uc = (UseCard *) closure;
	
	if (card != NULL && uc->dest->priv->card == NULL) {
		uc->dest->priv->card = card;
		g_object_ref (uc->dest->priv->card);
		e_destination_changed (uc->dest);
	}
	
	if (uc->cb) {
		uc->cb (uc->dest, uc->dest->priv->card, uc->closure);
	}
	
	/* We held a copy of the destination during the callback. */
	g_object_unref (uc->dest);
	g_free (uc);
}

void
e_destination_use_card (EDestination *dest, EDestinationCardCallback cb, gpointer closure)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	
	if (dest->priv->card != NULL) {
		if (cb)
			cb (dest, dest->priv->card, closure);
	} else if (dest->priv->book_uri != NULL && dest->priv->card_uid != NULL) {
		UseCard *uc = g_new (UseCard, 1);
		
		uc->dest = dest;
		/* Hold a reference to the destination during the callback. */
		g_object_ref (uc->dest);
		uc->cb = cb;
		uc->closure = closure;
		e_card_load_uri (dest->priv->book_uri, dest->priv->card_uid, use_card_cb, uc);
	} else {
		if (cb)
			cb (dest, NULL, closure);
	}
}

ECard *
e_destination_get_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	return dest->priv->card;
}

const gchar *
e_destination_get_card_uid (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	if (dest->priv->card_uid)
		return dest->priv->card_uid;
	
	if (dest->priv->card)
		return e_card_get_id (dest->priv->card);
	
	return NULL;
}

const gchar *
e_destination_get_book_uri (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	if (dest->priv->book_uri)
		return dest->priv->book_uri;
	
	if (dest->priv->card) {
		EBook *book = e_card_get_book (dest->priv->card);
		
		if (book) {
			return e_book_get_uri (book);
		}
	}
	
	return NULL;
}

gint
e_destination_get_email_num (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), -1);
	
	if (dest->priv->card == NULL && (dest->priv->book_uri == NULL || dest->priv->card_uid == NULL))
		return -1;
	
	return dest->priv->card_email_num;
}

const gchar *
e_destination_get_name (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->name == NULL) {
		if (priv->card != NULL) {
			priv->name = e_card_name_to_string (priv->card->name);
			
			if (priv->name == NULL || *priv->name == '\0') {
				g_free (priv->name);
				priv->name = g_strdup (priv->card->file_as);
			}
			
			if (priv->name == NULL || *priv->name == '\0') {
				g_free (priv->name);
				if (e_card_evolution_list (priv->card))
					priv->name = g_strdup (_("Unnamed List"));
				else
					priv->name = g_strdup (e_destination_get_email (dest));
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
e_destination_get_email (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->email == NULL) {
		if (priv->card != NULL) {
			/* Pull the address out of the card. */
			if (priv->card->email) {
				EIterator *iter = e_list_get_iterator (priv->card->email);
				int n = priv->card_email_num;
				
				if (n >= 0) {
					while (n > 0) {
						e_iterator_next (iter);
						n--;
					}
					
					if (e_iterator_is_valid (iter)) {
						gconstpointer ptr = e_iterator_get (iter);
						priv->email = g_strdup ((char *) ptr);
					}
				}
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
				
				if (!e_destination_is_empty (list_dest)) {
					camel_internet_address_add (addr, 
								    e_destination_get_name (list_dest),
								    e_destination_get_email (list_dest));
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
						    e_destination_get_name (dest),
						    e_destination_get_email (dest));
			
			priv->addr = camel_address_encode (CAMEL_ADDRESS (addr));
		}
		
		camel_object_unref (CAMEL_OBJECT (addr));
	}
	
	return priv->addr;
}

void
e_destination_set_raw (EDestination *dest, const gchar *raw)
{
	g_return_if_fail (E_IS_DESTINATION (dest));
	g_return_if_fail (raw != NULL);
	
	if (dest->priv->raw == NULL || strcmp (dest->priv->raw, raw)) {
		e_destination_freeze (dest);
		
		e_destination_clear (dest);
		dest->priv->raw = g_strdup (raw);
		e_destination_changed (dest);
		
		e_destination_thaw (dest);
	}
}

const gchar *
e_destination_get_textrep (const EDestination *dest, gboolean include_email)
{
	const char *name, *email;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	if (dest->priv->raw)
		return dest->priv->raw;
	
	name  = e_destination_get_name (dest);
	email = e_destination_get_email (dest);
	
	if (e_destination_from_card (dest) && name != NULL && (!include_email || !email || !*email))
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
	
	if (dest->priv->list_dests == NULL
	    && dest->priv->card != NULL
	    && dest->priv->card->email != NULL
	    && e_card_evolution_list (dest->priv->card)) {
		
		EIterator *iter = e_list_get_iterator (dest->priv->card->email);
		
		e_iterator_reset (iter);
		while (e_iterator_is_valid (iter)) {
			const char *dest_xml = (const char *) e_iterator_get (iter);
			EDestination *list_dest = e_destination_import (dest_xml);
			
			if (list_dest)
				dest->priv->list_dests = g_list_append (dest->priv->list_dests, list_dest);
			e_iterator_next (iter);
		}
	}
	
	return dest->priv->list_dests != NULL;
}

gboolean
e_destination_list_show_addresses (const EDestination *dest)
{
	g_return_val_if_fail (E_IS_DESTINATION (dest), FALSE);
	
	if (dest->priv->card != NULL)
		return e_card_evolution_list_show_addresses (dest->priv->card);
	
	return dest->priv->show_addresses;
}

gboolean
e_destination_get_html_mail_pref (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	
	if (dest->priv->html_mail_override || dest->priv->card == NULL)
		return dest->priv->wants_html_mail;
	
	return dest->priv->card->wants_html;
}

gboolean
e_destination_allow_cardification (const EDestination *dest)
{
	g_return_val_if_fail (E_IS_DESTINATION (dest), FALSE);
	
	return dest->priv->allow_cardify;
}

void
e_destination_set_allow_cardification (EDestination *dest, gboolean x)
{
	g_return_if_fail (E_IS_DESTINATION (dest));
	
	dest->priv->allow_cardify = x;
}

static void
set_cardify_book (EDestination *dest, EBook *book)
{
	if (dest->priv->cardify_book && dest->priv->cardify_book != book) {
		g_object_unref (dest->priv->cardify_book);
	}
		
	dest->priv->cardify_book = book;
	
	if (book)
		g_object_ref (book);
}

static void
name_and_email_simple_query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	EDestination *dest = E_DESTINATION (closure);
	
	if (status == E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS && g_list_length ((GList *) cards) == 1) {
		ECard *card = E_CARD (cards->data);
		const char *email = e_destination_get_email (dest);
		int email_num = 0;
		
		if (e_destination_is_valid (dest) && email && *email) {
			email_num = e_card_email_find_number (card, e_destination_get_email (dest));
		}
		
		if (email_num >= 0) {
			const char *book_uri;
			
			book_uri = e_book_get_uri (book);
			
			dest->priv->has_been_cardified = TRUE;
			e_destination_set_card (dest, card, email_num);
			e_destination_set_book_uri (dest, book_uri);
			g_signal_emit (dest, e_destination_signals[CARDIFIED], 0);
		}
	}
	
	if (!dest->priv->has_been_cardified) {
		dest->priv->cannot_cardify = TRUE;
	}
	
	g_object_unref (dest); /* drop the reference held by the query */
}


static void
nickname_simple_query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	EDestination *dest = E_DESTINATION (closure);
	
	if (status == E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS) {
		if (g_list_length ((GList *) cards) == 1) {
			const char *book_uri;
			
			book_uri = e_book_get_uri (book);
			
			dest->priv->has_been_cardified = TRUE;
			e_destination_set_card (dest, E_CARD (cards->data), 0); /* Uses primary e-mail by default. */
			e_destination_set_book_uri (dest, book_uri);
			g_signal_emit (dest, e_destination_signals[CARDIFIED], 0);
			
			g_object_unref (dest); /* drop the reference held by the query */
			
		} else {
			/* We can only end up here if we don't look at all like an e-mail address, so
			   we do a name-only query on the textrep */
			
			e_book_name_and_email_query (book,
						     e_destination_get_textrep (dest, FALSE),
						     NULL,
						     name_and_email_simple_query_cb,
						     dest);
		}
	} else {
		/* Something went wrong with the query: drop our ref to the destination and return. */
		g_object_unref (dest);
	}
}

static void
launch_cardify_query (EDestination *dest)
{
	if (! e_destination_is_valid (dest)) {
		/* If it doesn't look like an e-mail address, see if it is a nickname. */
		e_book_nickname_query (dest->priv->cardify_book,
				       e_destination_get_textrep (dest, FALSE),
				       nickname_simple_query_cb,
				       dest);

	} else {
		e_book_name_and_email_query (dest->priv->cardify_book,
					     e_destination_get_name (dest),
					     e_destination_get_email (dest),
					     name_and_email_simple_query_cb,
					     dest);
	}
}

static void
use_default_book_cb (EBook *book, gpointer closure)
{
	EDestination *dest = E_DESTINATION (closure);
	if (dest->priv->cardify_book == NULL) {
		dest->priv->cardify_book = book;
		g_object_ref (book);
	}
	
	launch_cardify_query (dest);
}


static gboolean
e_destination_reverting_is_a_good_idea (EDestination *dest)
{
	const char *textrep;
	int len, old_len;
	
	g_return_val_if_fail (E_IS_DESTINATION (dest), FALSE);
	if (dest->priv->old_textrep == NULL)
		return FALSE;
	
	textrep = e_destination_get_textrep (dest, FALSE);
	
	len = g_utf8_strlen (textrep, -1);
	old_len = g_utf8_strlen (dest->priv->old_textrep, -1);
	
	if (len <= old_len/2)
		return FALSE;
	
	return TRUE;
}

void
e_destination_cardify (EDestination *dest, EBook *book)
{
	g_return_if_fail (E_IS_DESTINATION (dest));
	g_return_if_fail (book == NULL || E_IS_BOOK (book));
	
	if (e_destination_is_evolution_list (dest))
		return;
	
	if (e_destination_contains_card (dest))
		return;
	
	if (!dest->priv->allow_cardify)
		return;
	
	if (dest->priv->cannot_cardify)
		return;
	
	e_destination_cancel_cardify (dest);
	
	/* In some cases, we can revert to the previous card. */
	if (!e_destination_is_valid (dest)
	    && e_destination_reverting_is_a_good_idea (dest)
	    && e_destination_revert (dest)) {
		return;
	}
	
	set_cardify_book (dest, book);
	
	/* Handle the case of an EDestination containing a card URL */
	if (e_destination_contains_card (dest)) {
		e_destination_use_card (dest, NULL, NULL);
		return;
	}
	
	/* If we have a book ready, proceed.  We hold a reference to ourselves
	   until our query is complete. */
	g_object_ref (dest);
	if (dest->priv->cardify_book != NULL) {
		launch_cardify_query (dest);
	} else {
		e_book_use_default_book (use_default_book_cb, dest);
	}
}

static int
do_cardify_delayed (gpointer ptr)
{
	EDestination *dest = E_DESTINATION (ptr);
	
	e_destination_cardify (dest, dest->priv->cardify_book);
	return FALSE;
}

void
e_destination_cardify_delayed (EDestination *dest, EBook *book, gint delay)
{
	g_return_if_fail (E_IS_DESTINATION (dest));
	g_return_if_fail (book == NULL || E_IS_BOOK (book));
	
	if (delay < 0)
		delay = 500;
	
	e_destination_cancel_cardify (dest);
	
	set_cardify_book (dest, book);
	
	dest->priv->pending_cardification = g_timeout_add (delay, do_cardify_delayed, dest);
}

void
e_destination_cancel_cardify (EDestination *dest)
{
	g_return_if_fail (E_IS_DESTINATION (dest));
	
	if (dest->priv->pending_cardification) {
		g_source_remove (dest->priv->pending_cardification);
		dest->priv->pending_cardification = 0;
	}
}

gboolean
e_destination_uncardify (EDestination *dest)
{
	char *email;
	
	g_return_val_if_fail (E_IS_DESTINATION (dest), FALSE);
	
	if (!e_destination_contains_card (dest))
		return FALSE;
	
	email = g_strdup (e_destination_get_email (dest));
	
	if (email == NULL)
		return FALSE;
	
	e_destination_freeze (dest);
	e_destination_clear (dest);
	e_destination_set_raw (dest, email);
	g_free (email);
	e_destination_thaw (dest);
	
	return TRUE;
}

gboolean
e_destination_revert (EDestination *dest)
{
	g_return_val_if_fail (E_IS_DESTINATION (dest), FALSE);
	
	if (dest->priv->old_card) {
		ECard *card;
		int card_email_num;
		
		card = dest->priv->old_card;
		card_email_num = dest->priv->old_card_email_num;
		
		dest->priv->old_card = NULL;
		g_free (dest->priv->old_textrep);
		dest->priv->old_textrep = NULL;
		
		e_destination_freeze (dest);
		e_destination_clear (dest);
		e_destination_set_card (dest, card, card_email_num);
		e_destination_thaw (dest);
		
		return TRUE;
	}
	
	return FALSE;
}

/*
 * Destination import/export
 */

gchar *
e_destination_get_address_textv (EDestination **destv)
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
		if (!e_destination_is_empty (destv[i])) {
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
			if (str)
				xmlNewTextChild (list_node, NULL, "name", str);
			
			str = e_destination_get_email (list_dest);
			if (str)
				xmlNewTextChild (list_node, NULL, "email", str);
			
			xmlAddChild (dest_node, list_node);
			
			iter = g_list_next (iter);
		}
		
		xmlNewProp (dest_node, "is_list", "yes");
		xmlNewProp (dest_node, "show_addresses", 
			    e_destination_list_show_addresses (dest) ? "yes" : "no");
	}
	
	str = e_destination_get_book_uri (dest);
	if (str) {
		xmlNewTextChild (dest_node, NULL, "book_uri", str);
	}
	
	str = e_destination_get_card_uid (dest);
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
	char *name = NULL, *email = NULL, *book_uri = NULL, *card_uid = NULL;
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
	
	e_destination_freeze (dest);
	
	e_destination_clear (dest);
	
	if (name) {
		e_destination_set_name (dest, name);
		g_free (name);
	}
	if (email) {
		e_destination_set_email (dest, email);
		g_free (email);
	}
	if (book_uri) {
		e_destination_set_book_uri (dest, book_uri);
		g_free (book_uri);
	}
	if (card_uid) {
		e_destination_set_card_uid (dest, card_uid, email_num);
		g_free (card_uid);
	}
	if (list_dests)
		dest->priv->list_dests = list_dests;
	
	dest->priv->html_mail_override = TRUE;
	dest->priv->wants_html_mail = html_mail;
	
	dest->priv->show_addresses = show_addr;
	
	dest->priv->auto_recipient = auto_recip;
	
	e_destination_thaw (dest);
	
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
e_destination_import (const gchar *str)
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

gchar *
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
		if (! e_destination_is_empty (destv[i])) {
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
e_destination_importv (const gchar *str)
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
		if (e_destination_xml_decode (dest, node) && !e_destination_is_empty (dest)) {
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

EDestination **
e_destination_list_to_vector_sized (GList *list, int n)
{
	EDestination **destv;
	int i = 0;
	
	if (n == -1)
		n = g_list_length (list);
	
	if (n == 0)
		return NULL;
	
	destv = g_new (EDestination *, n + 1);
	while (list != NULL && i < n) {
		destv[i] = E_DESTINATION (list->data);
		list->data = NULL;
		i++;
		list = g_list_next (list);
	}
	destv[i] = NULL;
	
	return destv;
}

EDestination **
e_destination_list_to_vector (GList *list)
{
	return e_destination_list_to_vector_sized (list, -1);
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

static void
touch_cb (EBook *book, const gchar *addr, ECard *card, gpointer closure)
{
	if (book != NULL && card != NULL) {
		e_card_touch (card);
		d(g_message ("Use score for \"%s\" is now %f", addr, e_card_get_use_score (card)));
		e_book_commit_card (book, card, NULL, NULL);
	}
}

void
e_destination_touch (EDestination *dest)
{
	const char *email;
	
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	
	if (!e_destination_is_auto_recipient (dest)) {
		email = e_destination_get_email (dest);
		
		if (email)
			e_book_query_address_default (email, touch_cb, NULL);
	}
}

void
e_destination_touchv (EDestination **destv)
{
	int i;
	
	g_return_if_fail (destv != NULL);
	
	for (i = 0; destv[i] != NULL; ++i) {
		e_destination_touch (destv[i]);
	}
}
