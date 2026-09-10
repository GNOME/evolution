/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>

#include "e-misc-utils.h"
#include "gal-view-virtual-tree.h"

struct _GalViewVirtualTree {
	GalView parent;
	GKeyFile *state;
	gchar *pending_legacy_xml;
	EVirtualTree *vtree;
	gulong columns_changed_id;
	gulong column_state_changed_id;
	gulong vtree_destroy_id;
	guint changed_idle_id;
	guint suppress_idle_id;
	gboolean suppress_changed;
};

G_DEFINE_FINAL_TYPE (GalViewVirtualTree, gal_view_virtual_tree, GAL_TYPE_VIEW)

static GKeyFile *
convert_legacy_xml_for_vtree (EVirtualTree *vtree,
			      const gchar *xml_data)
{
	const gchar * const *legacy_column_ids;
	guint n_legacy_column_ids = 0;

	legacy_column_ids = e_virtual_tree_get_legacy_etable_column_map (vtree, &n_legacy_column_ids);
	if (!legacy_column_ids)
		return NULL;

	return e_virtual_tree_convert_legacy_etable_state (
		xml_data, -1, legacy_column_ids, n_legacy_column_ids);
}

static GKeyFile *
copy_key_file (GKeyFile *src)
{
	GKeyFile *dest;
	gchar *data;

	if (!src)
		return NULL;

	dest = g_key_file_new ();
	data = g_key_file_to_data (src, NULL, NULL);
	if (data) {
		g_key_file_load_from_data (dest, data, -1, G_KEY_FILE_NONE, NULL);
		g_free (data);
	}

	return dest;
}

static gboolean
on_suppress_changed_clear_idle_cb (gpointer user_data)
{
	GalViewVirtualTree *self = user_data;

	self->suppress_idle_id = 0;

	if (self->vtree) {
		g_clear_pointer (&self->state, g_key_file_unref);
		self->state = g_key_file_new ();
		e_virtual_tree_save_column_state_to_key_file (self->vtree, self->state);
	}

	self->suppress_changed = FALSE;

	return G_SOURCE_REMOVE;
}

static void
reschedule_suppress_clear (GalViewVirtualTree *self)
{
	if (self->suppress_idle_id > 0)
		g_source_remove (self->suppress_idle_id);

	self->suppress_idle_id = g_timeout_add (100, on_suppress_changed_clear_idle_cb, self);
}

static gboolean
on_vtree_changed_idle_cb (gpointer user_data)
{
	GalViewVirtualTree *self = user_data;
	GKeyFile *current;
	gchar *current_data, *saved_data;
	gboolean changed;

	self->changed_idle_id = 0;

	if (!self->vtree) {
		gal_view_changed (GAL_VIEW (self));
		return G_SOURCE_REMOVE;
	}

	current = g_key_file_new ();
	e_virtual_tree_save_column_state_to_key_file (self->vtree, current);

	current_data = g_key_file_to_data (current, NULL, NULL);
	saved_data = self->state ? g_key_file_to_data (self->state, NULL, NULL) : NULL;
	changed = g_strcmp0 (current_data, saved_data) != 0;
	g_free (current_data);
	g_free (saved_data);

	if (changed) {
		g_clear_pointer (&self->state, g_key_file_unref);
		self->state = current;
		gal_view_changed (GAL_VIEW (self));
	} else {
		g_key_file_unref (current);
	}

	return G_SOURCE_REMOVE;
}

static void
on_vtree_changed (gpointer user_data)
{
	GalViewVirtualTree *self = user_data;

	if (self->vtree && e_virtual_tree_get_applying_proportional_widths (self->vtree))
		return;

	if (self->suppress_changed) {
		reschedule_suppress_clear (self);
		return;
	}

	if (!self->changed_idle_id)
		self->changed_idle_id = g_idle_add (on_vtree_changed_idle_cb, self);
}

static void
on_columns_changed (GtkTreeView *tree_view,
		    gpointer user_data)
{
	on_vtree_changed (user_data);
}

static void
on_column_state_changed (EVirtualTree *vtree,
			 gpointer user_data)
{
	on_vtree_changed (user_data);
}

