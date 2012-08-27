/*
 * e-mail-signature-editor.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-signature-editor.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-alert-bar.h"

#define E_MAIL_SIGNATURE_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SIGNATURE_EDITOR, EMailSignatureEditorPrivate))

typedef struct _AsyncContext AsyncContext;

struct _EMailSignatureEditorPrivate {
	GtkActionGroup *action_group;
	EFocusTracker *focus_tracker;
	GCancellable *cancellable;
	ESourceRegistry *registry;
	ESource *source;
	gchar *original_name;

	GtkWidget *entry;		/* not referenced */
	GtkWidget *alert_bar;		/* not referenced */
};

struct _AsyncContext {
	ESource *source;
	GCancellable *cancellable;
	gchar *contents;
	gsize length;
};

enum {
	PROP_0,
	PROP_FOCUS_TRACKER,
	PROP_REGISTRY,
	PROP_SOURCE
};

static const gchar *ui =
"<ui>\n"
"  <menubar name='main-menu'>\n"
"    <placeholder name='pre-edit-menu'>\n"
"      <menu action='file-menu'>\n"
"        <menuitem action='save-and-close'/>\n"
"        <separator/>"
"        <menuitem action='close'/>\n"
"      </menu>\n"
"    </placeholder>\n"
"  </menubar>\n"
"  <toolbar name='main-toolbar'>\n"
"    <placeholder name='pre-main-toolbar'>\n"
"      <toolitem action='save-and-close'/>\n"
"    </placeholder>\n"
"  </toolbar>\n"
"</ui>";

/* Forward Declarations */
static void	e_mail_signature_editor_alert_sink_init
					(EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EMailSignatureEditor,
	e_mail_signature_editor,
	E_TYPE_EDITOR_WINDOW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK,
		e_mail_signature_editor_alert_sink_init))

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->source != NULL)
		g_object_unref (async_context->source);

	if (async_context->cancellable != NULL)
		g_object_unref (async_context->cancellable);

	g_free (async_context->contents);

	g_slice_free (AsyncContext, async_context);
}

static void
mail_signature_editor_loaded_cb (GObject *object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	EEditor *editor;
	EEditorWidget *editor_widget;
	EEditorSelection *editor_selection;
	ESource *source;
	EMailSignatureEditor *window;
	ESourceMailSignature *extension;
	const gchar *extension_name;
	const gchar *mime_type;
	gchar *contents = NULL;
	gboolean is_html;
	GError *error = NULL;

	source = E_SOURCE (object);
	window = E_MAIL_SIGNATURE_EDITOR (user_data);

	e_source_mail_signature_load_finish (
		source, result, &contents, NULL, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (contents == NULL);
		g_object_unref (window);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (contents == NULL);
		e_alert_submit (
			E_ALERT_SINK (window),
			"widgets:no-load-signature",
			error->message, NULL);
		g_object_unref (window);
		g_error_free (error);
		return;
	}

	g_return_if_fail (contents != NULL);

	/* The load operation should have set the MIME type. */
	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	extension = e_source_get_extension (source, extension_name);
	mime_type = e_source_mail_signature_get_mime_type (extension);
	is_html = (g_strcmp0 (mime_type, "text/html") == 0);

	editor = e_editor_window_get_editor (E_EDITOR_WINDOW (object));
	editor_widget = e_editor_get_editor_widget (editor);
	e_editor_widget_set_mode (editor_widget,
		(is_html) ? E_EDITOR_WIDGET_MODE_HTML : E_EDITOR_WIDGET_MODE_PLAIN_TEXT);

	editor_selection = e_editor_widget_get_selection (editor_widget);
	if (is_html) {
		e_editor_selection_insert_html (editor_selection, contents);
	} else {
		e_editor_selection_insert_text (editor_selection, contents);
	}

	g_free (contents);

	g_object_unref (window);
}

static gboolean
mail_signature_editor_delete_event_cb (EMailSignatureEditor *editor,
                                       GdkEvent *event)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = editor->priv->action_group;
	action = gtk_action_group_get_action (action_group, "close");
	gtk_action_activate (action);

	return TRUE;
}

static void
action_close_cb (GtkAction *action,
                 EMailSignatureEditor *window)
{
	EEditor *editor;
	EEditorWidget *editor_widget;
	gboolean something_changed = FALSE;
	const gchar *original_name;
	const gchar *signature_name;

	original_name = window->priv->original_name;
	signature_name = gtk_entry_get_text (GTK_ENTRY (window->priv->entry));
	editor = e_editor_window_get_editor (E_EDITOR_WINDOW (window));
	editor_widget = e_editor_get_editor_widget (editor);

	something_changed |= webkit_web_view_can_undo (WEBKIT_WEB_VIEW (editor_widget));
	something_changed |= (strcmp (signature_name, original_name) != 0);

	if (something_changed) {
		gint response;

		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (window),
			"widgets:ask-signature-changed", NULL);
		if (response == GTK_RESPONSE_YES) {
			GtkActionGroup *action_group;

			action_group = window->priv->action_group;
			action = gtk_action_group_get_action (
				action_group, "save-and-close");
			gtk_action_activate (action);
			return;
		} else if (response == GTK_RESPONSE_CANCEL)
			return;
	}

	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
