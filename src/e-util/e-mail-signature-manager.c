/*
 * e-mail-signature-manager.c
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

#include "e-mail-signature-manager.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <libedataserver/libedataserver.h>

#include "e-dialog-widgets.h"
#include "e-misc-utils.h"
#include "e-mail-signature-preview.h"
#include "e-mail-signature-tree-view.h"
#include "e-mail-signature-script-dialog.h"

#define PREVIEW_HEIGHT 200

struct _EMailSignatureManagerPrivate {
	ESourceRegistry *registry;

	GtkWidget *tree_view;		/* not referenced */
	GtkWidget *add_button;		/* not referenced */
	GtkWidget *add_script_button;	/* not referenced */
	GtkWidget *edit_button;		/* not referenced */
	GtkWidget *remove_button;	/* not referenced */
	GtkWidget *preview;		/* not referenced */
	GtkWidget *preview_frame;	/* not referenced */

	EContentEditorMode prefer_mode;
};

enum {
	PROP_0,
	PROP_PREFER_MODE,
	PROP_REGISTRY
};

enum {
	ADD_SIGNATURE,
	ADD_SIGNATURE_SCRIPT,
	EDITOR_CREATED,
	EDIT_SIGNATURE,
	REMOVE_SIGNATURE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailSignatureManager, e_mail_signature_manager, GTK_TYPE_PANED)

static void
mail_signature_manager_emit_editor_created (EMailSignatureManager *manager,
                                            GtkWidget *editor)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_EDITOR (editor));

	g_signal_emit (manager, signals[EDITOR_CREATED], 0, editor);
}

static gboolean
mail_signature_manager_key_press_event_cb (EMailSignatureManager *manager,
                                           GdkEventKey *event)
{
	if (event->keyval == GDK_KEY_Delete) {
		e_mail_signature_manager_remove_signature (manager);
		return TRUE;
	}

	return FALSE;
}

