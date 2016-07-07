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

#include "e-mail-signature-editor.h"

#include <config.h>
#include <string.h>
#include <glib/gi18n.h>

#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-alert-bar.h"
#include "e-simple-async-result.h"

#define E_MAIL_SIGNATURE_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SIGNATURE_EDITOR, EMailSignatureEditorPrivate))

typedef struct _AsyncContext AsyncContext;

struct _EMailSignatureEditorPrivate {
	EHTMLEditor *editor;
	GtkActionGroup *action_group;
	EFocusTracker *focus_tracker;
	GCancellable *cancellable;
	ESourceRegistry *registry;
	ESource *source;
	gchar *original_name;

	GtkWidget *entry;		/* not referenced */
};

struct _AsyncContext {
	ESource *source;
	GCancellable *cancellable;
	gchar *contents;
	gsize length;
};

enum {
	PROP_0,
	PROP_EDITOR,
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

G_DEFINE_TYPE (
	EMailSignatureEditor,
	e_mail_signature_editor,
	GTK_TYPE_WINDOW)

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
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
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
			E_ALERT_SINK (e_mail_signature_editor_get_editor (window)),
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

	editor = e_mail_signature_editor_get_editor (window);
	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_html_mode (cnt_editor, is_html);

	if (is_html) {
		if (strstr (contents, "data-evo-signature-plain-text-mode")) {
			EContentEditorContentFlags flags;

			flags = e_content_editor_get_current_content_flags (cnt_editor);
			flags |= E_CONTENT_EDITOR_MESSAGE_DRAFT;

			e_content_editor_set_html_mode (cnt_editor, TRUE);
		}
		e_content_editor_insert_content (
			cnt_editor,
			contents,
			E_CONTENT_EDITOR_INSERT_TEXT_HTML |
			E_CONTENT_EDITOR_INSERT_REPLACE_ALL);
	} else
		e_content_editor_insert_content (
			cnt_editor,
			contents,
			E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
			E_CONTENT_EDITOR_INSERT_REPLACE_ALL);

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
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gboolean something_changed = FALSE;
	const gchar *original_name;
	const gchar *signature_name;

	editor = e_mail_signature_editor_get_editor (window);
	cnt_editor = e_html_editor_get_content_editor (editor);

	original_name = window->priv->original_name;
	signature_name = gtk_entry_get_text (GTK_ENTRY (window->priv->entry));

	something_changed |= e_content_editor_can_undo (cnt_editor);
	something_changed |= (strcmp (signature_name, original_name) != 0);

	if (something_changed) {
		gint response;

		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (window),
			"widgets:ask-signature-changed", NULL);
		if (response == GTK_RESPONSE_YES) {
			GtkActionGroup *action_group;
			GtkAction *action;

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
			E_ALERT_SINK (e_mail_signature_editor_get_editor (editor)),
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
			E_ALERT_SINK (e_mail_signature_editor_get_editor (editor)),
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
		 * (and ESource properties are left unchanged). */
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
mail_signature_editor_set_editor (EMailSignatureEditor *editor,
				  EHTMLEditor *html_editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (html_editor));
	g_return_if_fail (editor->priv->editor == NULL);

	editor->priv->editor = g_object_ref (html_editor);
}

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
		case PROP_EDITOR:
			mail_signature_editor_set_editor (
				E_MAIL_SIGNATURE_EDITOR (object),
				g_value_get_object (value));
			return;

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
		case PROP_EDITOR:
			g_value_set_object (
				value,
				e_mail_signature_editor_get_editor (
				E_MAIL_SIGNATURE_EDITOR (object)));
			return;

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