static void
on_column_notify (GObject *object,
		  GParamSpec *pspec,
		  gpointer user_data)
{
	on_vtree_changed (user_data);
}

static void
connect_column_signals (GalViewVirtualTree *self)
{
	GtkTreeView *tree_view;
	GList *columns, *link;

	tree_view = e_virtual_tree_get_tree_view (self->vtree);

	columns = gtk_tree_view_get_columns (tree_view);

	for (link = columns; link; link = link->next) {
		GtkTreeViewColumn *col = link->data;

		g_signal_connect (col, "notify::visible",
			G_CALLBACK (on_column_notify), self);
		g_signal_connect (col, "notify::fixed-width",
			G_CALLBACK (on_column_notify), self);
	}

	g_list_free (columns);
}

static void
disconnect_column_signals (GalViewVirtualTree *self)
{
	GtkTreeView *tree_view;
	GList *columns, *link;

	if (!self->vtree)
		return;

	tree_view = e_virtual_tree_get_tree_view (self->vtree);
	if (!tree_view)
		return;

	columns = gtk_tree_view_get_columns (tree_view);

	for (link = columns; link; link = link->next) {
		g_signal_handlers_disconnect_by_func (link->data, on_column_notify, self);
	}

	g_list_free (columns);
}

static void
gal_view_virtual_tree_load (GalView *view,
			    const gchar *filename)
{
	GalViewVirtualTree *self = GAL_VIEW_VIRTUAL_TREE (view);

	g_clear_pointer (&self->state, g_key_file_unref);
	g_clear_pointer (&self->pending_legacy_xml, g_free);

	self->state = g_key_file_new ();
	if (!g_key_file_load_from_file (self->state, filename, G_KEY_FILE_NONE, NULL)) {
		gchar *contents = NULL;

		g_clear_pointer (&self->state, g_key_file_unref);

		if (g_file_get_contents (filename, &contents, NULL, NULL)) {
			if (self->vtree)
				self->state = convert_legacy_xml_for_vtree (self->vtree, contents);
			else
				self->pending_legacy_xml = g_strdup (contents);

			g_free (contents);
		}

		if (!self->state && !self->pending_legacy_xml)
			return;
	}

	if (self->vtree && self->state) {
		self->suppress_changed = TRUE;
		e_virtual_tree_load_column_state_from_key_file (self->vtree, self->state);

		reschedule_suppress_clear (self);
	}
}

static void
gal_view_virtual_tree_save (GalView *view,
			    const gchar *filename)
{
	GalViewVirtualTree *self = GAL_VIEW_VIRTUAL_TREE (view);

	if (self->vtree) {
		g_clear_pointer (&self->state, g_key_file_unref);
		self->state = g_key_file_new ();
		e_virtual_tree_save_column_state_to_key_file (self->vtree, self->state);
	}

	if (self->state)
		g_key_file_save_to_file (self->state, filename, NULL);
}

static GalView *
gal_view_virtual_tree_clone (GalView *view)
{
	GalViewVirtualTree *self = GAL_VIEW_VIRTUAL_TREE (view);
	GalView *clone;
	GalViewVirtualTree *clone_vtree;

	clone = GAL_VIEW_CLASS (gal_view_virtual_tree_parent_class)->clone (view);
	clone_vtree = GAL_VIEW_VIRTUAL_TREE (clone);

	g_clear_pointer (&clone_vtree->state, g_key_file_unref);
	clone_vtree->state = copy_key_file (self->state);

	g_clear_pointer (&clone_vtree->pending_legacy_xml, g_free);
	clone_vtree->pending_legacy_xml = g_strdup (self->pending_legacy_xml);

	return clone;
}

static void
gal_view_virtual_tree_dispose (GObject *object)
{
	GalViewVirtualTree *self = GAL_VIEW_VIRTUAL_TREE (object);

	if (self->suppress_idle_id > 0) {
		g_source_remove (self->suppress_idle_id);
		self->suppress_idle_id = 0;
	}

	if (self->changed_idle_id > 0) {
		g_source_remove (self->changed_idle_id);
		self->changed_idle_id = 0;
	}

	gal_view_virtual_tree_detach (self);

	G_OBJECT_CLASS (gal_view_virtual_tree_parent_class)->dispose (object);
}

