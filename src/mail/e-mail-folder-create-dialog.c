/*
 * e-mail-folder-create-dialog.c
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

#include <glib/gi18n-lib.h>

#include "em-vfolder-editor-rule.h"
#include "mail-vfolder-ui.h"

#include "e-mail-folder-create-dialog.h"

typedef struct _AsyncContext AsyncContext;

struct _EMailFolderCreateDialogPrivate {
	EMailUISession *session;
	GtkWidget *name_entry;
};

struct _AsyncContext {
	EMailFolderCreateDialog *dialog;
	EActivity *activity;
};

enum {
	PROP_0,
	PROP_SESSION
};

enum {
	FOLDER_CREATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailFolderCreateDialog, e_mail_folder_create_dialog, EM_TYPE_FOLDER_SELECTOR)

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->dialog);
	g_clear_object (&async_context->activity);

	g_slice_free (AsyncContext, async_context);
}

static void
mail_folder_create_dialog_create_folder_cb (GObject *source_object,
                                            GAsyncResult *result,
                                            gpointer user_data)
{
	EMailFolderCreateDialog *dialog;
	EActivity *activity;
	EAlertSink *alert_sink;
	GdkWindow *gdk_window;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	dialog = async_context->dialog;
	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	/* Set the cursor back to normal. */
	gdk_window = gtk_widget_get_window (GTK_WIDGET (dialog));
	gdk_window_set_cursor (gdk_window, NULL);

	e_mail_store_create_folder_finish (
		CAMEL_STORE (source_object), result, &local_error);

	/* Ignore cancellations. */
	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"system:simple-error",
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}

	async_context_free (async_context);
}

static void
mail_folder_create_dialog_create_folder (EMailFolderCreateDialog *dialog)
{
	CamelStore *store = NULL;
	gchar *create_folder_name;
	gchar *parent_folder_name = NULL;
	const gchar *text;

	em_folder_selector_get_selected (
		EM_FOLDER_SELECTOR (dialog), &store, &parent_folder_name);

	g_return_if_fail (store != NULL);

	text = gtk_entry_get_text (GTK_ENTRY (dialog->priv->name_entry));

	if (parent_folder_name != NULL) {
		create_folder_name = g_strconcat (
			parent_folder_name, "/", text, NULL);
	} else {
		create_folder_name = g_strdup (text);
	}

	/* For the vfolder store, we just open the editor window. */
	if (CAMEL_IS_VEE_STORE (store)) {
		EMailUISession *session;
		EFilterRule *rule;

		session = e_mail_folder_create_dialog_get_session (dialog);
		rule = em_vfolder_editor_rule_new (E_MAIL_SESSION (session));
		e_filter_rule_set_name (rule, create_folder_name);
		vfolder_gui_add_rule (EM_VFOLDER_RULE (rule));

		gtk_widget_destroy (GTK_WIDGET (dialog));

	} else {
		AsyncContext *async_context;
		EActivity *activity;
		GCancellable *cancellable;
		GdkCursor *gdk_cursor;

		/* Make the cursor appear busy. */
		gdk_cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (dialog)), "wait");
		if (gdk_cursor) {
			GdkWindow *gdk_window;

			gdk_window = gtk_widget_get_window (GTK_WIDGET (dialog));
			gdk_window_set_cursor (gdk_window, gdk_cursor);
			g_object_unref (gdk_cursor);
		}

		activity = em_folder_selector_new_activity (
			EM_FOLDER_SELECTOR (dialog));

		async_context = g_slice_new0 (AsyncContext);
		async_context->dialog = g_object_ref (dialog);
		async_context->activity = g_object_ref (activity);

		cancellable = e_activity_get_cancellable (activity);

		e_mail_store_create_folder (
			store, create_folder_name,
			G_PRIORITY_DEFAULT, cancellable,
			mail_folder_create_dialog_create_folder_cb,
			async_context);

		g_object_unref (activity);
	}

	g_free (create_folder_name);
	g_free (parent_folder_name);

	g_object_unref (store);
}

