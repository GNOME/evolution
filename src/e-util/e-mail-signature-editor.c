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

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-alert-bar.h"
#include "e-misc-utils.h"
#include "e-simple-async-result.h"

#include "e-mail-signature-editor.h"

#include "e-menu-bar.h"

typedef struct _AsyncContext AsyncContext;

struct _EMailSignatureEditorPrivate {
	EHTMLEditor *editor;
	EUIActionGroup *action_group;
	EFocusTracker *focus_tracker;
	GCancellable *cancellable;
	ESourceRegistry *registry;
	ESource *source;
	gchar *original_name;

	GtkWidget *entry;		/* not referenced */

	EMenuBar *menu_bar;
	GtkWidget *menu_button; /* owned by menu_bar */
};

struct _AsyncContext {
	ESourceRegistry *registry;
	ESource *source;
	EContentEditorGetContentFlags contents_flag;
	EContentEditorMode editor_mode;
	gchar *contents;
	gsize length;
	GDestroyNotify destroy_contents;
};

enum {
	PROP_0,
	PROP_EDITOR,
	PROP_FOCUS_TRACKER,
	PROP_REGISTRY,
	PROP_SOURCE
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailSignatureEditor, e_mail_signature_editor, GTK_TYPE_WINDOW)

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->registry);
	g_clear_object (&async_context->source);

	if (async_context->destroy_contents)
		async_context->destroy_contents (async_context->contents);
	else
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
	EContentEditorMode mode;
	ESource *source;
	EMailSignatureEditor *window;
	ESourceMailSignature *extension;
	const gchar *extension_name;
	const gchar *mime_type;
	gchar *contents = NULL;
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
	if (g_strcmp0 (mime_type, "text/html") == 0)
		mode = E_CONTENT_EDITOR_MODE_HTML;
	else if (g_strcmp0 (mime_type, "text/markdown") == 0)
		mode = E_CONTENT_EDITOR_MODE_MARKDOWN;
	else if (g_strcmp0 (mime_type, "text/markdown-plain") == 0)
		mode = E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT;
	else if (g_strcmp0 (mime_type, "text/markdown-html") == 0)
		mode = E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;
	else
		mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

	if (mode == E_CONTENT_EDITOR_MODE_HTML &&
	    strstr (contents, "data-evo-signature-plain-text-mode"))
		mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

	editor = e_mail_signature_editor_get_editor (window);
	e_html_editor_set_mode (editor, mode);
	/* no need to transfer the current content from old editor to the new, the content is set below */
	e_html_editor_cancel_mode_change_content_update (editor);
	cnt_editor = e_html_editor_get_content_editor (editor);

	if (mode == E_CONTENT_EDITOR_MODE_HTML) {
		e_content_editor_insert_content (
			cnt_editor,
			contents,
			E_CONTENT_EDITOR_INSERT_TEXT_HTML |
			E_CONTENT_EDITOR_INSERT_REPLACE_ALL);
	} else {
		e_content_editor_insert_content (
			cnt_editor,
			contents,
			E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
			E_CONTENT_EDITOR_INSERT_REPLACE_ALL);
	}

	g_free (contents);

	g_object_unref (window);
}

static gboolean
mail_signature_editor_delete_event_cb (EMailSignatureEditor *editor,
                                       GdkEvent *event)
{
	EUIAction *action;

	action = e_ui_action_group_get_action (editor->priv->action_group, "close");
	g_action_activate (G_ACTION (action), NULL);

	return TRUE;
}

static void
action_close_cb (EUIAction *action,
		 GVariant *parametr,
		 gpointer user_data)
{
	EMailSignatureEditor *window = user_data;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gboolean something_changed = FALSE;
	const gchar *original_name;
	const gchar *signature_name;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (window));

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
			EUIAction *action2;

			action2 = e_ui_action_group_get_action (window->priv->action_group, "save-and-close");
			g_action_activate (G_ACTION (action2), NULL);
			return;
		} else if (response == GTK_RESPONSE_CANCEL)
			return;
	}

	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
