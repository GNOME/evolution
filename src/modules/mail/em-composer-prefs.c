/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#include "evolution-config.h"

#include "em-composer-prefs.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <composer/e-msg-composer.h>

#include <shell/e-shell-utils.h>

#include <mail/em-config.h>
#include <mail/em-folder-selection-button.h>
#include <mail/em-folder-selector.h>
#include <mail/em-folder-tree.h>
#include <mail/em-folder-tree-model.h>
#include <mail/em-composer-utils.h>
#include <mail/e-mail-backend.h>
#include <mail/e-mail-junk-options.h>
#include <mail/e-mail-ui-session.h>

G_DEFINE_TYPE (
	EMComposerPrefs,
	em_composer_prefs,
	GTK_TYPE_BOX)

static void
composer_prefs_dispose (GObject *object)
{
	EMComposerPrefs *prefs = (EMComposerPrefs *) object;

	g_clear_object (&prefs->builder);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_composer_prefs_parent_class)->dispose (object);
}

static void
em_composer_prefs_class_init (EMComposerPrefsClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = composer_prefs_dispose;
}

static void
em_composer_prefs_init (EMComposerPrefs *prefs)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (prefs), GTK_ORIENTATION_VERTICAL);
}

static void
spell_language_toggled_cb (GtkCellRendererToggle *renderer,
                           const gchar *path_string,
                           EMComposerPrefs *prefs)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean active;
	gboolean valid;

	model = prefs->language_model;

	/* Convert the path string to a tree iterator. */
	path = gtk_tree_path_new_from_string (path_string);
	valid = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	g_return_if_fail (valid);

	/* Toggle the active state. */
	gtk_tree_model_get (model, &iter, 0, &active, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, !active, -1);
}

static void
spell_language_save (EMComposerPrefs *prefs)
{
	GList *spell_languages = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;

	model = prefs->language_model;

	/* Build a list of active spell languages. */
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
		ESpellDictionary *language;
		gboolean active;

		gtk_tree_model_get (
			model, &iter, 0, &active, 2, &language, -1);

		if (active)
			spell_languages = g_list_prepend (
				spell_languages, (gpointer) language);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
	spell_languages = g_list_reverse (spell_languages);

	/* Update the GSettings value. */
	e_save_spell_languages (spell_languages);

	g_list_free (spell_languages);
}

static void
spell_setup (EMComposerPrefs *prefs)
{
	GList *list = NULL, *link;
	GtkListStore *store;

	store = GTK_LIST_STORE (prefs->language_model);
	list = e_spell_checker_list_available_dicts (prefs->spell_checker);

	/* Populate the GtkListStore. */
	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary;
		GtkTreeIter tree_iter;
		const gchar *name;
		const gchar *code;
		gboolean active;

		dictionary = E_SPELL_DICTIONARY (link->data);
		name = e_spell_dictionary_get_name (dictionary);
		code = e_spell_dictionary_get_code (dictionary);

		active = e_spell_checker_get_language_active (
			prefs->spell_checker, code);

		gtk_list_store_append (store, &tree_iter);

		gtk_list_store_set (
			store, &tree_iter,
			0, active, 1, name, 2, dictionary, -1);
	}

	g_list_free (list);
}

#define MAIL_SEND_ACCOUNT_OVERRIDE_KEY "sao-mail-send-account-override"
#define MAIL_CAMEL_SESSION_KEY "sao-mail-camel-session"

static gchar *
sao_dup_account_uid (GtkBuilder *builder,
		     gchar **out_alias_name,
		     gchar **out_alias_address)
{
	GtkWidget *widget;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *account_uid = NULL;

	widget = e_builder_get_widget (builder, "sao-account-treeview");
	g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), NULL);

	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_UID, &account_uid,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_NAME, out_alias_name,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_ADDRESS, out_alias_address,
		-1);

	return account_uid;
}

static void
sao_fill_overrides (GtkBuilder *builder,
                    const gchar *tree_view_name,
                    GList *overrides,
                    gboolean is_folder)
{
	CamelSession *session = NULL;
	GtkWidget *widget;
	GtkListStore *list_store;
	GtkTreeIter titer;
	GList *oiter;

	widget = e_builder_get_widget (builder, tree_view_name);
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	g_return_if_fail (list_store != NULL);

	gtk_list_store_clear (list_store);

	if (is_folder)
		session = g_object_get_data (G_OBJECT (builder), MAIL_CAMEL_SESSION_KEY);

	for (oiter = overrides; oiter; oiter = g_list_next (oiter)) {
		const gchar *value = oiter->data;
		gchar *markup = NULL;

		if (!value || !*value)
			continue;

		if (is_folder) {
			markup = e_mail_folder_uri_to_markup (session, value, NULL);
			if (!markup)
				continue;
		}

		gtk_list_store_append (list_store, &titer);

		if (is_folder) {
			gtk_list_store_set (list_store, &titer, 0, markup, 1, value, -1);
		} else {
			gtk_list_store_set (list_store, &titer, 0, value, -1);
		}

		g_free (markup);
	}
}

static void
sao_account_treeview_selection_changed_cb (GtkTreeSelection *selection,
                                           GtkBuilder *builder)
{
	GtkTreeModel *model;
	GtkWidget *widget;
	gboolean enable = FALSE;

	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "sao-folders-treeview");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	widget = e_builder_get_widget (builder, "sao-recipients-treeview");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	if (gtk_tree_selection_get_selected (selection, NULL, NULL)) {
		gchar *account_uid, *alias_name = NULL, *alias_address = NULL;

		account_uid = sao_dup_account_uid (builder, &alias_name, &alias_address);
		if (account_uid) {
			GList *folder_overrides = NULL;
			GList *recipient_overrides = NULL;

			enable = TRUE;

			e_mail_send_account_override_list_for_account (
				g_object_get_data (
					G_OBJECT (builder),
					MAIL_SEND_ACCOUNT_OVERRIDE_KEY),
				account_uid, alias_name, alias_address, &folder_overrides, &recipient_overrides);

			sao_fill_overrides (
				builder, "sao-folders-treeview",
				folder_overrides, TRUE);
			sao_fill_overrides (
				builder, "sao-recipients-treeview",
				recipient_overrides, FALSE);

			g_list_free_full (folder_overrides, g_free);
			g_list_free_full (recipient_overrides, g_free);
			g_free (account_uid);
			g_free (alias_name);
			g_free (alias_address);
		}
	}

	widget = e_builder_get_widget (builder, "sao-folders-frame");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, enable);

	widget = e_builder_get_widget (builder, "sao-recipients-frame");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, enable);
}

static void
sao_account_row_changed_cb (GtkTreeModel *model,
			    GtkTreePath *path,
			    GtkTreeIter *iter,
			    GtkBuilder *builder)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;

	if (gtk_tree_model_iter_n_children (model, NULL) != 1)
		return;

	widget = e_builder_get_widget (builder, "sao-account-treeview");
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	if (!gtk_tree_selection_get_selected (selection, NULL, NULL))
		gtk_tree_selection_select_iter (selection, iter);
}

static void
sao_overrides_changed_cb (EMailSendAccountOverride *account_override,
                          GtkBuilder *builder)
{
	GtkWidget *widget;
	GtkTreeSelection *selection;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "sao-account-treeview");
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	sao_account_treeview_selection_changed_cb (selection, builder);
}

static void
sao_block_changed_handler (GtkBuilder *builder)
{
	GObject *account_override;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	account_override = g_object_get_data (G_OBJECT (builder), MAIL_SEND_ACCOUNT_OVERRIDE_KEY),
	g_signal_handlers_block_by_func (account_override, sao_overrides_changed_cb, builder);
}

static void
sao_unblock_changed_handler (GtkBuilder *builder)
{
	GObject *account_override;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	account_override = g_object_get_data (G_OBJECT (builder), MAIL_SEND_ACCOUNT_OVERRIDE_KEY),
	g_signal_handlers_unblock_by_func (account_override, sao_overrides_changed_cb, builder);
}

static void
sao_folders_treeview_selection_changed_cb (GtkTreeSelection *selection,
                                           GtkBuilder *builder)
{
	GtkWidget *widget;
	gint nselected;

	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	nselected = gtk_tree_selection_count_selected_rows (selection);

	widget = e_builder_get_widget (builder, "sao-folders-remove-button");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, nselected > 0);
}