static gboolean
mail_folder_create_dialog_inputs_are_valid (EMailFolderCreateDialog *dialog)
{
	GtkEntry *entry;
	const gchar *original_text;
	gchar *stripped_text;
	gboolean folder_or_store_is_selected;
	gboolean inputs_are_valid;

	entry = GTK_ENTRY (dialog->priv->name_entry);

	original_text = gtk_entry_get_text (entry);
	stripped_text = e_util_strdup_strip (original_text);

	folder_or_store_is_selected =
		em_folder_selector_get_selected (
		EM_FOLDER_SELECTOR (dialog), NULL, NULL);

	inputs_are_valid =
		folder_or_store_is_selected &&
		(stripped_text != NULL) &&
		(strchr (stripped_text, '/') == NULL);

	g_free (stripped_text);

	return inputs_are_valid;
}

static void
mail_folder_create_dialog_entry_activate_cb (GtkEntry *entry,
                                             EMailFolderCreateDialog *dialog)
{
	if (mail_folder_create_dialog_inputs_are_valid (dialog))
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
mail_folder_create_dialog_entry_changed_cb (GtkEntry *entry,
                                            EMailFolderCreateDialog *dialog)
{
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK,
		mail_folder_create_dialog_inputs_are_valid (dialog));
}

static void
mail_folder_create_dialog_set_session (EMailFolderCreateDialog *dialog,
                                       EMailUISession *session)
{
	g_return_if_fail (E_IS_MAIL_UI_SESSION (session));
	g_return_if_fail (dialog->priv->session == NULL);

	dialog->priv->session = g_object_ref (session);
}

static void
mail_folder_create_dialog_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			mail_folder_create_dialog_set_session (
				E_MAIL_FOLDER_CREATE_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_folder_create_dialog_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_folder_create_dialog_get_session (
				E_MAIL_FOLDER_CREATE_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_folder_create_dialog_dispose (GObject *object)
{
	EMailFolderCreateDialog *self = E_MAIL_FOLDER_CREATE_DIALOG (object);

	g_clear_object (&self->priv->session);
	g_clear_object (&self->priv->name_entry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_folder_create_dialog_parent_class)->dispose (object);
}

static void
mail_folder_create_dialog_constructed (GObject *object)
{
	EMailFolderCreateDialog *dialog;
	EMailAccountStore *account_store;
	EMailUISession *session;
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	GQueue queue = G_QUEUE_INIT;
	GtkWidget *container;
	GtkWidget *widget;
	GtkLabel *label;

	dialog = E_MAIL_FOLDER_CREATE_DIALOG (object);
	session = e_mail_folder_create_dialog_get_session (dialog);
	model = em_folder_selector_get_model (EM_FOLDER_SELECTOR (dialog));

	/* Populate the tree model before chaining up, since the
	 * subclass will immediately try to restore the tree state. */

	account_store = e_mail_ui_session_get_account_store (session);
	e_mail_account_store_queue_enabled_services (account_store, &queue);

	while (!g_queue_is_empty (&queue)) {
		CamelService *service;
		CamelStoreFlags flags;

		service = g_queue_pop_head (&queue);
		g_warn_if_fail (CAMEL_IS_STORE (service));

		flags = camel_store_get_flags (CAMEL_STORE (service));
		if ((flags & CAMEL_STORE_CAN_EDIT_FOLDERS) == 0)
			continue;

		em_folder_tree_model_add_store (model, CAMEL_STORE (service));
	}

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_folder_create_dialog_parent_class)->constructed (object);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Create Folder"));

	em_folder_selector_set_caption (
		EM_FOLDER_SELECTOR (dialog),
		_("Specify where to create the folder:"));

	em_folder_selector_set_default_button_label (
		EM_FOLDER_SELECTOR (dialog), _("C_reate"));

	folder_tree = em_folder_selector_get_folder_tree (
		EM_FOLDER_SELECTOR (dialog));
	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOINFERIORS);

	/* Add a folder name entry field to the dialog. */

	container = em_folder_selector_get_content_area (
		EM_FOLDER_SELECTOR (dialog));

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("Folder _name:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	dialog->priv->name_entry = g_object_ref (widget);
	gtk_widget_grab_focus (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "activate",
		G_CALLBACK (mail_folder_create_dialog_entry_activate_cb),
		dialog);

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (mail_folder_create_dialog_entry_changed_cb),
		dialog);
}

