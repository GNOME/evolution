/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <errno.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>

#include "e-misc-utils.h"
#include "e-ui-action.h"
#include "e-ui-manager.h"
#include "e-ui-parser.h"
#include "e-util-enumtypes.h"

#include "e-ui-customizer.h"

/**
 * SECTION: e-ui-customizer
 * @include: e-util/e-util.h
 * @short_description: a UI customizer
 *
 * The #EUICustomizer allows to customize UI elements like menus,
 * toolbars and headerbars. The customizer is created and owned
 * by an #EUIManager, if the customizations are allowed for it.
 * Use e_ui_manager_get_customizer() to get it.
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.56
 **/

struct _EUICustomizer {
	GObject parent;

	EUIManager *manager; /* not referenced, manager owns the instance */

	gchar *filename;
	EUIParser *parser;
	GHashTable *registered_elems; /* gchar *id ~> gchar *display_name */
};

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_MANAGER,
	N_PROPS
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_ACCELS_CHANGED,
	N_SIGNALS
};

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE (EUICustomizer, e_ui_customizer, G_TYPE_OBJECT)

static void
ui_customizer_parser_accels_changed_cb (EUIParser *parser,
					const gchar *action_name,
					GPtrArray *old_accels,
					GPtrArray *new_accels,
					gpointer user_data)
{
	EUICustomizer *self = user_data;

	g_signal_emit (self, signals[SIGNAL_ACCELS_CHANGED], 0, action_name, old_accels, new_accels, NULL);
}

