/*
 * Copyright (C) 2020 Red Hat (www.redhat.com)
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
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <camel/camel.h>

#include "e-util/e-util.h"
#include "composer/e-msg-composer.h"
#include "gui/itip-utils.h"
#include "mail/em-event.h"
#include "shell/e-shell.h"

#define RA_CONF_KEY_NAME "assignments"
#define AR_CONF_KEY_NAME "account-for-recipients"

gint		e_plugin_lib_enable			(EPlugin *ep,
							 gint enable);
GtkWidget *	e_plugin_lib_get_configure_widget	(EPlugin *plugin);
void		org_gnome_evolution_sender_validation_presendchecks
							(EPlugin *ep,
							 EMEventTargetComposer *tt);

static gboolean plugin_enabled = TRUE;

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	plugin_enabled = enable != 0;

	return 0;
}

typedef struct _Assignment {
	const gchar *recipient;
	const gchar *account;
} Assignment;

static void
e_sender_validation_free_assignment (gpointer ptr)
{
	if (!ptr)
		return;

	g_slice_free (Assignment, ptr);
}

/* (transfer full): Internal data uses strings from in_assignments */
static GSList *
e_sender_validation_parse_assignments (gchar **in_assignments)
{
	GSList *items = NULL;
	guint ii;

	if (!in_assignments || !*in_assignments)
		return NULL;

	for (ii = 0; in_assignments[ii]; ii++) {
		Assignment *assignment;
		gchar *value = in_assignments[ii];
		gchar *tab;

		tab = strchr (value, '\t');
		if (!tab || tab == value || !tab[1])
			continue;

		*tab = '\0';

		assignment = g_slice_new (Assignment);
		assignment->recipient = value;
		assignment->account = tab + 1;

		items = g_slist_prepend (items, assignment);
	}

	return g_slist_reverse (items);
}

static gboolean
e_sender_validation_ask_ra (GtkWindow *window,
			    const gchar *recipient,
			    const gchar *expected_account,
			    const gchar *used_account)
{
	gint response;

	response = e_alert_run_dialog_for_args (window,
		"org.gnome.evolution.plugins.sender-validation:sender-validation",
		recipient, expected_account, used_account,
		NULL);

	return response == GTK_RESPONSE_YES;
}

static gboolean
e_sender_validation_ask_ar (GtkWindow *window,
			    const gchar *recipient,
			    const gchar *expected_recipient,
			    const gchar *used_account)
{
	gint response;

	response = e_alert_run_dialog_for_args (window,
		"org.gnome.evolution.plugins.sender-validation:sender-validation-ar",
		recipient, expected_recipient, used_account,
		NULL);

	return response == GTK_RESPONSE_YES;
}