static void
mail_signature_manager_run_script_dialog (EMailSignatureManager *manager,
                                          ESource *source,
                                          const gchar *title)
{
	ESourceRegistry *registry;
	GtkWidget *dialog;
	gpointer parent;

	registry = e_mail_signature_manager_get_registry (manager);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	dialog = e_mail_signature_script_dialog_new (registry, parent, source);
	gtk_window_set_title (GTK_WINDOW (dialog), title);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		EAsyncClosure *closure;
		GAsyncResult *result;
		GError *error = NULL;

		closure = e_async_closure_new ();

		/* FIXME Make this cancellable. */
		e_mail_signature_script_dialog_commit (
			E_MAIL_SIGNATURE_SCRIPT_DIALOG (dialog), NULL,
			e_async_closure_callback, closure);

		result = e_async_closure_wait (closure);

		e_mail_signature_script_dialog_commit_finish (
			E_MAIL_SIGNATURE_SCRIPT_DIALOG (dialog),
			result, &error);

		e_async_closure_free (closure);

		/* FIXME Make this into an EAlert. */
		if (error != NULL) {
			g_warning ("%s: %s", G_STRFUNC, error->message);
			g_error_free (error);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
mail_signature_manager_selection_changed_cb (EMailSignatureManager *manager,
                                             GtkTreeSelection *selection)
{
	EMailSignaturePreview *preview;
	EMailSignatureTreeView *tree_view;
	ESource *source;
	GtkWidget *edit_button;
	GtkWidget *remove_button;
	gboolean sensitive;
	const gchar *uid = NULL;

	edit_button = manager->priv->edit_button;
	remove_button = manager->priv->remove_button;

	tree_view = E_MAIL_SIGNATURE_TREE_VIEW (manager->priv->tree_view);
	source = e_mail_signature_tree_view_ref_selected_source (tree_view);

	if (source != NULL)
		uid = e_source_get_uid (source);

	preview = E_MAIL_SIGNATURE_PREVIEW (manager->priv->preview);
	e_mail_signature_preview_set_source_uid (preview, uid);

	sensitive = (source != NULL);
	gtk_widget_set_sensitive (edit_button, sensitive);
	gtk_widget_set_sensitive (remove_button, sensitive);

	if (source != NULL)
		g_object_unref (source);
}

static void
mail_signature_manager_set_registry (EMailSignatureManager *manager,
                                     ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (manager->priv->registry == NULL);

	manager->priv->registry = g_object_ref (registry);
}

static void
mail_signature_manager_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_MODE:
			e_mail_signature_manager_set_prefer_mode (
				E_MAIL_SIGNATURE_MANAGER (object),
				g_value_get_enum (value));
			return;

		case PROP_REGISTRY:
			mail_signature_manager_set_registry (
				E_MAIL_SIGNATURE_MANAGER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_manager_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_MODE:
			g_value_set_enum (
				value,
				e_mail_signature_manager_get_prefer_mode (
				E_MAIL_SIGNATURE_MANAGER (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_signature_manager_get_registry (
				E_MAIL_SIGNATURE_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_manager_dispose (GObject *object)
{
	EMailSignatureManager *self = E_MAIL_SIGNATURE_MANAGER (object);

	g_clear_object (&self->priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_signature_manager_parent_class)->dispose (object);
}

static void
mail_signature_manager_constructed (GObject *object)
{
	EMailSignatureManager *manager;
	GtkTreeSelection *selection;
	ESourceRegistry *registry;
	GSettings *settings;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *hbox;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_signature_manager_parent_class)->constructed (object);

	manager = E_MAIL_SIGNATURE_MANAGER (object);
	registry = e_mail_signature_manager_get_registry (manager);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (manager), GTK_ORIENTATION_VERTICAL);

	container = GTK_WIDGET (manager);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_bottom (widget, 12);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	gtk_widget_show (widget);

	container = hbox = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_mail_signature_tree_view_new (registry);
	gtk_container_add (GTK_CONTAINER (container), widget);
	manager->priv->tree_view = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (mail_signature_manager_key_press_event_cb),
		manager);

	g_signal_connect_swapped (
		widget, "row-activated",
		G_CALLBACK (e_mail_signature_manager_edit_signature),
		manager);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (mail_signature_manager_selection_changed_cb),
		manager);

	container = hbox;

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->add_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_signature_manager_add_signature),
		manager);

	widget = e_dialog_button_new_with_icon ("system-run", _("Add _Script"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->add_script_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	settings = e_util_ref_settings ("org.gnome.desktop.lockdown");

	g_settings_bind (
		settings, "disable-command-line",
		widget, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	g_object_unref (settings);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_signature_manager_add_signature_script),
		manager);

	widget = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->edit_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_signature_manager_edit_signature),
		manager);

	widget = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->remove_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_signature_manager_remove_signature),
		manager);

	container = GTK_WIDGET (manager);

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
	manager->priv->preview_frame = widget;  /* not referenced */
	gtk_widget_show (widget);

	container = widget;

	widget = e_mail_signature_preview_new (registry);
	gtk_container_add (GTK_CONTAINER (container), widget);
	manager->priv->preview = widget;  /* not referenced */
	gtk_widget_show (widget);

	gtk_paned_set_position (GTK_PANED (manager), PREVIEW_HEIGHT);
}

static void
mail_signature_manager_editor_created_add_signature_cb (GObject *source_object,
							GAsyncResult *result,
							gpointer user_data)
{
	EMailSignatureManager *manager = user_data;
	EHTMLEditor *editor;
	GtkWidget *widget;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager));

	widget = e_mail_signature_editor_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create signature editor: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
		g_clear_object (&manager);
		return;
	}

	editor = e_mail_signature_editor_get_editor (E_MAIL_SIGNATURE_EDITOR (widget));
	e_html_editor_set_mode (editor, manager->priv->prefer_mode);

	mail_signature_manager_emit_editor_created (manager, widget);

	gtk_widget_grab_focus (manager->priv->tree_view);

	g_clear_object (&manager);
}

