/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Categories configuration.
 *
 * Author:
 *   Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 */

#include <string.h>
#include <gtk/gtkdialog.h>
#include <libgnome/gnome-i18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gal/widgets/e-categories.h>
#include "e-categories-config.h"
#include "e-categories-master-list-wombat.h"

static gboolean initialized = FALSE;
static ECategoriesMasterListWombat *ecmlw = NULL;
static GHashTable *icons_table = NULL;

static void
initialize_categories_config (void)
{
	g_return_if_fail (initialized == FALSE);

	ecmlw = E_CATEGORIES_MASTER_LIST_WOMBAT (e_categories_master_list_wombat_new ());
	icons_table = g_hash_table_new (g_str_hash, g_str_equal);
	/* FIXME: must free the two objects above when exiting */

	initialized = TRUE;
}

/**
 * e_categories_config_get_color_for:
 * @category: Category to get the color for.
 *
 * Returns the representation of the color configured for the given
 * category
 *
 * Returns: An X color specification.
 */
const char *
e_categories_config_get_color_for (const char *category)
{
	int n;

	g_return_val_if_fail (category != NULL, NULL);

	if (!initialized)
		initialize_categories_config ();

	for (n = 0;
	     n < e_categories_master_list_count (E_CATEGORIES_MASTER_LIST (ecmlw));
	     n++) {
		char *tmp_cat;

		tmp_cat = (char *) e_categories_master_list_nth (E_CATEGORIES_MASTER_LIST (ecmlw), n);
		if (tmp_cat && !strcmp (tmp_cat, category))
			return e_categories_master_list_nth_color (E_CATEGORIES_MASTER_LIST (ecmlw), n);
	}

	return NULL; /* not found */
}

/**
 * e_categories_config_set_color_for
 */
void
e_categories_config_set_color_for (const char *category, const char *color)
{
	/* FIXME: implement */
}

/**
 * e_categories_config_get_icon_for:
 * @category: Category for which to get the icon.
 * @icon: A pointer to where the pixmap will be returned.
 * @mask: A pointer to where the mask will be returned.
 *
 * Returns the icon (and associated mask) configured for the
 * given category.
 */
gboolean
e_categories_config_get_icon_for (const char *category, GdkPixmap **pixmap, GdkBitmap **mask)
{
	char *icon_file;
	GdkPixbuf *pixbuf;
	GdkBitmap *tmp_mask;

	g_return_val_if_fail (pixmap != NULL, FALSE);

	icon_file = (char *) e_categories_config_get_icon_file_for (category);
	if (!icon_file) {
		*pixmap = NULL;
		if (mask != NULL)
			*mask = NULL;
		return FALSE;
	}

	/* load the icon in our list */
	pixbuf = g_hash_table_lookup (icons_table, icon_file);
	if (!pixbuf) {
		pixbuf = gdk_pixbuf_new_from_file (icon_file, NULL);
		if (!pixbuf) {
			*pixmap = NULL;
			if (mask != NULL)
				*mask = NULL;
			return FALSE;
		}

		g_hash_table_insert (icons_table, g_strdup (icon_file), pixbuf);
	}

	/* render the pixbuf to the pixmap and mask passed */
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, pixmap, &tmp_mask, 1);
	if (mask != NULL)
		*mask = tmp_mask;

	return TRUE;
}

/**
 * e_categories_config_get_icon_file_for
 * @category: Category for which to get the icon file
 */
const char *
e_categories_config_get_icon_file_for (const char *category)
{
	int n;

	g_return_val_if_fail (category != NULL, NULL);

	if (!initialized)
		initialize_categories_config ();

	for (n = 0;
	     n < e_categories_master_list_count (E_CATEGORIES_MASTER_LIST (ecmlw));
	     n++) {
		char *tmp_cat;

		tmp_cat = (char *) e_categories_master_list_nth (E_CATEGORIES_MASTER_LIST (ecmlw), n);
		if (tmp_cat && !strcmp (tmp_cat, category))
			return e_categories_master_list_nth_icon (E_CATEGORIES_MASTER_LIST (ecmlw), n);
	}

	return NULL; /* not found */
}

/**
 * e_categories_config_set_icon_for
 * @category: Category for which to set the icon.
 * @icon_file: Full path of the icon file.
 */
void
e_categories_config_set_icon_for (const char *category, const char *icon_file)
{
}

/**
 * e_categories_config_open_dialog_for_entry:
 * entry: A GtkEntry on which to get/set the categories list.
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
	const char *text;
	char *categories;
	int result;
	
	g_return_if_fail (entry != NULL);
	g_return_if_fail (GTK_IS_ENTRY (entry));
	
	if (!initialized)
		initialize_categories_config ();
	
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	dialog = GTK_DIALOG (e_categories_new (text));
	
	g_object_set (dialog, "ecml", ecmlw, NULL);
	
	/* run the dialog */
	result = gtk_dialog_run (dialog);
	
	if (result == GTK_RESPONSE_OK) {
		g_object_get (dialog, "categories", &categories, NULL);
		gtk_entry_set_text (GTK_ENTRY (entry), categories);
		g_free (categories);
	}
	
	gtk_object_destroy (GTK_OBJECT (dialog));
}
