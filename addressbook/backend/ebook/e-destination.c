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
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <string.h>
#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include "e-book.h"
#include "e-book-util.h"


struct _EDestinationPrivate {

	gchar *pending_card_id;

	ECard *card;
	gint card_email_num;

	gchar *name;
	gchar *string;
	gchar *string_email;
	gchar *string_email_verbose;

	gboolean html_mail_override;
	gboolean wants_html_mail;
};

static void e_destination_clear_card    (EDestination *);
static void e_destination_clear_strings (EDestination *);

static GtkObjectClass *parent_class;

static void
e_destination_destroy (GtkObject *obj)
{
	EDestination *dest = E_DESTINATION (obj);

	e_destination_clear_card (dest);
	e_destination_clear_strings (dest);

	g_free (dest->priv);

	if (parent_class->destroy)
		parent_class->destroy (obj);
}

static void
e_destination_class_init (EDestinationClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = GTK_OBJECT_CLASS (gtk_type_class (GTK_TYPE_OBJECT));

	object_class->destroy = e_destination_destroy;
}

static void
e_destination_init (EDestination *dest)
{
	dest->priv = g_new0 (struct _EDestinationPrivate, 1);
}

GtkType
e_destination_get_type (void)
{
	static GtkType dest_type = 0;

	if (!dest_type) {
		GtkTypeInfo dest_info = {
			"EDestination",
			sizeof (EDestination),
			sizeof (EDestinationClass),
			(GtkClassInitFunc) e_destination_class_init,
			(GtkObjectInitFunc) e_destination_init,
			NULL, NULL, /* reserved */
			(GtkClassInitFunc) NULL
		};

		dest_type = gtk_type_unique (gtk_object_get_type (), &dest_info);
	}

	return dest_type;
}

EDestination *
e_destination_new (void)
{
	return E_DESTINATION (gtk_type_new (E_TYPE_DESTINATION));
}

EDestination *
e_destination_copy (EDestination *dest)
{
	EDestination *new_dest;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	new_dest = e_destination_new ();

	new_dest->priv->pending_card_id = g_strdup (new_dest->priv->pending_card_id);

	new_dest->priv->card = dest->priv->card;
	if (new_dest->priv->card)
		gtk_object_ref (GTK_OBJECT (new_dest->priv->card));

        new_dest->priv->card_email_num = dest->priv->card_email_num;

	new_dest->priv->string = g_strdup (dest->priv->string);
	new_dest->priv->string_email = g_strdup (dest->priv->string_email);
	new_dest->priv->string_email_verbose = g_strdup (dest->priv->string_email_verbose);

	return new_dest;
}

gboolean
e_destination_is_empty (EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), TRUE);

	return !(dest->priv->card || dest->priv->pending_card_id || (dest->priv->string && *dest->priv->string));
}

static void
e_destination_clear_card (EDestination *dest)
{
	g_free (dest->priv->pending_card_id);
	dest->priv->pending_card_id = NULL;

	if (dest->priv->card)
		gtk_object_unref (GTK_OBJECT (dest->priv->card));
	dest->priv->card = NULL;

	dest->priv->card_email_num = -1;
}

static void
e_destination_clear_strings (EDestination *dest)
{
	g_free (dest->priv->name);
	g_free (dest->priv->string);
	g_free (dest->priv->string_email);
	g_free (dest->priv->string_email_verbose);

	dest->priv->name = NULL;
	dest->priv->string = NULL;
	dest->priv->string_email = NULL;
	dest->priv->string_email_verbose = NULL;
}

void
e_destination_set_card (EDestination *dest, ECard *card, gint email_num)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (card && E_IS_CARD (card));

	if (dest->priv->pending_card_id) {
		g_free (dest->priv->pending_card_id);
		dest->priv->pending_card_id = NULL;
	}

	if (dest->priv->card != card) {
		if (dest->priv->card)
			gtk_object_unref (GTK_OBJECT (dest->priv->card));
		dest->priv->card = card;
		gtk_object_ref (GTK_OBJECT (card));
	}

	dest->priv->card_email_num = email_num;

	e_destination_clear_strings (dest);
}

