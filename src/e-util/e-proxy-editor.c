/*
 * e-proxy-editor.c
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

/**
 * SECTION: e-proxy-editor
 * @include: e-util/e-util.h
 * @short_description: Edit proxy profile details
 *
 * #EProxyEditor is an editing widget for proxy profiles, as described by
 * #ESource instances with an #ESourceProxy extension.
 *
 * The editor defaults to showing the built-in proxy profile returned by
 * e_source_registry_ref_builtin_proxy(), but that can be overridden with
 * e_proxy_editor_set_source().
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-proxy-editor.h"

struct _EProxyEditorPrivate {
	ESourceRegistry *registry;
	ESource *source;

	/* The widgets are not referenced. */
	GtkWidget *method_combo_box;
	GtkWidget *http_host_entry;
	GtkWidget *http_port_spin_button;
	GtkWidget *https_host_entry;
	GtkWidget *https_port_spin_button;
	GtkWidget *socks_host_entry;
	GtkWidget *socks_port_spin_button;
	GtkWidget *ignore_hosts_entry;
	GtkWidget *autoconfig_url_entry;

	gchar *gcc_program_path;
};

enum {
	PROP_0,
	PROP_REGISTRY,
	PROP_SOURCE
};

G_DEFINE_TYPE_WITH_PRIVATE (EProxyEditor, e_proxy_editor, GTK_TYPE_GRID)

static void
proxy_editor_load (EProxyEditor *editor)
{
	ESource *source;
	ESourceProxy *extension;
	GEnumClass *enum_class;
	GEnumValue *enum_value;
	EProxyMethod method;
	const gchar *extension_name;
	gchar *autoconfig_url;
	gchar *joined_hosts;
	gchar **ignore_hosts;
	gchar *host;
	guint16 port;

	source = e_proxy_editor_ref_source (editor);
	g_return_if_fail (source != NULL);

	extension_name = E_SOURCE_EXTENSION_PROXY;
	extension = e_source_get_extension (source, extension_name);

	enum_class = g_type_class_ref (E_TYPE_PROXY_METHOD);
	method = e_source_proxy_get_method (extension);
	enum_value = g_enum_get_value (enum_class, method);
	if (enum_value != NULL)
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (editor->priv->method_combo_box),
			enum_value->value_nick);
	g_type_class_unref (enum_class);

	autoconfig_url = e_source_proxy_dup_autoconfig_url (extension);
	gtk_entry_set_text (
		GTK_ENTRY (editor->priv->autoconfig_url_entry),
		(autoconfig_url != NULL) ? autoconfig_url : "");
	g_free (autoconfig_url);

	ignore_hosts = e_source_proxy_dup_ignore_hosts (extension);
	if (ignore_hosts != NULL)
		joined_hosts = g_strjoinv (", ", ignore_hosts);
	else
		joined_hosts = NULL;
	gtk_entry_set_text (
		GTK_ENTRY (editor->priv->ignore_hosts_entry),
		(joined_hosts != NULL) ? joined_hosts : "");
	g_strfreev (ignore_hosts);
	g_free (joined_hosts);

	host = e_source_proxy_dup_http_host (extension);
	gtk_entry_set_text (
		GTK_ENTRY (editor->priv->http_host_entry),
		(host != NULL) ? host : "");
	g_free (host);

	port = e_source_proxy_get_http_port (extension);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (editor->priv->http_port_spin_button),
		(gdouble) port);

	host = e_source_proxy_dup_https_host (extension);
	gtk_entry_set_text (
		GTK_ENTRY (editor->priv->https_host_entry),
		(host != NULL) ? host : "");
	g_free (host);

	port = e_source_proxy_get_https_port (extension);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (editor->priv->https_port_spin_button),
		(gdouble) port);

	host = e_source_proxy_dup_socks_host (extension);
	gtk_entry_set_text (
		GTK_ENTRY (editor->priv->socks_host_entry),
		(host != NULL) ? host : "");
	g_free (host);

	port = e_source_proxy_get_socks_port (extension);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (editor->priv->socks_port_spin_button),
		(gdouble) port);

	g_object_unref (source);
}

