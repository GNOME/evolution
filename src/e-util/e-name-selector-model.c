/* e-name-selector-model.c - Model for contact selection.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
 * Authors: Hans Petter Jansson <hpj@novell.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include "e-name-selector-model.h"

typedef struct {
	gchar              *name;
	gchar              *pretty_name;

	EDestinationStore  *destination_store;
}
Section;

struct _ENameSelectorModelPrivate {
	GArray *sections;
	EContactStore *contact_store;
	ETreeModelGenerator *contact_filter;
	GHashTable *destination_uid_hash;
};

static gint generate_contact_rows  (EContactStore *contact_store, GtkTreeIter *iter,
				    ENameSelectorModel *name_selector_model);
static void override_email_address (EContactStore *contact_store, GtkTreeIter *iter,
				    gint permutation_n, gint column, GValue *value,
				    ENameSelectorModel *name_selector_model);
static void free_section           (ENameSelectorModel *name_selector_model, gint n);

/* ------------------ *
 * Class/object setup *
 * ------------------ */

/* Signals */

enum {
	SECTION_ADDED,
	SECTION_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (ENameSelectorModel, e_name_selector_model, G_TYPE_OBJECT)

static void
e_name_selector_model_init (ENameSelectorModel *name_selector_model)
{
	name_selector_model->priv = e_name_selector_model_get_instance_private (name_selector_model);

	name_selector_model->priv->sections = g_array_new (FALSE, FALSE, sizeof (Section));
	name_selector_model->priv->contact_store = e_contact_store_new ();

	name_selector_model->priv->contact_filter =
		e_tree_model_generator_new (GTK_TREE_MODEL (name_selector_model->priv->contact_store));
	e_tree_model_generator_set_generate_func (
		name_selector_model->priv->contact_filter,
		(ETreeModelGeneratorGenerateFunc) generate_contact_rows,
		name_selector_model, NULL);
	e_tree_model_generator_set_modify_func (name_selector_model->priv->contact_filter,
						(ETreeModelGeneratorModifyFunc) override_email_address,
						name_selector_model, NULL);

	g_object_unref (name_selector_model->priv->contact_store);

	name_selector_model->priv->destination_uid_hash = NULL;
}

static void
name_selector_model_finalize (GObject *object)
{
	ENameSelectorModel *self = E_NAME_SELECTOR_MODEL (object);
	gint i;

	for (i = 0; i < self->priv->sections->len; i++)
		free_section (E_NAME_SELECTOR_MODEL (object), i);

	g_array_free (self->priv->sections, TRUE);
	g_object_unref (self->priv->contact_filter);

	if (self->priv->destination_uid_hash)
		g_hash_table_destroy (self->priv->destination_uid_hash);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_name_selector_model_parent_class)->finalize (object);
}