static void
sao_folders_add_button_clicked_cb (GtkButton *button,
                                   GtkBuilder *builder)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	GtkWindow *window;
	gchar *account_uid, *alias_name = NULL, *alias_address = NULL;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	account_uid = sao_dup_account_uid (builder, &alias_name, &alias_address);
	g_return_if_fail (account_uid != NULL);

	widget = e_builder_get_widget (builder, "sao-folders-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));
	tree_view = GTK_TREE_VIEW (widget);

	window = GTK_WINDOW (gtk_widget_get_toplevel (widget));

	dialog = em_folder_selector_new (
		window, em_folder_tree_model_get_default ());

	gtk_window_set_title (
		GTK_WINDOW (dialog), _("Select Folder to Add"));

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_default_button_label (selector, _("_Add"));

	folder_tree = em_folder_selector_get_folder_tree (selector);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOSELECT);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		GList *list, *folder_uris;

		model = gtk_tree_view_get_model (tree_view);

		folder_uris = em_folder_tree_get_selected_uris (folder_tree);

		for (list = folder_uris; list; list = g_list_next (list)) {
			const gchar *uri = list->data;

			if (uri && *uri) {
				gboolean found = FALSE;

				if (gtk_tree_model_get_iter_first (model, &iter)) {
					do {
						gchar *old_uri = NULL;

						gtk_tree_model_get (model, &iter, 1, &old_uri, -1);

						found = g_strcmp0 (uri, old_uri) == 0;

						g_free (old_uri);
					} while (!found && gtk_tree_model_iter_next (model, &iter));
				}

				if (!found) {
					EMailSendAccountOverride *account_override;
					GtkListStore *list_store;
					CamelSession *session;
					gchar *markup;

					list_store = GTK_LIST_STORE (model);
					session = g_object_get_data (G_OBJECT (builder), MAIL_CAMEL_SESSION_KEY);
					markup = e_mail_folder_uri_to_markup (session, uri, NULL);

					gtk_list_store_append (list_store, &iter);
					gtk_list_store_set (list_store, &iter, 0, markup, 1, uri, -1);

					g_free (markup);

					sao_block_changed_handler (builder);

					account_override = g_object_get_data (G_OBJECT (builder), MAIL_SEND_ACCOUNT_OVERRIDE_KEY);
					e_mail_send_account_override_set_for_folder (account_override, uri, account_uid, alias_name, alias_address);

					sao_unblock_changed_handler (builder);
				}

				if (!list->next) {
					selection = gtk_tree_view_get_selection (tree_view);

					gtk_tree_selection_unselect_all (selection);
					gtk_tree_selection_select_iter (selection, &iter);
				}
			}
		}

		g_list_free_full (folder_uris, g_free);
	}

	gtk_widget_destroy (dialog);
	g_free (account_uid);
	g_free (alias_name);
	g_free (alias_address);
}

static void
sao_folders_remove_button_clicked_cb (GtkButton *button,
                                      GtkBuilder *builder)
{
	EMailSendAccountOverride *account_override;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	GList *selected, *siter;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "sao-folders-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));
	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);
	model = gtk_tree_view_get_model (tree_view);

	sao_block_changed_handler (builder);

	account_override = g_object_get_data (G_OBJECT (builder), MAIL_SEND_ACCOUNT_OVERRIDE_KEY);
	e_mail_send_account_override_freeze_save (account_override);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	selected = g_list_reverse (selected);

	for (siter = selected; siter; siter = g_list_next (siter)) {
		gchar *uri = NULL;

		if (!gtk_tree_model_get_iter (model, &iter, siter->data))
			continue;

		gtk_tree_model_get (model, &iter, 1, &uri, -1);

		if (uri && *uri)
			e_mail_send_account_override_remove_for_folder (account_override, uri);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		g_free (uri);
	}

	e_mail_send_account_override_thaw_save (account_override);
	sao_unblock_changed_handler (builder);

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
}

static void
sao_recipients_treeview_selection_changed_cb (GtkTreeSelection *selection,
                                              GtkBuilder *builder)
{
	GtkWidget *widget;
	gint nselected;

	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	nselected = gtk_tree_selection_count_selected_rows (selection);

	widget = e_builder_get_widget (builder, "sao-recipients-edit-button");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, nselected == 1);

	widget = e_builder_get_widget (builder, "sao-recipients-remove-button");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, nselected > 0);
}

static void
sao_recipient_edited_cb (GtkCellRendererText *renderer,
                         const gchar *path_str,
                         const gchar *new_text,
                         GtkBuilder *builder)
{
	EMailSendAccountOverride *account_override;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text, *old_text = NULL, *account_uid, *alias_name = NULL, *alias_address = NULL;
	GtkWidget *widget;

	g_return_if_fail (path_str != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "sao-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	path = gtk_tree_path_new_from_string (path_str);
	g_return_if_fail (path != NULL);

	account_uid = sao_dup_account_uid (builder, &alias_name, &alias_address);
	g_return_if_fail (account_uid != NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter, 0, &old_text, -1);

	sao_block_changed_handler (builder);

	account_override = g_object_get_data (G_OBJECT (builder), MAIL_SEND_ACCOUNT_OVERRIDE_KEY);

	text = g_strdup (new_text);
	if (text)
		g_strchomp (text);

	if (old_text && *old_text)
		e_mail_send_account_override_remove_for_recipient (account_override, old_text);

	if (!text || !*text) {
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	} else {
		GtkTreeIter new_iter = iter;
		gboolean is_new = TRUE;

		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gchar *old_recipient = NULL;

				gtk_tree_model_get (model, &iter, 0, &old_recipient, -1);

				is_new = !old_recipient || e_util_utf8_strcasecmp (text, old_recipient) != 0;

				g_free (old_recipient);
			} while (is_new && gtk_tree_model_iter_next (model, &iter));
		}

		if (is_new) {
			gtk_list_store_set (GTK_LIST_STORE (model), &new_iter, 0, text, -1);
			e_mail_send_account_override_set_for_recipient (account_override, text, account_uid, alias_name, alias_address);
		} else {
			GtkTreeSelection *selection;
			GtkTreePath *path1, *path2;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

			path1 = gtk_tree_model_get_path (model, &iter);
			path2 = gtk_tree_model_get_path (model, &new_iter);

			if (!path1 || !path2 || gtk_tree_path_compare (path1, path2) != 0)
				gtk_list_store_remove (GTK_LIST_STORE (model), &new_iter);

			gtk_tree_path_free (path1);
			gtk_tree_path_free (path2);

			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_iter (selection, &iter);
		}
	}

	sao_unblock_changed_handler (builder);

	g_free (account_uid);
	g_free (alias_name);
	g_free (alias_address);
	g_free (old_text);
	g_free (text);
}

static void
sao_recipient_editing_canceled_cb (GtkCellRenderer *renderer,
                                   GtkBuilder *builder)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "sao-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *old_recipient = NULL;

			gtk_tree_model_get (model, &iter, 0, &old_recipient, -1);

			if (!old_recipient || !*old_recipient) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				g_free (old_recipient);
				break;
			}

			g_free (old_recipient);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

static void
sao_recipients_add_button_clicked_cb (GtkButton *button,
                                      GtkBuilder *builder)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkWidget *widget;
	GList *cells;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "sao-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	column = gtk_tree_view_get_column (tree_view, 0);
	g_return_if_fail (column != NULL);

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	g_return_if_fail (cells != NULL);

	path = gtk_tree_model_get_path (model, &iter);
	if (path == NULL) {
		g_list_free (cells);
		return;
	}

	g_object_set (cells->data, "editable", TRUE, NULL);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, cells->data, TRUE);
	g_object_set (cells->data, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
	g_list_free (cells);
}

static void
sao_recipients_edit_button_clicked_cb (GtkButton *button,
                                       GtkBuilder *builder)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkWidget *widget;
	GList *cells, *selected;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "sao-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);

	g_return_if_fail (gtk_tree_selection_count_selected_rows (selection) == 1);

	selected = gtk_tree_selection_get_selected_rows (selection, NULL);
	g_return_if_fail (selected && selected->next == NULL);

	path = selected->data;
	/* 'path' is freed later in the function */
	g_list_free (selected);

	column = gtk_tree_view_get_column (tree_view, 0);
	g_return_if_fail (column != NULL);

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	g_return_if_fail (cells != NULL);

	g_object_set (cells->data, "editable", TRUE, NULL);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, cells->data, TRUE);
	g_object_set (cells->data, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
	g_list_free (cells);
}

