/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "mail/e-mail-account-store.h"
#include "mail/e-mail-ui-session.h"
#include "shell/e-shell.h"

#include "e-accounts-window-editors.h"

/* Standard GObject macros */
#define E_TYPE_ACCOUNTS_WINDOW_EDITORS \
	(e_accounts_window_editors_get_type ())
#define E_ACCOUNTS_WINDOW_EDITORS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACCOUNTS_WINDOW_EDITORS, EAccountsWindowEditors))
#define E_ACCOUNTS_WINDOW_EDITORS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACCOUNTS_WINDOW_EDITORS, EAccountsWindowEditorsClass))
#define E_IS_ACCOUNTS_WINDOW_EDITORS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACCOUNTS_WINDOW_EDITORS))
#define E_IS_ACCOUNTS_WINDOW_EDITORS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACCOUNTS_WINDOW_EDITORS))
#define E_ACCOUNTS_WINDOW_EDITORS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ACCOUNTS_WINDOW_EDITORS, EAccountsWindowEditorsClass))

typedef struct _EAccountsWindowEditors EAccountsWindowEditors;
typedef struct _EAccountsWindowEditorsClass EAccountsWindowEditorsClass;

struct _EAccountsWindowEditors {
	EExtension parent;

	gchar *gcc_program_path;
};

struct _EAccountsWindowEditorsClass {
	EExtensionClass parent_class;
};

GType e_accounts_window_editors_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (EAccountsWindowEditors, e_accounts_window_editors, E_TYPE_EXTENSION)

static void
accounts_window_editors_open_goa (EAccountsWindowEditors *editors,
				  ESource *source)
{
	gchar *goa_account_id;
	gchar *command_line;
	GError *error = NULL;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW_EDITORS (editors));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (e_source_has_extension (source, E_SOURCE_EXTENSION_GOA));
	g_return_if_fail (editors->gcc_program_path != NULL);

	goa_account_id = e_source_goa_dup_account_id (e_source_get_extension (source, E_SOURCE_EXTENSION_GOA));

	command_line = g_strjoin (
		" ",
		editors->gcc_program_path,
		"online-accounts",
		goa_account_id,
		NULL);
	g_spawn_command_line_async (command_line, &error);

	g_free (command_line);
	g_free (goa_account_id);

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static void
accounts_window_editors_open_uoa (EAccountsWindowEditors *editors,
				  ESource *source)
{
	guint uoa_account_id;
	gchar *account_details;
	gchar *command_line;
	GError *error = NULL;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW_EDITORS (editors));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (e_source_has_extension (source, E_SOURCE_EXTENSION_UOA));
	g_return_if_fail (editors->gcc_program_path != NULL);

	uoa_account_id = e_source_uoa_get_account_id (e_source_get_extension (source, E_SOURCE_EXTENSION_UOA));

	account_details = g_strdup_printf ("account-details=%u", uoa_account_id);
	command_line = g_strjoin (
		" ",
		editors->gcc_program_path,
		"credentials",
		account_details,
		NULL);
	g_spawn_command_line_async (command_line, &error);
	g_free (command_line);
	g_free (account_details);

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

#define COLLECTION_EDITOR_DATA_KEY "collection-editor-data-key"

typedef struct _CollectionEditorData {
	ESource *source;
	GtkWidget *alert_bar;
	GtkWidget *display_name;
	GtkWidget *mail_part;
	GtkWidget *calendar_part;
	GtkWidget *contacts_part;
} CollectionEditorData;

static void
collection_editor_data_free (gpointer ptr)
{
	CollectionEditorData *ced = ptr;

	if (ced) {
		g_clear_object (&ced->source);
		g_slice_free (CollectionEditorData, ced);
	}
}

static void
accounts_window_editors_source_written_cb (GObject *source_object,
					   GAsyncResult *result,
					   gpointer user_data)
{
	GtkWidget *dialog = user_data;
	CollectionEditorData *ced;
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	gtk_widget_set_sensitive (dialog, TRUE);

	ced = g_object_get_data (G_OBJECT (dialog), COLLECTION_EDITOR_DATA_KEY);
	g_return_if_fail (ced != NULL);

	if (!e_source_write_finish (E_SOURCE (source_object), result, &error)) {
		EAlert *alert;

		alert = e_alert_new ("system:simple-error", error ? error->message : _("Unknown error"), NULL);

		e_alert_bar_add_alert (E_ALERT_BAR (ced->alert_bar), alert);

		g_clear_error (&error);
	} else {
		gtk_widget_destroy (dialog);
	}
}

