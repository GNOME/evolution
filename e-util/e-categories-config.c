/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Categories configuration.
 *
 * Author:
 *   Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 */

#include <libgnomeui/gnome-dialog.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-categories.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include "e-categories-config.h"
#include "e-categories-master-list-wombat.h"

typedef struct {
	char *filename;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
} icon_data_t;

static GHashTable *cat_colors = NULL;
static GHashTable *cat_icons = NULL;
static gboolean initialized = FALSE;

char *
config_get_string (char *key)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	char *string;

	CORBA_exception_init (&ev);

	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	string = bonobo_config_get_string (db, key, NULL);

	bonobo_object_release_unref (db, &ev);
	CORBA_exception_free (&ev);

	return string;
}

static void
config_set_string (const char *key, const char *value)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return;
	}

	bonobo_config_set_string (db, key, value, &ev);

	bonobo_object_release_unref (db, &ev);
	CORBA_exception_free (&ev);
}

static void
initialize_categories_config (void)
{
	g_return_if_fail (initialized == FALSE);

	cat_colors = g_hash_table_new (g_str_hash, g_str_equal);
	cat_icons = g_hash_table_new (g_str_hash, g_str_equal);

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
	char *color;
	char *tmp;

	g_return_val_if_fail (category != NULL, NULL);

	if (!initialized)
		initialize_categories_config ();

	color = g_hash_table_lookup (cat_colors, category);
	if (color)
		return (const char *) color;

	/* not found, so get it from configuration */
	tmp = g_strdup_printf ("General/Categories/%s/Color", category);
        color = config_get_string (tmp);
	g_free (tmp);
	if (color)
		e_categories_config_set_color_for (category, (const char *) color);

	return color;
}

/**
 * e_categories_config_set_color_for
 */
void
e_categories_config_set_color_for (const char *category, const char *color)
{
	char *tmp_color;
	char *tmp;

	g_return_if_fail (category != NULL);
	g_return_if_fail (color != NULL);

	if (!initialized)
		initialize_categories_config ();

	tmp_color = g_hash_table_lookup (cat_colors, category);
	if (tmp_color != NULL) {
		g_hash_table_remove (cat_colors, category);
		g_free (tmp_color);
	}

	/* add new color to the hash table */
	tmp_color = g_strdup (color);
	g_hash_table_insert (cat_colors, (gpointer) category, (gpointer) tmp_color);

	/* ...and to the configuration */
	tmp = g_strdup_printf ("General/Categories/%s/Color", category);
	config_set_string (tmp, color);
	g_free (tmp);
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
void
e_categories_config_get_icon_for (const char *category, GdkPixmap **pixmap, GdkBitmap **mask)
{
	icon_data_t *icon_data;
	char *icon_file;
	char *tmp;

	g_return_if_fail (category != NULL);
	g_return_if_fail (pixmap != NULL);

	if (!initialized)
		initialize_categories_config ();

	icon_data = g_hash_table_lookup (cat_icons, category);
	if (icon_data != NULL) {
		*pixmap = icon_data->pixmap;
		if (mask != NULL)
			*mask = icon_data->mask;
		return;
	}

	/* not found, so look in the configuration */
	tmp = g_strdup_printf ("General/Categories/%s/Icon", category);
	icon_file = config_get_string (tmp);
	g_free (tmp);

	if (icon_file) {
		/* add new pixmap from file to the list */
		icon_data = g_new (icon_data_t, 1);
		icon_data->filename = icon_file;
		icon_data->pixmap = gdk_pixmap_create_from_xpm (NULL, &icon_data->mask, NULL, icon_file);
		g_hash_table_insert (cat_icons, (gpointer) category, (gpointer) icon_data);

		*pixmap = icon_data->pixmap;
		if (*mask)
			*mask = icon_data->mask;
	}
	else {
		*pixmap = NULL;
		if (mask != NULL)
			*mask = NULL;
	}
}

/**
 * e_categories_config_get_icon_file_for
 * @category: Category for which to get the icon file
 */
const char *
e_categories_config_get_icon_file_for (const char *category)
{
	icon_data_t *icon_data;
	char *icon_file;
	char *tmp;

	g_return_val_if_fail (category != NULL, NULL);

	if (!initialized)
		initialize_categories_config ();

	icon_data = g_hash_table_lookup (cat_icons, category);
	if (icon_data != NULL)
		return (const char *) icon_data->filename;

	/* not found, so look in the configuration */
	tmp = g_strdup_printf ("General/Categories/%s/Icon", category);
	icon_file = config_get_string (tmp);
	g_free (tmp);

	if (icon_file) {
		/* add new pixmap from file to the list */
		icon_data = g_new (icon_data_t, 1);
		icon_data->filename = icon_file;
		icon_data->pixmap = gdk_pixmap_create_from_xpm (NULL, &icon_data->mask, NULL, icon_file);
		g_hash_table_insert (cat_icons, (gpointer) category, (gpointer) icon_data);
	}

	return (const char *) icon_file;
}

/**
 * e_categories_config_set_icon_for
 * @category: Category for which to set the icon.
 * @pixmap_file: Full path of the pixmap file.
 */
void
e_categories_config_set_icon_for (const char *category, const char *pixmap_file)
{
	icon_data_t *icon_data;
	char *tmp;

	g_return_if_fail (category != NULL);
	g_return_if_fail (pixmap_file != NULL);

	if (!initialized)
		initialize_categories_config ();

	icon_data = g_hash_table_lookup (cat_icons, category);
	if (icon_data != NULL) {
		g_hash_table_remove (cat_icons, category);

		gdk_pixmap_unref (icon_data->pixmap);
		gdk_bitmap_unref (icon_data->mask);
		g_free (icon_data->filename);
		g_free (icon_data);
	}

	/* add new pixmap from file to the list */
	icon_data = g_new (icon_data_t, 1);
	icon_data->filename = g_strdup (pixmap_file);
	icon_data->pixmap = gdk_pixmap_create_from_xpm (NULL, &icon_data->mask, NULL, pixmap_file);
	g_hash_table_insert (cat_icons, (gpointer) category, (gpointer) icon_data);

	/* ...and to the configuration */
	tmp = g_strdup_printf ("General/Categories/%s/Icon", category);
	config_set_string (tmp, pixmap_file);
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
	char *categories;
	GnomeDialog *dialog;
	int result;
	ECategoriesMasterList *ecml;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (GTK_IS_ENTRY (entry));

	categories = e_utf8_gtk_entry_get_text (GTK_ENTRY (entry));
	dialog = GNOME_DIALOG (e_categories_new (categories));

	ecml = e_categories_master_list_wombat_new ();
	gtk_object_set (GTK_OBJECT (dialog),
			"ecml", ecml,
			NULL);
	gtk_object_unref (GTK_OBJECT (ecml));

	/* run the dialog */
	result = gnome_dialog_run (dialog);
	g_free (categories);

	if (result == 0) {
		gtk_object_get (GTK_OBJECT (dialog),
				"categories", &categories,
				NULL);
		e_utf8_gtk_entry_set_text (GTK_ENTRY (entry), categories);
		g_free (categories);
	}

	gtk_object_destroy (GTK_OBJECT (dialog));
}