static void
proxy_editor_combo_box_changed_cb (GtkComboBox *widget,
                                   EProxyEditor *editor)
{
	e_proxy_editor_save (editor);
}

static gboolean
proxy_editor_focus_out_event_cb (GtkWidget *widget,
                                 GdkEvent *event,
                                 EProxyEditor *editor)
{
	e_proxy_editor_save (editor);

	return FALSE;  /* propagate the event */
}

static void
proxy_editor_open_desktop_settings_cb (GtkButton *button,
                                       EProxyEditor *editor)
{
	gchar *command_line;
	GError *local_error = NULL;

	g_return_if_fail (editor->priv->gcc_program_path != NULL);

	command_line = g_strdup_printf (
		"%s network", editor->priv->gcc_program_path);
	g_spawn_command_line_async (command_line, &local_error);
	g_free (command_line);

	if (local_error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_error_free (local_error);
	}
}

static gboolean
proxy_editor_active_id_to_visible (GBinding *binding,
                                   const GValue *source_value,
                                   GValue *target_value,
                                   gpointer user_data)
{
	const gchar *value_nick = user_data;
	const gchar *active_id;
	gboolean visible;

	active_id = g_value_get_string (source_value);
	visible = (g_strcmp0 (active_id, value_nick) == 0);
	g_value_set_boolean (target_value, visible);

	return TRUE;
}

static void
proxy_editor_set_registry (EProxyEditor *editor,
                           ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (editor->priv->registry == NULL);

	editor->priv->registry = g_object_ref (registry);
}