static gboolean
e_sender_validation_check (EMsgComposer *composer)
{
	GSettings *settings;
	GSList *assignments; /* Assignment * */
	gchar **strv;
	gboolean can_send = TRUE;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.sender-validation");

	strv = g_settings_get_strv (settings, RA_CONF_KEY_NAME);
	assignments = e_sender_validation_parse_assignments (strv);

	if (assignments) {
		EComposerHeaderTable *header_table;
		const gchar *from_address;

		header_table = e_msg_composer_get_header_table (composer);
		from_address = e_composer_header_table_get_from_address (header_table);

		if (from_address && *from_address) {
			EDestination **destinations;
			guint ii;

			destinations = e_composer_header_table_get_destinations (header_table);

			for (ii = 0; destinations && destinations[ii]; ii++) {
				EDestination *dest = destinations[ii];
				const gchar *recipient;

				recipient = e_destination_get_address (dest);

				if (recipient && *recipient) {
					const Assignment *has_mismatch = NULL;
					gboolean has_match = FALSE;
					GSList *link;

					for (link = assignments; link && !has_match; link = g_slist_next (link)) {
						const Assignment *assignment = link->data;

						if (camel_strstrcase (recipient, assignment->recipient)) {
							if (camel_strstrcase (from_address, assignment->account)) {
								has_match = TRUE;
							} else if (!has_mismatch) {
								has_mismatch = assignment;
							}
						}
					}

					if (!has_match && has_mismatch) {
						can_send = e_sender_validation_ask_ra (GTK_WINDOW (composer), recipient, has_mismatch->account, from_address);
						break;
					}
				}
			}

			e_destination_freev (destinations);
		}
	}

	g_slist_free_full (assignments, e_sender_validation_free_assignment);
	/* Can free 'strv' only after 'assignments', because 'assignments' is using the memory from 'strv' */
	g_strfreev (strv);

	if (!can_send) {
		g_clear_object (&settings);
		return can_send;
	}

	strv = g_settings_get_strv (settings, AR_CONF_KEY_NAME);
	assignments = e_sender_validation_parse_assignments (strv);

	if (assignments) {
		EComposerHeaderTable *header_table;
		const gchar *from_address;

		header_table = e_msg_composer_get_header_table (composer);
		from_address = e_composer_header_table_get_from_address (header_table);

		if (from_address && *from_address) {
			GSList *link, *usable_assignments = NULL;

			for (link = assignments; link; link = g_slist_next (link)) {
				const Assignment *assignment = link->data;

				/* one account can be in the list multiple times */
				if (camel_strstrcase (from_address, assignment->account))
					usable_assignments = g_slist_prepend (usable_assignments, (gpointer) assignment);
			}

			usable_assignments = g_slist_reverse (usable_assignments);

			if (usable_assignments) {
				EDestination **destinations;
				guint ii;

				destinations = e_composer_header_table_get_destinations (header_table);

				for (ii = 0; destinations && destinations[ii]; ii++) {
					EDestination *dest = destinations[ii];
					const gchar *recipient;

					recipient = e_destination_get_address (dest);

					if (recipient && *recipient) {
						const Assignment *has_mismatch = NULL;
						gboolean has_match = FALSE;

						for (link = usable_assignments; link && !has_match; link = g_slist_next (link)) {
							const Assignment *assignment = link->data;

							if (camel_strstrcase (recipient, assignment->recipient))
								has_match = TRUE;
							else if (!has_mismatch)
								has_mismatch = assignment;
						}

						if (!has_match && has_mismatch) {
							can_send = e_sender_validation_ask_ar (GTK_WINDOW (composer), recipient, has_mismatch->recipient, from_address);
							break;
						}
					}
				}

				e_destination_freev (destinations);
			}
		}
	}

	g_slist_free_full (assignments, e_sender_validation_free_assignment);
	/* Can free 'strv' only after 'assignments', because 'assignments' is using the memory from 'strv' */
	g_strfreev (strv);
	g_clear_object (&settings);

	return can_send;
}

void
org_gnome_evolution_sender_validation_presendchecks (EPlugin *ep,
						     EMEventTargetComposer *tt)
{
	if (plugin_enabled &&
	    !e_sender_validation_check (tt->composer))
		g_object_set_data (G_OBJECT (tt->composer), "presend_check_status", GINT_TO_POINTER (1));
}

enum {
	RECIPIENT_COLUMN,
	ACCOUNT_COLUMN,
	N_COLUMNS
};

typedef struct {
	GSettings *settings;
	/* 'ra' for Recipients~>Account */
	GtkWidget *ra_tree_view;
	GtkWidget *ra_button_add;
	GtkWidget *ra_button_edit;
	GtkWidget *ra_button_remove;
	GtkListStore *ra_store;

	/* 'ar' for Account~>Recipients */
	GtkWidget *ar_tree_view;
	GtkWidget *ar_button_add;
	GtkWidget *ar_button_edit;
	GtkWidget *ar_button_remove;
	GtkListStore *ar_store;
} UIData;