static void
mail_folder_create_dialog_response (GtkDialog *dialog,
                                    gint response_id)
{
	/* Do not chain up.  GtkDialog does not implement this method. */

	switch (response_id) {
		case GTK_RESPONSE_OK:
			mail_folder_create_dialog_create_folder (
				E_MAIL_FOLDER_CREATE_DIALOG (dialog));
			break;
		case GTK_RESPONSE_CANCEL:
			gtk_widget_destroy (GTK_WIDGET (dialog));
			break;
		default:
			break;
	}
}

static void
mail_folder_create_dialog_folder_selected (EMFolderSelector *selector,
                                           CamelStore *store,
                                           const gchar *folder_name)
{
	EMailFolderCreateDialog *dialog;

	/* Do not chain up.  This overrides the subclass behavior. */

	dialog = E_MAIL_FOLDER_CREATE_DIALOG (selector);

	/* Can be NULL during dispose, when the folder tree model is being cleared */
	if (dialog->priv->name_entry)
		gtk_dialog_set_response_sensitive (
			GTK_DIALOG (dialog), GTK_RESPONSE_OK,
			mail_folder_create_dialog_inputs_are_valid (dialog));
}

static void
e_mail_folder_create_dialog_class_init (EMailFolderCreateDialogClass *class)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;
	EMFolderSelectorClass *selector_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_folder_create_dialog_set_property;
	object_class->get_property = mail_folder_create_dialog_get_property;
	object_class->dispose = mail_folder_create_dialog_dispose;
	object_class->constructed = mail_folder_create_dialog_constructed;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = mail_folder_create_dialog_response;

	selector_class = EM_FOLDER_SELECTOR_CLASS (class);
	selector_class->folder_selected = mail_folder_create_dialog_folder_selected;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			"Session",
			"An EMailUISession from which "
			"to list enabled mail stores",
			E_TYPE_MAIL_UI_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[FOLDER_CREATED] = g_signal_new (
		"folder-created",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailFolderCreateDialogClass, folder_created),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING);
}

static void
e_mail_folder_create_dialog_init (EMailFolderCreateDialog *dialog)
{
	dialog->priv = e_mail_folder_create_dialog_get_instance_private (dialog);
}

GtkWidget *
e_mail_folder_create_dialog_new (GtkWindow *parent,
                                 EMailUISession *session)
{
	GtkWidget *dialog;
	EMFolderTreeModel *model;

	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), NULL);

	/* XXX The folder tree model is a construct-only property in
	 *     EMFolderSelector, so create an empty model here and then
	 *     populate it during instance initialization.  Already too
	 *     much logic here for a "new" function, but works for now. */

	model = em_folder_tree_model_new ();
	em_folder_tree_model_set_session (model, E_MAIL_SESSION (session));

	dialog = g_object_new (
		E_TYPE_MAIL_FOLDER_CREATE_DIALOG,
		"transient-for", parent,
		"use-header-bar", e_util_get_use_header_bar (),
		"model", model,
		"session", session, NULL);

	g_object_unref (model);

	return dialog;
}

EMailUISession *
e_mail_folder_create_dialog_get_session (EMailFolderCreateDialog *dialog)
{
	g_return_val_if_fail (E_IS_MAIL_FOLDER_CREATE_DIALOG (dialog), NULL);

	return dialog->priv->session;
}