static void
proxy_editor_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			proxy_editor_set_registry (
				E_PROXY_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			e_proxy_editor_set_source (
				E_PROXY_EDITOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_editor_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_proxy_editor_get_registry (
				E_PROXY_EDITOR (object)));
			return;

		case PROP_SOURCE:
			g_value_take_object (
				value,
				e_proxy_editor_ref_source (
				E_PROXY_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_editor_dispose (GObject *object)
{
	EProxyEditor *self = E_PROXY_EDITOR (object);

	if (self->priv->source)
		e_proxy_editor_save (E_PROXY_EDITOR (object));

	g_clear_object (&self->priv->registry);
	g_clear_object (&self->priv->source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_proxy_editor_parent_class)->dispose (object);
}

static void
proxy_editor_finalize (GObject *object)
{
	EProxyEditor *self = E_PROXY_EDITOR (object);

	g_free (self->priv->gcc_program_path);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_proxy_editor_parent_class)->finalize (object);
}

static void
proxy_editor_constructed (GObject *object)
{
	EProxyEditor *editor;
	ESourceRegistry *registry;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	GtkSizeGroup *size_group;
	GEnumClass *enum_class;
	GEnumValue *enum_value;
	gint row = 0;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_proxy_editor_parent_class)->constructed (object);

	editor = E_PROXY_EDITOR (object);
	registry = e_proxy_editor_get_registry (editor);

	enum_class = g_type_class_ref (E_TYPE_PROXY_METHOD);

	/* Default to the built-in proxy profile source. */
	editor->priv->source = e_source_registry_ref_builtin_proxy (registry);

	gtk_grid_set_row_spacing (GTK_GRID (editor), 6);
	gtk_grid_set_column_spacing (GTK_GRID (editor), 6);

	/* This keeps all (visible) mnemonic labels the same width. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_set_ignore_hidden (size_group, TRUE);

	widget = gtk_label_new_with_mnemonic (_("_Method:"));
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_combo_box_text_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (editor), widget, 1, 0, 1, 1);
	editor->priv->method_combo_box = widget;  /* do not reference */
	gtk_widget_show (widget);

	/*** Defer to Desktop Settings ***/

	enum_value = g_enum_get_value (enum_class, E_PROXY_METHOD_DEFAULT);
	g_return_if_fail (enum_value != NULL);

	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (editor->priv->method_combo_box),
		enum_value->value_nick, _("Defer to Desktop Settings"));

	if (editor->priv->gcc_program_path != NULL) {
		widget = gtk_button_new_with_mnemonic (
			_("_Open Desktop Settings"));
		gtk_widget_set_halign (widget, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID (editor), widget, 1, ++row, 2, 1);

		g_signal_connect (
			widget, "clicked",
			G_CALLBACK (proxy_editor_open_desktop_settings_cb),
			editor);

		e_binding_bind_property_full (
			editor->priv->method_combo_box, "active-id",
			widget, "visible",
			G_BINDING_DEFAULT,
			proxy_editor_active_id_to_visible,
			NULL,
			(gpointer) enum_value->value_nick,
			(GDestroyNotify) NULL);
	}

	/*** Manual ***/

	enum_value = g_enum_get_value (enum_class, E_PROXY_METHOD_MANUAL);
	g_return_if_fail (enum_value != NULL);

	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (editor->priv->method_combo_box),
		enum_value->value_nick, _("Manual"));

	widget = gtk_grid_new ();
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, ++row, 2, 1);

	e_binding_bind_property_full (
		editor->priv->method_combo_box, "active-id",
		widget, "visible",
		G_BINDING_DEFAULT,
		proxy_editor_active_id_to_visible,
		NULL,
		(gpointer) enum_value->value_nick,
		(GDestroyNotify) NULL);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("_HTTP Proxy:"));
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	editor->priv->http_host_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (proxy_editor_focus_out_event_cb), editor);

	widget = gtk_spin_button_new_with_range (0, G_MAXUINT16, 1);
	gtk_spin_button_set_update_policy (
		GTK_SPIN_BUTTON (widget), GTK_UPDATE_IF_VALID);
	gtk_widget_set_size_request (widget, 100, -1);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 0, 1, 1);
	editor->priv->http_port_spin_button = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (proxy_editor_focus_out_event_cb), editor);

	widget = gtk_label_new_with_mnemonic (_("H_TTPS Proxy:"));
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	editor->priv->https_host_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (proxy_editor_focus_out_event_cb), editor);

	widget = gtk_spin_button_new_with_range (0, G_MAXUINT16, 1);
	gtk_spin_button_set_update_policy (
		GTK_SPIN_BUTTON (widget), GTK_UPDATE_IF_VALID);
	gtk_widget_set_size_request (widget, 100, -1);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 1, 1, 1);
	editor->priv->https_port_spin_button = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (proxy_editor_focus_out_event_cb), editor);

	widget = gtk_label_new_with_mnemonic (_("_Socks Proxy:"));
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
	editor->priv->socks_host_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (proxy_editor_focus_out_event_cb), editor);

	widget = gtk_spin_button_new_with_range (0, G_MAXUINT16, 1);
	gtk_spin_button_set_update_policy (
		GTK_SPIN_BUTTON (widget), GTK_UPDATE_IF_VALID);
	gtk_widget_set_size_request (widget, 100, -1);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 2, 1, 1);
	editor->priv->socks_port_spin_button = widget;
	gtk_widget_show (widget);

	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (proxy_editor_focus_out_event_cb), editor);

	widget = gtk_label_new_with_mnemonic (_("_Ignore Hosts:"));
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 3, 2, 1);
	editor->priv->ignore_hosts_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (proxy_editor_focus_out_event_cb), editor);

	/*** Automatic ***/

	enum_value = g_enum_get_value (enum_class, E_PROXY_METHOD_AUTO);
	g_return_if_fail (enum_value != NULL);

	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (editor->priv->method_combo_box),
		enum_value->value_nick, _("Automatic"));

	widget = gtk_grid_new ();
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_grid_attach (GTK_GRID (editor), widget, 0, ++row, 2, 1);

	e_binding_bind_property_full (
		editor->priv->method_combo_box, "active-id",
		widget, "visible",
		G_BINDING_DEFAULT,
		proxy_editor_active_id_to_visible,
		NULL,
		(gpointer) enum_value->value_nick,
		(GDestroyNotify) NULL);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("Configuration _URL:"));
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	editor->priv->autoconfig_url_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (proxy_editor_focus_out_event_cb), editor);

	/*** None ***/

	enum_value = g_enum_get_value (enum_class, E_PROXY_METHOD_NONE);
	g_return_if_fail (enum_value != NULL);

	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (editor->priv->method_combo_box),
		enum_value->value_nick, _("No proxy"));

	widget = gtk_label_new (
		_("Use a direct connection, no proxying required."));
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
	gtk_grid_attach (GTK_GRID (editor), widget, 1, ++row, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		editor->priv->method_combo_box, "active-id",
		widget, "visible",
		G_BINDING_DEFAULT,
		proxy_editor_active_id_to_visible,
		NULL,
		(gpointer) enum_value->value_nick,
		(GDestroyNotify) NULL);
	g_object_unref (size_group);
	g_type_class_unref (enum_class);

	/* Populate the widgets. */
	proxy_editor_load (editor);

	/* Connect to this signal after the initial load. */
	g_signal_connect_after (
		editor->priv->method_combo_box, "changed",
		G_CALLBACK (proxy_editor_combo_box_changed_cb), editor);
}

