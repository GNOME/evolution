/*
 * e-mail-shell-sidebar.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-shell-sidebar.h"

#include <string.h>
#include <camel/camel.h>

#include "e-util/e-binding.h"

#include "em-utils.h"
#include "em-folder-utils.h"

#include "e-mail-local.h"
#include "e-mail-store.h"

#define E_MAIL_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_SIDEBAR, EMailShellSidebarPrivate))

#define STATE_KEY_EXPANDED	"Expanded"

struct _EMailShellSidebarPrivate {
	GtkWidget *folder_tree;
};

enum {
	PROP_0,
	PROP_FOLDER_TREE
};

static gpointer parent_class;
static GType mail_shell_sidebar_type;

static void
mail_shell_sidebar_restore_state (EMailShellSidebar *mail_shell_sidebar)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	GtkTreeModel *tree_model;
	GtkTreeView *tree_view;
	GtkTreeIter iter;
	GKeyFile *key_file;
	gboolean valid;
	gchar *selected;
	gchar **groups;
	gint ii;

	shell_sidebar = E_SHELL_SIDEBAR (mail_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	key_file = e_shell_view_get_state_key_file (shell_view);

	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	tree_view = GTK_TREE_VIEW (folder_tree);
	tree_model = gtk_tree_view_get_model (tree_view);

	/* Restore selected folder. */

	selected = g_key_file_get_string (
		key_file, "Folder Tree", "Selected", NULL);
	if (selected != NULL) {
		em_folder_tree_set_selected (folder_tree, selected, FALSE);
		g_free (selected);
	}

	/* Set the initial folder tree expanded state in two stages:
	 *
	 * 1) Iterate over the "Store" and "Folder" state file groups
	 *    and apply the "Expanded" keys where possible.
	 *
	 * 2) Iterate over the top-level nodes in the folder tree
	 *    (these are all stores) and expand those that have no
	 *    corresponding "Expanded" key in the state file.  This
	 *    ensures that new stores are expanded by default.
	 */

	/* Stage 1 */

	groups = g_key_file_get_groups (key_file, NULL);

	for (ii = 0; groups[ii] != NULL; ii++) {
		GtkTreeRowReference *reference;
		GtkTreePath *path;
		GtkTreeIter iter;
		const gchar *group_name = groups[ii];
		const gchar *key = STATE_KEY_EXPANDED;
		const gchar *uri;
		gboolean expanded;

		if (g_str_has_prefix (group_name, "Store ")) {
			uri = group_name + 6;
			expanded = TRUE;
		} else if (g_str_has_prefix (group_name, "Folder ")) {
			uri = group_name + 7;
			expanded = FALSE;
		} else
			continue;

		if (g_key_file_has_key (key_file, group_name, key, NULL))
			expanded = g_key_file_get_boolean (
				key_file, group_name, key, NULL);

		if (!expanded)
			continue;

		reference = em_folder_tree_model_lookup_uri (
			EM_FOLDER_TREE_MODEL (tree_model), uri);
		if (reference == NULL)
			continue;

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_model_get_iter (tree_model, &iter, path);
		gtk_tree_view_expand_row (tree_view, path, FALSE);
		gtk_tree_path_free (path);
	}

	g_strfreev (groups);

	/* Stage 2 */

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (valid) {
		const gchar *key = STATE_KEY_EXPANDED;
		gchar *group_name;
		gchar *uri;

		gtk_tree_model_get (
			tree_model, &iter, COL_STRING_URI, &uri, -1);

		if (uri == NULL)
			goto next;

		group_name = g_strdup_printf ("Store %s", uri);

		if (!g_key_file_has_key (key_file, group_name, key, NULL)) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path (tree_model, &iter);
			gtk_tree_view_expand_row (tree_view, path, FALSE);
			gtk_tree_path_free (path);
		}

		g_free (group_name);
		g_free (uri);

	next:
		valid = gtk_tree_model_iter_next (tree_model, &iter);
	}
}

