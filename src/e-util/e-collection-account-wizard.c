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

/**
 * SECTION: e-collection-account-wizard
 * @include: e-util/e-util.h
 * @short_description: Collection account wizard
 *
 * #ECollectionAccountWizard is a configuration wizard which guides
 * user through steps to created collection accounts. Such accounts
 * provide multiple sources at once, being it address books, calendars,
 * mail and others.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>
#include <libedataserverui/libedataserverui.h>

#include "e-config-lookup.h"
#include "e-dialog-widgets.h"
#include "e-misc-utils.h"
#include "e-spinner.h"
#include "e-simple-async-result.h"

#include "e-collection-account-wizard.h"

/* There is no mail identity in the EConfigLookupResultKind enum, thus define a fake one */
#define FAKE_E_CONFIG_LOOKUP_RESULT_MAIL_IDENTITY E_CONFIG_LOOKUP_RESULT_UNKNOWN

struct _ECollectionAccountWizardPrivate {
	ESourceRegistry *registry;
	EConfigLookup *config_lookup;
	GHashTable *store_passwords; /* gchar *source-uid ~> gchar *password */
	GHashTable *workers; /* EConfigLookupWorker * ~> WorkerData * */
	guint running_workers;
	ESimpleAsyncResult *running_result;
	gboolean changed;

	ESource *sources[E_CONFIG_LOOKUP_RESULT_LAST_KIND + 1];

	/* Lookup page */
	GtkWidget *email_entry;
	GtkWidget *advanced_expander;
	GtkWidget *servers_entry;
	GtkWidget *results_label;

	/* Parts page */
	GtkTreeView *parts_tree_view;

	/* Finish page */
	GtkWidget *display_name_entry;
	GtkWidget *finish_running_box;
	GtkWidget *finish_spinner;
	GtkWidget *finish_label;
	GtkWidget *finish_cancel_button;
	GCancellable *finish_cancellable;
};

enum {
	PROP_0,
	PROP_REGISTRY,
	PROP_CHANGED,
	PROP_CAN_RUN
};

enum {
	DONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (ECollectionAccountWizard, e_collection_account_wizard, GTK_TYPE_NOTEBOOK)

typedef struct _WizardWindowData {
	GtkWidget *window;
	GtkWidget *prev_button;
	GtkButton *next_button;
	ECollectionAccountWizard *collection_wizard;
} WizardWindowData;

static void
collection_wizard_window_update_button_captions (WizardWindowData *wwd)
{
	g_return_if_fail (wwd != NULL);

	gtk_widget_set_sensitive (wwd->prev_button, gtk_notebook_get_current_page (GTK_NOTEBOOK (wwd->collection_wizard)) > 0);

	if (e_collection_account_wizard_is_finish_page (wwd->collection_wizard))
		gtk_button_set_label (wwd->next_button, _("_Finish"));
	else if (wwd->collection_wizard->priv->changed ||
		 !e_config_lookup_count_results (wwd->collection_wizard->priv->config_lookup))
		gtk_button_set_label (wwd->next_button, _("_Look Up"));
	else
		gtk_button_set_label (wwd->next_button, _("_Next"));
}

static void
collection_wizard_window_cancel_button_clicked_cb (GtkButton *button,
						 gpointer user_data)
{
	WizardWindowData *wwd = user_data;

	g_return_if_fail (wwd != NULL);

	e_collection_account_wizard_abort (wwd->collection_wizard);
	gtk_widget_destroy (wwd->window);
}

static void
collection_wizard_window_back_button_clicked_cb (GtkButton *button,
						 gpointer user_data)
{
	WizardWindowData *wwd = user_data;

	g_return_if_fail (wwd != NULL);

	if (!e_collection_account_wizard_prev (wwd->collection_wizard)) {
		e_collection_account_wizard_abort (wwd->collection_wizard);
		gtk_widget_destroy (wwd->window);
	} else {
		collection_wizard_window_update_button_captions (wwd);
	}
}

static void
collection_wizard_window_next_button_clicked_cb (GtkButton *button,
						 gpointer user_data)
{
	WizardWindowData *wwd = user_data;
	gboolean is_finish_page;

	g_return_if_fail (wwd != NULL);

	is_finish_page = e_collection_account_wizard_is_finish_page (wwd->collection_wizard);

	if (e_collection_account_wizard_next (wwd->collection_wizard)) {
		if (is_finish_page) {
			gtk_widget_destroy (wwd->window);
		} else {
			collection_wizard_window_update_button_captions (wwd);
		}
	}
}

static void
collection_wizard_window_done (WizardWindowData *wwd,
			       const gchar *uid)
{
	g_return_if_fail (wwd != NULL);

	e_collection_account_wizard_abort (wwd->collection_wizard);
	gtk_widget_destroy (wwd->window);
}

static GtkWindow *
collection_account_wizard_create_window (GtkWindow *parent,
					 GtkWidget *wizard)
{
	GtkWidget *widget, *vbox, *hbox, *scrolled_window;
	GtkWindow *window;
	GtkAccelGroup *accel_group;
	WizardWindowData *wwd;

	window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_default_size (window, 480, 410);
	gtk_window_set_title (window, _("New Collection Account"));
	gtk_window_set_position (window, parent ? GTK_WIN_POS_CENTER_ON_PARENT : GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);

	if (parent) {
		gtk_window_set_transient_for (window, parent);
		gtk_window_set_destroy_with_parent (window, TRUE);
	}

	wwd = g_new0 (WizardWindowData, 1);
	wwd->window = GTK_WIDGET (window);

	g_object_weak_ref (G_OBJECT (window), (GWeakNotify) g_free, wwd);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_NONE);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_container_add (GTK_CONTAINER (window), widget);
	gtk_widget_show (widget);

	scrolled_window = widget;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (widget), vbox);
	gtk_widget_show (vbox);

	widget = wizard;
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	wwd->collection_wizard = E_COLLECTION_ACCOUNT_WIZARD (widget);

	g_signal_connect_swapped (wwd->collection_wizard, "done",
		G_CALLBACK (collection_wizard_window_done), wwd);

	g_signal_connect_swapped (wwd->collection_wizard, "notify::changed",
		G_CALLBACK (collection_wizard_window_update_button_captions), wwd);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	g_object_set (G_OBJECT (hbox),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	widget = e_dialog_button_new_with_icon ("window-close", _("_Cancel"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	accel_group = gtk_accel_group_new ();
	gtk_widget_add_accelerator (
		widget, "activate", accel_group,
		GDK_KEY_Escape, (GdkModifierType) 0,
		GTK_ACCEL_VISIBLE);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (collection_wizard_window_cancel_button_clicked_cb), wwd);

	widget = e_dialog_button_new_with_icon ("go-previous", _("_Previous"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	wwd->prev_button = widget;

	g_signal_connect (widget, "clicked",
		G_CALLBACK (collection_wizard_window_back_button_clicked_cb), wwd);

	e_binding_bind_property (
		wwd->collection_wizard, "can-run",
		widget, "sensitive",
		G_BINDING_DEFAULT);

	widget = e_dialog_button_new_with_icon ("go-next", _("_Next"));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		"can-default", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	wwd->next_button = GTK_BUTTON (widget);

	e_binding_bind_property (
		wwd->collection_wizard, "can-run",
		widget, "sensitive",
		G_BINDING_DEFAULT);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (collection_wizard_window_next_button_clicked_cb), wwd);

	gtk_widget_grab_default (GTK_WIDGET (wwd->next_button));

	e_collection_account_wizard_reset (wwd->collection_wizard);
	collection_wizard_window_update_button_captions (wwd);

	e_signal_connect_notify_swapped (
		gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)), "notify::upper",
		G_CALLBACK (e_util_ensure_scrolled_window_height), scrolled_window);

	g_signal_connect (scrolled_window, "map", G_CALLBACK (e_util_ensure_scrolled_window_height), NULL);

	return window;
}

enum {
	PART_COLUMN_BOOL_ENABLED,		/* G_TYPE_BOOLEAN */
	PART_COLUMN_BOOL_ENABLED_VISIBLE,	/* G_TYPE_BOOLEAN */
	PART_COLUMN_BOOL_RADIO,			/* G_TYPE_BOOLEAN */
	PART_COLUMN_BOOL_SENSITIVE,		/* G_TYPE_BOOLEAN */
	PART_COLUMN_BOOL_IS_COLLECTION_GROUP,	/* G_TYPE_BOOLEAN */
	PART_COLUMN_BOOL_ICON_VISIBLE,		/* G_TYPE_BOOLEAN */
	PART_COLUMN_STRING_ICON_NAME,		/* G_TYPE_STRING */
	PART_COLUMN_STRING_DESCRIPTION,		/* G_TYPE_STRING */
	PART_COLUMN_STRING_PROTOCOL,		/* G_TYPE_STRING */
	PART_COLUMN_OBJECT_RESULT,		/* E_TYPE_CONFIG_LOOKUP_RESULT */
	PART_N_COLUMNS
};

typedef struct _WorkerData {
	GtkWidget *enabled_check;
	GtkWidget *running_box;
	GtkWidget *spinner;
	GtkWidget *running_label;
	GtkWidget *cancel_button;
	GCancellable *cancellable;
	gulong status_id;
	ENamedParameters *restart_params;
	gchar *certificate_error;
	gboolean remember_password;
} WorkerData;

static void
worker_data_free (gpointer ptr)
{
	WorkerData *wd = ptr;

	if (wd) {
		if (wd->cancellable) {
			g_cancellable_cancel (wd->cancellable);

			if (wd->status_id) {
				g_signal_handler_disconnect (wd->cancellable, wd->status_id);
				wd->status_id = 0;
			}

			g_clear_object (&wd->cancellable);
		}

		g_clear_pointer (&wd->certificate_error, g_free);
		g_clear_pointer (&wd->restart_params, e_named_parameters_free);

		g_free (wd);
	}
}

static void
collection_account_wizard_update_status_cb (CamelOperation *op,
					    const gchar *what,
					    gint pc,
					    gpointer user_data)
{
	GtkLabel *label = user_data;

	g_return_if_fail (GTK_IS_LABEL (label));

	if (what)
		gtk_label_set_label (label, what);
}

static void
collection_account_wizard_notify_can_run (GObject *wizard)
{
	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	g_object_notify (wizard, "can-run");
}

static gboolean
collection_account_wizard_get_changed (ECollectionAccountWizard *wizard)
{
	return wizard->priv->changed;
}

