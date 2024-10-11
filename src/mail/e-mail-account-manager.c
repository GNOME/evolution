/*
 * e-mail-account-manager.c
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
#include <gdk/gdkkeysyms.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include "e-mail-account-tree-view.h"

#include "e-mail-account-manager.h"

#define DEFAULT_ORDER_RESPONSE GTK_RESPONSE_APPLY
#define OPEN_ONLINE_ACCOUNTS_RESPONSE GTK_RESPONSE_APPLY

struct _EMailAccountManagerPrivate {
	EMailAccountStore *store;
	gulong row_changed_handler_id;

	GtkWidget *tree_view;		/* not referenced */
	GtkWidget *add_button;		/* not referenced */
	GtkWidget *edit_button;		/* not referenced */
	GtkWidget *delete_button;	/* not referenced */
	GtkWidget *default_button;	/* not referenced */
	GtkWidget *goa_message;		/* not referenced */
	GtkWidget *uoa_message;		/* not referenced */

	gchar *gcc_program_path;
	gchar *goa_account_id;
	guint uoa_account_id;
};

enum {
	PROP_0,
	PROP_STORE
};

enum {
	ADD_ACCOUNT,
	EDIT_ACCOUNT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailAccountManager, e_mail_account_manager, GTK_TYPE_GRID)

static void
mail_account_manager_open_goa_cb (GtkInfoBar *info_bar,
                                  gint response_id,
                                  EMailAccountManager *manager)
{
	if (response_id == OPEN_ONLINE_ACCOUNTS_RESPONSE) {
		gchar *command_line;
		GError *error = NULL;

		g_return_if_fail (manager->priv->gcc_program_path != NULL);
		g_return_if_fail (manager->priv->goa_account_id != NULL);

		command_line = g_strjoin (
			" ",
			manager->priv->gcc_program_path,
			"online-accounts",
			manager->priv->goa_account_id,
			NULL);
		g_spawn_command_line_async (command_line, &error);
		g_free (command_line);

		if (error != NULL) {
			g_warning ("%s: %s", G_STRFUNC, error->message);
			g_error_free (error);
		}
	}
}

static GtkWidget *
mail_account_manager_build_goa_message (EMailAccountManager *manager)
{
	GtkWidget *frame;
	GtkWidget *widget;
	GtkWidget *container;

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

	container = frame;

	widget = gtk_info_bar_new ();
	gtk_info_bar_set_message_type (
		GTK_INFO_BAR (widget), GTK_MESSAGE_INFO);
	if (manager->priv->gcc_program_path != NULL)
		gtk_info_bar_add_button (
			GTK_INFO_BAR (widget),
			_("Open _Online Accounts"),
			OPEN_ONLINE_ACCOUNTS_RESPONSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "response",
		G_CALLBACK (mail_account_manager_open_goa_cb),
		manager);

	container = gtk_info_bar_get_content_area (GTK_INFO_BAR (widget));

	/* Set spacing to 8 to match the default value
	 * of the "content-area-border" style property. */
	gtk_box_set_spacing (GTK_BOX (container), 8);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (container), GTK_ORIENTATION_HORIZONTAL);

	/* Icon is provided by gnome-control-center-data. */
	widget = gtk_image_new_from_icon_name (
		"goa-panel", GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (
		_("This account was created through "
		"the Online Accounts service."));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	return frame;
}