action_save_and_close_cb (GtkAction *action,
                          EMailSignatureEditor *editor)
{
	GtkEntry *entry;
	EAsyncClosure *closure;
	GAsyncResult *result;
	ESource *source;
	gchar *display_name;
	GError *error = NULL;

	entry = GTK_ENTRY (editor->priv->entry);
	source = e_mail_signature_editor_get_source (editor);

	display_name = g_strstrip (g_strdup (gtk_entry_get_text (entry)));

	/* Make sure the signature name is not blank. */
	if (*display_name == '\0') {
		e_alert_submit (
			E_ALERT_SINK (editor),
			"widgets:blank-signature", NULL);
		gtk_widget_grab_focus (GTK_WIDGET (entry));
		g_free (display_name);
		return;
	}

	e_source_set_display_name (source, display_name);

	g_free (display_name);

	/* Cancel any ongoing load or save operations. */
	if (editor->priv->cancellable != NULL) {
		g_cancellable_cancel (editor->priv->cancellable);
		g_object_unref (editor->priv->cancellable);
	}

	editor->priv->cancellable = g_cancellable_new ();

	closure = e_async_closure_new ();

	e_mail_signature_editor_commit (
		editor, editor->priv->cancellable,
		e_async_closure_callback, closure);

	result = e_async_closure_wait (closure);

	e_mail_signature_editor_commit_finish (editor, result, &error);

	e_async_closure_free (closure);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			E_ALERT_SINK (editor),
			"widgets:no-save-signature",
			error->message, NULL);
		g_error_free (error);

	/* Only destroy the editor if the save was successful. */
	} else {
		ESourceRegistry *registry;

		registry = e_mail_signature_editor_get_registry (editor);

		/* Only make sure that the 'source-changed' is called,
		 * thus the preview of the signature is updated on save.
		 * It is not called when only signature body is changed
		 * (and ESource properties are left unchanged).
		*/
		g_signal_emit_by_name (registry, "source-changed", source);

		gtk_widget_destroy (GTK_WIDGET (editor));
	}
}

static GtkActionEntry entries[] = {

	{ "close",
	  "window-close",
	  N_("_Close"),
	  "<Control>w",
	  N_("Close"),
	  G_CALLBACK (action_close_cb) },

	{ "save-and-close",
	  "document-save",
	  N_("_Save and Close"),
	  "<Control>Return",
	  N_("Save and Close"),
	  G_CALLBACK (action_save_and_close_cb) },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL }
};

static void
mail_signature_editor_set_registry (EMailSignatureEditor *editor,
                                    ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (editor->priv->registry == NULL);

	editor->priv->registry = g_object_ref (registry);
}

static void
mail_signature_editor_set_source (EMailSignatureEditor *editor,
                                  ESource *source)
{
	GDBusObject *dbus_object = NULL;
	const gchar *extension_name;
	GError *error = NULL;

	g_return_if_fail (source == NULL || E_IS_SOURCE (source));
	g_return_if_fail (editor->priv->source == NULL);

	if (source != NULL)
		dbus_object = e_source_ref_dbus_object (source);

	/* Clone the source so we can make changes to it freely. */
	editor->priv->source = e_source_new (dbus_object, NULL, &error);

	if (dbus_object != NULL)
		g_object_unref (dbus_object);

	/* This should rarely fail.  If the file was loaded successfully
	 * once then it should load successfully here as well, unless an
	 * I/O error occurs. */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	/* Make sure the source has a mail signature extension. */
	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	e_source_get_extension (editor->priv->source, extension_name);
}