static void
accounts_window_editors_collection_editor_response_cb (GtkWidget *dialog,
						       gint response_id,
						       gpointer user_data)
{
	CollectionEditorData *ced;

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	ced = g_object_get_data (G_OBJECT (dialog), COLLECTION_EDITOR_DATA_KEY);
	g_return_if_fail (ced != NULL);

	if (response_id == GTK_RESPONSE_OK) {
		ESourceCollection *collection_extension;
		gboolean changed;

		collection_extension = e_source_get_extension (ced->source, E_SOURCE_EXTENSION_COLLECTION);

		changed = g_strcmp0 (e_source_get_display_name (ced->source), gtk_entry_get_text (GTK_ENTRY (ced->display_name))) != 0;

		changed = changed || e_source_collection_get_mail_enabled (collection_extension) !=
			gtk_switch_get_active (GTK_SWITCH (ced->mail_part));

		changed = changed || e_source_collection_get_calendar_enabled (collection_extension) !=
			gtk_switch_get_active (GTK_SWITCH (ced->calendar_part));

		changed = changed || e_source_collection_get_contacts_enabled (collection_extension) !=
			gtk_switch_get_active (GTK_SWITCH (ced->contacts_part));

		if (changed) {
			e_alert_bar_clear (E_ALERT_BAR (ced->alert_bar));

			e_source_set_display_name (ced->source, gtk_entry_get_text (GTK_ENTRY (ced->display_name)));

			e_source_collection_set_mail_enabled (collection_extension,
				gtk_switch_get_active (GTK_SWITCH (ced->mail_part)));

			e_source_collection_set_calendar_enabled (collection_extension,
				gtk_switch_get_active (GTK_SWITCH (ced->calendar_part)));

			e_source_collection_set_contacts_enabled (collection_extension,
				gtk_switch_get_active (GTK_SWITCH (ced->contacts_part)));

			gtk_widget_set_sensitive (dialog, FALSE);

			e_source_write (ced->source, NULL,
				accounts_window_editors_source_written_cb, dialog);

			return;
		}
	}

	gtk_widget_destroy (dialog);
}

static void
accounts_window_editors_collection_editor_display_name_changed_cb (GtkEntry *entry,
								   gpointer user_data)
{
	GtkDialog *dialog = user_data;
	gchar *text;

	g_return_if_fail (GTK_IS_ENTRY (entry));
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	text = g_strdup (gtk_entry_get_text (entry));

	if (text)
		text = g_strstrip (text);

	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, text && *text);

	g_free (text);
}

