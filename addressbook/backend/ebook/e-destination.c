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
#include <gtk/gtk.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include "e-destination.h"

struct _EDestinationPrivate {
	ECard *card;
	gint card_email_num;

	gchar *string;
	gchar *string_email;
	gchar *string_email_verbose;
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

	new_dest->priv->card = dest->priv->card;
	if (new_dest->priv->card)
		gtk_object_ref (GTK_OBJECT (new_dest->priv->card));

        new_dest->priv->card_email_num = dest->priv->card_email_num;

	new_dest->priv->string = g_strdup (dest->priv->string);
	new_dest->priv->string_email = g_strdup (dest->priv->string_email);
	new_dest->priv->string_email_verbose = g_strdup (dest->priv->string_email_verbose);

	return new_dest;
}

static void
e_destination_clear_card (EDestination *dest)
{
	if (dest->priv->card)
		gtk_object_unref (GTK_OBJECT (dest->priv->card));
	dest->priv->card = NULL;

	dest->priv->card_email_num = -1;
}

static void
e_destination_clear_strings (EDestination *dest)
{
	g_free (dest->priv->string);
	g_free (dest->priv->string_email);
	g_free (dest->priv->string_email_verbose);

	dest->priv->string = NULL;
	dest->priv->string_email = NULL;
	dest->priv->string_email_verbose = NULL;
}

void
e_destination_set_card (EDestination *dest, ECard *card, gint email_num)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (card && E_IS_CARD (card));

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

		if (priv->card) {
			
			priv->string_email_verbose = g_strdup_printf ("%s <%s>",
								      e_card_name_to_string (priv->card->name),
								      email);
		} else {

			return email;
		}

	}

	return priv->string_email_verbose;
}