static void
collection_account_wizard_set_changed (ECollectionAccountWizard *wizard,
				       gboolean changed)
{
	if ((wizard->priv->changed ? 1 : 0) != (changed ? 1 : 0)) {
		wizard->priv->changed = changed;
		g_object_notify (G_OBJECT (wizard), "changed");
	}
}

static void
collection_account_wizard_mark_changed (ECollectionAccountWizard *wizard)
{
	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	collection_account_wizard_set_changed (wizard, TRUE);
}

static void
collection_account_wizard_worker_cancel_clicked_cb (GtkWidget *button,
						    gpointer user_data)
{
	WorkerData *wd = user_data;

	g_return_if_fail (wd != NULL);

	if (wd->cancellable)
		g_cancellable_cancel (wd->cancellable);
}

static void
collection_account_wizard_worker_started_cb (EConfigLookup *config_lookup,
					     EConfigLookupWorker *worker,
					     GCancellable *cancellable,
					     gpointer user_data)
{
	ECollectionAccountWizard *wizard = user_data;
	WorkerData *wd;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	wd = g_hash_table_lookup (wizard->priv->workers, worker);
	g_return_if_fail (wd != NULL);

	if (wizard->priv->changed)
		collection_account_wizard_set_changed (wizard, FALSE);

	wizard->priv->running_workers++;

	g_warn_if_fail (wd->cancellable == NULL);
	wd->cancellable = g_object_ref (cancellable);

	wd->status_id = 0;
	if (CAMEL_IS_OPERATION (wd->cancellable)) {
		wd->status_id = g_signal_connect (wd->cancellable, "status",
			G_CALLBACK (collection_account_wizard_update_status_cb), wd->running_label);
	}

	gtk_label_set_label (GTK_LABEL (wd->running_label), _("Looking up details, please wait…"));
	e_spinner_start (E_SPINNER (wd->spinner));
	gtk_widget_show (wd->spinner);
	gtk_widget_show (wd->cancel_button);
	gtk_widget_show (wd->running_box);

	if (wizard->priv->running_workers == 1) {
		GHashTableIter iter;
		gpointer value;

		g_hash_table_iter_init (&iter, wizard->priv->workers);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			WorkerData *wd2 = value;

			gtk_widget_set_sensitive (wd2->enabled_check, FALSE);
		}

		g_object_notify (G_OBJECT (wizard), "can-run");

		gtk_label_set_text (GTK_LABEL (wizard->priv->results_label), "");
	}
}

static void
collection_account_wizard_worker_finished_cb (EConfigLookup *config_lookup,
					      EConfigLookupWorker *worker,
					      const ENamedParameters *restart_params,
					      const GError *error,
					      gpointer user_data)
{
	ECollectionAccountWizard *wizard = user_data;
	WorkerData *wd;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	wd = g_hash_table_lookup (wizard->priv->workers, worker);
	g_return_if_fail (wd != NULL);

	wizard->priv->running_workers--;

	if (wd->status_id) {
		g_signal_handler_disconnect (wd->cancellable, wd->status_id);
		wd->status_id = 0;
	}
	g_clear_object (&wd->cancellable);
	g_clear_pointer (&wd->certificate_error, g_free);

	e_spinner_stop (E_SPINNER (wd->spinner));
	gtk_widget_hide (wd->spinner);
	gtk_widget_hide (wd->cancel_button);

	if (g_error_matches (error, E_CONFIG_LOOKUP_WORKER_ERROR, E_CONFIG_LOOKUP_WORKER_ERROR_REQUIRES_PASSWORD)) {
		gchar *markup, *link;

		link = g_markup_printf_escaped ("<a href=\"evo:enter-password\">%s</a>", _("Enter password"));

		if (error->message && *error->message) {
			gchar *escaped;

			escaped = g_markup_escape_text (error->message, -1);
			markup = g_strconcat (escaped, " ", link, NULL);
			g_free (escaped);
		} else {
			/* Translators: The %s is replaced with a clickable text "Enter password", thus it'll be "Requires password to continue. Enter password." at the end. */
			markup = g_strdup_printf (_("Requires password to continue. %s."), link);
		}

		gtk_label_set_markup (GTK_LABEL (wd->running_label), markup);

		g_free (markup);
		g_free (link);
	} else if (g_error_matches (error, E_CONFIG_LOOKUP_WORKER_ERROR, E_CONFIG_LOOKUP_WORKER_ERROR_CERTIFICATE) &&
		   restart_params && e_named_parameters_exists (restart_params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_PEM) &&
		   e_named_parameters_exists (restart_params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_HOST)) {
		gchar *markup, *link, *escaped = NULL;

		wd->certificate_error = g_strdup (error->message);

		link = g_markup_printf_escaped ("<a href=\"evo:view-certificate\">%s</a>", _("View certificate"));

		if (error->message && *error->message)
			escaped = g_markup_escape_text (error->message, -1);

		markup = g_strconcat (escaped ? escaped : "", escaped ? "\n" : "", link, NULL);

		gtk_label_set_markup (GTK_LABEL (wd->running_label), markup);

		g_free (escaped);
		g_free (markup);
		g_free (link);
	} else if (error) {
		gtk_label_set_text (GTK_LABEL (wd->running_label), error->message);
	} else {
		gtk_widget_hide (wd->running_box);
	}

	e_named_parameters_free (wd->restart_params);
	wd->restart_params = restart_params ? e_named_parameters_new_clone (restart_params) : NULL;

	if (!wizard->priv->running_workers) {
		GHashTableIter iter;
		gpointer value;
		gint n_results;
		gchar *str;

		g_hash_table_iter_init (&iter, wizard->priv->workers);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			WorkerData *wd2 = value;

			gtk_widget_set_sensitive (wd2->enabled_check, TRUE);
		}

		g_clear_pointer (&wizard->priv->running_result, e_simple_async_result_complete_idle_take);

		g_object_notify (G_OBJECT (wizard), "can-run");

		n_results = e_config_lookup_count_results (wizard->priv->config_lookup);

		if (!n_results) {
			gtk_label_set_text (GTK_LABEL (wizard->priv->results_label), _("Found no candidates. It can also mean that the server doesn’t provide any information about its configuration using the selected lookup methods. Enter the account manually instead or change above settings."));
		} else {
			str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Found one candidate", "Found %d candidates", n_results), n_results);
			gtk_label_set_text (GTK_LABEL (wizard->priv->results_label), str);
			g_free (str);
		}

		/* When there are results, the wizard can continue, otherwise it will Look Up again. */
		collection_account_wizard_set_changed (wizard, !n_results);

		/* This is to ensure invoke of collection_wizard_window_update_button_captions()
		   in the "notify::changed" callback, because the above set_changed() can change
		   only from FALSE to TRUE, but not vice versa, due to the 'changed' being FALSE
		   already. */
		g_object_notify (G_OBJECT (wizard), "changed");
	}
}

typedef struct _PasswordPromptData {
	ECollectionAccountWizard *wizard;
	EConfigLookupWorker *worker;
	GtkWidget *popover;
	GtkWidget *user_entry;
	GtkWidget *password_entry;
	GtkWidget *remember_check;
} PasswordPromptData;

static PasswordPromptData *
password_prompt_data_new (ECollectionAccountWizard *wizard,
			  EConfigLookupWorker *worker,
			  GtkWidget *popover,
			  GtkWidget *user_entry,
			  GtkWidget *password_entry,
			  GtkWidget *remember_check)
{
	PasswordPromptData *ppd;

	ppd = g_slice_new0 (PasswordPromptData);
	ppd->wizard = wizard;
	ppd->worker = worker;
	ppd->popover = popover;
	ppd->user_entry = user_entry;
	ppd->password_entry = password_entry;
	ppd->remember_check = remember_check;

	return ppd;
}

static void
password_prompt_data_free (gpointer data,
			   GClosure *closure)
{
	PasswordPromptData *ppd = data;

	if (ppd) {
		/* Nothing to free inside the structure */
		g_slice_free (PasswordPromptData, ppd);
	}
}

static void
collection_account_wizard_try_again_clicked_cb (GtkButton *button,
						gpointer user_data)
{
	PasswordPromptData *ppd = user_data;
	ENamedParameters *params;
	WorkerData *wd;

	g_return_if_fail (ppd != NULL);
	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (ppd->wizard));
	g_return_if_fail (GTK_IS_ENTRY (ppd->user_entry));
	g_return_if_fail (GTK_IS_ENTRY (ppd->password_entry));

	wd = g_hash_table_lookup (ppd->wizard->priv->workers, ppd->worker);
	g_return_if_fail (wd != NULL);

	params = e_named_parameters_new_clone (wd->restart_params);
	g_return_if_fail (params != NULL);

	wd->remember_password = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ppd->remember_check));

	gtk_entry_set_text (GTK_ENTRY (ppd->wizard->priv->email_entry), gtk_entry_get_text (GTK_ENTRY (ppd->user_entry)));

	e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS, gtk_entry_get_text (GTK_ENTRY (ppd->wizard->priv->email_entry)));
	e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_SERVERS, gtk_entry_get_text (GTK_ENTRY (ppd->wizard->priv->servers_entry)));
	e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_PASSWORD, gtk_entry_get_text (GTK_ENTRY (ppd->password_entry)));
	e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_REMEMBER_PASSWORD, wd->remember_password ? "1" : NULL);

	e_config_lookup_run_worker (ppd->wizard->priv->config_lookup, ppd->worker, params, NULL);

	e_named_parameters_free (params);

	gtk_widget_hide (ppd->popover);
}

static void
collection_account_wizard_update_entry_hint (GtkWidget *entry)
{
	const gchar *user = gtk_entry_get_text (GTK_ENTRY (entry));

	e_util_set_entry_issue_hint (entry, (!user || !*user || camel_string_is_all_ascii (user)) ? NULL :
		_("User name contains letters, which can prevent log in. Make sure the server accepts such written user name."));
}

