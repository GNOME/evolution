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
#include <libgnome/gnome-i18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-categories.h>
#include <bonobo-conf/Bonobo_Config.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include "e-categories-config.h"
#include "e-categories-master-list-wombat.h"

typedef struct {
	char *filename;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
} icon_data_t;

static GHashTable *cat_colors = NULL;
static GHashTable *cat_icons = NULL;
static gboolean initialized = FALSE;
static Bonobo_ConfigDatabase db = CORBA_OBJECT_NIL;

static void
initialize_categories_config (void)
{
	CORBA_Environment ev;
	static gboolean init_in_progress = FALSE;

	g_return_if_fail (initialized == FALSE);

	if (init_in_progress)
		return;
	init_in_progress = TRUE;

	cat_colors = g_hash_table_new (g_str_hash, g_str_equal);
	cat_icons = g_hash_table_new (g_str_hash, g_str_equal);

	/* get configuration component */
	CORBA_exception_init (&ev);

	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev))
		g_error ("Could not get wombat: moniker");
	else
		bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (db));

	/* generate default icon configuration if not present */
	if (!Bonobo_ConfigDatabase_dirExists (db, "General/Categories", &ev)
	    && !BONOBO_EX (&ev)) {
		e_categories_config_set_icon_for (
			_("Birthday"), EVOLUTION_CATEGORY_ICONS "/16_category_birthday.png");
		e_categories_config_set_icon_for (
			_("Business"), EVOLUTION_CATEGORY_ICONS "/16_category_business.png");
		e_categories_config_set_icon_for (
			_("Competition"), NULL);
		e_categories_config_set_icon_for (
			_("Favorites"), EVOLUTION_CATEGORY_ICONS "/16_category_favorites.png");
		e_categories_config_set_icon_for (
			_("Gifts"), EVOLUTION_CATEGORY_ICONS "/16_category_gifts.png");
		e_categories_config_set_icon_for (
			_("Goals/Objectives"), EVOLUTION_CATEGORY_ICONS "/16_category_goals.png");
		e_categories_config_set_icon_for (
			_("Holiday"), EVOLUTION_CATEGORY_ICONS "/16_category_holiday.png");
		e_categories_config_set_icon_for (
			_("Holiday Cards"), EVOLUTION_CATEGORY_ICONS "/16_category_holiday-cards.png");
		e_categories_config_set_icon_for (
			_("Hot Contacts"), EVOLUTION_CATEGORY_ICONS "/16_category_hot-contacts.png");
		e_categories_config_set_icon_for (
			_("Ideas"), EVOLUTION_CATEGORY_ICONS "/16_category_ideas.png");
		e_categories_config_set_icon_for (
			_("International"), EVOLUTION_CATEGORY_ICONS "/16_category_international.png");
		e_categories_config_set_icon_for (
			_("Key Customer"), EVOLUTION_CATEGORY_ICONS "/16_category_key-customer.png");
		e_categories_config_set_icon_for (
			_("Miscellaneous"), EVOLUTION_CATEGORY_ICONS "/16_category_miscellaneous.png");
		e_categories_config_set_icon_for (
			_("Personal"), EVOLUTION_CATEGORY_ICONS "/16_category_personal.png");
		e_categories_config_set_icon_for (
			_("Phone Calls"), EVOLUTION_CATEGORY_ICONS "/16_category_phonecalls.png");
		e_categories_config_set_icon_for (
			_("Status"), EVOLUTION_CATEGORY_ICONS "/16_category_status.png");
		e_categories_config_set_icon_for (
			_("Strategies"), EVOLUTION_CATEGORY_ICONS "/16_category_strategies.png");
		e_categories_config_set_icon_for (
			_("Suppliers"), EVOLUTION_CATEGORY_ICONS "/16_category_suppliers.png");
		e_categories_config_set_icon_for (
			_("Time & Expenses"), EVOLUTION_CATEGORY_ICONS "/16_category_time-and-expenses.png");
		e_categories_config_set_icon_for (
			_("VIP"), NULL);
		e_categories_config_set_icon_for (
			_("Waiting"), NULL);
	}
	
	CORBA_exception_free (&ev);

	initialized = TRUE;
	init_in_progress = FALSE;
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
        color = bonobo_config_get_string (db, tmp, NULL);
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
	bonobo_config_set_string (db, tmp, color, NULL);
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
	icon_file = bonobo_config_get_string (db, tmp, NULL);
	g_free (tmp);

	if (icon_file) {
		/* add new pixmap from file to the list */
		icon_data = g_new (icon_data_t, 1);
		icon_data->filename = icon_file;
		icon_data->pixmap = NULL;
		icon_data->mask = NULL;
		icon_data->pixbuf = gdk_pixbuf_new_from_file (icon_file);
		gdk_pixbuf_render_pixmap_and_mask (icon_data->pixbuf,
						   &icon_data->pixmap,
						   &icon_data->mask,
						   1);
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
	icon_file = bonobo_config_get_string (db, tmp, NULL);
	g_free (tmp);

	if (icon_file) {
		/* add new pixmap from file to the list */
		icon_data = g_new (icon_data_t, 1);
		icon_data->filename = icon_file;
		gdk_pixbuf_new_from_file (icon_file);
		gdk_pixbuf_render_pixmap_and_mask (icon_data->pixbuf,
						   &icon_data->pixmap,
						   &icon_data->mask,
						   0);
		g_hash_table_insert (cat_icons, (gpointer) category, (gpointer) icon_data);
	}

	return (const char *) icon_file;
}

/**
 * e_categories_config_set_icon_for
 * @category: Category for which to set the icon.
 * @icon_file: Full path of the icon file.
 */
void
e_categories_config_set_icon_for (const char *category, const char *icon_file)
{
	icon_data_t *icon_data;
	char *tmp;

	g_return_if_fail (category != NULL);
	g_return_if_fail (icon_file != NULL);

	if (!initialized)
		initialize_categories_config ();

	icon_data = g_hash_table_lookup (cat_icons, category);
	if (icon_data != NULL) {
		g_hash_table_remove (cat_icons, category);

		gdk_pixbuf_unref (icon_data->pixbuf);
		gdk_pixmap_unref (icon_data->pixmap);
		gdk_bitmap_unref (icon_data->mask);
		g_free (icon_data->filename);
		g_free (icon_data);
	}

	/* add new pixmap from file to the list */
	icon_data = g_new (icon_data_t, 1);
	icon_data->filename = g_strdup (icon_file);
	icon_data->pixbuf = gdk_pixbuf_new_from_file (icon_file);
	gdk_pixbuf_render_pixmap_and_mask (icon_data->pixbuf,
					   &icon_data->pixmap,
					   &icon_data->mask,
					   0);
	g_hash_table_insert (cat_icons, (gpointer) category, (gpointer) icon_data);

	/* ...and to the configuration */
	tmp = g_strdup_printf ("General/Categories/%s/Icon", category);
	bonobo_config_set_string (db, tmp, icon_file, NULL);
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