static void
mail_signature_editor_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			mail_signature_editor_set_registry (
				E_MAIL_SIGNATURE_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			mail_signature_editor_set_source (
				E_MAIL_SIGNATURE_EDITOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_editor_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOCUS_TRACKER:
			g_value_set_object (
				value,
				e_mail_signature_editor_get_focus_tracker (
				E_MAIL_SIGNATURE_EDITOR (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_signature_editor_get_registry (
				E_MAIL_SIGNATURE_EDITOR (object)));
			return;

		case PROP_SOURCE:
			g_value_set_object (
				value,
				e_mail_signature_editor_get_source (
				E_MAIL_SIGNATURE_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_editor_dispose (GObject *object)
{
	EMailSignatureEditorPrivate *priv;

	priv = E_MAIL_SIGNATURE_EDITOR_GET_PRIVATE (object);

	if (priv->action_group != NULL) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	if (priv->focus_tracker != NULL) {
		g_object_unref (priv->focus_tracker);
		priv->focus_tracker = NULL;
	}

	if (priv->cancellable != NULL) {
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_signature_editor_parent_class)->
		dispose (object);
}

static void
mail_signature_editor_finalize (GObject *object)
{
	EMailSignatureEditorPrivate *priv;

	priv = E_MAIL_SIGNATURE_EDITOR_GET_PRIVATE (object);

	g_free (priv->original_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_signature_editor_parent_class)->
		finalize (object);
}

static void
mail_signature_editor_constructed (GObject *object)
{
	EMailSignatureEditor *window;
	GtkActionGroup *action_group;
	EFocusTracker *focus_tracker;
	EEditor *editor;
	EEditorWidget *editor_widget;
	GtkUIManager *ui_manager;
	GDBusObject *dbus_object;
	ESource *source;
	GtkAction *action;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *display_name;
	GError *error = NULL;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_signature_editor_parent_class)->
		constructed (object);

	window = E_MAIL_SIGNATURE_EDITOR (object);
	editor = e_editor_window_get_editor (E_EDITOR_WINDOW (window));
	editor_widget = e_editor_get_editor_widget (editor);

	ui_manager = e_editor_get_ui_manager (editor);

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);

	action_group = gtk_action_group_new ("signature");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, entries,
		G_N_ELEMENTS (entries), window);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	window->priv->action_group = g_object_ref (action_group);

	/* Hide page properties because it is not inherited in the mail. */
	action = e_editor_get_action (editor, "properties-page");
	gtk_action_set_visible (action, FALSE);

	action = e_editor_get_action (editor, "context-properties-page");
	gtk_action_set_visible (action, FALSE);

	gtk_ui_manager_ensure_update (ui_manager);

	gtk_window_set_title (GTK_WINDOW (window), _("Edit Signature"));

	/* Construct the signature name entry. */

	container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	e_editor_window_pack_above (E_EDITOR_WINDOW (window), container);
	gtk_widget_show (container);

	/* Construct the alert bar for errors. */
	widget = e_alert_bar_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	/* Position 5 should be between the style toolbar and editing area. */
	gtk_box_reorder_child (GTK_BOX (container), widget, 5);
	window->priv->alert_bar = widget;  /* not referenced */
	/* EAlertBar controls its own visibility. */

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	e_editor_window_pack_above (E_EDITOR_WINDOW (window), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_entry_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 0);
	window->priv->entry = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Signature Name:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), window->priv->entry);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (mail_signature_editor_delete_event_cb), NULL);


	/* Configure an EFocusTracker to manage selection actions. */
	focus_tracker = e_focus_tracker_new (GTK_WINDOW (window));

	action = e_editor_get_action (editor, "cut");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);

	action = e_editor_get_action (editor, "copy");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);

	action = e_editor_get_action (editor, "paste");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);

	action = e_editor_get_action (editor, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);

	window->priv->focus_tracker = focus_tracker;

	source = e_mail_signature_editor_get_source (window);

	display_name = e_source_get_display_name (source);
	if (display_name == NULL || *display_name == '\0')
		display_name = _("Unnamed");

	/* Set the entry text before we grab focus. */
	g_free (window->priv->original_name);
	window->priv->original_name = g_strdup (display_name);
	gtk_entry_set_text (GTK_ENTRY (window->priv->entry), display_name);

	/* Set the focus appropriately.  If this is a new signature, draw
	 * the user's attention to the signature name entry.  Otherwise go
	 * straight to the editing area. */
	if (source == NULL) {
		gtk_widget_grab_focus (window->priv->entry);
	} else {
		gtk_widget_grab_focus (GTK_WIDGET (editor_widget));
	}

	/* Load file content only for an existing signature.
	 * (A new signature will not yet have a GDBusObject.) */
	dbus_object = e_source_ref_dbus_object (source);
	if (dbus_object != NULL) {
		GCancellable *cancellable;

		cancellable = g_cancellable_new ();

		e_source_mail_signature_load (
			source,
			G_PRIORITY_DEFAULT,
			cancellable,
			mail_signature_editor_loaded_cb,
			g_object_ref (window));

		g_warn_if_fail (window->priv->cancellable == NULL);
		window->priv->cancellable = cancellable;

		g_object_unref (dbus_object);
	}
}