static void
collection_account_wizard_show_password_prompt (ECollectionAccountWizard *wizard,
						EConfigLookupWorker *worker,
						WorkerData *wd)
{
	GtkWidget *widget, *label, *user_entry, *password_entry, *check, *button;
	GtkGrid *grid;
	const gchar *text;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));
	g_return_if_fail (E_IS_CONFIG_LOOKUP_WORKER (worker));
	g_return_if_fail (wd != NULL);

	widget = gtk_grid_new ();
	grid = GTK_GRID (widget);
	gtk_grid_set_column_spacing (grid, 6);
	gtk_grid_set_row_spacing (grid, 6);

	widget = gtk_label_new_with_mnemonic (_("_Username:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	label = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_entry_set_text (GTK_ENTRY (widget), gtk_entry_get_text (GTK_ENTRY (wizard->priv->email_entry)));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	user_entry = widget;

	g_signal_connect (user_entry, "changed",
		G_CALLBACK (collection_account_wizard_update_entry_hint), NULL);

	widget = gtk_label_new_with_mnemonic (_("_Password:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);
	label = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (widget), FALSE);
	gtk_entry_set_input_purpose (GTK_ENTRY (widget), GTK_INPUT_PURPOSE_PASSWORD);
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	password_entry = widget;

	widget = gtk_check_button_new_with_mnemonic (_("_Remember password"));
	gtk_grid_attach (grid, widget, 0, 2, 2, 1);
	check = widget;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), wd->remember_password);

	widget = gtk_button_new_with_mnemonic (_("_Try Again"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_widget_set_can_default (widget, TRUE);
	gtk_grid_attach (grid, widget, 0, 3, 2, 1);
	button = widget;

	gtk_widget_show_all (GTK_WIDGET (grid));

	widget = gtk_popover_new (wd->running_label);
	gtk_popover_set_position (GTK_POPOVER (widget), GTK_POS_BOTTOM);
	gtk_popover_set_default_widget (GTK_POPOVER (widget), button);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (grid));
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);

	g_signal_connect_data (button, "clicked",
		G_CALLBACK (collection_account_wizard_try_again_clicked_cb),
		password_prompt_data_new (wizard, worker, widget, user_entry, password_entry, check),
		password_prompt_data_free, 0);

	g_signal_connect (widget, "closed",
		G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (widget);

	text = gtk_entry_get_text (GTK_ENTRY (user_entry));

	if (!text || !*text)
		gtk_widget_grab_focus (user_entry);
	else
		gtk_widget_grab_focus (password_entry);
}

static void
collection_account_wizard_view_certificate (ECollectionAccountWizard *wizard,
					    EConfigLookupWorker *worker,
					    WorkerData *wd)
{
	ETrustPromptResponse response;
	GtkWidget *toplevel;
	GtkWindow *parent = NULL;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));
	g_return_if_fail (E_IS_CONFIG_LOOKUP_WORKER (worker));
	g_return_if_fail (wd != NULL);
	g_return_if_fail (wd->restart_params != NULL);
	g_return_if_fail (e_named_parameters_exists (wd->restart_params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_PEM));
	g_return_if_fail (e_named_parameters_exists (wd->restart_params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_HOST));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (wizard));
	if (GTK_IS_WINDOW (toplevel))
		parent = GTK_WINDOW (toplevel);

	response = e_trust_prompt_run_modal (parent, NULL, NULL,
		e_named_parameters_get (wd->restart_params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_HOST),
		e_named_parameters_get (wd->restart_params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_PEM),
		0, wd->certificate_error);

	if (response != E_TRUST_PROMPT_RESPONSE_UNKNOWN) {
		ENamedParameters *params;

		params = e_named_parameters_new_clone (wd->restart_params);
		g_return_if_fail (params != NULL);

		e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS, gtk_entry_get_text (GTK_ENTRY (wizard->priv->email_entry)));
		e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_SERVERS, gtk_entry_get_text (GTK_ENTRY (wizard->priv->servers_entry)));
		e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_TRUST, e_config_lookup_encode_certificate_trust (response));

		e_config_lookup_run_worker (wizard->priv->config_lookup, worker, params, NULL);

		e_named_parameters_free (params);
	}
}

static gboolean
collection_account_wizard_activate_link_cb (GtkWidget *label,
					    const gchar *uri,
					    gpointer user_data)
{
	ECollectionAccountWizard *wizard = user_data;
	EConfigLookupWorker *worker = NULL;
	WorkerData *wd;
	GHashTableIter iter;
	gpointer key, value;

	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), TRUE);

	g_hash_table_iter_init (&iter, wizard->priv->workers);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		worker = key;
		wd = value;

		if (worker && wd && wd->running_label == label)
			break;

		worker = NULL;
		wd = NULL;
	}

	if (worker && wd) {
		if (g_strcmp0 (uri, "evo:enter-password") == 0)
			collection_account_wizard_show_password_prompt (wizard, worker, wd);
		else if (g_strcmp0 (uri, "evo:view-certificate") == 0)
			collection_account_wizard_view_certificate (wizard, worker, wd);
		else
			g_warning ("%s: Do not know what to do with '%s'", G_STRFUNC, uri);
	}

	return TRUE;
}

static gboolean
collection_account_wizard_is_first_result_of_this_kind (GSList *known_results,
							EConfigLookupResult *result)
{
	GSList *link;
	gboolean known = FALSE;

	for (link = known_results; link && !known; link = g_slist_next (link)) {
		EConfigLookupResult *result2 = link->data;

		if (!result2 || result2 == result)
			continue;

		known = e_config_lookup_result_get_kind (result) ==
			e_config_lookup_result_get_kind (result2) &&
			g_strcmp0 (e_config_lookup_result_get_protocol (result),
			e_config_lookup_result_get_protocol (result2)) == 0;
	}

	return !known;
}

static gboolean
collection_account_wizard_fill_results (ECollectionAccountWizard *wizard)
{
	struct _results_info {
		EConfigLookupResultKind kind;
		const gchar *display_name;
		const gchar *icon_name;
		GSList *results; /* EConfigLookupResult * */
	} results_info[] = {
		{ E_CONFIG_LOOKUP_RESULT_COLLECTION, 	N_("Collection"),	"evolution",		NULL },
		{ E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,	N_("Mail Receive"),	"evolution-mail",	NULL },
		{ E_CONFIG_LOOKUP_RESULT_MAIL_SEND,	N_("Mail Send"),	"mail-send",		NULL },
		{ E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK,	N_("Address Book"),	"x-office-address-book",NULL },
		{ E_CONFIG_LOOKUP_RESULT_CALENDAR,	N_("Calendar"),		"x-office-calendar",	NULL },
		{ E_CONFIG_LOOKUP_RESULT_MEMO_LIST,	N_("Memo List"),	"evolution-memos",	NULL },
		{ E_CONFIG_LOOKUP_RESULT_TASK_LIST,	N_("Task List"),	"evolution-tasks",	NULL }
	};

	GtkTreeStore *tree_store;
	GtkTreeIter iter, parent;
	gint ii;
	gboolean found_any = FALSE;

	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), FALSE);

	tree_store = GTK_TREE_STORE (gtk_tree_view_get_model (wizard->priv->parts_tree_view));
	gtk_tree_store_clear (tree_store);

	for (ii = 0; ii < G_N_ELEMENTS (results_info); ii++) {
		results_info[ii].results = e_config_lookup_dup_results (wizard->priv->config_lookup, results_info[ii].kind, NULL);

		if (results_info[ii].results) {
			found_any = TRUE;

			results_info[ii].results = g_slist_sort (results_info[ii].results, e_config_lookup_result_compare);
		}
	}

	if (!found_any)
		return FALSE;

	for (ii = 0; ii < G_N_ELEMENTS (results_info); ii++) {
		GSList *results = results_info[ii].results, *link, *known_results = NULL, *klink;
		gboolean is_collection_kind = results_info[ii].kind == E_CONFIG_LOOKUP_RESULT_COLLECTION;
		gboolean group_enabled = TRUE;
		GtkTreePath *path;

		/* Skip empty groups */
		if (!results)
			continue;

		if (results_info[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE ||
		    results_info[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_SEND) {
			group_enabled = e_util_strcmp0 (gtk_entry_get_text (GTK_ENTRY (wizard->priv->email_entry)), NULL) != 0;
		}

		gtk_tree_store_append (tree_store, &parent, NULL);
		gtk_tree_store_set (tree_store, &parent,
			PART_COLUMN_BOOL_ENABLED, group_enabled,
			PART_COLUMN_BOOL_ENABLED_VISIBLE, TRUE,
			PART_COLUMN_BOOL_RADIO, FALSE,
			PART_COLUMN_BOOL_SENSITIVE, results != NULL,
			PART_COLUMN_BOOL_IS_COLLECTION_GROUP, is_collection_kind,
			PART_COLUMN_BOOL_ICON_VISIBLE, results_info[ii].icon_name != NULL,
			PART_COLUMN_STRING_ICON_NAME, results_info[ii].icon_name,
			PART_COLUMN_STRING_DESCRIPTION, _(results_info[ii].display_name),
			-1);

		for (link = results; link; link = g_slist_next (link)) {
			EConfigLookupResult *result = link->data;
			const gchar *display_name, *description;
			gchar *markup;

			if (!result)
				continue;

			for (klink = known_results; klink; klink = g_slist_next (klink)) {
				if (e_config_lookup_result_equal (result, klink->data))
					break;
			}

			/* Found one such processed already. */
			if (klink)
				continue;

			/* Just borrow it, no need to reference it. */
			known_results = g_slist_prepend (known_results, result);

			display_name = e_config_lookup_result_get_display_name (result);
			description = e_config_lookup_result_get_description (result);

			if (description && *description)
				markup = g_markup_printf_escaped ("%s\n<small>%s</small>", display_name, description);
			else
				markup = g_markup_printf_escaped ("%s", display_name);

			gtk_tree_store_append (tree_store, &iter, &parent);
			gtk_tree_store_set (tree_store, &iter,
				PART_COLUMN_BOOL_ENABLED, link == results || (is_collection_kind && collection_account_wizard_is_first_result_of_this_kind (known_results, result)),
				PART_COLUMN_BOOL_ENABLED_VISIBLE, g_slist_next (results) != NULL,
				PART_COLUMN_BOOL_RADIO, !is_collection_kind,
				PART_COLUMN_BOOL_SENSITIVE, group_enabled,
				PART_COLUMN_BOOL_ICON_VISIBLE, NULL,
				PART_COLUMN_STRING_ICON_NAME, NULL,
				PART_COLUMN_STRING_DESCRIPTION, markup,
				PART_COLUMN_STRING_PROTOCOL, e_config_lookup_result_get_protocol (result),
				PART_COLUMN_OBJECT_RESULT, result,
				-1);

			g_free (markup);
		}

		g_slist_free (known_results);

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_store), &parent);
		if (path) {
			gtk_tree_view_expand_to_path (wizard->priv->parts_tree_view, path);
			gtk_tree_path_free (path);
		}
	}

	for (ii = 0; ii < G_N_ELEMENTS (results_info); ii++) {
		g_slist_free_full (results_info[ii].results, g_object_unref);
		results_info[ii].results = NULL;
	}

	return TRUE;
}