static void
sao_recipients_remove_button_clicked_cb (GtkButton *button,
                                         GtkBuilder *builder)
{
	EMailSendAccountOverride *account_override;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	GList *selected, *siter;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "sao-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));
	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);
	model = gtk_tree_view_get_model (tree_view);

	sao_block_changed_handler (builder);

	account_override = g_object_get_data (G_OBJECT (builder), MAIL_SEND_ACCOUNT_OVERRIDE_KEY);
	e_mail_send_account_override_freeze_save (account_override);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	selected = g_list_reverse (selected);

	for (siter = selected; siter; siter = g_list_next (siter)) {
		gchar *recipient = NULL;

		if (!gtk_tree_model_get_iter (model, &iter, siter->data))
			continue;

		gtk_tree_model_get (model, &iter, 0, &recipient, -1);

		if (recipient && *recipient)
			e_mail_send_account_override_remove_for_recipient (account_override, recipient);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		g_free (recipient);
	}

	e_mail_send_account_override_thaw_save (account_override);
	sao_unblock_changed_handler (builder);

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
}

#define MAIL_COMPOSER_MODE_OVERRIDE_KEY "cmo-mail-composer-mode-override"
#define MAIL_CMO_CAMEL_SESSION_KEY "cmo-mail-camel-session"

enum {
	CMO_FOLDERS_COLUMN_MARKUP,
	CMO_FOLDERS_COLUMN_URI,
	CMO_FOLDERS_COLUMN_MODE,
	CMO_FOLDERS_COLUMN_MODE_NAME,
	CMO_FOLDERS_N_COLUMNS
};

enum {
	CMO_RECIPIENTS_COLUMN_RECIPIENT,
	CMO_RECIPIENTS_COLUMN_MODE,
	CMO_RECIPIENTS_COLUMN_MODE_NAME,
	CMO_RECIPIENTS_N_COLUMNS
};

enum {
	CMO_MODE_COMBO_COLUMN_NAME,
	CMO_MODE_COMBO_COLUMN_VALUE,
	CMO_MODE_COMBO_N_COLUMNS
};

static const gchar *
cmo_mode_to_name (EContentEditorMode mode)
{
	switch (mode) {
	case E_CONTENT_EDITOR_MODE_PLAIN_TEXT:
		return _("Plain Text");
	case E_CONTENT_EDITOR_MODE_HTML:
		return _("HTML");
	case E_CONTENT_EDITOR_MODE_MARKDOWN:
		return _("Markdown");
	case E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT:
		return _("Markdown as Plain Text");
	case E_CONTENT_EDITOR_MODE_MARKDOWN_HTML:
		return _("Markdown as HTML");
	default:
		break;
	}

	return _("Unknown");
}

static GtkListStore *
cmo_create_mode_model (void)
{
	GtkListStore *store;
	GtkTreeIter iter;
	guint ii;
	EContentEditorMode modes[] = {
		E_CONTENT_EDITOR_MODE_PLAIN_TEXT,
		E_CONTENT_EDITOR_MODE_HTML,
		E_CONTENT_EDITOR_MODE_MARKDOWN,
		E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT,
		E_CONTENT_EDITOR_MODE_MARKDOWN_HTML
	};

	store = gtk_list_store_new (CMO_MODE_COMBO_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

	for (ii = 0; ii < G_N_ELEMENTS (modes); ii++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
			CMO_MODE_COMBO_COLUMN_NAME, cmo_mode_to_name (modes[ii]),
			CMO_MODE_COMBO_COLUMN_VALUE, modes[ii],
			-1);
	}

	return store;
}

static void
cmo_fill_overrides (GtkBuilder *builder)
{
	EMailComposerModeOverride *cmo_override;
	CamelSession *session;
	GPtrArray *folders_array;
	GPtrArray *recipients_array;
	GtkWidget *widget;
	GtkListStore *list_store;
	GtkTreeIter titer;
	guint ii;

	cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);
	g_return_if_fail (cmo_override != NULL);

	session = g_object_get_data (G_OBJECT (builder), MAIL_CMO_CAMEL_SESSION_KEY);

	/* Fill folders treeview */
	widget = e_builder_get_widget (builder, "cmo-folders-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	g_return_if_fail (list_store != NULL);

	gtk_list_store_clear (list_store);

	folders_array = e_mail_composer_mode_override_list_all_folders (cmo_override);
	for (ii = 0; ii < folders_array->len; ii++) {
		EMailComposerModeOverrideEntry *entry = g_ptr_array_index (folders_array, ii);
		gchar *markup;

		if (!entry->key || !*entry->key)
			continue;

		markup = e_mail_folder_uri_to_markup (session, entry->key, NULL);
		if (!markup)
			continue;

		gtk_list_store_append (list_store, &titer);
		gtk_list_store_set (list_store, &titer,
			CMO_FOLDERS_COLUMN_MARKUP, markup,
			CMO_FOLDERS_COLUMN_URI, entry->key,
			CMO_FOLDERS_COLUMN_MODE, entry->mode,
			CMO_FOLDERS_COLUMN_MODE_NAME, cmo_mode_to_name (entry->mode),
			-1);

		g_free (markup);
	}
	g_ptr_array_unref (folders_array);

	/* Fill recipients treeview */
	widget = e_builder_get_widget (builder, "cmo-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	g_return_if_fail (list_store != NULL);

	gtk_list_store_clear (list_store);

	recipients_array = e_mail_composer_mode_override_list_all_recipients (cmo_override);
	for (ii = 0; ii < recipients_array->len; ii++) {
		EMailComposerModeOverrideEntry *entry = g_ptr_array_index (recipients_array, ii);

		if (!entry->key || !*entry->key)
			continue;

		gtk_list_store_append (list_store, &titer);
		gtk_list_store_set (list_store, &titer,
			CMO_RECIPIENTS_COLUMN_RECIPIENT, entry->key,
			CMO_RECIPIENTS_COLUMN_MODE, entry->mode,
			CMO_RECIPIENTS_COLUMN_MODE_NAME, cmo_mode_to_name (entry->mode),
			-1);
	}
	g_ptr_array_unref (recipients_array);
}

static void
cmo_overrides_changed_cb (EMailComposerModeOverride *cmo_override,
                          GtkBuilder *builder)
{
	g_return_if_fail (GTK_IS_BUILDER (builder));

	cmo_fill_overrides (builder);
}

static void
cmo_block_changed_handler (GtkBuilder *builder)
{
	GObject *cmo_override;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);
	g_signal_handlers_block_by_func (cmo_override, cmo_overrides_changed_cb, builder);
}

static void
cmo_unblock_changed_handler (GtkBuilder *builder)
{
	GObject *cmo_override;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);
	g_signal_handlers_unblock_by_func (cmo_override, cmo_overrides_changed_cb, builder);
}

static void
cmo_folders_treeview_selection_changed_cb (GtkTreeSelection *selection,
                                           GtkBuilder *builder)
{
	GtkWidget *widget;
	gint nselected;

	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	nselected = gtk_tree_selection_count_selected_rows (selection);

	widget = e_builder_get_widget (builder, "cmo-folders-remove-button");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, nselected > 0);
}