static void
e_ui_customizer_set_property (GObject *object,
			      guint prop_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	EUICustomizer *self = E_UI_CUSTOMIZER (object);

	switch (prop_id) {
	case PROP_FILENAME:
		g_free (self->filename);
		self->filename = g_value_dup_string (value);
		break;
	case PROP_MANAGER:
		self->manager = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_customizer_get_property (GObject *object,
			      guint prop_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	EUICustomizer *self = E_UI_CUSTOMIZER (object);

	switch (prop_id) {
	case PROP_FILENAME:
		g_value_set_string (value, e_ui_customizer_get_filename (self));
		break;
	case PROP_MANAGER:
		g_value_set_object (value, e_ui_customizer_get_manager (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_ui_customizer_constructed (GObject *object)
{
	EUICustomizer *self = E_UI_CUSTOMIZER (object);

	G_OBJECT_CLASS (e_ui_customizer_parent_class)->constructed (object);

	if (self->filename) {
		GError *local_error = NULL;

		if (!e_ui_customizer_load (self, &local_error))
			g_warning ("Failed to load UI customizer data: %s\n", local_error->message);

		g_clear_error (&local_error);
	}
}

static void
e_ui_customizer_finalize (GObject *object)
{
	EUICustomizer *self = E_UI_CUSTOMIZER (object);

	g_clear_object (&self->parser);
	g_clear_pointer (&self->filename, g_free);
	g_clear_pointer (&self->registered_elems, g_hash_table_destroy);

	G_OBJECT_CLASS (e_ui_customizer_parent_class)->finalize (object);
}

static void
e_ui_customizer_class_init (EUICustomizerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_ui_customizer_set_property;
	object_class->get_property = e_ui_customizer_get_property;
	object_class->constructed = e_ui_customizer_constructed;
	object_class->finalize = e_ui_customizer_finalize;

	/**
	 * EUICustomizer:filename:
	 *
	 * A file name, into which the customizations are saved and loaded from.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_FILENAME] = g_param_spec_string ("filename", NULL, NULL, NULL,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * EUICustomizer:manager:
	 *
	 * An #EUIManager holding the default settings/layout of the UI actions.
	 *
	 * Since: 3.56
	 **/
	properties[PROP_MANAGER] = g_param_spec_object ("manager", NULL, NULL, E_TYPE_UI_MANAGER,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	/* void		changed		(EUICustomizer *customizer); */
	/**
	 * EUICustomizer::changed:
	 * @customizer: an #EUICustomizer
	 *
	 * A signal called when the @customizer content changed. It's a signal to
	 * regenerate the UI elements.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_CHANGED] = g_signal_new ("changed",
		E_TYPE_UI_CUSTOMIZER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/* void   (* accels_changed)	(EUICustomizer *customizer,
					 const gchar *action_name,
					 GPtrArray *old_accels,
					 GPtrArray *new_accels); */
	/**
	 * EUICustomizer::accels-changed:
	 * @customizer: an #EUICustomizer
	 * @action_name: an action name
	 * @old_accels: (element-type utf8) (nullable): accels used before the change, or %NULL
	 * @new_accels: (element-type utf8) (nullable): accels used after the change, or %NULL
	 *
	 * Emitted when the settings for the accels change. When the @old_accels
	 * is %NULL, the there had not been set any accels for the @action_name
	 * yet. When the @new_accels is %NULL, the accels for the @action_name had
	 * been removed. For the %NULL the accels defined on the #EUIAction should
	 * be used.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_ACCELS_CHANGED] = g_signal_new ("accels-changed",
		E_TYPE_UI_CUSTOMIZER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		NULL,
		G_TYPE_NONE, 3,
		G_TYPE_STRING,
		G_TYPE_PTR_ARRAY,
		G_TYPE_PTR_ARRAY);
}

static void
e_ui_customizer_init (EUICustomizer *self)
{
	self->parser = e_ui_parser_new ();
	self->registered_elems = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free); /* gchar *id ~> gchar *display_name */

	g_signal_connect_object (self->parser, "accels-changed",
		G_CALLBACK (ui_customizer_parser_accels_changed_cb), self, 0);
}

/**
 * e_ui_customizer_get_filename:
 * @self: an #EUICustomizer
 *
 * Returns the file name the @self was constructed with.
 *
 * Returns: (transfer none): the file name the @self was constructed with
 *
 * Since: 3.56
 **/
const gchar *
e_ui_customizer_get_filename (EUICustomizer *self)
{
	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), NULL);

	return self->filename;
}

/**
 * e_ui_customizer_get_manager:
 * @self: an #EUICustomizer
 *
 * Returns the #EUIManager the @self was constructed for.
 *
 * Returns: (transfer none): the #EUIManager the @self was constructed for
 *
 * Since: 3.56
 **/
EUIManager *
e_ui_customizer_get_manager (EUICustomizer *self)
{
	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), NULL);

	return self->manager;
}

/**
 * e_ui_customizer_get_parser:
 * @self: an #EUICustomizer
 *
 * Returns the #EUIParser the @self uses to store the customizations.
 *
 * Returns: (transfer none): the #EUIParser the @self uses to store the customizations
 *
 * Since: 3.56
 **/
EUIParser *
e_ui_customizer_get_parser (EUICustomizer *self)
{
	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), NULL);

	return self->parser;
}

/**
 * e_ui_customizer_load:
 * @self: an #EUICustomizer
 * @error: an output variable for a #GError, or %NULL
 *
 * Loads any changes for the @self from the file path provided
 * in the construction time, discarding any current changes.
 * It's usually not needed to call this function, because it's
 * called during the @self construction.
 *
 * Note it's okay to call the function when the file does not exist.
 *
 * Returns: whether succeeded
 *
 * Since: 3.56
 **/
gboolean
e_ui_customizer_load (EUICustomizer *self,
		      GError **error)
{
	gboolean success = TRUE;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), FALSE);

	e_ui_parser_clear (self->parser);

	if (!self->filename) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "no file set");
		return FALSE;
	}

	if (!e_ui_parser_merge_file (self->parser, self->filename, &local_error) && local_error &&
	    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) &&
	    !g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
		g_prefix_error (&local_error, "Failed to read '%s': ", self->filename);
		g_propagate_error (error, local_error);
		local_error = NULL;
		success = FALSE;
	}

	g_clear_error (&local_error);

	return success;
}

/**
 * e_ui_customizer_save:
 * @self: an #EUICustomizer
 * @error: an output variable for a #GError, or %NULL
 *
 * Saves any changes in the @self to the file path provided
 * in the construction time.
 *
 * Returns: whether succeeded
 *
 * Since: 3.56
 **/
gboolean
e_ui_customizer_save (EUICustomizer *self,
		      GError **error)
{
	gchar *content;
	gboolean success = TRUE;

	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), FALSE);

	if (!self->filename) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "no file set");
		return FALSE;
	}

	content = e_ui_parser_export (self->parser, E_UI_PARSER_EXPORT_FLAG_INDENT);

	if (content && *content) {
		success = g_file_set_contents (self->filename, content, -1, error);
	} else if (g_unlink (self->filename) == -1) {
		gint err = errno;
		gint code = g_file_error_from_errno (err);

		if (code != G_FILE_ERROR_NOENT) {
			g_set_error_literal (error, G_FILE_ERROR, code,  g_strerror (err));
			success = FALSE;
		}
	}

	g_free (content);

	return success;
}

/**
 * e_ui_customizer_register:
 * @self: an #EUICustomizer
 * @id: an ID of a customizable UI element
 * @display_name: (nullable): name shown in the UI, localized, or %NULL
 *
 * Registers a UI element as customizable. The @id should reference
 * an existing element in the corresponding #EUIManager, which is of
 * kind menu, headerbar or toolbar.
 *
 * The @display_name can override the user-visible name of the customizable
 * element. If %NULL, the display name is assigned from the kind of the element.
 *
 * Since: 3.56
 **/