static void
collection_account_wizard_part_enabled_toggled_cb (GtkCellRendererToggle *cell_renderer,
						   const gchar *path_string,
						   gpointer user_data)
{
	ECollectionAccountWizard *wizard = user_data;
	GtkTreeStore *tree_store;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter, parent, child;
	EConfigLookupResult *src_result = NULL, *cur_result = NULL;
	gboolean set_enabled, is_radio = FALSE;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	model = gtk_tree_view_get_model (wizard->priv->parts_tree_view);
	tree_store = GTK_TREE_STORE (model);

	path = gtk_tree_path_new_from_string (path_string);
	if (!gtk_tree_model_get_iter (model, &child, path)) {
		g_warn_if_reached ();
		gtk_tree_path_free (path);

		return;
	}

	gtk_tree_path_free (path);

	set_enabled = !gtk_cell_renderer_toggle_get_active (cell_renderer);

	gtk_tree_model_get (model, &child,
		PART_COLUMN_BOOL_RADIO, &is_radio,
		PART_COLUMN_OBJECT_RESULT, &src_result,
		-1);

	/* Reflect the change for other radio-s in this level */
	if (is_radio) {
		GtkTreeIter sibling = child;

		iter = child;

		/* Move to the first sibling */
		if (gtk_tree_model_iter_parent (model, &parent, &child) &&
		    gtk_tree_model_iter_nth_child (model, &iter, &parent, 0)) {
			sibling = iter;
		} else {
			while (gtk_tree_model_iter_previous (model, &iter))
				sibling = iter;
		}

		do {
			is_radio = FALSE;

			gtk_tree_model_get (model, &sibling,
				PART_COLUMN_BOOL_RADIO, &is_radio,
				PART_COLUMN_OBJECT_RESULT, &cur_result,
				-1);

			if (is_radio) {
				gtk_tree_store_set (tree_store, &sibling,
					PART_COLUMN_BOOL_ENABLED, cur_result == src_result,
					-1);
			}

			g_clear_object (&cur_result);
		} while (gtk_tree_model_iter_next (model, &sibling));
	} else {
		gtk_tree_store_set (tree_store, &child,
			PART_COLUMN_BOOL_ENABLED, set_enabled,
			-1);
	}

	/* De/sensitize children of the group nodes */
	if (!gtk_tree_model_iter_parent (model, &parent, &child) &&
	    gtk_tree_model_iter_nth_child (model, &iter, &child, 0)) {
		do {
			gtk_tree_store_set (tree_store, &iter,
				PART_COLUMN_BOOL_SENSITIVE, set_enabled,
				-1);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	g_clear_object (&src_result);

	if (!is_radio)
		g_object_notify (G_OBJECT (wizard), "can-run");
}

static ESource *
collection_account_wizard_create_child_source (ECollectionAccountWizard *wizard,
					       const gchar *add_extension_name)
{
	ESource *source;

	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), NULL);
	g_return_val_if_fail (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION] != NULL, NULL);

	source = e_source_new (NULL, NULL, NULL);

	e_source_set_parent (source, e_source_get_uid (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION]));

	if (add_extension_name)
		e_source_get_extension (source, add_extension_name);

	return source;
}

static ESource *
collection_account_wizard_get_source (ECollectionAccountWizard *wizard,
				      EConfigLookupResultKind kind)
{
	ESource *source = NULL;
	const gchar *extension_name = NULL;

	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), NULL);

	switch (kind) {
	case E_CONFIG_LOOKUP_RESULT_COLLECTION:
		source = wizard->priv->sources[kind];
		g_warn_if_fail (source != NULL);
		break;
	case E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE:
		extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
		break;
	case FAKE_E_CONFIG_LOOKUP_RESULT_MAIL_IDENTITY: /* E_CONFIG_LOOKUP_RESULT_UNKNOWN */
		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		break;
	case E_CONFIG_LOOKUP_RESULT_MAIL_SEND:
		extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
		break;
	case E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK:
		extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
		break;
	case E_CONFIG_LOOKUP_RESULT_CALENDAR:
		extension_name = E_SOURCE_EXTENSION_CALENDAR;
		break;
	case E_CONFIG_LOOKUP_RESULT_MEMO_LIST:
		extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
		break;
	case E_CONFIG_LOOKUP_RESULT_TASK_LIST:
		extension_name = E_SOURCE_EXTENSION_TASK_LIST;
		break;
	}

	g_return_val_if_fail (kind >= 0 && kind <= E_CONFIG_LOOKUP_RESULT_LAST_KIND, NULL);

	source = wizard->priv->sources[kind];

	if (!source && kind != E_CONFIG_LOOKUP_RESULT_COLLECTION) {
		source = collection_account_wizard_create_child_source (wizard, extension_name);
		wizard->priv->sources[kind] = source;
	}

	return source;
}

static ESource *
collection_account_wizard_get_source_cb (ECollectionAccountWizard *wizard,
					 EConfigLookupSourceKind kind)
{
	ESource *source = NULL;

	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), NULL);

	switch (kind) {
	case E_CONFIG_LOOKUP_SOURCE_UNKNOWN:
		break;
	case E_CONFIG_LOOKUP_SOURCE_COLLECTION:
		source = collection_account_wizard_get_source (wizard, E_CONFIG_LOOKUP_RESULT_COLLECTION);
		break;
	case E_CONFIG_LOOKUP_SOURCE_MAIL_ACCOUNT:
		source = collection_account_wizard_get_source (wizard, E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE);
		break;
	case E_CONFIG_LOOKUP_SOURCE_MAIL_IDENTITY:
		source = collection_account_wizard_get_source (wizard, FAKE_E_CONFIG_LOOKUP_RESULT_MAIL_IDENTITY);
		break;
	case E_CONFIG_LOOKUP_SOURCE_MAIL_TRANSPORT:
		source = collection_account_wizard_get_source (wizard, E_CONFIG_LOOKUP_RESULT_MAIL_SEND);
		break;
	}

	return source;
}

static gboolean
collection_account_wizard_host_is_google_server (const gchar *host)
{
	if (!host || !*host)
		return FALSE;

	return e_util_host_is_in_domain (host, "gmail.com") ||
	       e_util_host_is_in_domain (host, "googlemail.com") ||
	       e_util_host_is_in_domain (host, "google.com") ||
	       e_util_host_is_in_domain (host, "googleusercontent.com");
}