static void
accounts_window_editors_edit_unmanaged_collection (EAccountsWindow *accounts_window,
						   ESource *source)
{
	GtkWidget *dialog, *widget, *label, *container;
	GtkGrid *grid;
	CollectionEditorData *ced;
	ESourceCollection *collection_extension;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION));

	ced = g_slice_new0 (CollectionEditorData);
	ced->source = g_object_ref (source);

	collection_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_COLLECTION);

	dialog = gtk_dialog_new_with_buttons (_("Edit Collection"), GTK_WINDOW (accounts_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "evolution");

	widget = e_alert_bar_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	ced->alert_bar = widget;

	widget = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);

	grid = GTK_GRID (widget);
	gtk_grid_set_column_spacing (grid, 6);
	gtk_grid_set_row_spacing (grid, 2);

	label = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_widget_set_halign (label, GTK_ALIGN_END);

	gtk_grid_attach (grid, label, 0, 0, 1, 1);

	widget = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (widget), e_source_get_display_name (source));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	ced->display_name = widget;

	gtk_grid_attach (grid, widget, 1, 0, 2, 1);

	g_signal_connect (ced->display_name, "changed",
		G_CALLBACK (accounts_window_editors_collection_editor_display_name_changed_cb), dialog);

	label = gtk_label_new (_("Use for"));
	gtk_widget_set_halign (label, GTK_ALIGN_END);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
	gtk_grid_attach (grid, label, 0, 1, 1, 1);

	label = gtk_label_new_with_mnemonic (_("_Mail"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
	gtk_grid_attach (grid, label, 1, 1, 1, 1);

	widget = gtk_switch_new ();
	gtk_switch_set_active (GTK_SWITCH (widget), e_source_collection_get_mail_enabled (collection_extension));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	ced->mail_part = widget;

	gtk_grid_attach (grid, widget, 2, 1, 1, 1);

	label = gtk_label_new_with_mnemonic (_("C_alendar"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
	gtk_grid_attach (grid, label, 1, 2, 1, 1);

	widget = gtk_switch_new ();
	gtk_switch_set_active (GTK_SWITCH (widget), e_source_collection_get_calendar_enabled (collection_extension));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	ced->calendar_part = widget;

	gtk_grid_attach (grid, widget, 2, 2, 1, 1);

	label = gtk_label_new_with_mnemonic (_("Co_ntacts"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
	gtk_grid_attach (grid, label, 1, 3, 1, 1);

	widget = gtk_switch_new ();
	gtk_switch_set_active (GTK_SWITCH (widget), e_source_collection_get_contacts_enabled (collection_extension));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	ced->contacts_part = widget;

	gtk_grid_attach (grid, widget, 2, 3, 1, 1);

	gtk_widget_show_all (GTK_WIDGET (grid));

	g_object_set_data_full (G_OBJECT (dialog), COLLECTION_EDITOR_DATA_KEY, ced, collection_editor_data_free);

	g_signal_connect (dialog, "response",
		G_CALLBACK (accounts_window_editors_collection_editor_response_cb), NULL);

	gtk_widget_show (dialog);
}

static gboolean
accounts_window_editors_get_editing_flags_cb (EAccountsWindow *accounts_window,
					      ESource *source,
					      guint *out_flags,
					      gpointer user_data)
{
	EAccountsWindowEditors *editors = user_data;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW_EDITORS (editors), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (out_flags != NULL, FALSE);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT) ||
	    e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK) ||
	    e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR) ||
	    e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST) ||
	    e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST)) {
		*out_flags = E_SOURCE_EDITING_FLAG_CAN_ENABLE | E_SOURCE_EDITING_FLAG_CAN_EDIT | E_SOURCE_EDITING_FLAG_CAN_DELETE;
		return TRUE;
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION)) {
		if (e_source_has_extension (source, E_SOURCE_EXTENSION_GOA) ||
		    e_source_has_extension (source, E_SOURCE_EXTENSION_UOA)) {
			if (editors->gcc_program_path)
				*out_flags = E_SOURCE_EDITING_FLAG_CAN_ENABLE | E_SOURCE_EDITING_FLAG_CAN_EDIT;
			else
				*out_flags = E_SOURCE_EDITING_FLAG_CAN_ENABLE;
		} else {
			*out_flags = E_SOURCE_EDITING_FLAG_CAN_ENABLE | E_SOURCE_EDITING_FLAG_CAN_EDIT | E_SOURCE_EDITING_FLAG_CAN_DELETE;
		}

		return TRUE;
	}

	return FALSE;
}

static void
accounts_window_editors_commit_changes_cb (ESourceConfig *config,
					   ESource *scratch_source,
					   gpointer user_data)
{
	EAccountsWindow *accounts_window;
	GWeakRef *weakref = user_data;

	g_return_if_fail (E_IS_SOURCE (scratch_source));
	g_return_if_fail (weakref != NULL);

	accounts_window = g_weak_ref_get (weakref);
	if (!accounts_window)
		return;

	e_accounts_window_select_source (accounts_window, e_source_get_uid (scratch_source));

	g_object_unref (accounts_window);
}

static void
accounts_window_editors_new_mail_source_cb (GtkWidget *assistant,
					    const gchar *uid,
					    gpointer user_data)
{
	EAccountsWindow *accounts_window;
	GWeakRef *weakref = user_data;

	g_return_if_fail (uid != NULL);
	g_return_if_fail (weakref != NULL);

	accounts_window = g_weak_ref_get (weakref);
	if (!accounts_window)
		return;

	e_accounts_window_select_source (accounts_window, uid);

	g_object_unref (accounts_window);
}

