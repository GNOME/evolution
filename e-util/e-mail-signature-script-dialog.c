/*
 * e-mail-signature-script-dialog.c
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

#include "e-mail-signature-script-dialog.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#define E_MAIL_SIGNATURE_SCRIPT_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SIGNATURE_SCRIPT_DIALOG, \
	EMailSignatureScriptDialogPrivate))

typedef struct _AsyncContext AsyncContext;

struct _EMailSignatureScriptDialogPrivate {
	ESourceRegistry *registry;
	ESource *source;

	GtkWidget *entry;		/* not referenced */
	GtkWidget *file_chooser;	/* not referenced */
	GtkWidget *alert;		/* not referenced */

	gchar *symlink_target;
};

struct _AsyncContext {
	ESource *source;
	GCancellable *cancellable;
	gchar *symlink_target;
};

enum {
	PROP_0,
	PROP_REGISTRY,
	PROP_SOURCE,
	PROP_SYMLINK_TARGET
};

G_DEFINE_TYPE (
	EMailSignatureScriptDialog,
	e_mail_signature_script_dialog,
	GTK_TYPE_DIALOG)

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->source != NULL)
		g_object_unref (async_context->source);

	if (async_context->cancellable != NULL)
		g_object_unref (async_context->cancellable);

	g_free (async_context->symlink_target);

	g_slice_free (AsyncContext, async_context);
}

static gboolean
mail_signature_script_dialog_filter_cb (const GtkFileFilterInfo *filter_info)
{
	return g_file_test (filter_info->filename, G_FILE_TEST_IS_EXECUTABLE);
}

static void
mail_signature_script_dialog_update_status (EMailSignatureScriptDialog *dialog)
{
	ESource *source;
	const gchar *display_name;
	const gchar *symlink_target;
	gboolean show_alert;
	gboolean sensitive;

	source = e_mail_signature_script_dialog_get_source (dialog);

	display_name = e_source_get_display_name (source);
	sensitive = (display_name != NULL && *display_name != '\0');

	symlink_target =
		e_mail_signature_script_dialog_get_symlink_target (dialog);

	if (symlink_target != NULL) {
		gboolean executable;

		executable = g_file_test (
			symlink_target, G_FILE_TEST_IS_EXECUTABLE);

		show_alert = !executable;
		sensitive &= executable;
	} else {
		sensitive = FALSE;
		show_alert = FALSE;
	}

	if (show_alert)
		gtk_widget_show (dialog->priv->alert);
	else
		gtk_widget_hide (dialog->priv->alert);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, sensitive);
}

static void
mail_signature_script_dialog_file_set_cb (GtkFileChooserButton *button,
                                          EMailSignatureScriptDialog *dialog)
{
	ESource *source;
	ESourceMailSignature *extension;
	GtkFileChooser *file_chooser;
	const gchar *extension_name;
	gchar *filename;

	file_chooser = GTK_FILE_CHOOSER (button);
	filename = gtk_file_chooser_get_filename (file_chooser);

	g_free (dialog->priv->symlink_target);
	dialog->priv->symlink_target = filename;  /* takes ownership */

	/* Invalidate the saved MIME type. */
	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	source = e_mail_signature_script_dialog_get_source (dialog);
	extension = e_source_get_extension (source, extension_name);
	e_source_mail_signature_set_mime_type (extension, NULL);

	g_object_notify (G_OBJECT (dialog), "symlink-target");

	mail_signature_script_dialog_update_status (dialog);
}

static void
mail_signature_script_dialog_query_cb (GFile *file,
                                       GAsyncResult *result,
                                       EMailSignatureScriptDialog *dialog)
{
	GFileInfo *file_info;
	const gchar *symlink_target;
	GError *error = NULL;

	file_info = g_file_query_info_finish (file, result, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (file_info == NULL);
		g_object_unref (dialog);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (file_info == NULL);
		g_warning ("%s", error->message);
		g_object_unref (dialog);
		g_error_free (error);
		return;
	}

	g_return_if_fail (G_IS_FILE_INFO (file_info));

	symlink_target = g_file_info_get_symlink_target (file_info);

	e_mail_signature_script_dialog_set_symlink_target (
		dialog, symlink_target);

	g_object_unref (file_info);
	g_object_unref (dialog);
}

static void
mail_signature_script_dialog_set_registry (EMailSignatureScriptDialog *dialog,
                                           ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (dialog->priv->registry == NULL);

	dialog->priv->registry = g_object_ref (registry);
}

