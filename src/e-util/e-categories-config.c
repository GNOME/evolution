/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-categories-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-categories-dialog.h"
#include "e-icon-factory.h"
#include "e-misc-utils.h"

static GHashTable *pixbufs_cache = NULL;

static void
categories_changed_cb (gpointer object,
                       gpointer user_data)
{
	if (pixbufs_cache)
		g_hash_table_remove_all (pixbufs_cache);
}

static void
free_pixbuf_cb (gpointer ptr)
{
	GdkPixbuf *pixbuf = ptr;

	if (pixbuf)
		g_object_unref (pixbuf);
}

/**
 * e_categories_config_get_icon_for:
 * @category: Category for which to get the icon.
 * @pixbuf: A pointer to where the pixbuf will be returned.
 *
 * Returns the icon configured for the given category.
 *
 * Returns: the icon configured for the given category
 */
gboolean
e_categories_config_get_icon_for (const gchar *category,
                                  GdkPixbuf **pixbuf)
{
	gchar *icon_file;

	g_return_val_if_fail (pixbuf != NULL, FALSE);
	g_return_val_if_fail (category != NULL, FALSE);

	if (!pixbufs_cache) {
		pixbufs_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, free_pixbuf_cb);
		e_categories_add_change_hook (
			(GHookFunc) categories_changed_cb, NULL);
	} else {
		gpointer key = NULL, value = NULL;

		if (g_hash_table_lookup_extended (pixbufs_cache, category, &key, &value)) {
			*pixbuf = value;
			if (*pixbuf)
				g_object_ref (*pixbuf);
			return *pixbuf != NULL;
		}
	}

	icon_file = e_categories_dup_icon_file_for (category);
	if (!icon_file) {
		*pixbuf = NULL;
	} else {
		GdkPixbuf *icon;

		/* load the icon in our list */
		icon = gdk_pixbuf_new_from_file (icon_file, NULL);

		if (icon) {
			/* ensure icon size */
			*pixbuf = e_icon_factory_pixbuf_scale (icon, 16, 16);
			g_object_unref (icon);
		} else {
			*pixbuf = NULL;
		}
	}
	g_free (icon_file);

	g_hash_table_insert (pixbufs_cache, g_strdup (category), *pixbuf == NULL ? NULL : g_object_ref (*pixbuf));

	return *pixbuf != NULL;
}

/**
 * e_categories_config_open_dialog_for_entry:
 * @entry: a #GtkEntry on which to get/set the categories list
 *
 * This is a self-contained function that lets you open a popup dialog for
 * the user to select a list of categories.
 *
 * The @entry parameter is used, at initialization time, as the list of
 * initial categories that are selected in the categories selection dialog.
 * Then, when the user commits its changes, the list of selected categories
 * is put back on the entry widget.
 */
void
e_categories_config_open_dialog_for_entry (GtkEntry *entry)
{
	GtkDialog *dialog;
	const gchar *text;
	gint result;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (GTK_IS_ENTRY (entry));

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	dialog = GTK_DIALOG (e_categories_dialog_new (text));

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (entry))));

	/* run the dialog */
	result = gtk_dialog_run (dialog);

	if (result == GTK_RESPONSE_OK) {
		gchar *categories;

		categories = e_categories_dialog_get_categories (E_CATEGORIES_DIALOG (dialog));
		gtk_entry_set_text (GTK_ENTRY (entry), categories);
		g_free (categories);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}