static void
mail_shell_sidebar_row_collapsed_cb (EShellSidebar *shell_sidebar,
                                     GtkTreeIter *iter,
                                     GtkTreePath *path,
                                     GtkTreeView *tree_view)
{
	EShellView *shell_view;
	GtkTreeModel *model;
	GKeyFile *key_file;
	const gchar *key;
	gboolean is_folder;
	gboolean is_store;
	gchar *group_name;
	gchar *uri;

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	key_file = e_shell_view_get_state_key_file (shell_view);

	model = gtk_tree_view_get_model (tree_view);

	gtk_tree_model_get (
		model, iter,
		COL_STRING_URI, &uri,
		COL_BOOL_IS_STORE, &is_store,
		COL_BOOL_IS_FOLDER, &is_folder, -1);

	g_return_if_fail (is_store || is_folder);

	key = STATE_KEY_EXPANDED;
	if (is_store)
		group_name = g_strdup_printf ("Store %s", uri);
	else
		group_name = g_strdup_printf ("Folder %s", uri);

	g_key_file_set_boolean (key_file, group_name, key, FALSE);
	e_shell_view_set_state_dirty (shell_view);

	g_free (group_name);
	g_free (uri);
}

static void
mail_shell_sidebar_row_expanded_cb (EShellSidebar *shell_sidebar,
                                    GtkTreeIter *unused,
                                    GtkTreePath *path,
                                    GtkTreeView *tree_view)
{
	EShellView *shell_view;
	GtkTreeModel *model;
	GKeyFile *key_file;
	const gchar *key;
	gboolean is_folder;
	gboolean is_store;
	gchar *group_name;
	gchar *uri;

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	key_file = e_shell_view_get_state_key_file (shell_view);

	path = gtk_tree_path_copy (path);
	model = gtk_tree_view_get_model (tree_view);

	/* Expand the node and all ancestors. */
	while (gtk_tree_path_get_depth (path) > 0) {
		GtkTreeIter iter;

		gtk_tree_model_get_iter (model, &iter, path);

		gtk_tree_model_get (
			model, &iter,
			COL_STRING_URI, &uri,
			COL_BOOL_IS_STORE, &is_store,
			COL_BOOL_IS_FOLDER, &is_folder, -1);

		g_return_if_fail (is_store || is_folder);

		key = STATE_KEY_EXPANDED;
		if (is_store)
			group_name = g_strdup_printf ("Store %s", uri);
		else
			group_name = g_strdup_printf ("Folder %s", uri);

		g_key_file_set_boolean (key_file, group_name, key, TRUE);
		e_shell_view_set_state_dirty (shell_view);

		g_free (group_name);
		g_free (uri);

		gtk_tree_path_up (path);
	}

	gtk_tree_path_free (path);
}