static void
cmo_folders_add_button_clicked_cb (GtkButton *button,
                                   GtkBuilder *builder)
{
	EMailComposerModeOverride *cmo_override;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	GtkWindow *window;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "cmo-folders-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));
	tree_view = GTK_TREE_VIEW (widget);

	window = GTK_WINDOW (gtk_widget_get_toplevel (widget));

	dialog = em_folder_selector_new (
		window, em_folder_tree_model_get_default ());

	gtk_window_set_title (
		GTK_WINDOW (dialog), _("Select Folder to Add"));

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_default_button_label (selector, _("_Add"));

	folder_tree = em_folder_selector_get_folder_tree (selector);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOSELECT);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		GList *list, *folder_uris;

		model = gtk_tree_view_get_model (tree_view);

		folder_uris = em_folder_tree_get_selected_uris (folder_tree);

		for (list = folder_uris; list; list = g_list_next (list)) {
			const gchar *uri = list->data;

			if (uri && *uri) {
				gboolean found = FALSE;

				if (gtk_tree_model_get_iter_first (model, &iter)) {
					do {
						gchar *old_uri = NULL;

						gtk_tree_model_get (model, &iter,
							CMO_FOLDERS_COLUMN_URI, &old_uri, -1);

						found = g_strcmp0 (uri, old_uri) == 0;

						g_free (old_uri);
					} while (!found && gtk_tree_model_iter_next (model, &iter));
				}

				if (!found) {
					GtkListStore *list_store;
					CamelSession *session;
					gchar *markup;
					EContentEditorMode mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

					list_store = GTK_LIST_STORE (model);
					session = g_object_get_data (G_OBJECT (builder), MAIL_CMO_CAMEL_SESSION_KEY);
					markup = e_mail_folder_uri_to_markup (session, uri, NULL);

					gtk_list_store_append (list_store, &iter);
					gtk_list_store_set (list_store, &iter,
						CMO_FOLDERS_COLUMN_MARKUP, markup,
						CMO_FOLDERS_COLUMN_URI, uri,
						CMO_FOLDERS_COLUMN_MODE, mode,
						CMO_FOLDERS_COLUMN_MODE_NAME, cmo_mode_to_name (mode),
						-1);

					g_free (markup);

					cmo_block_changed_handler (builder);

					cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);
					e_mail_composer_mode_override_set_for_folder (cmo_override, uri, mode);

					cmo_unblock_changed_handler (builder);
				}

				if (!list->next) {
					selection = gtk_tree_view_get_selection (tree_view);

					gtk_tree_selection_unselect_all (selection);
					gtk_tree_selection_select_iter (selection, &iter);
				}
			}
		}

		g_list_free_full (folder_uris, g_free);
	}

	gtk_widget_destroy (dialog);
}

static void
cmo_folders_remove_button_clicked_cb (GtkButton *button,
                                      GtkBuilder *builder)
{
	EMailComposerModeOverride *cmo_override;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	GList *selected, *siter;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "cmo-folders-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));
	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);
	model = gtk_tree_view_get_model (tree_view);

	cmo_block_changed_handler (builder);

	cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);
	e_mail_composer_mode_override_freeze_save (cmo_override);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	selected = g_list_reverse (selected);

	for (siter = selected; siter; siter = g_list_next (siter)) {
		gchar *uri = NULL;

		if (!gtk_tree_model_get_iter (model, &iter, siter->data))
			continue;

		gtk_tree_model_get (model, &iter, CMO_FOLDERS_COLUMN_URI, &uri, -1);

		if (uri && *uri)
			e_mail_composer_mode_override_remove_for_folder (cmo_override, uri);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		g_free (uri);
	}

	e_mail_composer_mode_override_thaw_save (cmo_override);
	cmo_unblock_changed_handler (builder);

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
}

static void
cmo_folders_mode_changed_cb (GtkCellRendererCombo *renderer,
                             const gchar *path_string,
                             GtkTreeIter *new_iter,
                             GtkBuilder *builder)
{
	EMailComposerModeOverride *cmo_override;
	GtkTreeModel *combo_model;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkWidget *widget;
	gchar *uri = NULL;
	gint mode;

	g_return_if_fail (path_string != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	g_object_get (renderer, "model", &combo_model, NULL);
	g_return_if_fail (combo_model != NULL);

	gtk_tree_model_get (combo_model, new_iter,
		CMO_MODE_COMBO_COLUMN_VALUE, &mode, -1);
	g_object_unref (combo_model);

	widget = e_builder_get_widget (builder, "cmo-folders-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	path = gtk_tree_path_new_from_string (path_string);
	g_return_if_fail (path != NULL);

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter, CMO_FOLDERS_COLUMN_URI, &uri, -1);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		CMO_FOLDERS_COLUMN_MODE, mode,
		CMO_FOLDERS_COLUMN_MODE_NAME, cmo_mode_to_name (mode),
		-1);

	if (uri && *uri) {
		cmo_block_changed_handler (builder);

		cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);
		e_mail_composer_mode_override_set_for_folder (cmo_override, uri, mode);

		cmo_unblock_changed_handler (builder);
	}

	g_free (uri);
}

static void
cmo_recipients_treeview_selection_changed_cb (GtkTreeSelection *selection,
                                              GtkBuilder *builder)
{
	GtkWidget *widget;
	gint nselected;

	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	nselected = gtk_tree_selection_count_selected_rows (selection);

	widget = e_builder_get_widget (builder, "cmo-recipients-edit-button");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, nselected == 1);

	widget = e_builder_get_widget (builder, "cmo-recipients-remove-button");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, nselected > 0);
}

static void
cmo_recipient_edited_cb (GtkCellRendererText *renderer,
                         const gchar *path_str,
                         const gchar *new_text,
                         GtkBuilder *builder)
{
	EMailComposerModeOverride *cmo_override;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	gchar *text, *old_text = NULL;
	gint mode;

	g_return_if_fail (path_str != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "cmo-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	path = gtk_tree_path_new_from_string (path_str);
	g_return_if_fail (path != NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter,
		CMO_RECIPIENTS_COLUMN_RECIPIENT, &old_text,
		CMO_RECIPIENTS_COLUMN_MODE, &mode,
		-1);

	cmo_block_changed_handler (builder);

	cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);

	text = g_strdup (new_text);
	if (text)
		g_strchomp (text);

	if (old_text && *old_text)
		e_mail_composer_mode_override_remove_for_recipient (cmo_override, old_text);

	if (!text || !*text) {
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	} else {
		GtkTreeIter new_iter = iter;
		gboolean is_new = TRUE;

		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gchar *old_recipient = NULL;

				gtk_tree_model_get (model, &iter,
					CMO_RECIPIENTS_COLUMN_RECIPIENT, &old_recipient, -1);

				is_new = !old_recipient || e_util_utf8_strcasecmp (text, old_recipient) != 0;

				g_free (old_recipient);
			} while (is_new && gtk_tree_model_iter_next (model, &iter));
		}

		if (is_new) {
			gtk_list_store_set (GTK_LIST_STORE (model), &new_iter,
				CMO_RECIPIENTS_COLUMN_RECIPIENT, text, -1);
			e_mail_composer_mode_override_set_for_recipient (cmo_override, text, mode);
		} else {
			GtkTreeSelection *selection;
			GtkTreePath *path1, *path2;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

			path1 = gtk_tree_model_get_path (model, &iter);
			path2 = gtk_tree_model_get_path (model, &new_iter);

			if (!path1 || !path2 || gtk_tree_path_compare (path1, path2) != 0)
				gtk_list_store_remove (GTK_LIST_STORE (model), &new_iter);

			gtk_tree_path_free (path1);
			gtk_tree_path_free (path2);

			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_iter (selection, &iter);
		}
	}

	cmo_unblock_changed_handler (builder);

	g_free (old_text);
	g_free (text);
}

static void
cmo_recipient_editing_canceled_cb (GtkCellRenderer *renderer,
                                   GtkBuilder *builder)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "cmo-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *old_recipient = NULL;

			gtk_tree_model_get (model, &iter,
				CMO_RECIPIENTS_COLUMN_RECIPIENT, &old_recipient, -1);

			if (!old_recipient || !*old_recipient) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				g_free (old_recipient);
				break;
			}

			g_free (old_recipient);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

static void
cmo_recipients_add_button_clicked_cb (GtkButton *button,
                                      GtkBuilder *builder)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkWidget *widget;
	GList *cells;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "cmo-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		CMO_RECIPIENTS_COLUMN_MODE, E_CONTENT_EDITOR_MODE_PLAIN_TEXT,
		CMO_RECIPIENTS_COLUMN_MODE_NAME, cmo_mode_to_name (E_CONTENT_EDITOR_MODE_PLAIN_TEXT),
		-1);

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	column = gtk_tree_view_get_column (tree_view, 0);
	g_return_if_fail (column != NULL);

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	g_return_if_fail (cells != NULL);

	path = gtk_tree_model_get_path (model, &iter);
	if (path == NULL) {
		g_list_free (cells);
		return;
	}

	g_object_set (cells->data, "editable", TRUE, NULL);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, cells->data, TRUE);
	g_object_set (cells->data, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
	g_list_free (cells);
}