static gboolean
accounts_window_editors_add_source_cb (EAccountsWindow *accounts_window,
				       const gchar *kind,
				       gpointer user_data)
{
	EAccountsWindowEditors *editors = user_data;
	ESourceRegistry *registry;
	GtkWidget *config = NULL;
	const gchar *icon_name = NULL;
	const gchar *title = NULL;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW_EDITORS (editors), FALSE);
	g_return_val_if_fail (kind && *kind, FALSE);

	registry = e_accounts_window_get_registry (accounts_window);

	if (g_strcmp0 (kind, "mail") == 0) {
		EShellBackend *shell_backend;
		EShell *shell = e_shell_get_default ();

		if (shell) {
			GtkWidget *assistant = NULL;

			shell_backend = e_shell_get_backend_by_name (shell, "mail");

			g_signal_emit_by_name (shell_backend, "new-account", GTK_WINDOW (accounts_window), &assistant);

			if (assistant) {
				g_signal_connect_data (assistant, "new-source",
					G_CALLBACK (accounts_window_editors_new_mail_source_cb),
					e_weak_ref_new (accounts_window), (GClosureNotify) e_weak_ref_free, 0);
			}
		}

		return TRUE;
	} else if (g_strcmp0 (kind, "book") == 0) {
		icon_name = "x-office-address-book";
		title = _("New Address Book");
		config = e_book_source_config_new (registry, NULL);
	} else if (g_strcmp0 (kind, "calendar") == 0) {
		icon_name = "x-office-calendar";
		title = _("New Calendar");
		config = e_cal_source_config_new (registry, NULL, E_CAL_CLIENT_SOURCE_TYPE_EVENTS);
	} else if (g_strcmp0 (kind, "memo-list") == 0) {
		icon_name = "evolution-memos";
		title = _("New Memo List");
		config = e_cal_source_config_new (registry, NULL, E_CAL_CLIENT_SOURCE_TYPE_MEMOS);
	} else if (g_strcmp0 (kind, "task-list") == 0) {
		icon_name = "evolution-tasks";
		title = _("New Task List");
		config = e_cal_source_config_new (registry, NULL, E_CAL_CLIENT_SOURCE_TYPE_TASKS);
	}

	if (config) {
		GtkWidget *dialog;

		g_signal_connect_data (config, "commit-changes",
			G_CALLBACK (accounts_window_editors_commit_changes_cb),
			e_weak_ref_new (accounts_window), (GClosureNotify) e_weak_ref_free, 0);

		dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (accounts_window));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
		gtk_window_set_title (GTK_WINDOW (dialog), title);

		gtk_widget_show (dialog);

		return TRUE;
	}

	return FALSE;
}

static gboolean
accounts_window_editors_edit_source_cb (EAccountsWindow *accounts_window,
					ESource *source,
					gpointer user_data)
{
	EAccountsWindowEditors *editors = user_data;
	ESourceRegistry *registry;
	GtkWidget *config = NULL;
	const gchar *icon_name = NULL;
	const gchar *title = NULL;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW_EDITORS (editors), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	registry = e_accounts_window_get_registry (accounts_window);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK)) {
		icon_name = "x-office-address-book";
		title = _("Address Book Properties");
		config = e_book_source_config_new (registry, source);
	} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR)) {
		icon_name = "x-office-calendar";
		title = _("Calendar Properties");
		config = e_cal_source_config_new (registry, source, E_CAL_CLIENT_SOURCE_TYPE_EVENTS);
	} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST)) {
		icon_name = "evolution-memos";
		title = _("Memo List Properties");
		config = e_cal_source_config_new (registry, source, E_CAL_CLIENT_SOURCE_TYPE_MEMOS);
	} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST)) {
		icon_name = "evolution-tasks";
		title = _("Task List Properties");
		config = e_cal_source_config_new (registry, source, E_CAL_CLIENT_SOURCE_TYPE_TASKS);
	}

	if (config) {
		GtkWidget *dialog;

		dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (accounts_window));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
		gtk_window_set_title (GTK_WINDOW (dialog), title);

		gtk_widget_show (dialog);

		return TRUE;
	} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
		EShellBackend *shell_backend;
		EShell *shell = e_shell_get_default ();

		if (shell) {
			shell_backend = e_shell_get_backend_by_name (shell, "mail");

			g_signal_emit_by_name (shell_backend, "edit-account", accounts_window, source);
		}

		return TRUE;
	} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION)) {
		if (e_source_has_extension (source, E_SOURCE_EXTENSION_GOA)) {
			accounts_window_editors_open_goa (editors, source);
		} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_UOA)) {
			accounts_window_editors_open_uoa (editors, source);
		} else {
			accounts_window_editors_edit_unmanaged_collection (accounts_window, source);
		}

		return TRUE;
	}

	return FALSE;
}

