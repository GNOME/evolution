/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-address-entry.c
 *
 * Copyright (C) 1999  Helix Code, Inc.
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

/* This is a custom GtkEntry for entering address lists.  For now, it does not
   have any fancy features, but in the future we might want to make it
   cooler.  */

#include <gnome.h>

#include "e-msg-composer-address-entry.h"


static GtkEntryClass *parent_class = NULL;


/* Initialization.  */

static void
class_init (EMsgComposerAddressEntryClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gtk_entry_get_type ());
}

static void
init (EMsgComposerAddressEntry *msg_composer_address_entry)
{
}

GtkType
e_msg_composer_address_entry_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposerAddressEntry",
			sizeof (EMsgComposerAddressEntry),
			sizeof (EMsgComposerAddressEntryClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gtk_entry_get_type (), &info);
	}

	return type;
}


GtkWidget *
e_msg_composer_address_entry_new (void)
{
	GtkWidget *new;

	new = gtk_type_new (e_msg_composer_address_entry_get_type ());

	return new;
}


/**
 * e_msg_composer_address_entry_get_addresses:
 * @entry: An address entry widget
 * 
 * Retrieve the list of addresses stored in @entry.
 * 
 * Return value: A GList of pointers to strings representing the addresses.
 * Notice that the strings must be freed by the caller when not needed anymore.
 **/
GList *
e_msg_composer_address_entry_get_addresses (EMsgComposerAddressEntry *entry)
{
	GList *list;
	const gchar *s;
	const gchar *p, *oldp;
	gboolean in_quotes;

	s = gtk_entry_get_text (GTK_ENTRY (entry));

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
				gchar *new_addr;

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

/**
 * e_msg_composer_address_entry_set_list:
 * @entry: An address entry
 * @list: List of pointers to strings representing the addresses that must
 * appear in the entry
 * 
 * Set the address list from @list.
 **/
void
e_msg_composer_address_entry_set_list (EMsgComposerAddressEntry *entry,
				       const GList *list)
{
	GString *string;
	const GList *p;

	g_return_if_fail (entry != NULL);

	if (list == NULL) {
		gtk_editable_delete_text (GTK_EDITABLE (entry), -1, -1);
		return;
	}

	string = g_string_new (NULL);
	for (p = list; p != NULL; p = p->next) {
		if (string->str[0] != '\0')
			g_string_append (string, ", ");
		g_string_append (string, p->data);
	}

	gtk_entry_set_text (GTK_ENTRY (entry), string->str);
	g_string_free (string, TRUE);
}