static void
collection_account_wizard_write_changes_thread (ESimpleAsyncResult *result,
						gpointer source_object,
						GCancellable *cancellable)
{
	ECollectionAccountWizard *wizard = source_object;
	ESourceCollection *collection_extension;
	ESource *source;
	gint ii;
	const gchar *text;
	GList *sources = NULL;
	gboolean google_supported, any_is_google = FALSE;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	/* Deal with LDAP addressbook first */
	source = wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK];
	if (source &&
	    e_source_has_extension (source, E_SOURCE_EXTENSION_LDAP_BACKEND) &&
	    e_source_has_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION)) {
		ESourceAuthentication *auth_extension;
		ESourceLDAP *ldap_extension;
		const gchar *root_dn;

		auth_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
		ldap_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_LDAP_BACKEND);
		root_dn = e_source_ldap_get_root_dn (ldap_extension);

		if (!root_dn || !*root_dn) {
			gchar **root_dse = NULL;
			ESourceLDAPSecurity security;
			gboolean success;

			camel_operation_push_message (cancellable, "%s", _("Looking up LDAP server’s search base…"));

			security = e_source_ldap_get_security (ldap_extension);
			success = e_util_query_ldap_root_dse_sync (
				e_source_authentication_get_host (auth_extension),
				e_source_authentication_get_port (auth_extension),
				security,
				&root_dse, cancellable, &local_error);

			if (!success && security != E_SOURCE_LDAP_SECURITY_NONE &&
			    g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED) &&
			    !g_cancellable_is_cancelled (cancellable)) {
				success = e_util_query_ldap_root_dse_sync (
					e_source_authentication_get_host (auth_extension),
					e_source_authentication_get_port (auth_extension),
					E_SOURCE_LDAP_SECURITY_NONE,
					&root_dse, cancellable, NULL);
			}

			if (success) {
				if (root_dse && root_dse[0])
					e_source_ldap_set_root_dn (ldap_extension, root_dse[0]);

				g_strfreev (root_dse);
			}

			camel_operation_pop_message (cancellable);

			g_clear_error (&local_error);
		}

		if (g_cancellable_set_error_if_cancelled (cancellable, &local_error)) {
			e_simple_async_result_set_user_data (result, local_error, (GDestroyNotify) g_error_free);
			return;
		}
	}

	if (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE]) {
		ESourceMailAccount *mail_account_extension;
		ESourceMailIdentity *mail_identity_extension;
		ESourceMailTransport *mail_transport_extension;
		ESourceMailSubmission *mail_submission_extension;

		mail_account_extension = e_source_get_extension (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE], E_SOURCE_EXTENSION_MAIL_ACCOUNT);
		e_source_mail_account_set_identity_uid (mail_account_extension, e_source_get_uid (wizard->priv->sources[FAKE_E_CONFIG_LOOKUP_RESULT_MAIL_IDENTITY]));

		text = e_source_backend_get_backend_name (E_SOURCE_BACKEND (mail_account_extension));
		if (!text || !*text)
			e_source_backend_set_backend_name (E_SOURCE_BACKEND (mail_account_extension), "none");

		mail_identity_extension = e_source_get_extension (wizard->priv->sources[FAKE_E_CONFIG_LOOKUP_RESULT_MAIL_IDENTITY], E_SOURCE_EXTENSION_MAIL_IDENTITY);
		text = e_source_mail_identity_get_name (mail_identity_extension);
		if (!text || !*text)
			e_source_mail_identity_set_name (mail_identity_extension, g_get_real_name ());

		mail_submission_extension = e_source_get_extension (wizard->priv->sources[FAKE_E_CONFIG_LOOKUP_RESULT_MAIL_IDENTITY], E_SOURCE_EXTENSION_MAIL_SUBMISSION);
		e_source_mail_submission_set_transport_uid (mail_submission_extension, e_source_get_uid (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_MAIL_SEND]));

		mail_transport_extension = e_source_get_extension (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_MAIL_SEND], E_SOURCE_EXTENSION_MAIL_TRANSPORT);

		text = e_source_backend_get_backend_name (E_SOURCE_BACKEND (mail_transport_extension));
		if (!text || !*text)
			e_source_backend_set_backend_name (E_SOURCE_BACKEND (mail_transport_extension), "none");
	}

	if (!e_source_has_extension (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION], E_SOURCE_EXTENSION_AUTHENTICATION)) {
		/* Make sure the collection source has the Authentication extension,
		   thus the credentials can be reused. It's fine when the extension
		   doesn't have set values. */
		e_source_get_extension (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION], E_SOURCE_EXTENSION_AUTHENTICATION);
	}

	collection_extension = e_source_get_extension (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION], E_SOURCE_EXTENSION_COLLECTION);

	/* Collections with empty backend-name are skipped by ESourceRegistry */
	text = e_source_backend_get_backend_name (E_SOURCE_BACKEND (collection_extension));
	if (!text || !*text)
		e_source_backend_set_backend_name (E_SOURCE_BACKEND (collection_extension), "none");

	google_supported = e_oauth2_services_is_oauth2_alias (e_source_registry_get_oauth2_services (wizard->priv->registry), "Google");

	for (ii = 0; ii <= E_CONFIG_LOOKUP_RESULT_LAST_KIND; ii++) {
		source = wizard->priv->sources[ii];

		if (!source)
			continue;

		/* This is not great, to special-case the Google server, but there's nothing
		   better at the moment and it's the only OAuth2 right now anyway. */
		if (google_supported && ii != E_CONFIG_LOOKUP_RESULT_COLLECTION &&
		    e_source_has_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION)) {
			ESourceAuthentication *authentication_extension;

			authentication_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
			if (collection_account_wizard_host_is_google_server (e_source_authentication_get_host (authentication_extension))) {
				any_is_google = TRUE;
				e_source_authentication_set_method (authentication_extension, "Google");
			}
		}

		sources = g_list_prepend (sources, source);
	}

	source = wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION];
	/* It is always true, but have the variables local in this place only */
	if (source) {
		ESourceAuthentication *authentication_extension;
		const gchar *host;

		authentication_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
		collection_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_COLLECTION);

		host = e_source_collection_get_calendar_url (collection_extension);
		if (collection_account_wizard_host_is_google_server (host)) {
			any_is_google = TRUE;
			if (strstr (host, "calendar.google.com")) {
				e_source_backend_set_backend_name (E_SOURCE_BACKEND (collection_extension), "webdav");

				if (google_supported)
					e_source_collection_set_calendar_url (collection_extension, "https://apidata.googleusercontent.com/caldav/v2/");
				else
					e_source_collection_set_calendar_url (collection_extension, "https://www.google.com/calendar/dav/");
			}
		}

		if (any_is_google && google_supported) {
			e_source_authentication_set_method (authentication_extension, "Google");
			e_source_backend_set_backend_name (E_SOURCE_BACKEND (collection_extension), "google");
		}
	}

	/* First store passwords, thus the evolution-source-registry has them ready if needed. */
	if (g_hash_table_size (wizard->priv->store_passwords) > 0) {
		GHashTableIter iter;
		gpointer key, value;

		g_hash_table_iter_init (&iter, wizard->priv->store_passwords);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			const gchar *uid = key, *password = value;

			if (uid && *uid && password && *password) {
				source = NULL;

				for (ii = 0; ii <= E_CONFIG_LOOKUP_RESULT_LAST_KIND; ii++) {
					source = wizard->priv->sources[ii];

					if (source && g_strcmp0 (uid, e_source_get_uid (source)) == 0)
						break;

					source = NULL;
				}

				if (source && !e_source_store_password_sync (source, password, TRUE, cancellable, &local_error)) {
					g_prefix_error (&local_error, "%s", _("Failed to store password: "));
					e_simple_async_result_set_user_data (result, local_error, (GDestroyNotify) g_error_free);
					break;
				}
			}
		}
	}

	if (!e_simple_async_result_get_user_data (result) && /* No error from password save */
	    !e_source_registry_create_sources_sync (wizard->priv->registry, sources, cancellable, &local_error) && local_error) {
		g_prefix_error (&local_error, "%s", _("Failed to create sources: "));
		e_simple_async_result_set_user_data (result, local_error, (GDestroyNotify) g_error_free);
	}

	g_list_free (sources);
}

static void
collection_account_wizard_write_changes_done (GObject *source_object,
					      GAsyncResult *result,
					      gpointer user_data)
{
	ECollectionAccountWizard *wizard;
	const GError *error;
	gboolean is_cancelled = FALSE;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (source_object));

	wizard = E_COLLECTION_ACCOUNT_WIZARD (source_object);

	error = e_simple_async_result_get_user_data (E_SIMPLE_ASYNC_RESULT (result));
	if (error) {
		is_cancelled = g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

		if (is_cancelled && !wizard->priv->finish_label)
			return;

		gtk_label_set_text (GTK_LABEL (wizard->priv->finish_label), error->message);
		gtk_label_set_selectable (GTK_LABEL (wizard->priv->finish_label), TRUE);
	}

	g_clear_object (&wizard->priv->finish_cancellable);
	g_hash_table_remove_all (wizard->priv->store_passwords);

	e_spinner_stop (E_SPINNER (wizard->priv->finish_spinner));

	gtk_widget_set_visible (wizard->priv->finish_running_box, error && !is_cancelled);
	gtk_widget_set_visible (wizard->priv->finish_spinner, FALSE);
	gtk_widget_set_visible (wizard->priv->finish_label, !is_cancelled);
	gtk_widget_set_visible (wizard->priv->finish_cancel_button, FALSE);

	g_object_notify (source_object, "can-run");

	if (!error) {
		ESource *source = wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION];

		g_warn_if_fail (source != NULL);

		g_signal_emit (wizard, signals[DONE], 0, e_source_get_uid (source));
	}
}

static void
collection_account_wizard_save_sources (ECollectionAccountWizard *wizard)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESimpleAsyncResult *simple_result;
	ESource *source;
	const gchar *display_name;
	const gchar *user;
	gint ii;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	g_hash_table_remove_all (wizard->priv->store_passwords);

	model = gtk_tree_view_get_model (wizard->priv->parts_tree_view);
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gboolean enabled = FALSE;

			gtk_tree_model_get (model, &iter,
				PART_COLUMN_BOOL_ENABLED, &enabled,
				-1);

			if (enabled) {
				GtkTreeIter child;

				if (gtk_tree_model_iter_nth_child (model, &child, &iter, 0)) {
					do {
						enabled = FALSE;

						gtk_tree_model_get (model, &child,
							PART_COLUMN_BOOL_ENABLED, &enabled,
							-1);

						if (enabled) {
							EConfigLookupResult *lookup_result = NULL;

							gtk_tree_model_get (model, &child,
								PART_COLUMN_OBJECT_RESULT, &lookup_result,
								-1);

							if (lookup_result) {
								source = collection_account_wizard_get_source (wizard, e_config_lookup_result_get_kind (lookup_result));
								if (source) {
									g_warn_if_fail (e_config_lookup_result_configure_source (lookup_result, wizard->priv->config_lookup, source));

									if (e_config_lookup_result_get_password (lookup_result)) {
										g_hash_table_insert (wizard->priv->store_passwords, e_source_dup_uid (source),
											g_strdup (e_config_lookup_result_get_password (lookup_result)));
									}
								}

								g_clear_object (&lookup_result);
							}
						}
					} while (gtk_tree_model_iter_next (model, &child));
				}
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	display_name = gtk_entry_get_text (GTK_ENTRY (wizard->priv->display_name_entry));

	if (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE] ||
	    wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_MAIL_SEND]) {
		ESourceMailIdentity *identity_extension;

		/* Ensure all three exist */
		collection_account_wizard_get_source (wizard, E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE);
		collection_account_wizard_get_source (wizard, E_CONFIG_LOOKUP_RESULT_MAIL_SEND);

		source = collection_account_wizard_get_source (wizard, FAKE_E_CONFIG_LOOKUP_RESULT_MAIL_IDENTITY);
		identity_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY);
		e_source_mail_identity_set_address (identity_extension, gtk_entry_get_text (GTK_ENTRY (wizard->priv->email_entry)));
	} else {
		g_clear_object (&wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE]);
		g_clear_object (&wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_MAIL_SEND]);
		g_clear_object (&wizard->priv->sources[FAKE_E_CONFIG_LOOKUP_RESULT_MAIL_IDENTITY]);
	}

	user = gtk_entry_get_text (GTK_ENTRY (wizard->priv->email_entry));

	for (ii = 0; ii <= E_CONFIG_LOOKUP_RESULT_LAST_KIND; ii++) {
		source = wizard->priv->sources[ii];

		if (source) {
			ESourceAuthentication *authentication_extension;

			if (ii == E_CONFIG_LOOKUP_RESULT_COLLECTION) {
				ESourceCollection *collection_extension;

				authentication_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
				collection_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_COLLECTION);

				if (!e_source_authentication_get_user (authentication_extension))
					e_source_authentication_set_user (authentication_extension, user);

				if (!e_source_collection_get_identity (collection_extension))
					e_source_collection_set_identity (collection_extension, user);
			} else {
				e_source_set_parent (source, e_source_get_uid (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION]));

				if (ii == E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE ||
				    ii == E_CONFIG_LOOKUP_RESULT_MAIL_SEND) {
					authentication_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);

					if (!e_source_authentication_get_user (authentication_extension))
						e_source_authentication_set_user (authentication_extension, user);
				}
			}

			e_source_set_display_name (source, display_name);
		}
	}

	g_warn_if_fail (wizard->priv->finish_cancellable == NULL);

	gtk_label_set_text (GTK_LABEL (wizard->priv->finish_label), _("Saving account settings, please wait…"));
	gtk_label_set_selectable (GTK_LABEL (wizard->priv->finish_label), FALSE);
	gtk_widget_show (wizard->priv->finish_spinner);
	gtk_widget_show (wizard->priv->finish_label);
	gtk_widget_show (wizard->priv->finish_cancel_button);
	gtk_widget_show (wizard->priv->finish_running_box);

	e_spinner_start (E_SPINNER (wizard->priv->finish_spinner));

	wizard->priv->finish_cancellable = camel_operation_new ();

	g_signal_connect (wizard->priv->finish_cancellable, "status",
		G_CALLBACK (collection_account_wizard_update_status_cb), wizard->priv->finish_label);

	simple_result = e_simple_async_result_new (G_OBJECT (wizard),
		collection_account_wizard_write_changes_done, NULL,
		collection_account_wizard_write_changes_done);

	e_simple_async_result_run_in_thread (simple_result, G_PRIORITY_HIGH_IDLE,
		collection_account_wizard_write_changes_thread, wizard->priv->finish_cancellable);

	g_object_unref (simple_result);

	g_object_notify (G_OBJECT (wizard), "can-run");
}