static void
e_proxy_editor_class_init (EProxyEditorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = proxy_editor_set_property;
	object_class->get_property = proxy_editor_get_property;
	object_class->dispose = proxy_editor_dispose;
	object_class->finalize = proxy_editor_finalize;
	object_class->constructed = proxy_editor_constructed;

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
			"The data source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_proxy_editor_init (EProxyEditor *editor)
{
	editor->priv = e_proxy_editor_get_instance_private (editor);

	editor->priv->gcc_program_path =
		g_find_program_in_path ("gnome-control-center");
}

/**
 * e_proxy_editor_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EProxyEditor widget, initially showing details of the
 * built-in proxy profile returned by e_source_registry_ref_builtin_proxy().
 *
 * Returns: a new #EProxyEditor
 **/
GtkWidget *
e_proxy_editor_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_PROXY_EDITOR,
		"registry", registry, NULL);
}

/**
 * e_proxy_editor_save:
 * @editor: an #EProxyEditor
 *
 * Writes the proxy settings displayed in the @editor to the #ESource
 * being edited.
 *
 * This function is called automatically when the editing widgets lose input
 * focus, but it may sometimes need to be called explicitly such as when the
 * top-level window is closing.
 **/
void
e_proxy_editor_save (EProxyEditor *editor)
{
	ESource *source;
	ESourceProxy *extension;
	GEnumClass *enum_class;
	GEnumValue *enum_value;
	const gchar *active_id;
	const gchar *entry_text;
	const gchar *extension_name;
	gint spin_button_value;
	gchar **strv;

	g_return_if_fail (E_IS_PROXY_EDITOR (editor));

	source = e_proxy_editor_ref_source (editor);
	g_return_if_fail (source != NULL);

	extension_name = E_SOURCE_EXTENSION_PROXY;
	extension = e_source_get_extension (source, extension_name);

	enum_class = g_type_class_ref (E_TYPE_PROXY_METHOD);
	active_id = gtk_combo_box_get_active_id (
		GTK_COMBO_BOX (editor->priv->method_combo_box));
	enum_value = g_enum_get_value_by_nick (enum_class, active_id);
	if (enum_value != NULL)
		e_source_proxy_set_method (extension, enum_value->value);
	g_type_class_unref (enum_class);

	entry_text = gtk_entry_get_text (
		GTK_ENTRY (editor->priv->autoconfig_url_entry));
	if (entry_text != NULL && *entry_text == '\0')
		entry_text = NULL;
	e_source_proxy_set_autoconfig_url (extension, entry_text);

	entry_text = gtk_entry_get_text (
		GTK_ENTRY (editor->priv->ignore_hosts_entry));
	strv = g_strsplit (entry_text, ",", -1);
	if (strv != NULL) {
		guint length, ii;

		/* Strip leading and trailing whitespace from each element
		 * to avoid a changed notification storm between the entry
		 * (which adds a space after each comma) and ESourceProxy. */
		length = g_strv_length (strv);
		for (ii = 0; ii < length; ii++)
			g_strstrip (strv[ii]);
	}
	e_source_proxy_set_ignore_hosts (
		extension, (const gchar * const *) strv);
	g_strfreev (strv);

	entry_text = gtk_entry_get_text (
		GTK_ENTRY (editor->priv->http_host_entry));
	if (entry_text != NULL && *entry_text == '\0')
		entry_text = NULL;
	e_source_proxy_set_http_host (extension, entry_text);

	spin_button_value = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (editor->priv->http_port_spin_button));
	e_source_proxy_set_http_port (extension, (guint16) spin_button_value);

	entry_text = gtk_entry_get_text (
		GTK_ENTRY (editor->priv->https_host_entry));
	if (entry_text != NULL && *entry_text == '\0')
		entry_text = NULL;
	e_source_proxy_set_https_host (extension, entry_text);

	spin_button_value = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (editor->priv->https_port_spin_button));
	e_source_proxy_set_https_port (extension, (guint16) spin_button_value);

	entry_text = gtk_entry_get_text (
		GTK_ENTRY (editor->priv->socks_host_entry));
	if (entry_text != NULL && *entry_text == '\0')
		entry_text = NULL;
	e_source_proxy_set_socks_host (extension, entry_text);

	spin_button_value = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (editor->priv->socks_port_spin_button));
	e_source_proxy_set_socks_port (extension, (guint16) spin_button_value);

	g_object_unref (source);
}