static void
mail_shell_sidebar_model_loaded_row_cb (EMailShellSidebar *mail_shell_sidebar,
                                        GtkTreePath *path,
                                        GtkTreeIter *iter,
                                        GtkTreeModel *model)
{
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	GtkTreeView *tree_view;
	GKeyFile *key_file;
	gboolean is_folder;
	gboolean is_store;
	const gchar *key;
	gchar *group_name;
	gchar *uri;
	gboolean expanded;

	shell_sidebar = E_SHELL_SIDEBAR (mail_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	key_file = e_shell_view_get_state_key_file (shell_view);

	tree_view = GTK_TREE_VIEW (mail_shell_sidebar->priv->folder_tree);

	gtk_tree_model_get (
		model, iter,
		COL_STRING_URI, &uri,
		COL_BOOL_IS_STORE, &is_store,
		COL_BOOL_IS_FOLDER, &is_folder, -1);

	g_return_if_fail (is_store || is_folder);

	key = STATE_KEY_EXPANDED;
	if (is_store) {
		group_name = g_strdup_printf ("Store %s", uri);
		expanded = TRUE;
	} else {
		group_name = g_strdup_printf ("Folder %s", uri);
		expanded = FALSE;
	}

	if (g_key_file_has_key (key_file, group_name, key, NULL))
		expanded = g_key_file_get_boolean (
			key_file, group_name, key, NULL);

	if (expanded)
		gtk_tree_view_expand_row (tree_view, path, FALSE);

	g_free (group_name);
	g_free (uri);
}

static void
mail_shell_sidebar_selection_changed_cb (EShellSidebar *shell_sidebar,
                                         GtkTreeSelection *selection)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GKeyFile *key_file;
	const gchar *icon_name;
	gchar *display_name = NULL;
	gchar *uri = NULL;
	gboolean is_folder = FALSE;
	guint flags = 0;

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	key_file = e_shell_view_get_state_key_file (shell_view);

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (
			model, &iter,
			COL_STRING_DISPLAY_NAME, &display_name,
			COL_STRING_URI, &uri,
			COL_BOOL_IS_FOLDER, &is_folder,
			COL_UINT_FLAGS, &flags, -1);

	if (uri != NULL)
		g_key_file_set_string (
			key_file, "Folder Tree", "Selected", uri);
	else
		g_key_file_remove_key (
			key_file, "Folder Tree", "Selected", NULL);

	e_shell_view_set_state_dirty (shell_view);

	if (is_folder)
		icon_name = em_folder_utils_get_icon_name (flags);
	else {
		icon_name = shell_view_class->icon_name;
		display_name = g_strdup (shell_view_class->label);
	}

	e_shell_sidebar_set_icon_name (shell_sidebar, icon_name);
	e_shell_sidebar_set_primary_text (shell_sidebar, display_name);

	g_free (display_name);
	g_free (uri);
}