static void
cmo_recipients_edit_button_clicked_cb (GtkButton *button,
                                       GtkBuilder *builder)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkWidget *widget;
	GList *cells, *selected;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "cmo-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);

	g_return_if_fail (gtk_tree_selection_count_selected_rows (selection) == 1);

	selected = gtk_tree_selection_get_selected_rows (selection, NULL);
	g_return_if_fail (selected && selected->next == NULL);

	path = selected->data;
	/* 'path' is freed later in the function */
	g_list_free (selected);

	column = gtk_tree_view_get_column (tree_view, 0);
	g_return_if_fail (column != NULL);

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	g_return_if_fail (cells != NULL);

	g_object_set (cells->data, "editable", TRUE, NULL);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, cells->data, TRUE);
	g_object_set (cells->data, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
	g_list_free (cells);
}

static void
cmo_recipients_remove_button_clicked_cb (GtkButton *button,
                                         GtkBuilder *builder)
{
	EMailComposerModeOverride *cmo_override;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	GList *selected, *siter;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "cmo-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));
	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);
	model = gtk_tree_view_get_model (tree_view);

	cmo_block_changed_handler (builder);

	cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);
	e_mail_composer_mode_override_freeze_save (cmo_override);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	selected = g_list_reverse (selected);

	for (siter = selected; siter; siter = g_list_next (siter)) {
		gchar *recipient = NULL;

		if (!gtk_tree_model_get_iter (model, &iter, siter->data))
			continue;

		gtk_tree_model_get (model, &iter,
			CMO_RECIPIENTS_COLUMN_RECIPIENT, &recipient, -1);

		if (recipient && *recipient)
			e_mail_composer_mode_override_remove_for_recipient (cmo_override, recipient);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		g_free (recipient);
	}

	e_mail_composer_mode_override_thaw_save (cmo_override);
	cmo_unblock_changed_handler (builder);

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
}

static void
cmo_recipients_mode_changed_cb (GtkCellRendererCombo *renderer,
                                const gchar *path_string,
                                GtkTreeIter *new_iter,
                                GtkBuilder *builder)
{
	EMailComposerModeOverride *cmo_override;
	GtkTreeModel *combo_model;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkWidget *widget;
	gchar *recipient = NULL;
	gint mode;

	g_return_if_fail (path_string != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	g_object_get (renderer, "model", &combo_model, NULL);
	g_return_if_fail (combo_model != NULL);

	gtk_tree_model_get (combo_model, new_iter,
		CMO_MODE_COMBO_COLUMN_VALUE, &mode, -1);
	g_object_unref (combo_model);

	widget = e_builder_get_widget (builder, "cmo-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	path = gtk_tree_path_new_from_string (path_string);
	g_return_if_fail (path != NULL);

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter,
		CMO_RECIPIENTS_COLUMN_RECIPIENT, &recipient, -1);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		CMO_RECIPIENTS_COLUMN_MODE, mode,
		CMO_RECIPIENTS_COLUMN_MODE_NAME, cmo_mode_to_name (mode),
		-1);

	if (recipient && *recipient) {
		cmo_block_changed_handler (builder);

		cmo_override = g_object_get_data (G_OBJECT (builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY);
		e_mail_composer_mode_override_set_for_recipient (cmo_override, recipient, mode);

		cmo_unblock_changed_handler (builder);
	}

	g_free (recipient);
}

static void
composer_mode_override_setup (GtkBuilder *builder,
                              EMailBackend *mail_backend)
{
	EMailComposerModeOverride *cmo_override;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkListStore *list_store;
	GtkListStore *mode_model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_BUILDER (builder));
	g_return_if_fail (E_IS_MAIL_BACKEND (mail_backend));

	g_object_set_data_full (
		G_OBJECT (builder), MAIL_CMO_CAMEL_SESSION_KEY,
		g_object_ref (e_mail_backend_get_session (mail_backend)), g_object_unref);

	mode_model = cmo_create_mode_model ();

	/* Folders treeview: markup, uri, mode, mode_name */
	widget = e_builder_get_widget (builder, "cmo-folders-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);

	list_store = gtk_list_store_new (CMO_FOLDERS_N_COLUMNS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	/* Folder column */
	gtk_tree_view_insert_column_with_attributes (
		tree_view, -1, _("Folder"),
		gtk_cell_renderer_text_new (),
		"markup", CMO_FOLDERS_COLUMN_MARKUP, NULL);

	/* Format combo column */
	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer),
		"model", mode_model,
		"text-column", CMO_MODE_COMBO_COLUMN_NAME,
		"editable", TRUE,
		"has-entry", FALSE,
		NULL);
	g_signal_connect (renderer, "changed",
		G_CALLBACK (cmo_folders_mode_changed_cb), builder);

	column = gtk_tree_view_column_new_with_attributes (
		_("Format"), renderer,
		"text", CMO_FOLDERS_COLUMN_MODE_NAME, NULL);
	gtk_tree_view_append_column (tree_view, column);

	g_object_unref (list_store);

	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (cmo_folders_treeview_selection_changed_cb), builder);

	widget = e_builder_get_widget (builder, "cmo-folders-add-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (cmo_folders_add_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "cmo-folders-remove-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (cmo_folders_remove_button_clicked_cb), builder);

	/* Recipients treeview: recipient, mode, mode_name */
	widget = e_builder_get_widget (builder, "cmo-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);

	/* Recipient text column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
	g_signal_connect (renderer, "edited",
		G_CALLBACK (cmo_recipient_edited_cb), builder);
	g_signal_connect (renderer, "editing-canceled",
		G_CALLBACK (cmo_recipient_editing_canceled_cb), builder);

	list_store = gtk_list_store_new (CMO_RECIPIENTS_N_COLUMNS,
		G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));
	gtk_tree_view_insert_column_with_attributes (
		tree_view, -1, _("Recipient"),
		renderer, "text", CMO_RECIPIENTS_COLUMN_RECIPIENT, NULL);

	/* Format combo column for recipients */
	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer),
		"model", mode_model,
		"text-column", CMO_MODE_COMBO_COLUMN_NAME,
		"editable", TRUE,
		"has-entry", FALSE,
		NULL);
	g_signal_connect (renderer, "changed",
		G_CALLBACK (cmo_recipients_mode_changed_cb), builder);

	column = gtk_tree_view_column_new_with_attributes (
		_("Format"), renderer,
		"text", CMO_RECIPIENTS_COLUMN_MODE_NAME, NULL);
	gtk_tree_view_append_column (tree_view, column);

	g_object_unref (list_store);

	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (cmo_recipients_treeview_selection_changed_cb), builder);

	widget = e_builder_get_widget (builder, "cmo-recipients-add-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (cmo_recipients_add_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "cmo-recipients-edit-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (cmo_recipients_edit_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "cmo-recipients-remove-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (cmo_recipients_remove_button_clicked_cb), builder);

	/* Fill initial data */
	cmo_fill_overrides (builder);

	/* Connect to external changes */
	cmo_override = e_mail_backend_get_composer_mode_override (mail_backend);
	g_signal_connect_object (cmo_override, "changed",
		G_CALLBACK (cmo_overrides_changed_cb), builder, 0);

	g_object_unref (mode_model);
}

static void
send_account_override_setup (GtkBuilder *builder,
                             EMailBackend *mail_backend,
                             ESourceRegistry *registry)
{
	EMailIdentityComboBox *identity_combo_box;
	EMailSendAccountOverride *account_override;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkListStore *list_store;
	GtkCellRenderer *renderer;
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_BUILDER (builder));
	g_return_if_fail (E_IS_MAIL_BACKEND (mail_backend));
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));

	/* use its model to avoid code duplication */
	widget = e_mail_identity_combo_box_new (registry);
	e_mail_identity_combo_box_set_allow_aliases (E_MAIL_IDENTITY_COMBO_BOX (widget), TRUE);

	identity_combo_box = E_MAIL_IDENTITY_COMBO_BOX (g_object_ref_sink (widget));

	widget = e_builder_get_widget (builder, "sao-account-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);

	g_object_set_data_full (
		G_OBJECT (tree_view), "identity-combo-box",
		identity_combo_box, (GDestroyNotify) gtk_widget_destroy);
	g_object_set_data_full (
		G_OBJECT (builder), MAIL_CAMEL_SESSION_KEY,
		g_object_ref (e_mail_backend_get_session (mail_backend)), g_object_unref);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (identity_combo_box));

	gtk_tree_view_set_model (tree_view, model);
	gtk_tree_view_insert_column_with_attributes (
		tree_view, -1, _("Account"),
		gtk_cell_renderer_text_new (),
		"text", 0, NULL);

	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (sao_account_treeview_selection_changed_cb), builder);

	g_signal_connect (model, "row-changed", G_CALLBACK (sao_account_row_changed_cb), builder);

	widget = e_builder_get_widget (builder, "sao-folders-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);

	/* markup, folder-uri */
	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));
	gtk_tree_view_insert_column_with_attributes (
		tree_view, -1, _("Folder"),
		gtk_cell_renderer_text_new (),
		"markup", 0, NULL);
	g_object_unref (list_store);

	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (sao_folders_treeview_selection_changed_cb), builder);

	widget = e_builder_get_widget (builder, "sao-folders-add-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sao_folders_add_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "sao-folders-remove-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sao_folders_remove_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "sao-recipients-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (sao_recipient_edited_cb), builder);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (sao_recipient_editing_canceled_cb), builder);

	list_store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));
	gtk_tree_view_insert_column_with_attributes (
		tree_view, -1, _("Recipient"),
		renderer, "text", 0, NULL);
	g_object_unref (list_store);

	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (sao_recipients_treeview_selection_changed_cb), builder);

	widget = e_builder_get_widget (builder, "sao-recipients-add-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sao_recipients_add_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "sao-recipients-edit-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sao_recipients_edit_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "sao-recipients-remove-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sao_recipients_remove_button_clicked_cb), builder);

	/* init view */
	widget = e_builder_get_widget (builder, "sao-account-treeview");
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	sao_account_treeview_selection_changed_cb (selection, builder);

	account_override = e_mail_backend_get_send_account_override (mail_backend);
	g_signal_connect_object (account_override, "changed", G_CALLBACK (sao_overrides_changed_cb), builder, 0);
}