	if (priv->editor != NULL) {
		g_object_unref (priv->editor);
		priv->editor = NULL;
	}

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
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GtkUIManager *ui_manager;
	GDBusObject *dbus_object;
	ESource *source;
	GtkAction *action;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *hbox;
	const gchar *display_name;
	GError *error = NULL;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_signature_editor_parent_class)->constructed (object);

	window = E_MAIL_SIGNATURE_EDITOR (object);
	editor = e_mail_signature_editor_get_editor (window);
	cnt_editor = e_html_editor_get_content_editor (editor);

	ui_manager = e_html_editor_get_ui_manager (editor);

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
	action = e_html_editor_get_action (editor, "properties-page");
	gtk_action_set_visible (action, FALSE);

	action = e_html_editor_get_action (editor, "context-properties-page");
	gtk_action_set_visible (action, FALSE);

	gtk_ui_manager_ensure_update (ui_manager);

	gtk_window_set_title (GTK_WINDOW (window), _("Edit Signature"));
	gtk_window_set_default_size (GTK_WINDOW (window), 600, 440);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (window), widget);
	gtk_widget_show (widget);

	container = widget;

	/* Construct the main menu and toolbar. */

	widget = e_html_editor_get_managed_widget (editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_html_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* Construct the signature name entry. */

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	hbox = widget;

	widget = gtk_entry_new ();
	gtk_box_pack_end (GTK_BOX (hbox), widget, TRUE, TRUE, 0);
	window->priv->entry = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Signature Name:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), window->priv->entry);
	gtk_box_pack_end (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* Construct the main editing area. */

	widget = GTK_WIDGET (editor);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (mail_signature_editor_delete_event_cb), NULL);

	/* Configure an EFocusTracker to manage selection actions. */
	focus_tracker = e_focus_tracker_new (GTK_WINDOW (window));

	action = e_html_editor_get_action (editor, "cut");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (editor, "copy");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (editor, "paste");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (editor, "select-all");
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
		gtk_widget_grab_focus (GTK_WIDGET (cnt_editor));
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
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
			NULL,
			NULL,
			E_TYPE_HTML_EDITOR,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

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
e_mail_signature_editor_init (EMailSignatureEditor *editor)
{
	editor->priv = E_MAIL_SIGNATURE_EDITOR_GET_PRIVATE (editor);
}

typedef struct _CreateEditorData {
	ESourceRegistry *registry;
	ESource *source;
} CreateEditorData;

static void
create_editor_data_free (gpointer ptr)
{
	CreateEditorData *ced = ptr;

	if (ced) {
		g_clear_object (&ced->registry);
		g_clear_object (&ced->source);
		g_free (ced);
	}
}

static void
mail_signature_editor_html_editor_created_cb (GObject *source_object,
					      GAsyncResult *async_result,
					      gpointer user_data)
{
	GtkWidget *html_editor, *signature_editor;
	ESimpleAsyncResult *eresult = user_data;
	CreateEditorData *ced;
	GError *error = NULL;

	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (eresult));

	ced = e_simple_async_result_get_user_data (eresult);
	g_return_if_fail (ced != NULL);

	html_editor = e_html_editor_new_finish (async_result, &error);
	if (error) {
		g_warning ("%s: Failed to create HTML editor: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	}

	signature_editor = g_object_new (E_TYPE_MAIL_SIGNATURE_EDITOR,
		"registry", ced->registry,
		"source", ced->source,
		"editor", html_editor,
		NULL);

	e_simple_async_result_set_op_pointer (eresult, signature_editor);

	e_simple_async_result_complete (eresult);

	g_object_unref (eresult);
}

void
e_mail_signature_editor_new (ESourceRegistry *registry,
			     ESource *source,
			     GAsyncReadyCallback callback,
			     gpointer user_data)
{
	ESimpleAsyncResult *eresult;
	CreateEditorData *ced;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));

	if (source != NULL)
		g_return_if_fail (E_IS_SOURCE (source));

	ced = g_new0 (CreateEditorData, 1);
	ced->registry = g_object_ref (registry);
	ced->source = source ? g_object_ref (source) : NULL;

	eresult = e_simple_async_result_new (NULL, callback, user_data, e_mail_signature_editor_new);
	e_simple_async_result_set_user_data (eresult, ced, create_editor_data_free);

	e_html_editor_new_async (mail_signature_editor_html_editor_created_cb, eresult);
}

GtkWidget *
e_mail_signature_editor_new_finish (GAsyncResult *result,
				    GError **error)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_signature_editor_new), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return e_simple_async_result_get_op_pointer (eresult);
}

EHTMLEditor *
e_mail_signature_editor_get_editor (EMailSignatureEditor *editor)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (editor), NULL);

	return editor->priv->editor;
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
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (window));

	registry = e_mail_signature_editor_get_registry (window);
	source = e_mail_signature_editor_get_source (window);

	editor = e_mail_signature_editor_get_editor (window);
	cnt_editor = e_html_editor_get_content_editor (editor);

	mime_type = "text/html";
	contents = e_content_editor_get_content (
		cnt_editor,
		E_CONTENT_EDITOR_GET_TEXT_HTML |
		E_CONTENT_EDITOR_GET_BODY,
		NULL, NULL);

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