static void
mail_signature_editor_submit_alert (EAlertSink *alert_sink,
                                    EAlert *alert)
{
	EMailSignatureEditorPrivate *priv;
	EAlertBar *alert_bar;
	GtkWidget *dialog;
	GtkWindow *parent;

	priv = E_MAIL_SIGNATURE_EDITOR_GET_PRIVATE (alert_sink);

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
		case GTK_MESSAGE_WARNING:
		case GTK_MESSAGE_ERROR:
			alert_bar = E_ALERT_BAR (priv->alert_bar);
			e_alert_bar_add_alert (alert_bar, alert);
			break;

		default:
			parent = GTK_WINDOW (alert_sink);
			dialog = e_alert_dialog_new (parent, alert);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			break;
	}
}

static void
e_mail_signature_editor_class_init (EMailSignatureEditorClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailSignatureEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_signature_editor_set_property;
	object_class->get_property = mail_signature_editor_get_property;
	object_class->dispose = mail_signature_editor_dispose;
	object_class->finalize = mail_signature_editor_finalize;
	object_class->constructed = mail_signature_editor_constructed;

	g_object_class_install_property (
		object_class,
		PROP_FOCUS_TRACKER,
		g_param_spec_object (
			"focus-tracker",
			NULL,
			NULL,
			E_TYPE_FOCUS_TRACKER,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

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
			NULL,
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_signature_editor_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = mail_signature_editor_submit_alert;
}

static void
e_mail_signature_editor_init (EMailSignatureEditor *editor)
{
	editor->priv = E_MAIL_SIGNATURE_EDITOR_GET_PRIVATE (editor);
}

GtkWidget *
e_mail_signature_editor_new (ESourceRegistry *registry,
                             ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	if (source != NULL)
		g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return g_object_new (
		E_TYPE_MAIL_SIGNATURE_EDITOR,
		"registry", registry,
		"source", source, NULL);
}

EFocusTracker *
e_mail_signature_editor_get_focus_tracker (EMailSignatureEditor *editor)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (editor), NULL);

	return editor->priv->focus_tracker;
}

ESourceRegistry *
e_mail_signature_editor_get_registry (EMailSignatureEditor *editor)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (editor), NULL);

	return editor->priv->registry;
}

ESource *
e_mail_signature_editor_get_source (EMailSignatureEditor *editor)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (editor), NULL);

	return editor->priv->source;
}

/********************** e_mail_signature_editor_commit() *********************/

static void
mail_signature_editor_replace_cb (GObject *object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	GSimpleAsyncResult *simple;
	GError *error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);

	e_source_mail_signature_replace_finish (
		E_SOURCE (object), result, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);

	g_simple_async_result_complete (simple);

	g_object_unref (simple);
}

static void
mail_signature_editor_commit_cb (GObject *object,
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
	e_source_mail_signature_replace (
		async_context->source,
		async_context->contents,
		async_context->length,
		G_PRIORITY_DEFAULT,
		async_context->cancellable,
		mail_signature_editor_replace_cb,
		simple);
}

void
e_mail_signature_editor_commit (EMailSignatureEditor *window,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	ESourceMailSignature *extension;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *extension_name;
	const gchar *mime_type;
	gchar *contents;
	EEditor  *editor;
	EEditorWidget *editor_widget;
	EEditorWidgetMode mode;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (window));

	registry = e_mail_signature_editor_get_registry (window);
	source = e_mail_signature_editor_get_source (window);
	editor = e_editor_window_get_editor (E_EDITOR_WINDOW (window));
	editor_widget = e_editor_get_editor_widget (editor);
	mode = e_editor_widget_get_mode (editor_widget);

	if (mode == E_EDITOR_WIDGET_MODE_HTML) {
		mime_type = "text/html";
		contents = e_editor_widget_get_text_html (editor_widget);
	} else {
		mime_type = "text/plain";
		contents = e_editor_widget_get_text_plain (editor_widget);
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	extension = e_source_get_extension (source, extension_name);
	e_source_mail_signature_set_mime_type (extension, mime_type);

	async_context = g_slice_new0 (AsyncContext);
	async_context->source = g_object_ref (source);
	async_context->contents = contents;  /* takes ownership */
	async_context->length = strlen (contents);

	if (G_IS_CANCELLABLE (cancellable))
		async_context->cancellable = g_object_ref (cancellable);

	simple = g_simple_async_result_new (
		G_OBJECT (window), callback, user_data,
		e_mail_signature_editor_commit);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	e_source_registry_commit_source (
		registry, source,
		async_context->cancellable,
		mail_signature_editor_commit_cb,
		simple);
}

gboolean
e_mail_signature_editor_commit_finish (EMailSignatureEditor *editor,
                                       GAsyncResult *result,
                                       GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (editor),
		e_mail_signature_editor_commit), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

EEditorWidget *
e_mail_signature_editor_get_editor_widget (EMailSignatureEditor *window)
{
	EEditor *editor;

	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (window), NULL);

	editor = e_editor_window_get_editor (E_EDITOR_WINDOW (window));
	return e_editor_get_editor_widget (editor);
}