static GtkWidget *
emcp_widget_glade (EConfig *ec,
                   EConfigItem *item,
                   GtkWidget *parent,
                   GtkWidget *old,
                   gint position,
                   gpointer data)
{
	EMComposerPrefs *prefs = data;

	return e_builder_get_widget (prefs->builder, item->label);
}

/* plugin meta-data */
static EMConfigItem emcp_items[] = {

	{ E_CONFIG_BOOK,
	  (gchar *) "",
	  (gchar *) "composer_toplevel",
	  emcp_widget_glade },

	{ E_CONFIG_PAGE,
	  (gchar *) "00.general",
	  (gchar *) "vboxComposerGeneral",
	  emcp_widget_glade },

	{ E_CONFIG_SECTION,
	  (gchar *) "00.general/00.behavior",
	  (gchar *) "default-behavior-vbox",
	  emcp_widget_glade },

	{ E_CONFIG_PAGE,
	  (gchar *) "10.signatures",
	  (gchar *) "vboxSignatures",
	  emcp_widget_glade },

	/* signature/signatures and signature/preview parts not usable */

	{ E_CONFIG_PAGE,
	  (gchar *) "20.spellcheck",
	  (gchar *) "vboxSpellChecking",
	  emcp_widget_glade },

	{ E_CONFIG_SECTION,
	  (gchar *) "20.spellcheck/00.options",
	  (gchar *) "spell-options-vbox",
	  emcp_widget_glade },
};

static gboolean
em_composer_prefs_outbox_delay_setting_to_id (GValue *value,
					      GVariant *variant,
					      gpointer user_data)
{
	gint to_set = g_variant_get_int32 (variant);
	gchar *str;

	if (to_set < 0)
		to_set = -1;
	else if (to_set != 0 && to_set != 1 && to_set != 5)
		to_set = 5;

	str = g_strdup_printf ("%d", to_set);
	g_value_set_string (value, str);
	g_free (str);

	return TRUE;
}

static GVariant *
em_composer_prefs_outbox_delay_id_to_setting (const GValue *value,
					      const GVariantType *expected_type,
					      gpointer user_data)
{
	gint to_set;

	to_set = g_value_get_string (value) ? atoi (g_value_get_string (value)) : -1;
	if (to_set == 0 && g_strcmp0 (g_value_get_string (value), "0") != 0)
		to_set = -1;

	return g_variant_new_int32 (to_set);
}

static void
emcp_free (EConfig *ec,
           GSList *items,
           gpointer data)
{
	/* the prefs data is freed automagically */
	g_slist_free (items);
}

static gboolean
emcp_composer_mode_to_current_value_cb (GValue *value,
					GVariant *variant,
					gpointer user_data)
{
	const gchar *str;
	gint mode = E_CONTENT_EDITOR_MODE_UNKNOWN;

	str = g_variant_get_string (variant, NULL);

	if (!e_enum_from_string (E_TYPE_CONTENT_EDITOR_MODE, str, &mode))
		mode = E_CONTENT_EDITOR_MODE_UNKNOWN;

	if (mode == E_CONTENT_EDITOR_MODE_UNKNOWN)
		mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

	g_value_set_int (value, mode);

	return TRUE;
}

static GVariant *
emcp_current_value_to_composer_mode_cb (const GValue *value,
					const GVariantType *expected_type,
					gpointer user_data)
{
	const gchar *str;
	gint mode;

	mode = g_value_get_int (value);

	str = e_enum_to_string (E_TYPE_CONTENT_EDITOR_MODE, mode);

	if (!str)
		str = e_enum_to_string (E_TYPE_CONTENT_EDITOR_MODE, E_CONTENT_EDITOR_MODE_PLAIN_TEXT);

	return g_variant_new_string (str);
}

static void
accept_html_save (GtkTreeModel *model)
{
	GSettings *settings;
	GPtrArray *array;
	GtkTreeIter iter;
	gboolean changed = TRUE;
	gchar **strv;
	guint ii;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	array = g_ptr_array_new_with_free_func (g_free);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *text = NULL;

			gtk_tree_model_get (model, &iter, 0, &text, -1);

			if (text && *text)
				g_ptr_array_add (array, text);
			else
				g_free (text);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	/* NULL-terminated array */
	g_ptr_array_add (array, NULL);

	strv = g_settings_get_strv (settings, "composer-addresses-accept-html");
	if (strv != NULL) {
		changed = FALSE;

		for (ii = 0; !changed && ii < array->len - 1 && strv[ii]; ii++) {
			changed = g_strcmp0 (g_ptr_array_index (array, ii), strv[ii]) != 0;
		}

		changed = changed || (ii < array->len - 1) || (strv[ii] != NULL);
	}

	if (changed)
		g_settings_set_strv (settings, "composer-addresses-accept-html", (const gchar * const *) array->pdata);

	g_ptr_array_unref (array);
	g_object_unref (settings);
	g_strfreev (strv);
}

static void
accept_html_load (GtkBuilder *builder)
{
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkListStore *list_store;
	GSettings *settings;
	gchar **strv;

	widget = e_builder_get_widget (builder, "accept-html-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	g_return_if_fail (GTK_IS_LIST_STORE (model));

	list_store = GTK_LIST_STORE (model);

	gtk_list_store_clear (list_store);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	strv = g_settings_get_strv (settings, "composer-addresses-accept-html");
	if (strv != NULL) {
		guint ii;

		for (ii = 0; strv[ii] != NULL; ii++) {
			const gchar *text;

			text = g_strchomp (strv[ii]);
			if (text && *text) {
				GtkTreeIter iter;

				gtk_list_store_append (list_store, &iter);
				gtk_list_store_set (list_store, &iter, 0, text, -1);
			}
		}
	}

	g_object_unref (settings);
	g_strfreev (strv);
}

static void
accept_html_treeview_selection_changed_cb (GtkTreeSelection *selection,
					   GtkBuilder *builder)
{
	GtkWidget *widget;
	gint nselected;

	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	nselected = gtk_tree_selection_count_selected_rows (selection);

	widget = e_builder_get_widget (builder, "accept-html-edit-button");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, nselected == 1);

	widget = e_builder_get_widget (builder, "accept-html-remove-button");
	g_return_if_fail (GTK_IS_WIDGET (widget));
	gtk_widget_set_sensitive (widget, nselected > 0);
}