static void
commit_changes (UIData *ui,
		GtkWidget *tree_view,
		const gchar *conf_key)
{
	GtkTreeModel *model = NULL;
	GVariantBuilder builder;
	GVariant *var;
	GtkTreeIter iter;
	gboolean valid;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

	while (valid) {
		gchar *recipient = NULL, *account = NULL;

		gtk_tree_model_get (model, &iter,
			RECIPIENT_COLUMN, &recipient,
			ACCOUNT_COLUMN, &account,
			-1);

		if (recipient && g_utf8_strlen (g_strstrip (recipient), -1) > 0 &&
		    account && g_utf8_strlen (g_strstrip (account), -1) > 0) {
			gchar *value;

			value = g_strconcat (recipient, "\t", account, NULL);

			g_variant_builder_add (&builder, "s", value);

			g_free (value);
		}

		g_free (recipient);
		g_free (account);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	/* A floating GVariant is returned, which is consumed by the g_settings_set_value() */
	var = g_variant_builder_end (&builder);
	g_settings_set_value (ui->settings, conf_key, var);
}

static void
column_edited (UIData *ui,
	       gint column_index,
	       const gchar *path_string,
	       const gchar *new_text,
	       GtkWidget *tree_view,
	       const gchar *conf_key)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_index, new_text, -1);
	commit_changes (ui, tree_view, conf_key);
}

static void
ra_recipient_edited_cb (GtkCellRendererText *cell,
			const gchar *path_string,
			const gchar *new_text,
			UIData *ui)
{
	column_edited (ui, RECIPIENT_COLUMN, path_string, new_text, ui->ra_tree_view, RA_CONF_KEY_NAME);
}

static void
ra_account_edited_cb (GtkCellRendererText *cell,
		      const gchar *path_string,
		      const gchar *new_text,
		      UIData *ui)
{
	column_edited (ui, ACCOUNT_COLUMN, path_string, new_text, ui->ra_tree_view, RA_CONF_KEY_NAME);
}

static void
ar_recipient_edited_cb (GtkCellRendererText *cell,
			const gchar *path_string,
			const gchar *new_text,
			UIData *ui)
{
	column_edited (ui, RECIPIENT_COLUMN, path_string, new_text, ui->ar_tree_view, AR_CONF_KEY_NAME);
}

static void
ar_account_edited_cb (GtkCellRendererText *cell,
		      const gchar *path_string,
		      const gchar *new_text,
		      UIData *ui)
{
	column_edited (ui, ACCOUNT_COLUMN, path_string, new_text, ui->ar_tree_view, AR_CONF_KEY_NAME);
}

static void
add_clicked (GtkTreeView *tree_view,
	     gint column_index)
{
	GtkTreeModel *model;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (tree_view);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	path = gtk_tree_model_get_path (model, &iter);
	column = gtk_tree_view_get_column (tree_view, column_index);
	gtk_tree_view_set_cursor (tree_view, path, column, TRUE);
	gtk_tree_view_row_activated (tree_view, path, column);
	gtk_tree_path_free (path);
}

static void
ra_button_add_clicked (GtkButton *button,
		       UIData *ui)
{
	add_clicked (GTK_TREE_VIEW (ui->ra_tree_view), RECIPIENT_COLUMN);
}

static void
ar_button_add_clicked (GtkButton *button,
		       UIData *ui)
{
	add_clicked (GTK_TREE_VIEW (ui->ar_tree_view), ACCOUNT_COLUMN);
}

static void
remove_clicked (UIData *ui,
		GtkTreeView *tree_view,
		GtkWidget *button_edit,
		GtkWidget *button_remove,
		const gchar *conf_key)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid;
	gint len;

	valid = FALSE;
	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	/* Get the path and move to the previous node */
	path = gtk_tree_model_get_path (model, &iter);
	if (path)
		valid = gtk_tree_path_prev (path);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	len = gtk_tree_model_iter_n_children (model, NULL);
	if (len > 0) {
		if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (model), &iter)) {
			gtk_tree_selection_select_iter (selection, &iter);
		} else {
			if (path && valid) {
				gtk_tree_model_get_iter (model, &iter, path);
				gtk_tree_selection_select_iter (selection, &iter);
			}
		}
	} else {
		gtk_widget_set_sensitive (button_edit, FALSE);
		gtk_widget_set_sensitive (button_remove, FALSE);
	}

	gtk_widget_grab_focus (GTK_WIDGET (tree_view));
	gtk_tree_path_free (path);

	commit_changes (ui, GTK_WIDGET (tree_view), conf_key);
}