static void
mail_signature_manager_add_signature (EMailSignatureManager *manager)
{
	ESourceRegistry *registry;

	registry = e_mail_signature_manager_get_registry (manager);

	e_mail_signature_editor_new (registry, NULL,
		mail_signature_manager_editor_created_add_signature_cb, g_object_ref (manager));
}

static void
mail_signature_manager_add_signature_script (EMailSignatureManager *manager)
{
	const gchar *title;

	title = _("Add Signature Script");
	mail_signature_manager_run_script_dialog (manager, NULL, title);

	gtk_widget_grab_focus (manager->priv->tree_view);
}

static void
mail_signature_manager_editor_created (EMailSignatureManager *manager,
                                       EMailSignatureEditor *editor)
{
	GtkWindowPosition position;
	gpointer parent;

	position = GTK_WIN_POS_CENTER_ON_PARENT;
	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	gtk_window_set_transient_for (GTK_WINDOW (editor), parent);
	gtk_window_set_position (GTK_WINDOW (editor), position);
	gtk_widget_set_size_request (GTK_WIDGET (editor), 450, 300);
	gtk_widget_show (GTK_WIDGET (editor));
}

static void
mail_signature_manager_editor_created_edit_signature_cb (GObject *source_object,
							 GAsyncResult *result,
							 gpointer user_data)
{
	EMailSignatureManager *manager = user_data;
	GtkWidget *widget;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager));

	widget = e_mail_signature_editor_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create signature editor: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
		g_clear_object (&manager);
		return;
	}

	mail_signature_manager_emit_editor_created (manager, widget);

	g_clear_object (&manager);
}