static void
accept_html_edited_cb (GtkCellRendererText *renderer,
		       const gchar *path_str,
		       const gchar *new_text,
		       GtkBuilder *builder)
{
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	gchar *text = NULL;

	g_return_if_fail (path_str != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "accept-html-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	path = gtk_tree_path_new_from_string (path_str);
	g_return_if_fail (path != NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_path_free (path);

	text = g_strdup (new_text);
	if (text)
		g_strchomp (text);

	if (!text || !*text) {
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		accept_html_save (model);
	} else {
		GtkTreeIter new_iter = iter;
		gboolean is_new = TRUE;

		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gchar *old_text = NULL;

				gtk_tree_model_get (model, &iter, 0, &old_text, -1);

				is_new = !old_text || e_util_utf8_strcasecmp (text, old_text) != 0;

				g_free (old_text);
			} while (is_new && gtk_tree_model_iter_next (model, &iter));
		}

		if (is_new) {
			gtk_list_store_set (GTK_LIST_STORE (model), &new_iter, 0, text, -1);
			accept_html_save (model);
		} else {
			GtkTreeSelection *selection;
			GtkTreePath *path1, *path2;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

			path1 = gtk_tree_model_get_path (model, &iter);
			path2 = gtk_tree_model_get_path (model, &new_iter);

			if (!path1 || !path2 || gtk_tree_path_compare (path1, path2) != 0)
				gtk_list_store_remove (GTK_LIST_STORE (model), &new_iter);

			gtk_tree_path_free (path1);
			gtk_tree_path_free (path2);

			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_iter (selection, &iter);
		}
	}

	g_free (text);
}

static void
accept_html_editing_canceled_cb (GtkCellRenderer *renderer,
				 GtkBuilder *builder)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "accept-html-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *old_text = NULL;

			gtk_tree_model_get (model, &iter, 0, &old_text, -1);

			if (!old_text || !*old_text) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				g_free (old_text);
				break;
			}

			g_free (old_text);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

static void
accept_html_add_button_clicked_cb (GtkButton *button,
				   GtkBuilder *builder)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkWidget *widget;
	GList *cells;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "accept-html-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	column = gtk_tree_view_get_column (tree_view, 0);
	g_return_if_fail (column != NULL);

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	g_return_if_fail (cells != NULL);

	path = gtk_tree_model_get_path (model, &iter);
	if (path == NULL) {
		g_list_free (cells);
		return;
	}

	g_object_set (cells->data, "editable", TRUE, NULL);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, cells->data, TRUE);
	g_object_set (cells->data, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
	g_list_free (cells);
}

static void
accept_html_edit_button_clicked_cb (GtkButton *button,
				    GtkBuilder *builder)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkWidget *widget;
	GList *cells, *selected;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "accept-html-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);

	g_return_if_fail (gtk_tree_selection_count_selected_rows (selection) == 1);

	selected = gtk_tree_selection_get_selected_rows (selection, NULL);
	g_return_if_fail (selected && selected->next == NULL);

	path = selected->data;
	/* 'path' is freed later in the function */
	g_list_free (selected);

	column = gtk_tree_view_get_column (tree_view, 0);
	g_return_if_fail (column != NULL);

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	g_return_if_fail (cells != NULL);

	g_object_set (cells->data, "editable", TRUE, NULL);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, cells->data, TRUE);
	g_object_set (cells->data, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
	g_list_free (cells);
}

static void
accept_html_remove_button_clicked_cb (GtkButton *button,
				      GtkBuilder *builder)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	GList *selected, *siter;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "accept-html-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));
	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);
	model = gtk_tree_view_get_model (tree_view);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	selected = g_list_reverse (selected);

	for (siter = selected; siter; siter = g_list_next (siter)) {
		if (!gtk_tree_model_get_iter (model, &iter, siter->data))
			continue;

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}

	accept_html_save (model);

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
}

static void
accept_html_settings_changed_cb (GSettings *settings,
				 const gchar *key,
				 gpointer user_data)
{
	GtkBuilder *builder = user_data;
	GtkWidget *widget;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GHashTable *existing_keys;
	gchar **strv;
	gboolean changed = TRUE;

	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = e_builder_get_widget (builder, "accept-html-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	g_return_if_fail (GTK_IS_LIST_STORE (model));

	existing_keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *text = NULL;

			gtk_tree_model_get (model, &iter, 0, &text, -1);

			if (text && *text)
				g_hash_table_add (existing_keys, text);
			else
				g_free (text);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	strv = g_settings_get_strv (settings, "composer-addresses-accept-html");
	if (strv != NULL) {
		guint ii, n_verified = 0;

		changed = FALSE;

		for (ii = 0; !changed && strv[ii] != NULL; ii++) {
			const gchar *text;

			text = g_strchomp (strv[ii]);
			if (text && *text) {
				changed = !g_hash_table_contains (existing_keys, text);
				n_verified++;
			}
		}

		changed = changed || n_verified != g_hash_table_size (existing_keys);
	}

	if (changed)
		accept_html_load (builder);

	g_hash_table_destroy (existing_keys);
	g_object_unref (settings);
	g_strfreev (strv);
}

static void
accept_html_setup (GtkBuilder *builder)
{
	GtkCellRenderer *renderer;
	GtkListStore *list_store;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkWidget *widget;
	GSettings *settings;

	widget = e_builder_get_widget (builder, "accept-html-add-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	g_signal_connect (widget, "clicked",
		G_CALLBACK (accept_html_add_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "accept-html-edit-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (widget, "clicked",
		G_CALLBACK (accept_html_edit_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "accept-html-remove-button");
	g_return_if_fail (GTK_IS_BUTTON (widget));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (widget, "clicked",
		G_CALLBACK (accept_html_remove_button_clicked_cb), builder);

	widget = e_builder_get_widget (builder, "accept-html-treeview");
	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	tree_view = GTK_TREE_VIEW (widget);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (accept_html_edited_cb), builder);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (accept_html_editing_canceled_cb), builder);

	list_store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));
	gtk_tree_view_insert_column_with_attributes (
		tree_view, -1, _("Recipient"),
		renderer, "text", 0, NULL);
	g_object_unref (list_store);

	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (accept_html_treeview_selection_changed_cb), builder);

	accept_html_load (builder);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_signal_connect_object (settings, "changed::composer-addresses-accept-html",
		G_CALLBACK (accept_html_settings_changed_cb), builder, 0);
	g_clear_object (&settings);
}

static void
emc_prefs_bind_settings_ex (EMComposerPrefs *prefs,
			    GSettings *settings,
			    const gchar *key,
			    const gchar *widget_name,
			    const gchar *property,
			    GSettingsBindFlags flags)
{
	GtkWidget *widget;

	widget = e_builder_get_widget (prefs->builder, widget_name);
	g_settings_bind (
		settings, key,
		widget, property,
		flags);
}

static void
emc_prefs_bind_settings (EMComposerPrefs *prefs,
			 GSettings *settings,
			 const gchar *key,
			 const gchar *widget_name,
			 const gchar *property)
{
	emc_prefs_bind_settings_ex (prefs, settings, key, widget_name, property, G_SETTINGS_BIND_DEFAULT);
}