static void
mail_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER_TREE:
			g_value_set_object (
				value, e_mail_shell_sidebar_get_folder_tree (
				E_MAIL_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_sidebar_dispose (GObject *object)
{
	EMailShellSidebarPrivate *priv;

	priv = E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->folder_tree != NULL) {
		g_object_unref (priv->folder_tree);
		priv->folder_tree = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_shell_sidebar_finalize (GObject *object)
{
	EMailShellSidebarPrivate *priv;

	priv = E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mail_shell_sidebar_constructed (GObject *object)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellSettings *shell_settings;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EShell *shell;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkWidget *container;
	GtkWidget *widget;

	/* Chain up to parent's constructed method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (object);

	/* Build sidebar widgets. */

	container = GTK_WIDGET (object);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = em_folder_tree_new ();
	em_folder_tree_set_excluded (EM_FOLDER_TREE (widget), 0);
	em_folder_tree_enable_drag_and_drop (EM_FOLDER_TREE (widget));
	gtk_container_add (GTK_CONTAINER (container), widget);
	mail_shell_sidebar->priv->folder_tree = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_new (
		shell_settings, "mail-side-bar-search",
		widget, "enable-search");

	tree_view = GTK_TREE_VIEW (mail_shell_sidebar->priv->folder_tree);
	selection = gtk_tree_view_get_selection (tree_view);
	model = gtk_tree_view_get_model (tree_view);

	if (em_folder_tree_model_get_selection (
		EM_FOLDER_TREE_MODEL (model)) == NULL)
		mail_shell_sidebar_restore_state (mail_shell_sidebar);

	em_folder_tree_model_set_selection (
		EM_FOLDER_TREE_MODEL (model), selection);

	g_signal_connect_swapped (
		tree_view, "row-collapsed",
		G_CALLBACK (mail_shell_sidebar_row_collapsed_cb),
		shell_sidebar);

	g_signal_connect_swapped (
		tree_view, "row-expanded",
		G_CALLBACK (mail_shell_sidebar_row_expanded_cb),
		shell_sidebar);

	g_signal_connect_swapped (
		model, "loaded-row",
		G_CALLBACK (mail_shell_sidebar_model_loaded_row_cb),
		shell_sidebar);

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (mail_shell_sidebar_selection_changed_cb),
		shell_sidebar);
}

static guint32
mail_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellView *shell_view;
	EMFolderTree *folder_tree;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelFolder *folder;
	CamelStore *local_store;
	CamelStore *store;
	gchar *full_name;
	gchar *uri;
	gboolean allows_children = TRUE;
	gboolean can_delete = TRUE;
	gboolean is_junk = FALSE;
	gboolean is_outbox = FALSE;
	gboolean is_store;
	gboolean is_trash = FALSE;
	guint32 folder_flags = 0;
	guint32 state = 0;

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);

	local_store = e_mail_local_get_store ();

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	tree_view = GTK_TREE_VIEW (folder_tree);

	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return 0;

	gtk_tree_model_get (
		model, &iter,
		COL_POINTER_CAMEL_STORE, &store,
		COL_STRING_FULL_NAME, &full_name,
		COL_BOOL_IS_STORE, &is_store,
		COL_UINT_FLAGS, &folder_flags,
		COL_STRING_URI, &uri, -1);

	if (!is_store) {
		is_junk = (strcmp (full_name, CAMEL_VJUNK_NAME) == 0);
		is_trash = (strcmp (full_name, CAMEL_VTRASH_NAME) == 0);
		allows_children = !(is_junk || is_trash);

		/* Don't allow deletion of special local folders. */
		if (store == local_store)
			can_delete =
				(strcmp (full_name, "Drafts") != 0) &&
				(strcmp (full_name, "Inbox") != 0) &&
				(strcmp (full_name, "Outbox") != 0) &&
				(strcmp (full_name, "Sent") != 0) &&
				(strcmp (full_name, "Templates") != 0);

		folder = em_folder_tree_get_selected_folder (folder_tree);
		is_outbox = em_utils_folder_is_outbox (folder, NULL);
		can_delete &= !(folder_flags & CAMEL_FOLDER_SYSTEM);
	}

	if (allows_children)
		state |= E_MAIL_SHELL_SIDEBAR_FOLDER_ALLOWS_CHILDREN;
	if (can_delete)
		state |= E_MAIL_SHELL_SIDEBAR_FOLDER_CAN_DELETE;
	if (is_junk)
		state |= E_MAIL_SHELL_SIDEBAR_FOLDER_IS_JUNK;
	if (is_outbox)
		state |= E_MAIL_SHELL_SIDEBAR_FOLDER_IS_OUTBOX;
	if (is_store)
		state |= E_MAIL_SHELL_SIDEBAR_FOLDER_IS_STORE;
	if (is_trash)
		state |= E_MAIL_SHELL_SIDEBAR_FOLDER_IS_TRASH;

	return state;
}

static void
mail_shell_sidebar_class_init (EMailShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_shell_sidebar_get_property;
	object_class->dispose = mail_shell_sidebar_dispose;
	object_class->finalize = mail_shell_sidebar_finalize;
	object_class->constructed = mail_shell_sidebar_constructed;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = mail_shell_sidebar_check_state;

	g_object_class_install_property (
		object_class,
		PROP_FOLDER_TREE,
		g_param_spec_object (
			"folder-tree",
			NULL,
			NULL,
			EM_TYPE_FOLDER_TREE,
			G_PARAM_READABLE));
}

static void
mail_shell_sidebar_init (EMailShellSidebar *mail_shell_sidebar)
{
	mail_shell_sidebar->priv =
		E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (mail_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_mail_shell_sidebar_get_type (void)
{
	return mail_shell_sidebar_type;
}

void
e_mail_shell_sidebar_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EMailShellSidebarClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_shell_sidebar_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailShellSidebar),
		0,     /* n_preallocs */
		(GInstanceInitFunc) mail_shell_sidebar_init,
		NULL   /* value_table */
	};

	mail_shell_sidebar_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_SIDEBAR,
		"EMailShellSidebar", &type_info, 0);
}

GtkWidget *
e_mail_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

EMFolderTree *
e_mail_shell_sidebar_get_folder_tree (EMailShellSidebar *mail_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_SIDEBAR (mail_shell_sidebar), NULL);

	return EM_FOLDER_TREE (mail_shell_sidebar->priv->folder_tree);
}