static void
mail_signature_manager_edit_signature (EMailSignatureManager *manager)
{
	EMailSignatureTreeView *tree_view;
	ESourceMailSignature *extension;
	ESourceRegistry *registry;
	ESource *source;
	GFileInfo *file_info;
	GFile *file;
	const gchar *attribute;
	const gchar *extension_name;
	const gchar *title;
	GError *error = NULL;

	registry = e_mail_signature_manager_get_registry (manager);
	tree_view = E_MAIL_SIGNATURE_TREE_VIEW (manager->priv->tree_view);
	source = e_mail_signature_tree_view_ref_selected_source (tree_view);
	g_return_if_fail (source != NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	extension = e_source_get_extension (source, extension_name);
	file = e_source_mail_signature_get_file (extension);

	attribute = G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE;

	/* XXX This blocks but it should just be a local file. */
	file_info = g_file_query_info (
		file, attribute, G_FILE_QUERY_INFO_NONE, NULL, &error);

	/* FIXME Make this into an EAlert. */
	if (error != NULL) {
		g_warn_if_fail (file_info == NULL);
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_object_unref (source);
		g_error_free (error);
		return;
	}

	if (g_file_info_get_attribute_boolean (file_info, attribute))
		goto script;

	e_mail_signature_editor_new (registry, source,
		mail_signature_manager_editor_created_edit_signature_cb, g_object_ref (manager));

	goto exit;

script:
	title = _("Edit Signature Script");
	mail_signature_manager_run_script_dialog (manager, source, title);

exit:
	gtk_widget_grab_focus (GTK_WIDGET (tree_view));

	g_object_unref (file_info);
	g_object_unref (source);
}

static void
mail_signature_manager_remove_signature (EMailSignatureManager *manager)
{
	EMailSignatureTreeView *tree_view;
	ESourceMailSignature *extension;
	ESource *source;
	GFile *file;
	const gchar *extension_name;
	GError *error = NULL;

	tree_view = E_MAIL_SIGNATURE_TREE_VIEW (manager->priv->tree_view);
	source = e_mail_signature_tree_view_ref_selected_source (tree_view);

	if (source == NULL)
		return;

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	extension = e_source_get_extension (source, extension_name);

	file = e_source_mail_signature_get_file (extension);

	/* XXX This blocks but it should just be a local file. */
	if (!g_file_delete (file, NULL, &error)) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	/* Remove the mail signature data source asynchronously.
	 * XXX No callback function because there's not much we can do
	 *     if this fails.  We should probably implement EAlertSink. */
	e_source_remove (source, NULL, NULL, NULL);

	gtk_widget_grab_focus (GTK_WIDGET (tree_view));

	g_object_unref (source);
}

static void
e_mail_signature_manager_class_init (EMailSignatureManagerClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_signature_manager_set_property;
	object_class->get_property = mail_signature_manager_get_property;
	object_class->dispose = mail_signature_manager_dispose;
	object_class->constructed = mail_signature_manager_constructed;

	class->add_signature = mail_signature_manager_add_signature;
	class->add_signature_script = mail_signature_manager_add_signature_script;
	class->editor_created = mail_signature_manager_editor_created;
	class->edit_signature = mail_signature_manager_edit_signature;
	class->remove_signature = mail_signature_manager_remove_signature;

	g_object_class_install_property (
		object_class,
		PROP_PREFER_MODE,
		g_param_spec_enum (
			"prefer-mode",
			"Prefer editor mode",
			NULL,
			E_TYPE_CONTENT_EDITOR_MODE,
			E_CONTENT_EDITOR_MODE_HTML,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	signals[ADD_SIGNATURE] = g_signal_new (
		"add-signature",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailSignatureManagerClass, add_signature),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[ADD_SIGNATURE_SCRIPT] = g_signal_new (
		"add-signature-script",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (
			EMailSignatureManagerClass, add_signature_script),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[EDITOR_CREATED] = g_signal_new (
		"editor-created",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailSignatureManagerClass, editor_created),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_MAIL_SIGNATURE_EDITOR);

	signals[EDIT_SIGNATURE] = g_signal_new (
		"edit-signature",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailSignatureManagerClass, edit_signature),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[REMOVE_SIGNATURE] = g_signal_new (
		"remove-signature",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailSignatureManagerClass, remove_signature),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_signature_manager_init (EMailSignatureManager *manager)
{
	manager->priv = e_mail_signature_manager_get_instance_private (manager);
}

GtkWidget *
e_mail_signature_manager_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_SIGNATURE_MANAGER,
		"registry", registry, NULL);
}

void
e_mail_signature_manager_add_signature (EMailSignatureManager *manager)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager));

	g_signal_emit (manager, signals[ADD_SIGNATURE], 0);
}

void
e_mail_signature_manager_add_signature_script (EMailSignatureManager *manager)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager));

	g_signal_emit (manager, signals[ADD_SIGNATURE_SCRIPT], 0);
}

void
e_mail_signature_manager_edit_signature (EMailSignatureManager *manager)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager));

	g_signal_emit (manager, signals[EDIT_SIGNATURE], 0);
}

void
e_mail_signature_manager_remove_signature (EMailSignatureManager *manager)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager));

	g_signal_emit (manager, signals[REMOVE_SIGNATURE], 0);
}

EContentEditorMode
e_mail_signature_manager_get_prefer_mode (EMailSignatureManager *manager)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager), FALSE);

	return manager->priv->prefer_mode;
}

void
e_mail_signature_manager_set_prefer_mode (EMailSignatureManager *manager,
                                          EContentEditorMode prefer_mode)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager));

	if (prefer_mode == E_CONTENT_EDITOR_MODE_UNKNOWN)
		prefer_mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

	if (manager->priv->prefer_mode == prefer_mode)
		return;

	manager->priv->prefer_mode = prefer_mode;

	g_object_notify (G_OBJECT (manager), "prefer-mode");
}

ESourceRegistry *
e_mail_signature_manager_get_registry (EMailSignatureManager *manager)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_MANAGER (manager), NULL);

	return manager->priv->registry;
}