static void
e_name_selector_model_class_init (ENameSelectorModelClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = name_selector_model_finalize;

	signals[SECTION_ADDED] = g_signal_new (
		"section-added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ENameSelectorModelClass, section_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);

	signals[SECTION_REMOVED] = g_signal_new (
		"section-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ENameSelectorModelClass, section_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

/**
 * e_name_selector_model_new:
 *
 * Creates a new #ENameSelectorModel.
 *
 * Returns: A new #ENameSelectorModel.
 **/
ENameSelectorModel *
e_name_selector_model_new (void)
{
	return E_NAME_SELECTOR_MODEL (g_object_new (E_TYPE_NAME_SELECTOR_MODEL, NULL));
}

/* ---------------------------- *
 * GtkTreeModelFilter filtering *
 * ---------------------------- */

static void
deep_free_list (GList *list)
{
	GList *l;

	for (l = list; l; l = g_list_next (l))
		g_free (l->data);

	g_list_free (list);
}

static gint
generate_contact_rows (EContactStore *contact_store,
                       GtkTreeIter *iter,
                       ENameSelectorModel *name_selector_model)
{
	EContact    *contact;
	const gchar *contact_uid;
	gint         n_rows, used_rows = 0;
	gint         i;

	contact = e_contact_store_get_contact (contact_store, iter);
	g_return_val_if_fail (contact != NULL, 0);

	contact_uid = e_contact_get_const (contact, E_CONTACT_UID);
	if (!contact_uid)
		return 0;  /* Can happen with broken databases */

	for (i = 0; i < name_selector_model->priv->sections->len; i++) {
		Section *section;
		GList   *destinations;
		GList   *l;

		section = &g_array_index (name_selector_model->priv->sections, Section, i);
		destinations = e_destination_store_list_destinations (section->destination_store);

		for (l = destinations; l; l = g_list_next (l)) {
			EDestination *destination = l->data;
			const gchar  *destination_uid;

			destination_uid = e_destination_get_contact_uid (destination);
			if (destination_uid && !strcmp (contact_uid, destination_uid)) {
				used_rows++;
			}
		}

		g_list_free (destinations);
	}

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		n_rows = 1 - used_rows;
	} else {
		GList    *email_list;

		email_list = e_contact_get (contact, E_CONTACT_EMAIL);
		n_rows = g_list_length (email_list) - used_rows;
		deep_free_list (email_list);
	}

	g_return_val_if_fail (n_rows >= 0, 0);

	return n_rows;
}

static void
override_email_address (EContactStore *contact_store,
                        GtkTreeIter *iter,
                        gint permutation_n,
                        gint column,
                        GValue *value,
                        ENameSelectorModel *name_selector_model)
{
	if (column == E_CONTACT_EMAIL_1) {
		EContact *contact;
		GList    *email_list;
		gchar    *email;

		contact = e_contact_store_get_contact (contact_store, iter);
		email_list = e_name_selector_model_get_contact_emails_without_used (name_selector_model, contact, TRUE);
		g_return_if_fail (g_list_length (email_list) <= permutation_n);
		email = g_strdup (g_list_nth_data (email_list, permutation_n));
		g_value_set_string (value, email);
		e_name_selector_model_free_emails_list (email_list);
	} else {
		gtk_tree_model_get_value (GTK_TREE_MODEL (contact_store), iter, column, value);
	}
}

/* --------------- *
 * Section helpers *
 * --------------- */

typedef struct
{
	ENameSelectorModel *name_selector_model;
	GHashTable         *other_hash;
}
HashCompare;

static void
emit_destination_uid_changes_cb (gchar *uid_num,
                                 gpointer value,
                                 HashCompare *hash_compare)
{
	EContactStore *contact_store = hash_compare->name_selector_model->priv->contact_store;

	if (!hash_compare->other_hash || !g_hash_table_lookup (hash_compare->other_hash, uid_num)) {
		GtkTreeIter  iter;
		GtkTreePath *path;
		gchar *sep;

		sep = strrchr (uid_num, ':');
		g_return_if_fail (sep != NULL);

		*sep = '\0';
		if (e_contact_store_find_contact (contact_store, uid_num, &iter)) {
			*sep = ':';

			path = gtk_tree_model_get_path (GTK_TREE_MODEL (contact_store), &iter);
			gtk_tree_model_row_changed (GTK_TREE_MODEL (contact_store), path, &iter);
			gtk_tree_path_free (path);
		} else {
			*sep = ':';
		}
	}
}

static void
destinations_changed (ENameSelectorModel *name_selector_model)
{
	GHashTable  *destination_uid_hash_new;
	GHashTable  *destination_uid_hash_old;
	HashCompare  hash_compare;
	gint         i;

	destination_uid_hash_new = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (i = 0; i < name_selector_model->priv->sections->len; i++) {
		Section *section = &g_array_index (name_selector_model->priv->sections, Section, i);
		GList   *destinations;
		GList   *l;

		destinations = e_destination_store_list_destinations (section->destination_store);

		for (l = destinations; l; l = g_list_next (l)) {
			EDestination *destination = l->data;
			const gchar  *destination_uid;

			destination_uid = e_destination_get_contact_uid (destination);
			if (destination_uid)
				g_hash_table_insert (
					destination_uid_hash_new,
					g_strdup_printf (
						"%s:%d", destination_uid,
						e_destination_get_email_num (destination)),
					GINT_TO_POINTER (TRUE));
		}

		g_list_free (destinations);
	}

	destination_uid_hash_old = name_selector_model->priv->destination_uid_hash;
	name_selector_model->priv->destination_uid_hash = destination_uid_hash_new;

	hash_compare.name_selector_model = name_selector_model;

	hash_compare.other_hash = destination_uid_hash_old;
	g_hash_table_foreach (
		destination_uid_hash_new,
		(GHFunc) emit_destination_uid_changes_cb,
		&hash_compare);

	if (destination_uid_hash_old) {
		hash_compare.other_hash = destination_uid_hash_new;
		g_hash_table_foreach (
			destination_uid_hash_old,
			(GHFunc) emit_destination_uid_changes_cb,
			&hash_compare);

		g_hash_table_destroy (destination_uid_hash_old);
	}
}

static void
free_section (ENameSelectorModel *name_selector_model,
              gint n)
{
	Section *section;

	g_return_if_fail (n >= 0);
	g_return_if_fail (n < name_selector_model->priv->sections->len);

	section = &g_array_index (name_selector_model->priv->sections, Section, n);

	g_signal_handlers_disconnect_matched (
		section->destination_store, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, name_selector_model);

	g_free (section->name);
	g_free (section->pretty_name);
	g_object_unref (section->destination_store);
}

static gint
find_section_by_name (ENameSelectorModel *name_selector_model,
                      const gchar *name)
{
	gint i;

	g_return_val_if_fail (name != NULL, -1);

	for (i = 0; i < name_selector_model->priv->sections->len; i++) {
		Section *section = &g_array_index (name_selector_model->priv->sections, Section, i);

		if (!strcmp (name, section->name))
			return i;
	}

	return -1;
}

/* ---------------------- *
 * ENameSelectorModel API *
 * ---------------------- */

/**
 * e_name_selector_model_peek_contact_store:
 * @name_selector_model: an #ENameSelectorModel
 *
 * Gets the #EContactStore associated with @name_selector_model.
 *
 * Returns: An #EContactStore.
 **/
EContactStore *
e_name_selector_model_peek_contact_store (ENameSelectorModel *name_selector_model)
{
	g_return_val_if_fail (E_IS_NAME_SELECTOR_MODEL (name_selector_model), NULL);

	return name_selector_model->priv->contact_store;
}

/**
 * e_name_selector_model_peek_contact_filter:
 * @name_selector_model: an #ENameSelectorModel
 *
 * Gets the #ETreeModelGenerator being used to filter and/or extend the
 * list of contacts in @name_selector_model's #EContactStore.
 *
 * Returns: An #ETreeModelGenerator.
 **/
ETreeModelGenerator *
e_name_selector_model_peek_contact_filter (ENameSelectorModel *name_selector_model)
{
	g_return_val_if_fail (E_IS_NAME_SELECTOR_MODEL (name_selector_model), NULL);

	return name_selector_model->priv->contact_filter;
}

/**
 * e_name_selector_model_list_sections:
 * @name_selector_model: an #ENameSelectorModel
 *
 * Gets a list of the destination sections in @name_selector_model.
 *
 * Returns: A #GList of pointers to strings. The #GList and the
 * strings belong to the caller, and must be freed when no longer needed.
 **/
GList *
e_name_selector_model_list_sections (ENameSelectorModel *name_selector_model)
{
	GList *section_names = NULL;
	gint   i;

	g_return_val_if_fail (E_IS_NAME_SELECTOR_MODEL (name_selector_model), NULL);

	/* Do this backwards so we can use g_list_prepend () and get correct order */
	for (i = name_selector_model->priv->sections->len - 1; i >= 0; i--) {
		Section *section = &g_array_index (name_selector_model->priv->sections, Section, i);
		gchar   *name;

		name = g_strdup (section->name);
		section_names = g_list_prepend (section_names, name);
	}

	return section_names;
}

/**
 * e_name_selector_model_add_section:
 * @name_selector_model: an #ENameSelectorModel
 * @name: internal name of this section
 * @pretty_name: user-visible name of this section
 * @destination_store: the #EDestinationStore to use to store the destinations for this
 * section, or %NULL if @name_selector_model should create its own.
 *
 * Adds a destination section to @name_selector_model.
 **/
void
e_name_selector_model_add_section (ENameSelectorModel *name_selector_model,
                                   const gchar *name,
                                   const gchar *pretty_name,
                                   EDestinationStore *destination_store)
{
	Section section;

	g_return_if_fail (E_IS_NAME_SELECTOR_MODEL (name_selector_model));
	g_return_if_fail (name != NULL);
	g_return_if_fail (pretty_name != NULL);

	if (find_section_by_name (name_selector_model, name) >= 0) {
		g_warning ("ENameSelectorModel already has a section called '%s'!", name);
		return;
	}

	memset (&section, 0, sizeof (Section));

	section.name = g_strdup (name);
	section.pretty_name = g_strdup (pretty_name);

	if (destination_store)
		section.destination_store = g_object_ref (destination_store);
	else
		section.destination_store = e_destination_store_new ();

	g_signal_connect_swapped (
		section.destination_store, "row-changed",
		G_CALLBACK (destinations_changed), name_selector_model);
	g_signal_connect_swapped (
		section.destination_store, "row-deleted",
		G_CALLBACK (destinations_changed), name_selector_model);
	g_signal_connect_swapped (
		section.destination_store, "row-inserted",
		G_CALLBACK (destinations_changed), name_selector_model);

	g_array_append_val (name_selector_model->priv->sections, section);

	destinations_changed (name_selector_model);
	g_signal_emit (name_selector_model, signals[SECTION_ADDED], 0, name);
}

/**
 * e_name_selector_model_remove_section:
 * @name_selector_model: an #ENameSelectorModel
 * @name: internal name of the section to remove
 *
 * Removes a destination section from @name_selector_model.
 **/
void
e_name_selector_model_remove_section (ENameSelectorModel *name_selector_model,
                                      const gchar *name)
{
	gint     n;

	g_return_if_fail (E_IS_NAME_SELECTOR_MODEL (name_selector_model));
	g_return_if_fail (name != NULL);

	n = find_section_by_name (name_selector_model, name);
	if (n < 0) {
		g_warning ("ENameSelectorModel does not have a section called '%s'!", name);
		return;
	}

	free_section (name_selector_model, n);
	g_array_remove_index_fast (name_selector_model->priv->sections, n);  /* Order doesn't matter */

	destinations_changed (name_selector_model);
	g_signal_emit (name_selector_model, signals[SECTION_REMOVED], 0, name);
}

/**
 * e_name_selector_model_peek_section:
 * @name_selector_model: an #ENameSelectorModel
 * @name: internal name of the section to peek
 * @pretty_name: location in which to store a pointer to the user-visible name of the section,
 * or %NULL if undesired.
 * @destination_store: location in which to store a pointer to the #EDestinationStore being used
 * by the section, or %NULL if undesired
 *
 * Gets the parameters for a destination section.
 **/
gboolean
e_name_selector_model_peek_section (ENameSelectorModel *name_selector_model,
                                    const gchar *name,
                                    gchar **pretty_name,
                                    EDestinationStore **destination_store)
{
	Section *section;
	gint     n;

	g_return_val_if_fail (E_IS_NAME_SELECTOR_MODEL (name_selector_model), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	n = find_section_by_name (name_selector_model, name);
	if (n < 0) {
		g_warning ("ENameSelectorModel does not have a section called '%s'!", name);
		return FALSE;
	}

	section = &g_array_index (name_selector_model->priv->sections, Section, n);

	if (pretty_name)
		*pretty_name = g_strdup (section->pretty_name);
	if (destination_store)
		*destination_store = section->destination_store;

	return TRUE;
}

/**
 * e_name_selector_model_get_contact_emails_without_used:
 * @name_selector_model: an #ENameSelectorModel
 * @contact: to get emails from
 * @remove_used: set to %TRUE to remove used from a list; or set to %FALSE to
 * set used indexes to %NULL and keep them in the returned list
 *
 * Returns list of all email from @contact, without all used
 * in any section. Each item is a string, an email address.
 * Returned list should be freed with @e_name_selector_model_free_emails_list.
 *
 * Since: 2.30
 **/
GList *
e_name_selector_model_get_contact_emails_without_used (ENameSelectorModel *name_selector_model,
                                                       EContact *contact,
                                                       gboolean remove_used)
{
	GList *email_list;
	gint emails;
	gint i;
	const gchar *contact_uid;

	g_return_val_if_fail (name_selector_model != NULL, NULL);
	g_return_val_if_fail (E_IS_NAME_SELECTOR_MODEL (name_selector_model), NULL);
	g_return_val_if_fail (contact != NULL, NULL);
	g_return_val_if_fail (E_IS_CONTACT (contact), NULL);

	contact_uid = e_contact_get_const (contact, E_CONTACT_UID);
	g_return_val_if_fail (contact_uid != NULL, NULL);

	email_list = e_contact_get (contact, E_CONTACT_EMAIL);
	emails = g_list_length (email_list);

	for (i = 0; i < name_selector_model->priv->sections->len; i++) {
		Section *section;
		GList   *destinations;
		GList   *l;

		section = &g_array_index (name_selector_model->priv->sections, Section, i);
		destinations = e_destination_store_list_destinations (section->destination_store);

		for (l = destinations; l; l = g_list_next (l)) {
			EDestination *destination = l->data;
			const gchar  *destination_uid;

			destination_uid = e_destination_get_contact_uid (destination);
			if (destination_uid && !strcmp (contact_uid, destination_uid)) {
				gint email_num = e_destination_get_email_num (destination);

				if (email_num < 0 || email_num >= emails) {
					g_warning ("%s: Destination's email_num %d out of bounds 0..%d", G_STRFUNC, email_num, emails - 1);
				} else {
					GList *nth = g_list_nth (email_list, email_num);

					if (nth) {
						g_free (nth->data);
						nth->data = NULL;
					} else {
						g_warn_if_reached ();
					}
				}
			}
		}

		g_list_free (destinations);
	}

	if (remove_used) {
		/* remove all with data NULL, which are those used already */
		do {
			emails = g_list_length (email_list);
			email_list = g_list_remove (email_list, NULL);
		} while (g_list_length (email_list) != emails);
	}

	return email_list;
}

/**
 * e_name_selector_model_free_emails_list:
 * @email_list: list of emails returned from @e_name_selector_model_get_contact_emails_without_used
 *
 * Frees a list of emails returned from @e_name_selector_model_get_contact_emails_without_used.
 *
 * Since: 2.30
 **/
void
e_name_selector_model_free_emails_list (GList *email_list)
{
	deep_free_list (email_list);
}