static void
ra_button_remove_clicked (GtkButton *button,
			  UIData *ui)
{
	remove_clicked (ui, GTK_TREE_VIEW (ui->ra_tree_view), ui->ra_button_edit, ui->ra_button_remove, RA_CONF_KEY_NAME);
}

static void
ar_button_remove_clicked (GtkButton *button,
			  UIData *ui)
{
	remove_clicked (ui, GTK_TREE_VIEW (ui->ar_tree_view), ui->ar_button_edit, ui->ar_button_remove, AR_CONF_KEY_NAME);
}

static void
edit_clicked (UIData *ui,
	      GtkTreeView *tree_view,
	      gint column_index)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeViewColumn *focus_col;

	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	focus_col = gtk_tree_view_get_column (tree_view, column_index);
	path = gtk_tree_model_get_path (model, &iter);

	if (path) {
		gtk_tree_view_set_cursor (tree_view, path, focus_col, TRUE);
		gtk_tree_path_free (path);
	}
}

static void
ra_button_edit_clicked (GtkButton *button,
			UIData *ui)
{
	edit_clicked (ui, GTK_TREE_VIEW (ui->ra_tree_view), RECIPIENT_COLUMN);
}

static void
ar_button_edit_clicked (GtkButton *button,
			UIData *ui)
{
	edit_clicked (ui, GTK_TREE_VIEW (ui->ar_tree_view), ACCOUNT_COLUMN);
}

static void
selection_changed (GtkTreeSelection *selection,
		   GtkWidget *button_edit,
		   GtkWidget *button_remove)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (button_edit, TRUE);
		gtk_widget_set_sensitive (button_remove, TRUE);
	} else {
		gtk_widget_set_sensitive (button_edit, FALSE);
		gtk_widget_set_sensitive (button_remove, FALSE);
	}
}

static void
ra_selection_changed_cb (GtkTreeSelection *selection,
			 UIData *ui)
{
	selection_changed (selection, ui->ra_button_edit, ui->ra_button_remove);
}

static void
ar_selection_changed_cb (GtkTreeSelection *selection,
			 UIData *ui)
{
	selection_changed (selection, ui->ar_button_edit, ui->ar_button_remove);
}

static void
destroy_ui_data (gpointer data)
{
	UIData *ui = (UIData *) data;

	if (!ui)
		return;

	g_object_unref (ui->settings);
	g_slice_free (UIData, ui);
}