static void
collection_account_wizard_finish_cancel_clicked_cb (GtkButton *button,
						    gpointer user_data)
{
	ECollectionAccountWizard *wizard = user_data;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	if (wizard->priv->finish_cancellable)
		g_cancellable_cancel (wizard->priv->finish_cancellable);
}

static void
collection_account_wizard_email_entry_changed (ECollectionAccountWizard *wizard,
					       GtkWidget *entry)
{

	collection_account_wizard_notify_can_run (G_OBJECT (wizard));
	collection_account_wizard_mark_changed (wizard);

	collection_account_wizard_update_entry_hint (entry);
}

static void
collection_account_wizard_set_registry (ECollectionAccountWizard *wizard,
					ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (wizard->priv->registry == NULL);

	wizard->priv->registry = g_object_ref (registry);
}

static void
collection_account_wizard_set_property (GObject *object,
					guint property_id,
					const GValue *value,
					GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			collection_account_wizard_set_registry (
				E_COLLECTION_ACCOUNT_WIZARD (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
collection_account_wizard_get_property (GObject *object,
					guint property_id,
					GValue *value,
					GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_RUN:
			g_value_set_boolean (
				value,
				e_collection_account_wizard_get_can_run (
				E_COLLECTION_ACCOUNT_WIZARD (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value,
				collection_account_wizard_get_changed (
				E_COLLECTION_ACCOUNT_WIZARD (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_collection_account_wizard_get_registry (
				E_COLLECTION_ACCOUNT_WIZARD (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
collection_account_wizard_constructed (GObject *object)
{
	ECollectionAccountWizard *wizard = E_COLLECTION_ACCOUNT_WIZARD (object);
	GtkBox *hbox, *vbox;
	GtkGrid *grid;
	GtkWidget *label, *widget, *expander, *scrolled_window;
	GtkTreeStore *tree_store;
	GSList *workers, *link;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;
	gchar *markup;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_collection_account_wizard_parent_class)->constructed (object);

	g_object_set (object,
		"show-border", FALSE,
		"show-tabs", FALSE,
		NULL);

	wizard->priv->config_lookup = e_config_lookup_new (wizard->priv->registry);

	g_signal_connect_swapped (wizard->priv->config_lookup, "get-source",
		G_CALLBACK (collection_account_wizard_get_source_cb), wizard);

	g_signal_connect (wizard->priv->config_lookup, "worker-started",
		G_CALLBACK (collection_account_wizard_worker_started_cb), wizard);

	g_signal_connect (wizard->priv->config_lookup, "worker-finished",
		G_CALLBACK (collection_account_wizard_worker_finished_cb), wizard);

	/* Lookup page */

	vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
	g_object_set (G_OBJECT (vbox),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		NULL);

	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (G_OBJECT (grid),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		"border-width", 12,
		"row-spacing", 6,
		"column-spacing", 6,
		NULL);

	widget = gtk_frame_new (_("User details"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_CENTER,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		NULL);

	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (grid));

	gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic (_("_Email Address or User name:"));
	g_object_set (G_OBJECT (label),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", TRUE,
		NULL);

	widget = gtk_entry_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", TRUE,
		"activates-default", TRUE,
		NULL);
	wizard->priv->email_entry = widget;

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

	gtk_grid_attach (grid, label, 0, 0, 1, 1);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);

	g_signal_connect_swapped (wizard->priv->email_entry, "changed",
		G_CALLBACK (collection_account_wizard_email_entry_changed), wizard);

	expander = gtk_expander_new_with_mnemonic (_("_Advanced Options"));
	gtk_widget_show (expander);
	wizard->priv->advanced_expander = expander;
	gtk_grid_attach (grid, expander, 0, 1, 2, 1);

	label = gtk_label_new_with_mnemonic (_("_Server:"));
	g_object_set (G_OBJECT (label),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", FALSE,
		NULL);

	widget = gtk_entry_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", FALSE,
		"activates-default", TRUE,
		NULL);
	wizard->priv->servers_entry = widget;
	gtk_widget_set_tooltip_text (widget, _("Semicolon (“;”) separated list of servers to look up information for, in addition to the domain of the e-mail address."));

	g_signal_connect_swapped (wizard->priv->servers_entry, "changed",
		G_CALLBACK (collection_account_wizard_notify_can_run), wizard);

	g_signal_connect_swapped (wizard->priv->servers_entry, "changed",
		G_CALLBACK (collection_account_wizard_mark_changed), wizard);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

	gtk_grid_attach (grid, label, 0, 2, 1, 1);
	gtk_grid_attach (grid, widget, 1, 2, 1, 1);

	e_binding_bind_property (expander, "expanded", label, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
	e_binding_bind_property (expander, "expanded", widget, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

	scrolled_window = widget;

	label = gtk_label_new ("");
	g_object_set (G_OBJECT (label),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"margin-start", 12,
		"margin-top", 12,
		"visible", TRUE,
		"max-width-chars", 120,
		"wrap", TRUE,
		NULL);

	gtk_box_pack_start (vbox, label, FALSE, FALSE, 0);

	wizard->priv->results_label = label;

	gtk_notebook_append_page (GTK_NOTEBOOK (wizard), GTK_WIDGET (vbox), NULL);

	vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 4));
	g_object_set (G_OBJECT (vbox),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		NULL);

	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (vbox));

	workers = e_config_lookup_dup_registered_workers (wizard->priv->config_lookup);

	for (link = workers; link; link = g_slist_next (link)) {
		EConfigLookupWorker *worker = link->data;
		WorkerData *wd;

		if (!worker)
			continue;

		wd = g_new0 (WorkerData, 1);
		wd->remember_password = TRUE;

		widget = gtk_check_button_new_with_label (e_config_lookup_worker_get_display_name (worker));
		g_object_set (G_OBJECT (widget),
			"hexpand", TRUE,
			"halign", GTK_ALIGN_FILL,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"margin-start", 12,
			"visible", TRUE,
			"active", TRUE,
			NULL);
		wd->enabled_check = widget;

		g_signal_connect_swapped (wd->enabled_check, "toggled",
			G_CALLBACK (collection_account_wizard_notify_can_run), wizard);

		g_signal_connect_swapped (wd->enabled_check, "toggled",
			G_CALLBACK (collection_account_wizard_mark_changed), wizard);

		gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

		widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
		g_object_set (G_OBJECT (widget),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_START,
			"margin-start", 12,
			"visible", TRUE,
			NULL);
		gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

		hbox = GTK_BOX (widget);

		/* spacer */
		widget = gtk_label_new ("");
		g_object_set (G_OBJECT (widget),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_START,
			"margin-start", 12,
			"visible", TRUE,
			NULL);
		gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

		widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
		g_object_set (G_OBJECT (widget),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_START,
			"margin-start", 12,
			"visible", FALSE,
			NULL);
		wd->running_box = widget;

		gtk_box_pack_start (hbox, widget, TRUE, TRUE, 0);

		hbox = GTK_BOX (widget);

		widget = e_spinner_new ();
		g_object_set (G_OBJECT (widget),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"visible", TRUE,
			NULL);
		wd->spinner = widget;

		gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

		label = gtk_label_new (NULL);
		g_object_set (G_OBJECT (label),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"visible", TRUE,
			"ellipsize", PANGO_ELLIPSIZE_END,
			"selectable", TRUE,
			NULL);
		wd->running_label = label;

		gtk_box_pack_start (hbox, label, FALSE, FALSE, 0);

		g_signal_connect (wd->running_label, "activate-link",
			G_CALLBACK (collection_account_wizard_activate_link_cb), wizard);

		e_binding_bind_property (wd->enabled_check, "sensitive", wd->running_label, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

		widget = e_dialog_button_new_with_icon ("process-stop", _("_Cancel"));
		g_object_set (G_OBJECT (widget),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"visible", TRUE,
			NULL);
		wd->cancel_button = widget;

		g_signal_connect (wd->cancel_button, "clicked",
			G_CALLBACK (collection_account_wizard_worker_cancel_clicked_cb), wd);

		gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

		g_hash_table_insert (wizard->priv->workers, g_object_ref (worker), wd);
	}

	g_slist_free_full (workers, g_object_unref);

	e_signal_connect_notify_swapped (
		gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)), "notify::upper",
		G_CALLBACK (e_util_ensure_scrolled_window_height), scrolled_window);

	g_signal_connect (scrolled_window, "map", G_CALLBACK (e_util_ensure_scrolled_window_height), NULL);

	/* Parts page */

	vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
	g_object_set (G_OBJECT (vbox),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		"margin-bottom", 12,
		NULL);

	label = gtk_label_new (_("Select which parts should be configured:"));
	g_object_set (G_OBJECT (label),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", TRUE,
		NULL);

	gtk_box_pack_start (vbox, label, FALSE, FALSE, 0);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		"margin-bottom", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	scrolled_window = widget;

	gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

	tree_store = gtk_tree_store_new (PART_N_COLUMNS,
		G_TYPE_BOOLEAN,	/* PART_COLUMN_BOOL_ENABLED */
		G_TYPE_BOOLEAN,	/* PART_COLUMN_BOOL_ENABLED_VISIBLE */
		G_TYPE_BOOLEAN,	/* PART_COLUMN_BOOL_RADIO */
		G_TYPE_BOOLEAN,	/* PART_COLUMN_BOOL_SENSITIVE */
		G_TYPE_BOOLEAN,	/* PART_COLUMN_BOOL_IS_COLLECTION_GROUP */
		G_TYPE_BOOLEAN,	/* PART_COLUMN_BOOL_ICON_VISIBLE */
		G_TYPE_STRING,	/* PART_COLUMN_STRING_ICON_NAME */
		G_TYPE_STRING,	/* PART_COLUMN_STRING_DESCRIPTION */
		G_TYPE_STRING,	/* PART_COLUMN_STRING_PROTOCOL */
		E_TYPE_CONFIG_LOOKUP_RESULT); /* PART_COLUMN_OBJECT_RESULT */

	widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (tree_store));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		NULL);
	g_object_unref (tree_store);
	wizard->priv->parts_tree_view = GTK_TREE_VIEW (widget);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	gtk_notebook_append_page (GTK_NOTEBOOK (wizard), GTK_WIDGET (vbox), NULL);

	/* Column: Description */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Description"));

	cell_renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	g_signal_connect (cell_renderer, "toggled",
		G_CALLBACK (collection_account_wizard_part_enabled_toggled_cb), wizard);

	gtk_tree_view_column_set_attributes (column, cell_renderer,
		"sensitive", PART_COLUMN_BOOL_SENSITIVE,
		"active", PART_COLUMN_BOOL_ENABLED,
		"visible", PART_COLUMN_BOOL_ENABLED_VISIBLE,
		"radio", PART_COLUMN_BOOL_RADIO,
		NULL);

	cell_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell_renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_set_attributes (column, cell_renderer,
		"sensitive", PART_COLUMN_BOOL_SENSITIVE,
		"icon-name", PART_COLUMN_STRING_ICON_NAME,
		"visible", PART_COLUMN_BOOL_ICON_VISIBLE,
		NULL);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_set_attributes (column, cell_renderer,
		"sensitive", PART_COLUMN_BOOL_SENSITIVE,
		"markup", PART_COLUMN_STRING_DESCRIPTION,
		NULL);

	gtk_tree_view_append_column (wizard->priv->parts_tree_view, column);
	gtk_tree_view_set_expander_column (wizard->priv->parts_tree_view, column);

	/* Column: Type */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Type"));

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	gtk_tree_view_column_set_attributes (column, cell_renderer,
		"sensitive", PART_COLUMN_BOOL_SENSITIVE,
		"text", PART_COLUMN_STRING_PROTOCOL,
		NULL);

	gtk_tree_view_append_column (wizard->priv->parts_tree_view, column);

	/* Finish page */

	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (G_OBJECT (grid),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		"column-spacing", 4,
		NULL);
	markup = g_markup_printf_escaped ("<b>%s</b>", _("Account Information"));
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_widget_set_margin_bottom (widget, 12);
	gtk_grid_attach (grid, widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	label = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_widget_set_margin_start (label, 12);
	gtk_label_set_xalign (GTK_LABEL (label), 1.0);
	gtk_grid_attach (grid, label, 0, 1, 1, 1);
	gtk_widget_show (label);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	wizard->priv->display_name_entry = widget;

	g_signal_connect_swapped (wizard->priv->display_name_entry, "changed",
		G_CALLBACK (collection_account_wizard_notify_can_run), wizard);

	widget = gtk_label_new ("The above name will be used to identify this account.\nUse for example, “Work” or “Personal”.");
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (grid, widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
	g_object_set (G_OBJECT (vbox),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		NULL);

	gtk_box_pack_end (vbox, GTK_WIDGET (grid), FALSE, FALSE, 0);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-start", 12,
		"margin-top", 24,
		"visible", FALSE,
		NULL);
	wizard->priv->finish_running_box = widget;

	gtk_grid_attach (grid, widget, 0, 3, 2, 1);

	hbox = GTK_BOX (widget);

	widget = e_spinner_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", TRUE,
		NULL);
	wizard->priv->finish_spinner = widget;

	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

	label = gtk_label_new (NULL);
	g_object_set (G_OBJECT (label),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", TRUE,
		"max-width-chars", 120,
		"wrap", TRUE,
		NULL);
	wizard->priv->finish_label = label;

	gtk_box_pack_start (hbox, label, FALSE, FALSE, 0);

	widget = e_dialog_button_new_with_icon ("process-stop", _("_Cancel"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", TRUE,
		NULL);
	wizard->priv->finish_cancel_button = widget;

	g_signal_connect (wizard->priv->finish_cancel_button, "clicked",
		G_CALLBACK (collection_account_wizard_finish_cancel_clicked_cb), wizard);

	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (wizard), GTK_WIDGET (vbox), NULL);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard), 0);
}

static void
collection_account_wizard_dispose (GObject *object)
{
	ECollectionAccountWizard *wizard = E_COLLECTION_ACCOUNT_WIZARD (object);
	gint ii;

	g_cancellable_cancel (wizard->priv->finish_cancellable);

	g_clear_object (&wizard->priv->registry);
	g_clear_object (&wizard->priv->config_lookup);
	g_clear_object (&wizard->priv->finish_cancellable);
	g_clear_pointer (&wizard->priv->workers, g_hash_table_destroy);
	g_clear_pointer (&wizard->priv->store_passwords, g_hash_table_destroy);
	g_clear_pointer (&wizard->priv->running_result, e_simple_async_result_complete_idle_take);

	wizard->priv->email_entry = NULL;
	wizard->priv->advanced_expander = NULL;
	wizard->priv->servers_entry = NULL;
	wizard->priv->results_label = NULL;
	wizard->priv->parts_tree_view = NULL;
	wizard->priv->display_name_entry = NULL;
	wizard->priv->finish_running_box = NULL;
	wizard->priv->finish_spinner = NULL;
	wizard->priv->finish_label = NULL;
	wizard->priv->finish_cancel_button = NULL;

	for (ii = 0; ii <= E_CONFIG_LOOKUP_RESULT_LAST_KIND; ii++) {
		g_clear_object (&wizard->priv->sources[ii]);
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_collection_account_wizard_parent_class)->dispose (object);
}

static void
e_collection_account_wizard_class_init (ECollectionAccountWizardClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = collection_account_wizard_set_property;
	object_class->get_property = collection_account_wizard_get_property;
	object_class->constructed = collection_account_wizard_constructed;
	object_class->dispose = collection_account_wizard_dispose;

	/**
	 * ECollectionAccountWizard:registry:
	 *
	 * The #ESourceRegistry manages #ESource instances.
	 *
	 * Since: 3.28
	 **/
	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * ECollectionAccountWizard:can-run:
	 *
	 * Whether can call e_collection_account_wizard_run().
	 * See e_collection_account_wizard_get_can_run() for more information.
	 *
	 * Since: 3.28
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CAN_RUN,
		g_param_spec_boolean (
			"can-run",
			"Can Run",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * ECollectionAccountWizard:changed:
	 *
	 * Whether the settings of the wizard changed. When it did,
	 * a lookup will be run instead of moving to the next step.
	 *
	 * Since: 3.34
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			"Whether changed",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * ECollectionAccountWizard::done:
	 * @uid: an #ESource UID which had been created
	 *
	 * Emitted to notify about the wizard being done.
	 *
	 * Since: 3.28
	 **/
	signals[DONE] = g_signal_new (
		"done",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ECollectionAccountWizardClass, done),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
e_collection_account_wizard_init (ECollectionAccountWizard *wizard)
{
	gint ii;

	wizard->priv = e_collection_account_wizard_get_instance_private (wizard);
	wizard->priv->workers = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, worker_data_free);
	wizard->priv->store_passwords = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	wizard->priv->running_workers = 0;

	for (ii = 0; ii <= E_CONFIG_LOOKUP_RESULT_LAST_KIND; ii++) {
		wizard->priv->sources[ii] = NULL;
	}
}

/**
 * e_collection_account_wizard_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #ECollectionAccountWizard instance.
 *
 * Returns: (transfer full): a new #ECollectionAccountWizard
 *
 * Since: 3.28
 **/
GtkWidget *
e_collection_account_wizard_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (E_TYPE_COLLECTION_ACCOUNT_WIZARD,
		"registry", registry,
		NULL);
}

/**
 * e_collection_account_wizard_new_window:
 * @parent: (nullable): an optional #GtkWindow parent of the new window
 * @registry: an #ESourceRegistry
 *
 * Creates a new #ECollectionAccountWizard instance as part of a #GtkWindow.
 * This window takes care of all the #ECollectionAccountWizard functionality.
 *
 * Returns: (transfer full): a new #GtkWindow containing an #ECollectionAccountWizard
 *
 * Since: 3.32
 **/
GtkWindow *
e_collection_account_wizard_new_window (GtkWindow *parent,
					ESourceRegistry *registry)
{
	GtkWidget *wizard;

	if (parent)
		g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	wizard = e_collection_account_wizard_new (registry);
	g_return_val_if_fail (wizard != NULL, NULL);

	return collection_account_wizard_create_window (parent, wizard);
}

/**
 * e_collection_account_wizard_get_registry:
 * @wizard: an #ECollectionAccountWizard
 *
 * Returns the #ESourceRegistry passed to e_collection_account_wizard_new().
 *
 * Returns: (transfer none): an #ESourceRegistry
 *
 * Since: 3.28
 **/
ESourceRegistry *
e_collection_account_wizard_get_registry (ECollectionAccountWizard *wizard)
{
	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), NULL);

	return wizard->priv->registry;
}