void
e_ui_customizer_register (EUICustomizer *self,
			  const gchar *id,
			  const gchar *display_name)
{
	EUIParser *parser;
	EUIElement *root, *elem;
	EUIElementKind kind;

	g_return_if_fail (E_IS_UI_CUSTOMIZER (self));
	g_return_if_fail (id != NULL);

	parser = e_ui_manager_get_parser (self->manager);
	root = e_ui_parser_get_root (parser);
	g_return_if_fail (root != NULL);

	elem = e_ui_element_get_child_by_id (root, id);
	g_return_if_fail (elem != NULL);

	kind = e_ui_element_get_kind (elem);
	g_return_if_fail (kind == E_UI_ELEMENT_KIND_HEADERBAR || kind == E_UI_ELEMENT_KIND_TOOLBAR || kind == E_UI_ELEMENT_KIND_MENU);

	if (display_name == NULL) {
		switch (kind) {
		case E_UI_ELEMENT_KIND_HEADERBAR:
			display_name = _("Headerbar");
			break;
		case E_UI_ELEMENT_KIND_TOOLBAR:
			display_name = _("Toolbar");
			break;
		case E_UI_ELEMENT_KIND_MENU:
			display_name = _("Menu");
			break;
		default:
			g_return_if_reached ();
		}
	}

	g_hash_table_insert (self->registered_elems, g_strdup (id), g_strdup (display_name));
}

/**
 * e_ui_customizer_list_registered:
 * @self: an #EUICustomizer
 *
 * Returns an array of ID-s of all the registered elements by the e_ui_customizer_register().
 * Free the returned array, if not %NULL, with the g_ptr_array_unref(), when no longer needed.
 *
 * Returns: (transfer container) (element-type utf8) (nullable): a #GPtrArray with id-s
 *    of the registered customizable elements, or %NULL, when none was registered
 *
 * Since: 3.56
 **/
GPtrArray *
e_ui_customizer_list_registered (EUICustomizer *self)
{
	GPtrArray *ids;
	GHashTableIter iter;
	gpointer key = NULL;

	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), NULL);

	if (!g_hash_table_size (self->registered_elems))
		return NULL;

	ids = g_ptr_array_new_full (g_hash_table_size (self->registered_elems), g_free);

	g_hash_table_iter_init (&iter, self->registered_elems);
	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		g_ptr_array_add (ids, g_strdup (key));
	}

	return ids;
}

/**
 * e_ui_customizer_get_registered_display_name:
 * @self: an #EUICustomizer
 * @id: an ID of the registered element
 *
 * Returns stored display name for the registered @id by the e_ui_customizer_register().
 * This display name represents a user-visible text, under which the part shows
 * up in the customization dialog.
 *
 * Returns: (nullable): a stored display name for the @id, or %NULL, when
 *    the @id is unknown.
 *
 * Since: 3.56
 **/
const gchar *
e_ui_customizer_get_registered_display_name (EUICustomizer *self,
					     const gchar *id)
{
	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	return g_hash_table_lookup (self->registered_elems, id);
}

/**
 * e_ui_customizer_get_element:
 * @self: an #EUICustomizer
 * @id: an element ID
 *
 * Returns a toplevel headerbar, toolbar or menu with ID @id, if such
 * had been customized.
 *
 * Returns: (nullable) (transfer none): a customized #EUIElement
 *    of the ID @id, or %NULL when not customized
 *
 * Since: 3.56
 **/
EUIElement *
e_ui_customizer_get_element (EUICustomizer *self,
			     const gchar *id)
{
	EUIElement *root;

	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	root = e_ui_parser_get_root (self->parser);
	if (!root)
		return NULL;

	return e_ui_element_get_child_by_id (root, id);
}

/**
 * e_ui_customizer_get_accels:
 * @self: an #EUICustomizer
 * @action_name: an action name
 *
 * Returns an array of the defined accelerators for the @action_name, to be used
 * instead of those defined in the code. An empty array means no accels to be used,
 * while a %NULL means no accels had been set for the @action_name.
 *
 * The first item of the returned array is meant as the main accelerator,
 * while the following are secondary accelerators.
 *
 * Returns: (nullable) (transfer none) (element-type utf8): a #GPtrArray with
 *    the accelerators for the @action_name, or %NULL
 *
 * Since: 3.56
 **/