void
e_destination_set_string (EDestination *dest, const gchar *string)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (string != NULL);

	g_free (dest->priv->string);
	dest->priv->string = g_strdup (string);
}

void
e_destination_set_html_mail_pref (EDestination *dest, gboolean x)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	dest->priv->html_mail_override = TRUE;
	dest->priv->wants_html_mail = x;
}

gboolean
e_destination_has_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	return dest->priv->card != NULL;
}

gboolean
e_destination_has_pending_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	return dest->priv->pending_card_id != NULL;
}


typedef struct _UseCard UseCard;
struct _UseCard {
	EDestination *dest;
	EDestinationCardCallback cb;
	gpointer closure;
};

static void
use_card_cb (EBook *book, gpointer closure)
{
	UseCard *uc = (UseCard *) closure;
	ECard *card;

	if (book != NULL && uc->dest->priv->card == NULL) {

		if (uc->dest->priv->pending_card_id) {

			card = e_book_get_card (book, uc->dest->priv->pending_card_id);

			if (card) {
				ECard *old = uc->dest->priv->card;
				uc->dest->priv->card = card;
				gtk_object_ref (GTK_OBJECT (card));
				if (old)
					gtk_object_unref (GTK_OBJECT (old));
			}
				
			g_free (uc->dest->priv->pending_card_id);
			uc->dest->priv->pending_card_id = NULL;
		
		}

	}

	if (uc->cb)
		uc->cb (uc->dest, uc->dest->priv->card, uc->closure);

	gtk_object_unref (GTK_OBJECT (uc->dest));
	g_free (uc);
}

void
e_destination_use_card (EDestination *dest, EDestinationCardCallback cb, gpointer closure)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	if (dest->priv->card) {

		if (cb) {
			cb (dest, dest->priv->card, closure);
		}

	} else {
		UseCard *uc = g_new (UseCard, 1);
		uc->dest = dest;
		gtk_object_ref (GTK_OBJECT (uc->dest));
		uc->cb = cb;
		uc->closure = closure;
		e_book_use_local_address_book (use_card_cb, uc);
	}
}

ECard *
e_destination_get_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	return dest->priv->card;
}

gint
e_destination_get_email_num (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), -1);

	return dest->priv->card_email_num;
}

const gchar *
e_destination_get_string (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;
		
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->string == NULL) {

		if (priv->card) {

			priv->string = e_card_name_to_string (priv->card->name);
			if (priv->string) {
				g_strstrip (priv->string);
				if (*(priv->string) == '\0') {
					g_free (priv->string);
					priv->string = NULL;
				}
			}

			if (priv->string == NULL)
				priv->string = g_strdup (e_destination_get_email (dest));

			if (priv->string == NULL)
				priv->string = g_strdup (_("???"));
		}
	}
	
	return priv->string;
}

gint
e_destination_get_strlen (const EDestination *dest)
{
	const gchar *str;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), 0);

	str = e_destination_get_string (dest);
	return str ? strlen (str) : 0;
}