static void
mail_signature_script_dialog_set_source (EMailSignatureScriptDialog *dialog,
                                         ESource *source)
{
	GDBusObject *dbus_object = NULL;
	const gchar *extension_name;
	GError *error = NULL;

	g_return_if_fail (source == NULL || E_IS_SOURCE (source));
	g_return_if_fail (dialog->priv->source == NULL);

	if (source != NULL)
		dbus_object = e_source_ref_dbus_object (source);

	/* Clone the source so we can make changes to it freely. */
	dialog->priv->source = e_source_new (dbus_object, NULL, &error);

	/* This should rarely fail.  If the file was loaded successfully
	 * once then it should load successfully here as well, unless an
	 * I/O error occurs. */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	/* Make sure the source has a mail signature extension. */
	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	e_source_get_extension (dialog->priv->source, extension_name);

	/* If we're editing an existing signature, query the symbolic
	 * link target of the signature file so we can initialize the
	 * file chooser button.  Note: The asynchronous callback will
	 * run after the dialog initialization is complete. */
	if (dbus_object != NULL) {
		ESourceMailSignature *extension;
		const gchar *extension_name;
		GFile *file;

		extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
		extension = e_source_get_extension (source, extension_name);
		file = e_source_mail_signature_get_file (extension);

		g_file_query_info_async (
			file, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
			G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
			NULL, (GAsyncReadyCallback)
			mail_signature_script_dialog_query_cb,
			g_object_ref (dialog));

		g_object_unref (dbus_object);
	}
}