static void
accounts_window_editors_enabled_toggled_cb (EAccountsWindow *accounts_window,
					    ESource *source,
					    gpointer user_data)
{
	EShell *shell;
	ESource *mail_account_source = NULL;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	g_return_if_fail (E_IS_SOURCE (source));

	shell = e_shell_get_default ();
	if (!shell)
		return;

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION)) {
		GList *sources, *link;
		const gchar *uid = e_source_get_uid (source);

		sources = e_source_registry_list_sources (e_accounts_window_get_registry (accounts_window), E_SOURCE_EXTENSION_MAIL_ACCOUNT);
		for (link = sources; link; link = g_list_next (link)) {
			ESource *adept = link->data;

			if (g_strcmp0 (uid, e_source_get_parent (adept)) == 0) {
				mail_account_source = g_object_ref (adept);
				break;
			}
		}

		g_list_free_full (sources, g_object_unref);
	}

	if (mail_account_source || e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
		EShellBackend *shell_backend;
		EMailSession *session = NULL;

		shell_backend = e_shell_get_backend_by_name (shell, "mail");
		g_object_get (G_OBJECT (shell_backend), "session", &session, NULL);

		if (session) {
			CamelService *service;

			service = camel_session_ref_service (CAMEL_SESSION (session), e_source_get_uid (mail_account_source ? mail_account_source : source));

			if (service) {
				EMailAccountStore *account_store;

				account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));

				if (e_source_get_enabled (source)) {
					e_mail_account_store_enable_service (account_store, GTK_WINDOW (accounts_window), service);
				} else {
					e_mail_account_store_disable_service (account_store, GTK_WINDOW (accounts_window), service);
				}

				g_object_unref (service);
			}

			g_object_unref (session);
		}
	}

	if (!e_source_get_enabled (source))
		e_shell_allow_auth_prompt_for (shell, source);

	g_clear_object (&mail_account_source);
}

static void
accounts_window_editors_constructed (GObject *object)
{
	EAccountsWindow *accounts_window;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_accounts_window_editors_parent_class)->constructed (object);

	accounts_window = E_ACCOUNTS_WINDOW (e_extension_get_extensible (E_EXTENSION (object)));

	g_signal_connect (accounts_window, "get-editing-flags",
		G_CALLBACK (accounts_window_editors_get_editing_flags_cb), object);

	g_signal_connect (accounts_window, "add-source",
		G_CALLBACK (accounts_window_editors_add_source_cb), object);

	g_signal_connect (accounts_window, "edit-source",
		G_CALLBACK (accounts_window_editors_edit_source_cb), object);

	g_signal_connect (accounts_window, "enabled-toggled",
		G_CALLBACK (accounts_window_editors_enabled_toggled_cb), object);
}

static void
accounts_window_editors_finalize (GObject *object)
{
	EAccountsWindowEditors *editors = E_ACCOUNTS_WINDOW_EDITORS (object);

	g_free (editors->gcc_program_path);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_accounts_window_editors_parent_class)->finalize (object);
}

static void
e_accounts_window_editors_class_init (EAccountsWindowEditorsClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = accounts_window_editors_constructed;
	object_class->finalize = accounts_window_editors_finalize;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_ACCOUNTS_WINDOW;
}

static void
e_accounts_window_editors_class_finalize (EAccountsWindowEditorsClass *class)
{
}

static void
e_accounts_window_editors_init (EAccountsWindowEditors *extension)
{
	extension->gcc_program_path = g_find_program_in_path ("gnome-control-center");
}

void
e_accounts_window_editors_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_accounts_window_editors_register_type (type_module);
}