static void
em_composer_prefs_construct (EMComposerPrefs *prefs,
                             EShell *shell)
{
	GtkWidget *toplevel, *widget, *info_pixmap;
	GtkWidget *container;
	GSettings *settings;
	EActionComboBox *action_combo_box;
	ESourceRegistry *registry;
	GtkTreeView *view;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkComboBoxText *combo_text;
	EMConfig *ec;
	EMConfigTargetPrefs *target;
	EMailBackend *mail_backend;
	EMailSendAccountOverride *send_override;
	EMailComposerModeOverride *cmo_override;
	GSList *l;
	gint i;

	registry = e_shell_get_registry (shell);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_MAIL_JUNK_OPTIONS);

	prefs->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->builder, "mail-config.ui");

	prefs->spell_checker = e_spell_checker_new ();

	/** @HookPoint-EMConfig: Mail Composer Preferences
	 * @Id: org.gnome.evolution.mail.composerPrefs
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetPrefs
	 *
	 * The mail composer preferences settings page.
	 */
	ec = em_config_new ("org.gnome.evolution.mail.composerPrefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (emcp_items); i++)
		l = g_slist_prepend (l, &emcp_items[i]);
	e_config_add_items ((EConfig *) ec, l, emcp_free, prefs);

	/* General tab */

	/* Default Behavior */

	container = e_builder_get_widget (prefs->builder, "hboxSendMode");
	action_combo_box = E_ACTION_COMBO_BOX (e_html_editor_util_new_mode_combobox (NULL));
	gtk_widget_show (GTK_WIDGET (action_combo_box));
	gtk_box_pack_start (GTK_BOX (container), GTK_WIDGET (action_combo_box), FALSE, FALSE, 0);
	widget = e_builder_get_widget (prefs->builder, "lblSendMode");
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), GTK_WIDGET (action_combo_box));
	g_settings_bind_with_mapping (
		settings, "composer-mode",
		action_combo_box, "current-value",
		G_SETTINGS_BIND_DEFAULT,
		emcp_composer_mode_to_current_value_cb,
		emcp_current_value_to_composer_mode_cb,
		NULL, NULL);

	emc_prefs_bind_settings (prefs, settings, "composer-inherit-theme-colors", "chkInheritThemeColors", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-accel-send", "chkPromptAccelSend", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-empty-subject", "chkPromptEmptySubject", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-only-bcc", "chkPromptBccOnly", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-private-list-reply", "chkPromptPrivateListReply", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-reply-many-recips", "chkPromptReplyManyRecips", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-list-reply-to", "chkPromptListReplyTo", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-many-to-cc-recips", "chkPromptManyToCCRecips", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-invalid-recip", "chkPromptSendInvalidRecip", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-composer-mode-switch", "chkPromptComposerModeSwitch", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-changed-attachment", "chkPromptChangedAttachment", "active");
	emc_prefs_bind_settings (prefs, settings, "prompt-on-unwanted-html", "chkPromptWantHTML", "active");

	accept_html_setup (prefs->builder);

	emc_prefs_bind_settings (prefs, settings, "composer-magic-smileys", "chkAutoSmileys", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-unicode-smileys", "chkUnicodeSmileys", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-request-receipt", "chkRequestReceipt", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-request-dsn", "chkRequestDSN", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-reply-start-bottom", "chkReplyStartBottom", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-word-wrap-length", "spinWordWrapLength", "value");
	emc_prefs_bind_settings (prefs, settings, "composer-outlook-filenames", "chkOutlookFilenames", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-use-outbox", "chkUseOutbox", "active");

	widget = e_builder_get_widget (prefs->builder, "comboboxFlushOutbox");

	combo_text = GTK_COMBO_BOX_TEXT (widget);
	gtk_combo_box_text_append (combo_text, "-1", _("Keep in Outbox"));
	gtk_combo_box_text_append (combo_text,  "0", _("Send immediately"));
	gtk_combo_box_text_append (combo_text,  "1", _("Send after 1 minute"));
	gtk_combo_box_text_append (combo_text,  "5", _("Send after 5 minutes"));

	g_settings_bind_with_mapping (
		settings, "composer-delay-outbox-flush",
		widget, "active-id",
		G_SETTINGS_BIND_DEFAULT,
		em_composer_prefs_outbox_delay_setting_to_id,
		em_composer_prefs_outbox_delay_id_to_setting,
		NULL, NULL);

	e_binding_bind_property (
		e_builder_get_widget (prefs->builder, "chkUseOutbox"), "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	emc_prefs_bind_settings (prefs, settings, "composer-ignore-list-reply-to", "chkIgnoreListReplyTo", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-group-reply-to-list", "chkGroupReplyToList", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-sign-reply-if-signed", "chkSignReplyIfSigned", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-wrap-quoted-text-in-replies", "chkWrapQuotedTextInReplies", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-signature-in-new-only", "chkSignatureInNewOnly", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-top-signature", "chkTopSignature", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-inline-spelling", "chkEnableSpellChecking", "active");
	emc_prefs_bind_settings (prefs, settings, "composer-attach-separate-messages", "chkAttachSeparateMessages", "active");

	widget = e_charset_combo_box_new ();
	container = e_builder_get_widget (prefs->builder, "hboxComposerCharset");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_settings_bind (
		settings, "composer-charset",
		widget, "charset",
		G_SETTINGS_BIND_DEFAULT);

	container = e_builder_get_widget (prefs->builder, "lblCharset");
	gtk_label_set_mnemonic_widget (GTK_LABEL (container), widget);

	/* Spell Checking */
	widget = e_builder_get_widget (prefs->builder, "listSpellCheckLanguage");
	view = GTK_TREE_VIEW (widget);
	store = gtk_list_store_new (
		3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	prefs->language_model = GTK_TREE_MODEL (store);
	gtk_tree_view_set_model (view, prefs->language_model);
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (spell_language_toggled_cb), prefs);
	gtk_tree_view_insert_column_with_attributes (
		view, -1, _("Enabled"),
		renderer, "active", 0, NULL);

	gtk_tree_view_insert_column_with_attributes (
		view, -1, _("Language(s)"),
		gtk_cell_renderer_text_new (),
		"text", 1, NULL);
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);
	info_pixmap = e_builder_get_widget (prefs->builder, "pixmapSpellInfo");
	gtk_image_set_from_icon_name (
		GTK_IMAGE (info_pixmap),
		"dialog-information", GTK_ICON_SIZE_BUTTON);

	spell_setup (prefs);

	g_signal_connect_swapped (
		store, "row-changed",
		G_CALLBACK (spell_language_save), prefs);

	/* Forwards and Replies */
	emc_prefs_bind_settings (prefs, settings, "forward-style-name", "comboboxForwardStyle", "active-id");
	emc_prefs_bind_settings (prefs, settings, "reply-style-name", "comboboxReplyStyle", "active-id");

	widget = e_builder_get_widget (prefs->builder, "comboboxForwardReplyAttribLanguage");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "", _("Same as user interface"));
	em_utils_add_installed_languages (GTK_COMBO_BOX_TEXT (widget));

	g_settings_bind (
		settings, "composer-attribution-language",
		widget, "active-id",
		G_SETTINGS_BIND_DEFAULT);

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	/* Signatures */
	container = e_builder_get_widget (
		prefs->builder, "signature-section");
	widget = e_mail_signature_manager_new (registry);
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	/* The mail shell backend responds to the "window-added" signal
	 * that this triggers and configures it with composer preferences. */
	g_signal_connect_swapped (
		widget, "editor-created",
		G_CALLBACK (gtk_application_add_window), shell);

	g_settings_bind (
		settings, "composer-mode",
		widget, "prefer-mode",
		G_SETTINGS_BIND_GET);

	/* Send Account override */
	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_if_fail (mail_backend != NULL);

	send_override = e_mail_backend_get_send_account_override (mail_backend);
	g_object_set_data_full (
		G_OBJECT (prefs->builder), MAIL_SEND_ACCOUNT_OVERRIDE_KEY,
		g_object_ref (send_override), g_object_unref);

	send_account_override_setup (prefs->builder, mail_backend, registry);

	widget = e_builder_get_widget (prefs->builder, "sao-prefer-folder-check");
	e_binding_bind_property (
		send_override, "prefer-folder",
		widget, "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	/* Composer Mode override */
	cmo_override = e_mail_backend_get_composer_mode_override (mail_backend);
	g_object_set_data_full (
		G_OBJECT (prefs->builder), MAIL_COMPOSER_MODE_OVERRIDE_KEY,
		g_object_ref (cmo_override), g_object_unref);

	composer_mode_override_setup (prefs->builder, mail_backend);

	widget = e_builder_get_widget (prefs->builder, "cmo-prefer-folder-check");
	e_binding_bind_property (
		cmo_override, "prefer-folder",
		widget, "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	/* get our toplevel widget */
	target = em_config_target_new_prefs (ec);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);
	toplevel = e_config_create_widget ((EConfig *) ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);

	g_object_unref (settings);
}

GtkWidget *
em_composer_prefs_new (EPreferencesWindow *window)
{
	EShell *shell;
	EMComposerPrefs *prefs;

	shell = e_preferences_window_get_shell (window);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	prefs = g_object_new (EM_TYPE_COMPOSER_PREFS, NULL);
	em_composer_prefs_construct (prefs, shell);

	return GTK_WIDGET (prefs);
}