static void
mail_signature_script_dialog_set_property (GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			mail_signature_script_dialog_set_registry (
				E_MAIL_SIGNATURE_SCRIPT_DIALOG (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			mail_signature_script_dialog_set_source (
				E_MAIL_SIGNATURE_SCRIPT_DIALOG (object),
				g_value_get_object (value));
			return;

		case PROP_SYMLINK_TARGET:
			e_mail_signature_script_dialog_set_symlink_target (
				E_MAIL_SIGNATURE_SCRIPT_DIALOG (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_script_dialog_get_property (GObject *object,
                                           guint property_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_signature_script_dialog_get_registry (
				E_MAIL_SIGNATURE_SCRIPT_DIALOG (object)));
			return;

		case PROP_SOURCE:
			g_value_set_object (
				value,
				e_mail_signature_script_dialog_get_source (
				E_MAIL_SIGNATURE_SCRIPT_DIALOG (object)));
			return;

		case PROP_SYMLINK_TARGET:
			g_value_set_string (
				value,
				e_mail_signature_script_dialog_get_symlink_target (
				E_MAIL_SIGNATURE_SCRIPT_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_script_dialog_dispose (GObject *object)
{
	EMailSignatureScriptDialogPrivate *priv;

	priv = E_MAIL_SIGNATURE_SCRIPT_DIALOG_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_signature_script_dialog_parent_class)->
		dispose (object);
}

static void
mail_signature_script_dialog_finalize (GObject *object)
{
	EMailSignatureScriptDialogPrivate *priv;

	priv = E_MAIL_SIGNATURE_SCRIPT_DIALOG_GET_PRIVATE (object);

	g_free (priv->symlink_target);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_signature_script_dialog_parent_class)->
		finalize (object);
}

static void
mail_signature_script_dialog_constructed (GObject *object)
{
	EMailSignatureScriptDialog *dialog;
	GtkFileFilter *filter;
	GtkWidget *container;
	GtkWidget *widget;
	ESource *source;
	const gchar *display_name;
	gchar *markup;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_signature_script_dialog_parent_class)->constructed (object);

	dialog = E_MAIL_SIGNATURE_SCRIPT_DIALOG (object);

	source = e_mail_signature_script_dialog_get_source (dialog);
	display_name = e_source_get_display_name (source);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	gtk_dialog_add_button (
		GTK_DIALOG (dialog),
		_("_Cancel"), GTK_RESPONSE_CANCEL);

	gtk_dialog_add_button (
		GTK_DIALOG (dialog),
		_("_Save"), GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = gtk_table_new (4, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 6);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 6);
	gtk_table_set_row_spacing (GTK_TABLE (widget), 0, 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name (
		"dialog-information", GTK_ICON_SIZE_DIALOG);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 0, 1, 0, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (_(
		"The output of this script will be used as your\n"
		"signature. The name you specify will be used\n"
		"for display purposes only."));
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (widget), display_name);
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	dialog->priv->entry = widget;  /* not referenced */
	gtk_widget_show (widget);

	e_binding_bind_property (
		widget, "text",
		source, "display-name",
		G_BINDING_DEFAULT);

	widget = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->entry);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_file_chooser_button_new (
		NULL, GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	dialog->priv->file_chooser = widget;  /* not referenced */
	gtk_widget_show (widget);

	/* Restrict file selection to executable files. */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_custom (
		filter, GTK_FILE_FILTER_FILENAME,
		(GtkFileFilterFunc) mail_signature_script_dialog_filter_cb,
		NULL, NULL);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (widget), filter);

	/* We create symbolic links to script files from the "signatures"
	 * directory, so restrict the selection to local files only. */
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (widget), TRUE);

	widget = gtk_label_new_with_mnemonic (_("S_cript:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->file_chooser);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	/* This is just a placeholder. */
	widget = gtk_label_new (NULL);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 3, 4, 0, 0, 0, 0);
	dialog->priv->alert = widget;  /* not referenced */
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name (
		"dialog-warning", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	markup = g_markup_printf_escaped (
		"<small>%s</small>",
		_("Script file must be executable."));
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	g_signal_connect (
		dialog->priv->file_chooser, "file-set",
		G_CALLBACK (mail_signature_script_dialog_file_set_cb), dialog);

	g_signal_connect_swapped (
		dialog->priv->entry, "changed",
		G_CALLBACK (mail_signature_script_dialog_update_status), dialog);

	mail_signature_script_dialog_update_status (dialog);
}

static void
e_mail_signature_script_dialog_class_init (EMailSignatureScriptDialogClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailSignatureScriptDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_signature_script_dialog_set_property;
	object_class->get_property = mail_signature_script_dialog_get_property;
	object_class->dispose = mail_signature_script_dialog_dispose;
	object_class->finalize = mail_signature_script_dialog_finalize;
	object_class->constructed = mail_signature_script_dialog_constructed;

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

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SYMLINK_TARGET,
		g_param_spec_string (
			"symlink-target",
			"Symlink Target",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_signature_script_dialog_init (EMailSignatureScriptDialog *dialog)
{
	dialog->priv = E_MAIL_SIGNATURE_SCRIPT_DIALOG_GET_PRIVATE (dialog);
}

GtkWidget *
e_mail_signature_script_dialog_new (ESourceRegistry *registry,
                                    GtkWindow *parent,
                                    ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	if (source != NULL)
		g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return g_object_new (
		E_TYPE_MAIL_SIGNATURE_SCRIPT_DIALOG,
		"registry", registry,
		"transient-for", parent,
		"source", source, NULL);
}

ESourceRegistry *
e_mail_signature_script_dialog_get_registry (EMailSignatureScriptDialog *dialog)
{
	g_return_val_if_fail (
		E_IS_MAIL_SIGNATURE_SCRIPT_DIALOG (dialog), NULL);

	return dialog->priv->registry;
}

ESource *
e_mail_signature_script_dialog_get_source (EMailSignatureScriptDialog *dialog)
{
	g_return_val_if_fail (
		E_IS_MAIL_SIGNATURE_SCRIPT_DIALOG (dialog), NULL);

	return dialog->priv->source;
}

const gchar *
e_mail_signature_script_dialog_get_symlink_target (EMailSignatureScriptDialog *dialog)
{
	g_return_val_if_fail (
		E_IS_MAIL_SIGNATURE_SCRIPT_DIALOG (dialog), NULL);

	return dialog->priv->symlink_target;
}

void
e_mail_signature_script_dialog_set_symlink_target (EMailSignatureScriptDialog *dialog,
                                                   const gchar *symlink_target)
{
	GtkFileChooser *file_chooser;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_SCRIPT_DIALOG (dialog));
	g_return_if_fail (symlink_target != NULL);

	g_free (dialog->priv->symlink_target);
	dialog->priv->symlink_target = g_strdup (symlink_target);

	file_chooser = GTK_FILE_CHOOSER (dialog->priv->file_chooser);
	gtk_file_chooser_set_filename (file_chooser, symlink_target);

	g_object_notify (G_OBJECT (dialog), "symlink-target");

	mail_signature_script_dialog_update_status (dialog);
}

/****************** e_mail_signature_script_dialog_commit() ******************/

static void
mail_signature_script_dialog_symlink_cb (GObject *object,
                                         GAsyncResult *result,
                                         gpointer user_data)
{
	GSimpleAsyncResult *simple;
	GError *error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);

	e_source_mail_signature_symlink_finish (
		E_SOURCE (object), result, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);

	g_simple_async_result_complete (simple);

	g_object_unref (simple);
}

static void
mail_signature_script_dialog_commit_cb (GObject *object,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	GError *error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	e_source_registry_commit_source_finish (
		E_SOURCE_REGISTRY (object), result, &error);

	if (error != NULL) {
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
		return;
	}

	/* We can call this on our scratch source because only its UID is
	 * really needed, which even a new scratch source already knows. */
	e_source_mail_signature_symlink (
		async_context->source,
		async_context->symlink_target,
		G_PRIORITY_DEFAULT,
		async_context->cancellable,
		mail_signature_script_dialog_symlink_cb,
		simple);
}

void
e_mail_signature_script_dialog_commit (EMailSignatureScriptDialog *dialog,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *symlink_target;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_SCRIPT_DIALOG (dialog));

	registry = e_mail_signature_script_dialog_get_registry (dialog);
	source = e_mail_signature_script_dialog_get_source (dialog);

	symlink_target =
		e_mail_signature_script_dialog_get_symlink_target (dialog);

	async_context = g_slice_new0 (AsyncContext);
	async_context->source = g_object_ref (source);
	async_context->symlink_target = g_strdup (symlink_target);

	if (G_IS_CANCELLABLE (cancellable))
		async_context->cancellable = g_object_ref (cancellable);

	simple = g_simple_async_result_new (
		G_OBJECT (dialog), callback, user_data,
		e_mail_signature_script_dialog_commit);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	e_source_registry_commit_source (
		registry, source,
		async_context->cancellable,
		mail_signature_script_dialog_commit_cb,
		simple);
}

gboolean
e_mail_signature_script_dialog_commit_finish (EMailSignatureScriptDialog *dialog,
                                              GAsyncResult *result,
                                              GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (dialog),
		e_mail_signature_script_dialog_commit), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