static void
gal_view_virtual_tree_finalize (GObject *object)
{
	GalViewVirtualTree *self = GAL_VIEW_VIRTUAL_TREE (object);

	g_clear_pointer (&self->state, g_key_file_unref);
	g_clear_pointer (&self->pending_legacy_xml, g_free);

	G_OBJECT_CLASS (gal_view_virtual_tree_parent_class)->finalize (object);
}

static void
gal_view_virtual_tree_class_init (GalViewVirtualTreeClass *klass)
{
	GObjectClass *object_class;
	GalViewClass *gal_view_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = gal_view_virtual_tree_dispose;
	object_class->finalize = gal_view_virtual_tree_finalize;

	gal_view_class = GAL_VIEW_CLASS (klass);
	gal_view_class->type_code = "vtree";
	gal_view_class->legacy_type_code = "etable";
	gal_view_class->load = gal_view_virtual_tree_load;
	gal_view_class->save = gal_view_virtual_tree_save;
	gal_view_class->clone = gal_view_virtual_tree_clone;
}

static void
gal_view_virtual_tree_init (GalViewVirtualTree *self)
{
}

GalView *
gal_view_virtual_tree_new (const gchar *title)
{
	return g_object_new (GAL_TYPE_VIEW_VIRTUAL_TREE, "title", title, NULL);
}

static void
on_vtree_destroy (GtkWidget *widget,
		  gpointer user_data)
{
	gal_view_virtual_tree_detach (user_data);
}

void
gal_view_virtual_tree_attach (GalViewVirtualTree *self,
			      EVirtualTree *vtree)
{
	GtkTreeView *tree_view;

	g_return_if_fail (GAL_IS_VIEW_VIRTUAL_TREE (self));
	g_return_if_fail (E_IS_VIRTUAL_TREE (vtree));

	gal_view_virtual_tree_detach (self);

	self->vtree = g_object_ref (vtree);
	tree_view = e_virtual_tree_get_tree_view (vtree);

	self->suppress_changed = TRUE;

	if (!self->state && self->pending_legacy_xml) {
		self->state = convert_legacy_xml_for_vtree (vtree, self->pending_legacy_xml);
		g_clear_pointer (&self->pending_legacy_xml, g_free);
	}

	if (self->state)
		e_virtual_tree_load_column_state_from_key_file (vtree, self->state);

	self->columns_changed_id = g_signal_connect (tree_view, "columns-changed",
		G_CALLBACK (on_columns_changed), self);

	self->column_state_changed_id = g_signal_connect (vtree, "column-state-changed",
		G_CALLBACK (on_column_state_changed), self);

	self->vtree_destroy_id = g_signal_connect (vtree, "destroy",
		G_CALLBACK (on_vtree_destroy), self);

	connect_column_signals (self);

	reschedule_suppress_clear (self);
}

void
gal_view_virtual_tree_detach (GalViewVirtualTree *self)
{
	g_return_if_fail (GAL_IS_VIEW_VIRTUAL_TREE (self));

	if (!self->vtree)
		return;

	if (self->suppress_idle_id > 0) {
		g_source_remove (self->suppress_idle_id);
		self->suppress_idle_id = 0;
		self->suppress_changed = FALSE;
	}

	if (self->vtree_destroy_id > 0) {
		g_signal_handler_disconnect (self->vtree, self->vtree_destroy_id);
		self->vtree_destroy_id = 0;
	}

	if (self->column_state_changed_id > 0) {
		g_signal_handler_disconnect (self->vtree, self->column_state_changed_id);
		self->column_state_changed_id = 0;
	}

	disconnect_column_signals (self);

	if (self->columns_changed_id > 0) {
		GtkTreeView *tree_view;

		tree_view = e_virtual_tree_get_tree_view (self->vtree);
		if (tree_view)
			g_signal_handler_disconnect (tree_view, self->columns_changed_id);
		self->columns_changed_id = 0;
	}

	g_clear_object (&self->vtree);
}