const gchar *
e_destination_get_name (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->name == NULL) {

		if (priv->card) {

			priv->name = e_card_name_to_string (priv->card->name);
			
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

	if (priv->string_email == NULL) {

		if (priv->card) { /* Pull the address out of the card. */

			EIterator *iter = e_list_get_iterator (priv->card->email);
			gint n = priv->card_email_num;

			if (n >= 0) {
				while (n > 0) {
					e_iterator_next (iter);
					--n;
				}

				if (e_iterator_is_valid (iter)) {
					gconstpointer ptr = e_iterator_get (iter);
					priv->string_email = g_strdup ((gchar *) ptr);
				}
			}

		} else if (priv->string) { /* Use the string as an e-mail address */
			return priv->string;
		} 

		/* else we just return NULL */
	}

	return priv->string_email;
}

const gchar *
e_destination_get_email_verbose (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */

	if (priv->string_email_verbose == NULL) {
		
		const gchar *email = e_destination_get_email (dest);
		const gchar *name  = e_destination_get_name  (dest);

		if (name) {

			priv->string_email_verbose = g_strdup_printf ("%s <%s>", name, email);

		} else {

			return email;
		}

	}

	return priv->string_email_verbose;
}

gboolean
e_destination_get_html_mail_pref (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);

	if (dest->priv->html_mail_override || dest->priv->card == NULL)
		return dest->priv->wants_html_mail;

	return dest->priv->card->wants_html;
}

gchar *
e_destination_get_address_textv (EDestination **destv)
{
	gint i, j, len = 0;
	gchar **strv;
	gchar *str;
	g_return_val_if_fail (destv, NULL);

	while (destv[len]) {
		g_return_val_if_fail (E_IS_DESTINATION (destv[len]), NULL);
		++len;
	}

	strv = g_new0 (gchar *, len+1);
	for (i = 0, j = 0; destv[i]; ++i) {

		if (! e_destination_is_empty (destv[i])) {
			const gchar *addr = e_destination_get_email_verbose (destv[i]);
			strv[j++] = addr ? (gchar *) addr : "";
		}
	}

	str = g_strjoinv (", ", strv);

	g_free (strv);

	return str;
}

/*
 *
 *  Serialization code
 *
 */

#define DESTINATION_TAG       "DEST"

#define DESTINATION_SEPARATOR      "|"
#define DESTINATION_SEPARATOR_CHAR '|'

#define VEC_SEPARATOR              "\1"
#define VEC_SEPARATOR_CHAR         '\1'

static gchar *
join_strings (gchar **strv)
{
	/* FIXME: Should also quote any |'s that occur in any of the strings. */
	/* (We fake it by mapping | to _ when building our fields below) */
	return g_strjoinv (DESTINATION_SEPARATOR, strv);
}

static gchar **
unjoin_string (const gchar *str)
{
	/* FIXME: Should properly handle quoteded |'s in the string. */
	/* (We fake it by mapping | to _ when building our fields below) */
	return g_strsplit (str, DESTINATION_SEPARATOR, 0);
}

static gchar *
build_field (const gchar *key, const gchar *value)
{
	gchar *field;
	gchar *c;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	field = g_strdup_printf ("%s=%s", key, value);

	/* Paranoia: Convert any '=' in the key to '_' */
	c = field;
	while (*key) {
		if (*c == '=')
			*c = '_';
		++key;
		++c;
	}
	
	/* Paranoia: Convert any '\1' or '|' in the key or value to '_' */
	for (c=field; *c; ++c) {
		if (*c == VEC_SEPARATOR_CHAR || *c == DESTINATION_SEPARATOR_CHAR)
			*c = '_';
	}

	return field;
}

/* Modifies string in place, \0-terminates after the key, returns pointer to "value",
   or NULL if the field is malformed. */
static gchar *
extract_field (gchar *field)
{
	gchar *s = strchr (field, '=');
	if (s == NULL)
		return NULL;
	*s = '\0';
	return s+1;
}

#define EXPORT_MAX_FIELDS 10
gchar *
e_destination_export (const EDestination *dest)
{
	ECard *card;
	gchar **fields;
	gchar *str;
	gint i;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	fields = g_new (gchar *, EXPORT_MAX_FIELDS);
	fields[0] = g_strdup (DESTINATION_TAG);
	fields[1] = build_field ("addr", e_destination_get_email (dest));

	i = 2;

	if (e_destination_get_name (dest))
		fields[i++] = build_field ("name", e_destination_get_name (dest));

	fields[i++] = build_field ("html",
				   e_destination_get_html_mail_pref (dest) ? "Y" : "N");

	card = e_destination_get_card (dest);
	if (card)
		fields[i++] = build_field ("card", e_card_get_id (card));

	fields[i] = NULL;
	

	str = join_strings (fields);
	g_strfreev (fields);
	
	return str;
}