/**
 * e_collection_account_wizard_get_can_run:
 * @wizard: an #ECollectionAccountWizard
 *
 * Returns whether e_collection_account_wizard_run() can be called, that is,
 * whether at least one worker is enabled to run and the @wizard is not
 * running.
 *
 * Returns: whether e_collection_account_wizard_run() can be called.
 *
 * Since: 3.28
 **/
gboolean
e_collection_account_wizard_get_can_run (ECollectionAccountWizard *wizard)
{
	GHashTableIter iter;
	gpointer value;
	const gchar *email, *servers;
	gint current_page;

	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), FALSE);

	if (wizard->priv->running_workers ||
	    wizard->priv->running_result ||
	    wizard->priv->finish_cancellable)
		return FALSE;

	email = gtk_entry_get_text (GTK_ENTRY (wizard->priv->email_entry));
	servers = gtk_entry_get_text (GTK_ENTRY (wizard->priv->servers_entry));

	if ((!email || !*email) && (!servers || !*servers))
		return FALSE;

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard));

	if (current_page == 1) { /* Parts page */
		GtkTreeModel *model;
		GtkTreeIter titer;

		model = gtk_tree_view_get_model (wizard->priv->parts_tree_view);
		if (gtk_tree_model_get_iter_first (model, &titer)) {
			do {
				gboolean enabled = FALSE, is_collection_group = FALSE;

				gtk_tree_model_get (model, &titer,
					PART_COLUMN_BOOL_ENABLED, &enabled,
					PART_COLUMN_BOOL_IS_COLLECTION_GROUP, &is_collection_group,
					-1);

				if (enabled && is_collection_group) {
					/* Collection is not with radio, verify at least one child is selected */
					GtkTreeIter child;

					if (gtk_tree_model_iter_nth_child (model, &child, &titer, 0)) {
						do {
							enabled = FALSE;

							gtk_tree_model_get (model, &child,
								PART_COLUMN_BOOL_ENABLED, &enabled,
								-1);

							if (enabled)
								return TRUE;
						} while (gtk_tree_model_iter_next (model, &child));
					}
				} else if (enabled) {
					return TRUE;
				}
			} while (gtk_tree_model_iter_next (model, &titer));
		}

		return FALSE;
	} else if (current_page == 2) { /* Finish page */
		gchar *display_name;
		gboolean can_run;

		display_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (wizard->priv->display_name_entry)));
		if (!display_name)
			return FALSE;

		g_strstrip (display_name);

		can_run = display_name && *display_name;

		g_free (display_name);

		return can_run;
	}

	/* Look up page */

	g_hash_table_iter_init (&iter, wizard->priv->workers);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		WorkerData *wd = value;

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wd->enabled_check)))
			return TRUE;
	}

	return FALSE;
}