GPtrArray *
e_ui_customizer_get_accels (EUICustomizer *self,
			    const gchar *action_name)
{
	g_return_val_if_fail (E_IS_UI_CUSTOMIZER (self), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	return e_ui_parser_get_accels (self->parser, action_name);
}

/**
 * e_ui_customizer_take_accels:
 * @self: an #EUICustomizer
 * @action_name: an action name
 * @accels: (nullable) (transfer full) (element-type utf8): accelerators to use, or %NULL to unset
 *
 * Sets the @accels as the accelerators for the action @action_name.
 * Use %NULL to unset any previous values.
 *
 * The function assumes ownership of the @accels.
 *
 * Since: 3.56
 **/
void
e_ui_customizer_take_accels (EUICustomizer *self,
			     const gchar *action_name,
			     GPtrArray *accels)
{
	g_return_if_fail (E_IS_UI_CUSTOMIZER (self));
	g_return_if_fail (action_name != NULL);

	e_ui_parser_take_accels (self->parser, action_name, accels);
}

/**
 * e_ui_customizer_util_dup_filename_for_component:
 * @component: a component name
 *
 * Builds a full path to a file, where the @component may store
 * its UI customizations.
 *
 * Returns: (transfer full): a full path to store UI customizations
 *
 * Since: 3.56
 **/
gchar *
e_ui_customizer_util_dup_filename_for_component (const gchar *component)
{
	gchar *full_path, *filename;

	g_return_val_if_fail (component != NULL, NULL);

	filename = g_strconcat (component, ".eui", NULL);
	full_path = g_build_filename (e_get_user_config_dir (), filename, NULL);
	g_free (filename);

	return full_path;
}

typedef struct _ContextMenuData {
	GtkWidget *widget;
	gchar *id;
	EUICustomizeFunc func;
	gpointer user_data;
} ContextMenuData;

static void
context_menu_data_free (ContextMenuData *data)
{
	if (data) {
		g_free (data->id);
		g_free (data);
	}
}

static void
e_ui_customizer_toolbar_context_menu_activate_cb (GtkMenuItem *item,
						  gpointer user_data)
{
	ContextMenuData *data = user_data;

	data->func (data->widget, data->id, data->user_data);
}

static gboolean
e_ui_customizer_toolbar_context_menu_cb (GtkWidget *toolbar,
					 gint xx,
					 gint yy,
					 gint button,
					 gpointer user_data)
{
	ContextMenuData *data = user_data, *data2;
	GdkEvent *event;
	GtkMenu *menu;
	GtkWidget *item;

	g_return_val_if_fail (data != NULL, FALSE);

	menu = GTK_MENU (gtk_menu_new ());
	item = gtk_menu_item_new_with_mnemonic (_("_Customize Toolbar…"));
	gtk_widget_set_visible (item, TRUE);

	data2 = g_new0 (ContextMenuData, 1);
	data2->widget = data->widget;
	data2->id = g_strdup (data->id);
	data2->func = data->func;
	data2->user_data = data->user_data;

	g_signal_connect_data (item, "activate",
		G_CALLBACK (e_ui_customizer_toolbar_context_menu_activate_cb), data2, (GClosureNotify) context_menu_data_free, 0);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	gtk_menu_attach_to_widget (menu, toolbar, NULL);
	e_util_connect_menu_detach_after_deactivate (menu);

	event = gtk_get_current_event ();

	gtk_menu_popup_at_pointer (menu, event);

	g_clear_pointer (&event, gdk_event_free);

	return TRUE;
}

/**
 * e_ui_customizer_util_attach_toolbar_context_menu:
 * @widget: a #GtkToolbar widget
 * @toolbar_id: id of the toolbar in a .eui definition
 * @func: an #EUICustomizeFunc to call
 * @user_data: user data passed to the @func
 *
 * Attaches a context menu popup signal handler for the @widget,
 * which will popup a menu with "Customize Toolbar..." option and
 * when this is selected, the @func is called.
 *
 * Since: 3.56
 **/
void
e_ui_customizer_util_attach_toolbar_context_menu (GtkWidget *widget,
						  const gchar *toolbar_id,
						  EUICustomizeFunc func,
						  gpointer user_data)
{
	ContextMenuData *data;

	g_return_if_fail (GTK_IS_TOOLBAR (widget));
	g_return_if_fail (toolbar_id != NULL);
	g_return_if_fail (func != NULL);

	data = g_new0 (ContextMenuData, 1);
	data->widget = widget;
	data->id = g_strdup (toolbar_id);
	data->func = func;
	data->user_data = user_data;

	g_signal_connect_data (widget, "popup-context-menu",
		G_CALLBACK (e_ui_customizer_toolbar_context_menu_cb), data, (GClosureNotify) context_menu_data_free, 0);
}