EVirtualTree *
gal_view_virtual_tree_get_virtual_tree (GalViewVirtualTree *view)
{
	g_return_val_if_fail (GAL_IS_VIEW_VIRTUAL_TREE (view), NULL);

	return view->vtree;
}

gboolean
gal_view_virtual_tree_util_migrate_state_file (const gchar *path,
					       const gchar * const *legacy_column_ids,
					       guint n_legacy_column_ids)
{
	GKeyFile *key_file;
	gchar *contents = NULL;
	gsize length = 0;
	gboolean changed = FALSE;

	g_return_val_if_fail (path != NULL, FALSE);

	if (!g_file_get_contents (path, &contents, &length, NULL))
		return FALSE;

	key_file = e_virtual_tree_convert_legacy_etable_state (contents, (gssize) length, legacy_column_ids, n_legacy_column_ids);

	g_free (contents);

	if (key_file) {
		g_key_file_save_to_file (key_file, path, NULL);
		g_key_file_unref (key_file);
		changed = TRUE;
	}

	return changed;
}

static void
gal_view_virtual_tree_util_migrate_current_view_type_file (const gchar *path)
{
	gchar *contents = NULL;
	GString *result;

	if (!g_file_get_contents (path, &contents, NULL, NULL))
		return;

	if (!strstr (contents, "current_view_type=\"etable\"")) {
		g_free (contents);
		return;
	}

	result = e_str_replace_string (contents, "current_view_type=\"etable\"", "current_view_type=\"vtree\"");
	if (result) {
		g_file_set_contents (path, result->str, result->len, NULL);
		g_string_free (result, TRUE);
	}

	g_free (contents);
}

static void
gal_view_virtual_tree_util_migrate_galview_index_file (const gchar *path)
{
	gchar *contents = NULL;
	GString *result;

	if (!g_file_get_contents (path, &contents, NULL, NULL))
		return;

	if (!strstr (contents, "type=\"etable\"")) {
		g_free (contents);
		return;
	}

	result = e_str_replace_string (contents, "type=\"etable\"", "type=\"vtree\"");
	if (result) {
		g_file_set_contents (path, result->str, result->len, NULL);
		g_string_free (result, TRUE);
	}

	g_free (contents);
}

static gboolean
gal_view_virtual_tree_util_name_matches_any_prefix (const gchar *name,
						     const gchar * const *prefixes)
{
	guint ii;

	if (!prefixes)
		return FALSE;

	for (ii = 0; prefixes[ii]; ii++) {
		if (g_str_has_prefix (name, prefixes[ii]))
			return TRUE;
	}

	return FALSE;
}

void
gal_view_virtual_tree_util_migrate_views_dir (const gchar *views_dir,
					      const gchar * const *legacy_column_ids,
					      guint n_legacy_column_ids,
					      const gchar * const *extra_state_file_prefixes,
					      const gchar * const *extra_current_view_prefixes)
{
	gchar *galview_index;
	GDir *dir;

	g_return_if_fail (views_dir != NULL);

	galview_index = g_build_filename (views_dir, "galview.xml", NULL);
	gal_view_virtual_tree_util_migrate_galview_index_file (galview_index);
	g_free (galview_index);

	dir = g_dir_open (views_dir, 0, NULL);
	if (dir) {
		const gchar *name;

		while ((name = g_dir_read_name (dir)) != NULL) {
			gchar *path = g_build_filename (views_dir, name, NULL);

			if (g_str_has_suffix (name, ".galview") ||
			    gal_view_virtual_tree_util_name_matches_any_prefix (name, extra_state_file_prefixes))
				gal_view_virtual_tree_util_migrate_state_file (path, legacy_column_ids, n_legacy_column_ids);
			else if (g_str_has_prefix (name, "current_view-") ||
				 gal_view_virtual_tree_util_name_matches_any_prefix (name, extra_current_view_prefixes))
				gal_view_virtual_tree_util_migrate_current_view_type_file (path);

			g_free (path);
		}

		g_dir_close (dir);
	}
}