/**
 * e_proxy_editor_get_registry:
 * @editor: an #EProxyEditor
 *
 * Returns the #ESourceRegistry passed to e_proxy_editor_get_registry().
 *
 * Returns: an #ESourceRegistry
 **/
ESourceRegistry *
e_proxy_editor_get_registry (EProxyEditor *editor)
{
	g_return_val_if_fail (E_IS_PROXY_EDITOR (editor), NULL);

	return editor->priv->registry;
}

/**
 * e_proxy_editor_ref_source:
 * @editor: an #EProxyEditor
 *
 * Returns the network proxy profile #ESource being edited.
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #ESource
 **/
ESource *
e_proxy_editor_ref_source (EProxyEditor *editor)
{
	g_return_val_if_fail (E_IS_PROXY_EDITOR (editor), NULL);

	return g_object_ref (editor->priv->source);
}

/**
 * e_proxy_editor_set_source:
 * @editor: an #EProxyEditor
 * @source: an #ESource
 *
 * Sets the network proxy profile #ESource to edit.
 *
 * This first writes the displayed proxy settings to the previous #ESource,
 * then displays the proxy details for @source.  If @source is already being
 * edited then nothing happens.
 **/
void
e_proxy_editor_set_source (EProxyEditor *editor,
                           ESource *source)
{
	g_return_if_fail (E_IS_PROXY_EDITOR (editor));
	g_return_if_fail (E_IS_SOURCE (source));

	if (e_source_equal (source, editor->priv->source))
		return;

	e_proxy_editor_save (editor);

	g_clear_object (&editor->priv->source);
	editor->priv->source = g_object_ref (source);

	proxy_editor_load (editor);

	g_object_notify (G_OBJECT (editor), "source");
}