static void
e_sender_validation_fill_accounts (GtkCellRendererCombo *renderer)
{
	EShell *shell;
	GtkListStore *store = NULL;
	CamelInternetAddress *address;
	gchar **identities;
	guint ii;

	shell = e_shell_get_default ();
	if (!shell)
		return;

	address = camel_internet_address_new ();
	identities = itip_get_user_identities (e_shell_get_registry (shell));

	for (ii = 0; identities && identities[ii]; ii++) {
		const gchar *identity = identities[ii];
		guint len;

		while (len = camel_address_length (CAMEL_ADDRESS (address)), len > 0)
			camel_address_remove (CAMEL_ADDRESS (address), len - 1);

		if (camel_address_unformat (CAMEL_ADDRESS (address), identity) > 0) {
			const gchar *email;
			guint jj;

			for (jj = 0; camel_internet_address_get (address, jj, NULL, &email); jj++) {
				if (email && *email) {
					GtkTreeIter iter;

					if (!store)
						store = gtk_list_store_new (1, G_TYPE_STRING);

					gtk_list_store_append (store, &iter);
					gtk_list_store_set (store, &iter, 0, email, -1);
				}
			}
		}
	}

	g_clear_object (&address);
	g_strfreev (identities);

	if (store) {
		g_object_set (G_OBJECT (renderer),
			"has-entry", TRUE,
			"model", store,
			"text-column", 0,
			NULL);

		g_object_unref (store);
	}
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *plugin)
{
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *container;
	GtkWidget *scrolledwindow;
	GtkWidget *vbuttonbox;
	GSList *assignments, *link;
	UIData *ui;
	gchar **strv;

	ui = g_slice_new0 (UIData);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (vbox);
	gtk_widget_set_size_request (vbox, 385, 189);

	label = gtk_label_new (_("Set which account should be used for certain recipient patterns.\n"
		"For example, setting recipient as “@company.org” for account “me@company.org” will warn when a recipient containing “@company.org” is not used with the “me@company.org” account."));
	g_object_set (label,
		"halign", GTK_ALIGN_START,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"wrap", TRUE,
		"wrap-mode", PANGO_WRAP_WORD,
		"max-width-chars", 80,
		NULL);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 6);

	container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_show (container);
	gtk_box_pack_start (GTK_BOX (vbox), container, TRUE, TRUE, 0);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow);
	gtk_box_pack_start (GTK_BOX (container), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	ui->ra_tree_view = gtk_tree_view_new ();
	gtk_widget_show (ui->ra_tree_view);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), ui->ra_tree_view);
	gtk_container_set_border_width (GTK_CONTAINER (ui->ra_tree_view), 1);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->ra_tree_view), TRUE);
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (ui->ra_tree_view), FALSE);

	vbuttonbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (vbuttonbox);
	gtk_box_pack_start (GTK_BOX (container), vbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox), 6);

	ui->ra_button_add = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_widget_show (ui->ra_button_add);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), ui->ra_button_add);
	gtk_widget_set_can_default (ui->ra_button_add, TRUE);

	ui->ra_button_edit = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_widget_show (ui->ra_button_edit);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), ui->ra_button_edit);
	gtk_widget_set_can_default (ui->ra_button_edit, TRUE);

	ui->ra_button_remove = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	gtk_widget_show (ui->ra_button_remove);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), ui->ra_button_remove);
	gtk_widget_set_can_default (ui->ra_button_remove, TRUE);

	ui->settings = e_util_ref_settings ("org.gnome.evolution.plugin.sender-validation");
	ui->ra_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->ra_tree_view), GTK_TREE_MODEL (ui->ra_store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (ui->ra_tree_view), -1, _("Recipient Contains"),
		renderer, "text", RECIPIENT_COLUMN, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (ra_recipient_edited_cb), ui);

	renderer = gtk_cell_renderer_combo_new ();
	e_sender_validation_fill_accounts (GTK_CELL_RENDERER_COMBO (renderer));
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (ui->ra_tree_view), -1, _("Account to Use"),
		renderer, "text", ACCOUNT_COLUMN, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (ra_account_edited_cb), ui);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->ra_tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (ra_selection_changed_cb), ui);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->ra_tree_view), TRUE);

	g_signal_connect (
		ui->ra_button_add, "clicked",
		G_CALLBACK (ra_button_add_clicked), ui);

	g_signal_connect (
		ui->ra_button_remove, "clicked",
		G_CALLBACK (ra_button_remove_clicked), ui);
	gtk_widget_set_sensitive (ui->ra_button_remove, FALSE);

	g_signal_connect (
		ui->ra_button_edit, "clicked",
		G_CALLBACK (ra_button_edit_clicked), ui);
	gtk_widget_set_sensitive (ui->ra_button_edit, FALSE);

	strv = g_settings_get_strv (ui->settings, RA_CONF_KEY_NAME);
	assignments = e_sender_validation_parse_assignments (strv);

	for (link = assignments; link; link = g_slist_next (link)) {
		const Assignment *assignment = link->data;

		gtk_list_store_append (ui->ra_store, &iter);
		gtk_list_store_set (ui->ra_store, &iter,
			RECIPIENT_COLUMN, assignment->recipient,
			ACCOUNT_COLUMN, assignment->account,
			-1);
	}

	g_slist_free_full (assignments, e_sender_validation_free_assignment);
	g_strfreev (strv);

	label = gtk_label_new (_("Set which recipient patterns can be used for certain account.\n"
		"For example, setting account “me@company.org” for recipients “@company.org” will warn when any of the recipients does not contain “@company.org” when sending with the “me@company.org” account."));
	g_object_set (label,
		"halign", GTK_ALIGN_START,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"wrap", TRUE,
		"wrap-mode", PANGO_WRAP_WORD,
		"max-width-chars", 80,
		NULL);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 6);

	container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_show (container);
	gtk_box_pack_start (GTK_BOX (vbox), container, TRUE, TRUE, 0);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow);
	gtk_box_pack_start (GTK_BOX (container), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	ui->ar_tree_view = gtk_tree_view_new ();
	gtk_widget_show (ui->ar_tree_view);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), ui->ar_tree_view);
	gtk_container_set_border_width (GTK_CONTAINER (ui->ar_tree_view), 1);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->ar_tree_view), TRUE);
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (ui->ar_tree_view), FALSE);

	vbuttonbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (vbuttonbox);
	gtk_box_pack_start (GTK_BOX (container), vbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox), 6);

	ui->ar_button_add = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_widget_show (ui->ar_button_add);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), ui->ar_button_add);

	ui->ar_button_edit = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_widget_show (ui->ar_button_edit);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), ui->ar_button_edit);
	gtk_widget_set_can_default (ui->ar_button_edit, TRUE);

	ui->ar_button_remove = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	gtk_widget_show (ui->ar_button_remove);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), ui->ar_button_remove);
	gtk_widget_set_can_default (ui->ar_button_remove, TRUE);

	ui->ar_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->ar_tree_view), GTK_TREE_MODEL (ui->ar_store));

	renderer = gtk_cell_renderer_combo_new ();
	e_sender_validation_fill_accounts (GTK_CELL_RENDERER_COMBO (renderer));
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (ui->ar_tree_view), -1, _("Account"),
		renderer, "text", ACCOUNT_COLUMN, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (ar_account_edited_cb), ui);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (ui->ar_tree_view), -1, _("Allow Recipients Which Contain"),
		renderer, "text", RECIPIENT_COLUMN, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (ar_recipient_edited_cb), ui);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->ar_tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (ar_selection_changed_cb), ui);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->ar_tree_view), TRUE);

	g_signal_connect (
		ui->ar_button_add, "clicked",
		G_CALLBACK (ar_button_add_clicked), ui);

	g_signal_connect (
		ui->ar_button_remove, "clicked",
		G_CALLBACK (ar_button_remove_clicked), ui);
	gtk_widget_set_sensitive (ui->ar_button_remove, FALSE);

	g_signal_connect (
		ui->ar_button_edit, "clicked",
		G_CALLBACK (ar_button_edit_clicked), ui);
	gtk_widget_set_sensitive (ui->ar_button_edit, FALSE);

	strv = g_settings_get_strv (ui->settings, AR_CONF_KEY_NAME);
	assignments = e_sender_validation_parse_assignments (strv);

	for (link = assignments; link; link = g_slist_next (link)) {
		const Assignment *assignment = link->data;

		gtk_list_store_append (ui->ar_store, &iter);
		gtk_list_store_set (ui->ar_store, &iter,
			RECIPIENT_COLUMN, assignment->recipient,
			ACCOUNT_COLUMN, assignment->account,
			-1);
	}

	g_slist_free_full (assignments, e_sender_validation_free_assignment);
	g_strfreev (strv);

	hbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	/* to let free data properly on destroy of configuration widget */
	g_object_set_data_full (G_OBJECT (hbox), "myui-data", ui, destroy_ui_data);

	return hbox;
}