mail_signature_editor_commit_ready_cb (GObject *source_object,
				       GAsyncResult *result,
				       gpointer user_data)
{
	EMailSignatureEditor *editor;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (source_object));

	editor = E_MAIL_SIGNATURE_EDITOR (source_object);

	e_mail_signature_editor_commit_finish (editor, result, &error);

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
		ESource *source;

		registry = e_mail_signature_editor_get_registry (editor);
		source = e_mail_signature_editor_get_source (editor);

		/* Only make sure that the 'source-changed' is called,
		 * thus the preview of the signature is updated on save.
		 * It is not called when only signature body is changed
		 * (and ESource properties are left unchanged). */
		g_signal_emit_by_name (registry, "source-changed", source);

		gtk_widget_destroy (GTK_WIDGET (editor));
	}
}

static void
action_save_and_close_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EMailSignatureEditor *editor = user_data;
	GtkEntry *entry;
	ESource *source;
	gchar *display_name;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (editor));

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

	e_mail_signature_editor_commit (
		editor, editor->priv->cancellable,
		mail_signature_editor_commit_ready_cb, NULL);
}

static gboolean
e_mail_signature_editor_ui_manager_create_item_cb (EUIManager *ui_manager,
						   EUIElement *elem,
						   EUIAction *action,
						   EUIElementKind for_kind,
						   GObject **out_item,
						   gpointer user_data)
{
	EMailSignatureEditor *self = user_data;
	const gchar *name;

	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (self), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EMailSignatureEditor::"))
		return FALSE;

	if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		if (g_str_equal (name, "EMailSignatureEditor::menu-button"))
			*out_item = G_OBJECT (g_object_ref (self->priv->menu_button));
		else
			g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	return TRUE;
}

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
	EMailSignatureEditor *self = E_MAIL_SIGNATURE_EDITOR (object);

	g_clear_object (&self->priv->editor);
	g_clear_object (&self->priv->action_group);
	g_clear_object (&self->priv->focus_tracker);
	g_clear_object (&self->priv->menu_bar);

	if (self->priv->cancellable != NULL) {
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	g_clear_object (&self->priv->registry);
	g_clear_object (&self->priv->source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_signature_editor_parent_class)->dispose (object);
}

static void
mail_signature_editor_finalize (GObject *object)
{
	EMailSignatureEditor *self = E_MAIL_SIGNATURE_EDITOR (object);

	g_free (self->priv->original_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_signature_editor_parent_class)->finalize (object);
}

static void
mail_signature_editor_constructed (GObject *object)
{
	static const gchar *eui =
		"<eui>"
		  "<headerbar id='main-headerbar' type='gtk'>"
		    "<start>"
		      "<item action='save-and-close' icon_only='false' css_classes='suggested-action'/>"
		    "</start>"
		    "<end>"
		      "<item action='EMailSignatureEditor::menu-button'/>"
		    "</end>"
		  "</headerbar>"
		  "<menu id='main-menu'>"
		    "<placeholder id='pre-edit-menu'>"
		      "<submenu action='file-menu'>"
			"<item action='save-and-close'/>"
		        "<separator/>"
			"<item action='close'/>"
		      "</submenu>"
		    "</placeholder>"
		  "</menu>"
		  "<toolbar id='main-toolbar-without-headerbar'>"
		    "<placeholder id='pre-main-toolbar'>"
		      "<item action='save-and-close'/>"
		    "</placeholder>"
		  "</toolbar>"
		"</eui>";

	static const EUIActionEntry entries[] = {

		{ "close",
		  "window-close",
		  N_("_Close"),
		  "<Control>w",
		  N_("Close"),
		  action_close_cb, NULL, NULL, NULL },

		{ "save-and-close",
		  "document-save",
		  N_("_Save and Close"),
		  "<Control>Return",
		  N_("Save and Close"),
		  action_save_and_close_cb, NULL, NULL, NULL },

		{ "file-menu", NULL, N_("_File"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EMailSignatureEditor::menu-button", NULL, N_("Menu"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	EMailSignatureEditor *window;
	EFocusTracker *focus_tracker;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EUIManager *ui_manager;
	EUIAction *action;
	ESource *source;
	GObject *ui_item;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *hbox;
	const gchar *display_name;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_signature_editor_parent_class)->constructed (object);

	window = E_MAIL_SIGNATURE_EDITOR (object);
	editor = e_mail_signature_editor_get_editor (window);
	cnt_editor = e_html_editor_get_content_editor (editor);
	ui_manager = e_html_editor_get_ui_manager (editor);

	g_signal_connect_object (ui_manager, "create-item",
		G_CALLBACK (e_mail_signature_editor_ui_manager_create_item_cb), window, 0);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "signature", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), window, eui);

	e_ui_action_set_usable_for_kinds (e_ui_manager_get_action (ui_manager, "EMailSignatureEditor::menu-button"), E_UI_ELEMENT_KIND_HEADERBAR);

	window->priv->action_group = g_object_ref (e_ui_manager_get_action_group (ui_manager, "signature"));

	/* Hide page properties because it is not inherited in the mail. */
	action = e_html_editor_get_action (editor, "properties-page");
	e_ui_action_set_visible (action, FALSE);

	action = e_html_editor_get_action (editor, "context-properties-page");
	e_ui_action_set_visible (action, FALSE);

	gtk_window_set_default_size (GTK_WINDOW (window), -1, 440);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (window), widget);
	gtk_widget_show (widget);

	container = widget;

	ui_item = e_ui_manager_create_item (ui_manager, "main-menu");
	widget = gtk_menu_bar_new_from_model (G_MENU_MODEL (ui_item));
	g_clear_object (&ui_item);

	window->priv->menu_bar = e_menu_bar_new (GTK_MENU_BAR (widget), GTK_WINDOW (window), &window->priv->menu_button);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	/* Construct the main menu and toolbar. */

	if (e_util_get_use_header_bar ()) {
		ui_item = e_ui_manager_create_item (ui_manager, "main-headerbar");
		widget = GTK_WIDGET (ui_item);
		gtk_header_bar_set_title (GTK_HEADER_BAR (widget), _("Edit Signature"));

		ui_item = e_ui_manager_create_item (ui_manager, "main-toolbar-with-headerbar");
	} else {
		gtk_window_set_title (GTK_WINDOW (window), _("Edit Signature"));

		ui_item = e_ui_manager_create_item (ui_manager, "main-toolbar-without-headerbar");
	}

	widget = GTK_WIDGET (ui_item);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

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

	e_html_editor_connect_focus_tracker (editor, focus_tracker);

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
}