static void
mail_account_manager_open_uoa_cb (GtkInfoBar *info_bar,
                                  gint response_id,
                                  EMailAccountManager *manager)
{
	if (response_id == OPEN_ONLINE_ACCOUNTS_RESPONSE) {
		gchar *account_details;
		gchar *command_line;
		GError *error = NULL;

		g_return_if_fail (manager->priv->gcc_program_path != NULL);
		g_return_if_fail (manager->priv->uoa_account_id > 0);

		account_details = g_strdup_printf (
			"account-details=%u",
			manager->priv->uoa_account_id);
		command_line = g_strjoin (
			" ",
			manager->priv->gcc_program_path,
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
}

static GtkWidget *
mail_account_manager_build_uoa_message (EMailAccountManager *manager)
{
	GtkWidget *frame;
	GtkWidget *widget;
	GtkWidget *container;

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

	container = frame;

	widget = gtk_info_bar_new ();
	gtk_info_bar_set_message_type (
		GTK_INFO_BAR (widget), GTK_MESSAGE_INFO);
	if (manager->priv->gcc_program_path != NULL)
		gtk_info_bar_add_button (
			GTK_INFO_BAR (widget),
			_("Open _Online Accounts"),
			OPEN_ONLINE_ACCOUNTS_RESPONSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "response",
		G_CALLBACK (mail_account_manager_open_uoa_cb),
		manager);

	container = gtk_info_bar_get_content_area (GTK_INFO_BAR (widget));

	/* Set spacing to 8 to match the default value
	 * of the "content-area-border" style property. */
	gtk_box_set_spacing (GTK_BOX (container), 8);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (container), GTK_ORIENTATION_HORIZONTAL);

	/* Icon is provided by gnome-control-center-signon. */
	widget = gtk_image_new_from_icon_name (
		"credentials-preferences", GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (
		_("This account was created through "
		"the Online Accounts service."));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	return frame;
}

static void
mail_account_manager_add_cb (EMailAccountManager *manager)
{
	e_mail_account_manager_add_account (manager);
}

static void
mail_account_manager_edit_cb (EMailAccountManager *manager)
{
	EMailAccountTreeView *tree_view;
	EMailAccountStore *store;
	ESourceRegistry *registry;
	EMailSession *session;
	CamelService *service;
	ESource *source;
	const gchar *uid;

	store = e_mail_account_manager_get_store (manager);
	session = e_mail_account_store_get_session (store);
	registry = e_mail_session_get_registry (session);

	tree_view = E_MAIL_ACCOUNT_TREE_VIEW (manager->priv->tree_view);
	service = e_mail_account_tree_view_get_selected_service (tree_view);

	uid = camel_service_get_uid (service);
	source = e_source_registry_ref_source (registry, uid);
	g_return_if_fail (source != NULL);

	e_mail_account_manager_edit_account (manager, source);

	g_object_unref (source);
}

static void
mail_account_manager_remove_cb (EMailAccountManager *manager)
{
	EMailAccountTreeView *tree_view;
	EMailAccountStore *store;
	CamelService *service;
	gpointer parent;

	tree_view = E_MAIL_ACCOUNT_TREE_VIEW (manager->priv->tree_view);
	service = e_mail_account_tree_view_get_selected_service (tree_view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	store = e_mail_account_manager_get_store (manager);
	e_mail_account_store_remove_service (store, parent, service);
}

static void
mail_account_manager_enable_cb (EMailAccountManager *manager)
{
	EMailAccountTreeView *tree_view;
	EMailAccountStore *store;
	CamelService *service;
	gpointer parent;

	tree_view = E_MAIL_ACCOUNT_TREE_VIEW (manager->priv->tree_view);
	service = e_mail_account_tree_view_get_selected_service (tree_view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	store = e_mail_account_manager_get_store (manager);
	e_mail_account_store_enable_service (store, parent, service);
}

static void
mail_account_manager_disable_cb (EMailAccountManager *manager)
{
	EMailAccountTreeView *tree_view;
	EMailAccountStore *store;
	CamelService *service;
	gpointer parent;

	tree_view = E_MAIL_ACCOUNT_TREE_VIEW (manager->priv->tree_view);
	service = e_mail_account_tree_view_get_selected_service (tree_view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	store = e_mail_account_manager_get_store (manager);
	e_mail_account_store_disable_service (store, parent, service);
}

static void
mail_account_manager_default_cb (EMailAccountManager *manager)
{
	EMailAccountTreeView *tree_view;
	EMailAccountStore *store;
	CamelService *service;

	tree_view = E_MAIL_ACCOUNT_TREE_VIEW (manager->priv->tree_view);
	service = e_mail_account_tree_view_get_selected_service (tree_view);

	store = e_mail_account_manager_get_store (manager);
	e_mail_account_store_set_default_service (store, service);
}

static void
mail_account_manager_row_activated_cb (GtkTreeView *tree_view,
                                       GtkTreePath *path,
                                       GtkTreeViewColumn *column,
                                       EMailAccountManager *manager)
{
	GtkWidget *edit_button;

	edit_button = manager->priv->edit_button;

	if (gtk_widget_is_sensitive (edit_button))
		gtk_button_clicked (GTK_BUTTON (edit_button));
}

static void
mail_account_manager_info_bar_response_cb (EMailAccountManager *manager,
                                           gint response)
{
	EMailAccountStore *store;

	store = e_mail_account_manager_get_store (manager);

	if (response == DEFAULT_ORDER_RESPONSE)
		e_mail_account_store_reorder_services (store, NULL);
}

static gboolean
mail_account_manager_key_press_event_cb (EMailAccountManager *manager,
                                         GdkEventKey *event)
{
	if (event->keyval == GDK_KEY_Delete) {
		mail_account_manager_remove_cb (manager);
		return TRUE;
	}

	return FALSE;
}

static void
mail_account_manager_row_changed_cb (GtkTreeModel *tree_model,
                                     GtkTreePath *path,
                                     GtkTreeIter *iter,
                                     EMailAccountManager *manager)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (manager->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	/* Update buttons for the selected row (which is not
	 * necessarily the row that changed, but do it anyway). */
	g_signal_emit_by_name (selection, "changed");
}

static void
mail_account_manager_selection_changed_cb (EMailAccountManager *manager,
                                           GtkTreeSelection *selection)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	EMailAccountStore *store;
	CamelService *default_service;
	CamelService *service;
	GtkWidget *add_button;
	GtkWidget *edit_button;
	GtkWidget *delete_button;
	GtkWidget *default_button;
	GtkWidget *goa_message;
	GtkWidget *uoa_message;
	gboolean builtin;
	gboolean sensitive;
	gboolean visible;
	gboolean not_default;
	gboolean removable;

	add_button = manager->priv->add_button;
	edit_button = manager->priv->edit_button;
	delete_button = manager->priv->delete_button;
	default_button = manager->priv->default_button;
	goa_message = manager->priv->goa_message;
	uoa_message = manager->priv->uoa_message;

	g_free (manager->priv->goa_account_id);
	manager->priv->goa_account_id = NULL;
	manager->priv->uoa_account_id = 0;

	if (gtk_tree_selection_get_selected (selection, &tree_model, &iter)) {
		gtk_tree_model_get (
			tree_model, &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, &service,
			E_MAIL_ACCOUNT_STORE_COLUMN_BUILTIN, &builtin,
			-1);
		removable = !builtin;
	} else {
		service = NULL;
		builtin = FALSE;
		removable = FALSE;
	}

	store = e_mail_account_manager_get_store (manager);
	default_service = e_mail_account_store_get_default_service (store);
	not_default = (service != default_service);

	if (service == NULL)
		gtk_widget_grab_focus (add_button);
	else {
		ESource *source;
		EMailSession *session;
		ESourceRegistry *registry;
		const gchar *uid;

		session = e_mail_account_store_get_session (store);
		registry = e_mail_session_get_registry (session);

		uid = camel_service_get_uid (service);
		source = e_source_registry_ref_source (registry, uid);

		if (source != NULL) {
			ESource *collection;
			const gchar *extension_name;

			extension_name = E_SOURCE_EXTENSION_COLLECTION;
			collection = e_source_registry_find_extension (
				registry, source, extension_name);
			if (collection != NULL) {
				g_object_unref (source);
				source = collection;
			}

			removable = !builtin && e_source_get_removable (source);

			extension_name = E_SOURCE_EXTENSION_GOA;
			if (e_source_has_extension (source, extension_name)) {
				ESourceGoa *extension;

				extension = e_source_get_extension (
					source, extension_name);
				manager->priv->goa_account_id =
					e_source_goa_dup_account_id (extension);
			}

			extension_name = E_SOURCE_EXTENSION_UOA;
			if (e_source_has_extension (source, extension_name)) {
				ESourceUoa *extension;

				extension = e_source_get_extension (
					source, extension_name);
				manager->priv->uoa_account_id =
					e_source_uoa_get_account_id (extension);
			}

			g_object_unref (source);
		}
	}

	sensitive = (service != NULL && !builtin);
	gtk_widget_set_sensitive (edit_button, sensitive);

	sensitive = (service != NULL && !builtin && removable);
	gtk_widget_set_sensitive (delete_button, sensitive);

	sensitive = (service != NULL && !builtin && not_default);
	gtk_widget_set_sensitive (default_button, sensitive);

	visible = (manager->priv->goa_account_id != NULL);
	gtk_widget_set_visible (goa_message, visible);

	visible = (manager->priv->uoa_account_id > 0);
	gtk_widget_set_visible (uoa_message, visible);
}

static void
mail_account_manager_set_store (EMailAccountManager *manager,
                                EMailAccountStore *store)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (manager->priv->store == NULL);

	manager->priv->store = g_object_ref (store);
}

static void
mail_account_manager_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_STORE:
			mail_account_manager_set_store (
				E_MAIL_ACCOUNT_MANAGER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_manager_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_STORE:
			g_value_set_object (
				value,
				e_mail_account_manager_get_store (
				E_MAIL_ACCOUNT_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_manager_dispose (GObject *object)
{
	EMailAccountManager *self = E_MAIL_ACCOUNT_MANAGER (object);

	if (self->priv->store != NULL) {
		g_signal_handler_disconnect (
			self->priv->store, self->priv->row_changed_handler_id);
		g_clear_object (&self->priv->store);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_account_manager_parent_class)->dispose (object);
}

static void
mail_account_manager_finalize (GObject *object)
{
	EMailAccountManager *self = E_MAIL_ACCOUNT_MANAGER (object);

	g_free (self->priv->gcc_program_path);
	g_free (self->priv->goa_account_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_account_manager_parent_class)->finalize (object);
}

static void
mail_account_manager_constructed (GObject *object)
{
	EMailAccountManager *manager;
	EMailAccountStore *store;
	GtkTreeSelection *selection;
	GtkWidget *container;
	GtkWidget *widget;
	gulong handler_id;

	manager = E_MAIL_ACCOUNT_MANAGER (object);
	store = e_mail_account_manager_get_store (manager);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_account_manager_parent_class)->constructed (object);

	e_binding_bind_property (
		store, "busy",
		manager, "sensitive",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	handler_id = g_signal_connect (
		store, "row-changed",
		G_CALLBACK (mail_account_manager_row_changed_cb),
		manager);

	/* We disconnect the handler in dispose(). */
	manager->priv->row_changed_handler_id = handler_id;

	gtk_grid_set_column_spacing (GTK_GRID (manager), 6);

	container = GTK_WIDGET (manager);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	container = widget;

	widget = e_mail_account_tree_view_new (store);
	gtk_container_add (GTK_CONTAINER (container), widget);
	manager->priv->tree_view = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "enable",
		G_CALLBACK (mail_account_manager_enable_cb), manager);

	g_signal_connect_swapped (
		widget, "disable",
		G_CALLBACK (mail_account_manager_disable_cb), manager);

	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (mail_account_manager_key_press_event_cb),
		manager);

	g_signal_connect (
		widget, "row-activated",
		G_CALLBACK (mail_account_manager_row_activated_cb), manager);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (mail_account_manager_selection_changed_cb),
		manager);

	container = GTK_WIDGET (manager);

	widget = mail_account_manager_build_goa_message (manager);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	manager->priv->goa_message = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = mail_account_manager_build_uoa_message (manager);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	manager->priv->uoa_message = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 1, 1);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_info_bar_new ();
	gtk_info_bar_set_message_type (
		GTK_INFO_BAR (widget), GTK_MESSAGE_INFO);
	gtk_info_bar_add_button (
		GTK_INFO_BAR (widget), _("_Reset Order"),
		DEFAULT_ORDER_RESPONSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "response",
		G_CALLBACK (mail_account_manager_info_bar_response_cb),
		manager);

	container = gtk_info_bar_get_content_area (GTK_INFO_BAR (widget));

	widget = gtk_label_new (
		_("You can drag and drop account names to reorder them."));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = GTK_WIDGET (manager);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 2);
	gtk_widget_show (widget);

	container = widget;

	widget = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->add_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (mail_account_manager_add_cb), manager);

	widget = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->edit_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (mail_account_manager_edit_cb), manager);

	widget = e_dialog_button_new_with_icon ("edit-delete", _("_Delete"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->delete_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (mail_account_manager_remove_cb), manager);

	widget = e_dialog_button_new_with_icon ("emblem-default", _("De_fault"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->default_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (mail_account_manager_default_cb), manager);

	/* Initialize button states. */
	g_signal_emit_by_name (selection, "changed");
}

static void
e_mail_account_manager_class_init (EMailAccountManagerClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_account_manager_set_property;
	object_class->get_property = mail_account_manager_get_property;
	object_class->dispose = mail_account_manager_dispose;
	object_class->finalize = mail_account_manager_finalize;
	object_class->constructed = mail_account_manager_constructed;

	g_object_class_install_property (
		object_class,
		PROP_STORE,
		g_param_spec_object (
			"store",
			"Store",
			NULL,
			E_TYPE_MAIL_ACCOUNT_STORE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[ADD_ACCOUNT] = g_signal_new (
		"add-account",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountManagerClass, add_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[EDIT_ACCOUNT] = g_signal_new (
		"edit-account",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountManagerClass, edit_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);
}

static void
e_mail_account_manager_init (EMailAccountManager *manager)
{
	manager->priv = e_mail_account_manager_get_instance_private (manager);

	manager->priv->gcc_program_path =
		g_find_program_in_path ("gnome-control-center");
}

GtkWidget *
e_mail_account_manager_new (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), NULL);

	return g_object_new (
		E_TYPE_MAIL_ACCOUNT_MANAGER,
		"store", store, NULL);
}

EMailAccountStore *
e_mail_account_manager_get_store (EMailAccountManager *manager)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_MANAGER (manager), NULL);

	return manager->priv->store;
}

void
e_mail_account_manager_add_account (EMailAccountManager *manager)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_MANAGER (manager));

	g_signal_emit (manager, signals[ADD_ACCOUNT], 0);
}

void
e_mail_account_manager_edit_account (EMailAccountManager *manager,
                                     ESource *source)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_MANAGER (manager));
	g_return_if_fail (E_IS_SOURCE (source));

	g_signal_emit (manager, signals[EDIT_ACCOUNT], 0, source);
}