/**
 * e_collection_account_wizard_reset:
 * @wizard: an #ECollectionAccountWizard
 *
 * Resets content of the @wizard to the initial state. This might be called
 * whenever the widget is going to be shown.
 *
 * Since: 3.28
 **/
void
e_collection_account_wizard_reset (ECollectionAccountWizard *wizard)
{
	GtkTreeModel *model;
	GHashTableIter iter;
	gpointer value;
	gint ii;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	e_collection_account_wizard_abort (wizard);

	g_hash_table_iter_init (&iter, wizard->priv->workers);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		WorkerData *wd = value;

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wd->enabled_check), TRUE);
		gtk_widget_hide (wd->running_box);
		e_named_parameters_free (wd->restart_params);
		wd->restart_params = NULL;
		wd->remember_password = TRUE;
	}

	gtk_entry_set_text (GTK_ENTRY (wizard->priv->email_entry), "");
	gtk_entry_set_text (GTK_ENTRY (wizard->priv->servers_entry), "");
	gtk_label_set_text (GTK_LABEL (wizard->priv->results_label), "");
	gtk_entry_set_text (GTK_ENTRY (wizard->priv->display_name_entry), "");
	gtk_expander_set_expanded (GTK_EXPANDER (wizard->priv->advanced_expander), FALSE);
	e_config_lookup_clear_results (wizard->priv->config_lookup);

	model = gtk_tree_view_get_model (wizard->priv->parts_tree_view);
	gtk_tree_store_clear (GTK_TREE_STORE (model));

	collection_account_wizard_set_changed (wizard, FALSE);

	for (ii = 0; ii <= E_CONFIG_LOOKUP_RESULT_LAST_KIND; ii++) {
		g_clear_object (&wizard->priv->sources[ii]);
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard), 0);

	g_object_notify (G_OBJECT (wizard), "can-run");
}

/**
 * e_collection_account_wizard_next:
 * @wizard: an #ECollectionAccountWizard
 *
 * Instructs the @wizard to advance to the next step. It does nothing
 * when there is an ongoing lookup or when the current page cannot
 * be advanced.
 *
 * This can influence e_collection_account_wizard_is_finish_page().
 *
 * Returns: %TRUE, when the step had been changed, %FALSE otherwise.
 *   Note that when this is called on a finish page, then the %TRUE
 *   means that the @wizard finished all its settings and should be
 *   closed now.
 *
 * Since: 3.28
 **/
gboolean
e_collection_account_wizard_next (ECollectionAccountWizard *wizard)
{
	gboolean changed = FALSE;
	const gchar *text;
	gint ii;

	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), FALSE);

	switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard))) {
	case 0: /* Lookup page */
		if (wizard->priv->changed ||
		    !e_config_lookup_count_results (wizard->priv->config_lookup)) {
			for (ii = 0; ii <= E_CONFIG_LOOKUP_RESULT_LAST_KIND; ii++) {
				g_clear_object (&wizard->priv->sources[ii]);
			}

			wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION] = e_source_new (NULL, NULL, NULL);
			e_source_get_extension (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION], E_SOURCE_EXTENSION_COLLECTION);

			e_collection_account_wizard_run (wizard, NULL, NULL);
			changed = TRUE;
		} else if (collection_account_wizard_fill_results (wizard)) {
			gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard), 1);
			changed = TRUE;
		}
		break;
	case 1: /* Parts page */
		g_warn_if_fail (wizard->priv->sources[E_CONFIG_LOOKUP_RESULT_COLLECTION] != NULL);

		text = gtk_entry_get_text (GTK_ENTRY (wizard->priv->display_name_entry));
		if (!text || !*text) {
			gchar *tmp = NULL;

			text = gtk_entry_get_text (GTK_ENTRY (wizard->priv->email_entry));

			if (!text || !*text) {
				text = gtk_entry_get_text (GTK_ENTRY (wizard->priv->servers_entry));

				if (text && *text) {
					gchar *ptr;

					if (g_ascii_strncasecmp (text, "http://", 7) == 0)
						text += 7;
					else if (g_ascii_strncasecmp (text, "https://", 8) == 0)
						text += 8;

					/* get the first entered server name */
					ptr = strchr (text, ';');
					tmp = ptr ? g_strndup (text, ptr - text) : g_strdup (text);

					/* eventually skip the path */
					ptr = strchr (tmp, '/');
					if (ptr)
						*ptr = '\0';

					text = tmp;
				}
			}

			if (text && *text)
				gtk_entry_set_text (GTK_ENTRY (wizard->priv->display_name_entry), text);

			g_free (tmp);
		}

		gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard), 2);
		changed = TRUE;
		break;
	case 2: /* Finish page */
		/* It's an asynchronous operation, just fail or run it */
		collection_account_wizard_save_sources (wizard);
		changed = FALSE;
		break;
	}

	/* To update sensitivity of the "Next"/"Finish" button */
	if (changed)
		g_object_notify (G_OBJECT (wizard), "can-run");

	return changed;
}

/**
 * e_collection_account_wizard_prev:
 * @wizard: an #ECollectionAccountWizard
 *
 * Instructs the @wizard to go back to the previous step.
 *
 * This can influence e_collection_account_wizard_is_finish_page().
 *
 * Returns: %TRUE, when the step had been changed, %FALSE otherwise.
 *
 * Since: 3.28
 **/
gboolean
e_collection_account_wizard_prev (ECollectionAccountWizard *wizard)
{
	gint current_page;

	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), FALSE);

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard));

	if (current_page < 1)
		return FALSE;

	gtk_notebook_set_current_page (GTK_NOTEBOOK (wizard), current_page - 1);

	/* To update sensitivity of the "Next"/"Finish" button */
	g_object_notify (G_OBJECT (wizard), "can-run");

	return TRUE;
}

/**
 * e_collection_account_wizard_is_finish_page:
 * @wizard: an #ECollectionAccountWizard
 *
 * Returns: whether the @wizard is at the last page.
 *
 * Since: 3.28
 **/
gboolean
e_collection_account_wizard_is_finish_page (ECollectionAccountWizard *wizard)
{
	g_return_val_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard), FALSE);

	return gtk_notebook_get_current_page (GTK_NOTEBOOK (wizard)) == gtk_notebook_get_n_pages (GTK_NOTEBOOK (wizard)) - 1;
}

/**
 * e_collection_account_wizard_run:
 * @wizard: an #ECollectionAccountWizard
 * @callback: a callback to call, when the run is finished
 * @user_data: user data for the @callback
 *
 * Runs lookup for all enabled lookup workers. Finish the call
 * with e_collection_account_wizard_run_finish() from the @callback.
 *
 * This function can be called only if e_collection_account_wizard_get_can_run()
 * returns %TRUE.
 *
 * Since: 3.28
 **/
void
e_collection_account_wizard_run (ECollectionAccountWizard *wizard,
				 GAsyncReadyCallback callback,
				 gpointer user_data)
{
	GHashTableIter iter;
	gpointer key, value;
	gboolean any_worker = FALSE;

	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));
	g_return_if_fail (e_collection_account_wizard_get_can_run (wizard));

	e_config_lookup_clear_results (wizard->priv->config_lookup);

	wizard->priv->running_result = e_simple_async_result_new (G_OBJECT (wizard), callback, user_data, e_collection_account_wizard_run);

	g_hash_table_iter_init (&iter, wizard->priv->workers);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		EConfigLookupWorker *worker = key;
		WorkerData *wd = value;

		if (worker && wd &&
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wd->enabled_check))) {
			ENamedParameters *params;

			params = e_named_parameters_new_clone (wd->restart_params);
			if (!params)
				params = e_named_parameters_new ();
			e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS, gtk_entry_get_text (GTK_ENTRY (wizard->priv->email_entry)));
			e_named_parameters_set (params, E_CONFIG_LOOKUP_PARAM_SERVERS, gtk_entry_get_text (GTK_ENTRY (wizard->priv->servers_entry)));

			any_worker = TRUE;
			e_config_lookup_run_worker (wizard->priv->config_lookup, worker, params, NULL);

			e_named_parameters_free (params);
		}
	}

	if (!any_worker) {
		e_simple_async_result_complete_idle_take (wizard->priv->running_result);
		wizard->priv->running_result = NULL;
	}
}

/**
 * e_collection_account_wizard_run_finish:
 * @wizard: an #ECollectionAccountWizard
 * @result: result of the operation
 *
 * Finishes the wizard run issued by e_collection_account_wizard_run().
 * It doesn't return anything, because everything is handled within
 * the @wizard, thus it is provided mainly for consistency with asynchronous API.
 *
 * Since: 3.28
 **/
void
e_collection_account_wizard_run_finish (ECollectionAccountWizard *wizard,
					GAsyncResult *result)
{
	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	g_return_if_fail (g_async_result_is_tagged (result, e_collection_account_wizard_run));
}

/**
 * e_collection_account_wizard_abort:
 * @wizard: an #ECollectionAccountWizard
 *
 * Aborts any ongoing operation the @wizard may run. If there is nothing
 * running, then does nothing.
 *
 * Since: 3.28
 **/
void
e_collection_account_wizard_abort (ECollectionAccountWizard *wizard)
{
	g_return_if_fail (E_IS_COLLECTION_ACCOUNT_WIZARD (wizard));

	e_config_lookup_cancel_all (wizard->priv->config_lookup);

	if (wizard->priv->finish_cancellable)
		g_cancellable_cancel (wizard->priv->finish_cancellable);
}