static void
e_mail_signature_editor_class_init (EMailSignatureEditorClass *class)
{
	GObjectClass *object_class;

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
	editor->priv = e_mail_signature_editor_get_instance_private (editor);
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
		g_slice_free (CreateEditorData, ced);
	}
}

static void
mail_signature_editor_html_editor_created_cb (GObject *source_object,
					      GAsyncResult *async_result,
					      gpointer user_data)
{
	GtkWidget *html_editor;
	EMailSignatureEditor *signature_editor;
	ESimpleAsyncResult *eresult = user_data;
	CreateEditorData *ced;
	GDBusObject *dbus_object;
	ESource *source;
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

	g_object_ref (signature_editor);

	e_simple_async_result_set_op_pointer (eresult, signature_editor, NULL);

	e_simple_async_result_complete (eresult);

	g_object_unref (eresult);

	source = e_mail_signature_editor_get_source (signature_editor);

	/* Load file content only for an existing signature.
	 * (A new signature will not yet have a GDBusObject.) */
	dbus_object = source ? e_source_ref_dbus_object (source) : NULL;
	if (dbus_object != NULL) {
		GCancellable *cancellable;

		cancellable = g_cancellable_new ();

		e_source_mail_signature_load (
			source,
			G_PRIORITY_DEFAULT,
			cancellable,
			mail_signature_editor_loaded_cb,
			g_object_ref (signature_editor));

		g_warn_if_fail (signature_editor->priv->cancellable == NULL);
		signature_editor->priv->cancellable = cancellable;

		g_object_unref (dbus_object);
	}

	g_object_unref (signature_editor);
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

	ced = g_slice_new0 (CreateEditorData);
	ced->registry = g_object_ref (registry);
	ced->source = source ? g_object_ref (source) : NULL;

	eresult = e_simple_async_result_new (NULL, callback, user_data, e_mail_signature_editor_new);
	e_simple_async_result_set_user_data (eresult, ced, create_editor_data_free);

	e_html_editor_new (mail_signature_editor_html_editor_created_cb, eresult);
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
	GTask *task;
	GError *error = NULL;

	task = G_TASK (user_data);

	e_source_mail_signature_replace_finish (
		E_SOURCE (object), result, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

static void
mail_signature_editor_commit_cb (GObject *object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	GTask *task;
	AsyncContext *async_context;
	GError *error = NULL;

	task = G_TASK (user_data);
	async_context = g_task_get_task_data (task);

	e_source_registry_commit_source_finish (
		E_SOURCE_REGISTRY (object), result, &error);

	if (error != NULL) {
		g_task_return_error (task, g_steal_pointer (&error));
		g_object_unref (task);
		return;
	}

	/* We can call this on our scratch source because only its UID is
	 * really needed, which even a new scratch source already knows. */
	e_source_mail_signature_replace (
		async_context->source,
		async_context->contents,
		async_context->length,
		G_PRIORITY_DEFAULT,
		g_task_get_cancellable (task),
		mail_signature_editor_replace_cb,
		task);
}

static void
mail_signature_editor_content_hash_ready_cb (GObject *source_object,
					     GAsyncResult *result,
					     gpointer user_data)
{
	GTask *task = user_data;
	EContentEditorContentHash *content_hash;
	ESourceMailSignature *extension;
	AsyncContext *async_context;
	const gchar *mime_type = "text/plain";
	GError *error = NULL;

	g_return_if_fail (E_IS_CONTENT_EDITOR (source_object));

	content_hash = e_content_editor_get_content_finish (E_CONTENT_EDITOR (source_object), result, &error);

	if (!content_hash) {
		g_task_return_error (task, g_steal_pointer (&error));
		g_object_unref (task);
		return;
	}

	async_context = g_task_get_task_data (task);

	async_context->contents = e_content_editor_util_steal_content_data (content_hash,
		async_context->contents_flag, &async_context->destroy_contents);

	e_content_editor_util_free_content_hash (content_hash);

	if (!async_context->contents) {
		g_warning ("%s: Failed to retrieve content", G_STRFUNC);

		async_context->contents = g_strdup ("");
		async_context->destroy_contents = NULL;
	}

	async_context->length = strlen (async_context->contents);

	switch (async_context->editor_mode) {
	case E_CONTENT_EDITOR_MODE_UNKNOWN:
		g_warn_if_reached ();
		break;
	case E_CONTENT_EDITOR_MODE_PLAIN_TEXT:
		mime_type = "text/plain";
		break;
	case E_CONTENT_EDITOR_MODE_HTML:
		mime_type = "text/html";
		break;
	case E_CONTENT_EDITOR_MODE_MARKDOWN:
		mime_type = "text/markdown";
		break;
	case E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT:
		mime_type = "text/markdown-plain";
		break;
	case E_CONTENT_EDITOR_MODE_MARKDOWN_HTML:
		mime_type = "text/markdown-html";
		break;
	}

	extension = e_source_get_extension (async_context->source, E_SOURCE_EXTENSION_MAIL_SIGNATURE);
	e_source_mail_signature_set_mime_type (extension, mime_type);

	e_source_registry_commit_source (
		async_context->registry, async_context->source,
		g_task_get_cancellable (task),
		mail_signature_editor_commit_cb,
		task);
}

void
e_mail_signature_editor_commit (EMailSignatureEditor *window,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	GTask *task;
	AsyncContext *async_context;
	ESourceRegistry *registry;
	ESource *source;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (window));

	registry = e_mail_signature_editor_get_registry (window);
	source = e_mail_signature_editor_get_source (window);

	editor = e_mail_signature_editor_get_editor (window);
	cnt_editor = e_html_editor_get_content_editor (editor);

	async_context = g_slice_new0 (AsyncContext);
	async_context->registry = g_object_ref (registry);
	async_context->source = g_object_ref (source);
	async_context->editor_mode = e_html_editor_get_mode (editor);
	async_context->contents_flag = async_context->editor_mode == E_CONTENT_EDITOR_MODE_HTML ?
		E_CONTENT_EDITOR_GET_RAW_BODY_HTML : E_CONTENT_EDITOR_GET_TO_SEND_PLAIN;

	task = g_task_new (window, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_signature_editor_commit);
	g_task_set_task_data (task, async_context, (GDestroyNotify) async_context_free);

	e_content_editor_get_content (cnt_editor, async_context->contents_flag, NULL,
		cancellable, mail_signature_editor_content_hash_ready_cb, task);
}

gboolean
e_mail_signature_editor_commit_finish (EMailSignatureEditor *editor,
                                       GAsyncResult *result,
                                       GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, editor), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_signature_editor_commit), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

