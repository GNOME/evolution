/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-icon-factory.c - Icon factory for the Evolution shell.
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-icon-factory.h"

#include "e-shell-constants.h"


/* One icon.  Keep both a small (16x16) and a large (48x48) version around.  */
struct _Icon {
	char *name;
	GdkPixbuf *small_pixbuf;
	GdkPixbuf *large_pixbuf;
};
typedef struct _Icon Icon;

/* Hash of all the icons.  */
static GHashTable *name_to_icon = NULL;


/* Creating and destroying icons.  */

static Icon *
icon_new (const char *name,
	  GdkPixbuf *small_pixbuf,
	  GdkPixbuf *large_pixbuf)
{
	Icon *icon;

	icon = g_new (Icon, 1);
	icon->name         = g_strdup (name);
	icon->small_pixbuf = small_pixbuf;
	icon->large_pixbuf = large_pixbuf;

	if (small_pixbuf != NULL)
		gdk_pixbuf_ref (small_pixbuf);
	if (large_pixbuf != NULL)
		gdk_pixbuf_ref (large_pixbuf);

	return icon;
}

#if 0

/* (This is not currently used since we never prune icons out of the
   cache.)  */
static void
icon_free (Icon *icon)
{
	g_free (icon->name);

	if (icon->large_pixbuf != NULL)
		gdk_pixbuf_unref (icon->large_pixbuf);
	if (icon->small_pixbuf != NULL)
		gdk_pixbuf_unref (icon->small_pixbuf);

	g_free (icon);
}

#endif


/* Loading icons.  */

static Icon *
load_icon (const char *icon_name)
{
	GdkPixbuf *small_pixbuf;
	GdkPixbuf *large_pixbuf;
	Icon *icon;
	char *path;

	path = g_strconcat (EVOLUTION_IMAGES, "/", icon_name, "-mini.png", NULL);
	small_pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);

	path = g_strconcat (EVOLUTION_IMAGES, "/", icon_name, ".png", NULL);
	large_pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);

	if (large_pixbuf == NULL || small_pixbuf == NULL)
		return NULL;

	icon = icon_new (icon_name, small_pixbuf, large_pixbuf);

	gdk_pixbuf_unref (small_pixbuf);
	gdk_pixbuf_unref (large_pixbuf);

	return icon;
}


/* Public API.  */

void
e_icon_factory_init (void)
{
	if (name_to_icon != NULL) {
		/* Already initialized.  */
		return;
	}

	name_to_icon = g_hash_table_new (g_str_hash, g_str_equal);
}

GdkPixbuf *
e_icon_factory_get_icon (const char *icon_name,
			 gboolean mini)
{
	Icon *icon;

	g_return_val_if_fail (icon_name != NULL, NULL);

	icon = g_hash_table_lookup (name_to_icon, icon_name);
	if (icon == NULL) {
		icon = load_icon (icon_name);
		if (icon == NULL) {
			g_warning ("Icon not found -- %s", icon_name);

			/* Create an empty icon so that we don't keep spitting
			   out the same warning over and over, every time this
			   icon is requested.  */

			icon = icon_new (icon_name, NULL, NULL);
			g_hash_table_insert (name_to_icon, icon->name, icon);
			return NULL;
		}

		g_hash_table_insert (name_to_icon, icon->name, icon);
	}

	if (mini) {
		gdk_pixbuf_ref (icon->small_pixbuf);
		return icon->small_pixbuf;
	} else {
		gdk_pixbuf_ref (icon->large_pixbuf);
		return icon->large_pixbuf;
	}
}