EDestination *
e_destination_import (const gchar *str)
{
	EDestination *dest;
	gchar **fields;
	gint i;

	gchar *addr = NULL, *name = NULL, *card = NULL;
	gboolean want_html = FALSE;
	
	g_return_val_if_fail (str, NULL);

	fields = unjoin_string (str);
	g_return_val_if_fail (fields && fields[0], NULL);
	g_return_val_if_fail (!strcmp (fields[0], DESTINATION_TAG), NULL);
	
	for (i = 1; fields[i]; ++i) {
		gchar *key = fields[i];
		gchar *value = extract_field (fields[i]);

		if (value) {
			
			if (!strcmp ("addr", key)) {

				if (addr) {
					g_warning ("addr redefined: \"%s\" => \"%s\"", addr, value);
				}

				addr = g_strdup (value);
			
			} else if (!strcmp ("name", key)) {

				if (name) {
					g_warning ("name redefined: \"%s\" => \"%s\"", name, value);
				}

				name = g_strdup (value);

			} else if (!strcmp ("html", key)) {

				want_html = (*value == 'Y');

			} else if (!strcmp ("card", key)) {

				if (card) {
					g_warning ("card redefined: \"%s\" => \"%s\"", card, value);
				}

				card = g_strdup (value);

			}

		}

	}

	dest = e_destination_new ();

	/* We construct this part of the object in a rather abusive way. */
	dest->priv->string_email = addr;
	dest->priv->name = name;
	dest->priv->pending_card_id = card;

	e_destination_set_html_mail_pref (dest, want_html);

	g_strfreev (fields);

	return dest;
}

gchar *
e_destination_exportv (EDestination **destv)
{
	gint i, j, len = 0;
	gchar **strv;
	gchar *str;

	g_return_val_if_fail (destv, NULL);

	while (destv[len]) {
		g_return_val_if_fail (E_IS_DESTINATION (destv[len]), NULL);
		++len;
	}

	strv = g_new0 (gchar *, len+1);
	for (i = 0, j = 0; i < len; ++i) {
		if (! e_destination_is_empty (destv[i]))
			strv[j++] = e_destination_export (destv[i]);
	}

	str = g_strjoinv (VEC_SEPARATOR, strv);

	for (i = 0; i < len; ++i) {
		if (strv[i]) {
			g_free (strv[i]);
		}
	}
	g_free (strv);

	return str;
}

EDestination **
e_destination_importv (const gchar *str)
{
	gchar** strv;
	EDestination **destv;
	gint i = 0, j = 0, len = 0;
	
	if (!(str && *str))
		return NULL;
	
	strv = g_strsplit (str, VEC_SEPARATOR, 0);
	while (strv[len])
		++len;

	destv = g_new0 (EDestination *, len+1);

	while (strv[i]) {
		EDestination *dest = e_destination_import (strv[i]);
		if (dest) {
			destv[j++] = dest;
		}
		++i;
	}

	g_strfreev (strv);
	return destv;
}

static void
touch_cb (EBook *book, const gchar *addr, ECard *card, gpointer closure)
{
	if (book != NULL && card != NULL) {
		e_card_touch (card);
		g_message ("Use score for \"%s\" is now %f", addr, e_card_get_use_score (card));
		e_book_commit_card (book, card, NULL, NULL);
	}
}

void
e_destination_touch (EDestination *dest)
{
	const gchar *email;

	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	email = e_destination_get_email (dest);

	if (email) {
		e_book_query_address_locally (email, touch_cb, NULL);
	}
}
