/*
 * e-emoticon.c
 *
 * Copyright (C) 2008 Novell, Inc.
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "e-emoticon.h"

#include <gtk/gtk.h>

static EEmoticon *
emoticon_copy (const EEmoticon *emoticon)
{
	EEmoticon *copy;

	copy = g_slice_new (EEmoticon);
	copy->label = g_strdup (emoticon->label);
	copy->icon_name = g_strdup (emoticon->icon_name);
	copy->unicode_character = g_strdup (emoticon->unicode_character);
	copy->text_face = g_strdup (emoticon->text_face);

	return copy;
}

static void
emoticon_free (EEmoticon *emoticon)
{
	g_free (emoticon->label);
	g_free (emoticon->icon_name);
	g_free (emoticon->unicode_character);
	g_free (emoticon->text_face);
	g_slice_free (EEmoticon, emoticon);
}

GType
e_emoticon_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
		type = g_boxed_type_register_static (
			"EEmoticon",
			(GBoxedCopyFunc) emoticon_copy,
			(GBoxedFreeFunc) emoticon_free);

	return type;
}

gboolean
e_emoticon_equal (const EEmoticon *emoticon_a,
                  const EEmoticon *emoticon_b)
{
	if (((emoticon_a == NULL) && (emoticon_b != NULL)) ||
	    ((emoticon_a != NULL) && (emoticon_b == NULL)))
		return FALSE;

	if (emoticon_a == emoticon_b)
		return TRUE;

	if (g_strcmp0 (emoticon_a->label, emoticon_b->label) != 0)
		return FALSE;

	if (g_strcmp0 (emoticon_a->icon_name, emoticon_b->icon_name) != 0)
		return FALSE;

	if (g_strcmp0 (emoticon_a->unicode_character, emoticon_b->unicode_character) != 0)
		return FALSE;

	if (g_strcmp0 (emoticon_a->text_face, emoticon_b->text_face) != 0)
		return FALSE;

	return TRUE;
}

EEmoticon *
e_emoticon_copy (const EEmoticon *emoticon)
{
	return g_boxed_copy (E_TYPE_EMOTICON, emoticon);
}

void
e_emoticon_free (EEmoticon *emoticon)
{
	g_boxed_free (E_TYPE_EMOTICON, emoticon);
}

gchar *
e_emoticon_dup_uri (const EEmoticon *emoticon)
{
	GtkIconInfo *icon_info;
	GtkIconTheme *icon_theme;
	const gchar *filename;
	gchar *uri = NULL;

	icon_theme = gtk_icon_theme_get_default ();
	icon_info = gtk_icon_theme_lookup_icon (
		icon_theme, emoticon->icon_name, 16, 0);
	g_return_val_if_fail (icon_info != NULL, NULL);

	filename = gtk_icon_info_get_filename (icon_info);
	if (filename != NULL) {
		uri = g_filename_to_uri (filename, NULL, NULL);
	}
	g_object_unref (icon_info);
	g_return_val_if_fail (uri != NULL, NULL);

	return uri;
}

const gchar *
e_emoticon_get_name (const EEmoticon *emoticon)
{
	return emoticon->icon_name;
}
