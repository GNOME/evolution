/*
 * e-proxy-preferences.c
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
 * SECTION: e-proxy-preferences
 * @include: e-util/e-util.h
 * @short_description: Manage network proxy preferences
 *
 * #EProxyPreferences is the main widget for displaying network proxy
 * preferences.  A link button toggles between a basic mode (for most
 * users) and advanced mode.  Basic mode only shows proxy details for
 * the built-in proxy profile, which all new accounts use by default.
 * Advanced mode reveals a sidebar of proxy profiles, allowing users
 * to create or delete custom profiles and apply them to particular
 * accounts.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-proxy-editor.h"
#include "e-proxy-link-selector.h"
#include "e-proxy-selector.h"

#include "e-proxy-preferences.h"

/* Rate-limit committing proxy changes to the registry. */
#define COMMIT_DELAY_SECS 2

struct _EProxyPreferencesPrivate {
	ESourceRegistry *registry;
	gulong source_changed_handler_id;

	/* The widgets are not referenced. */
	GtkWidget *proxy_selector;
	GtkWidget *proxy_editor;
	GtkWidget *toplevel;

	gulong toplevel_notify_id;

	GMutex commit_lock;
	guint commit_timeout_id;
	GHashTable *commit_sources;

	gboolean show_advanced;
};

enum {
	PROP_0,
	PROP_REGISTRY,
	PROP_SHOW_ADVANCED
};

/* Forward Declarations */
static void	proxy_preferences_commit_changes
					(EProxyPreferences *preferences);
static void	proxy_preferences_toplevel_notify_visible_cb
					(GtkWidget *widget,
					 GParamSpec *param,
					 EProxyPreferences *preferences);

G_DEFINE_TYPE_WITH_PRIVATE (EProxyPreferences, e_proxy_preferences, GTK_TYPE_BOX)

static gboolean
proxy_preferences_commit_timeout_cb (gpointer user_data)
{
	EProxyPreferences *preferences = user_data;

	proxy_preferences_commit_changes (preferences);

	return FALSE;
}

static void
proxy_preferences_commit_stash (EProxyPreferences *preferences,
                                ESource *source,
                                gboolean start_timeout)
{
	gboolean commit_now = FALSE;

	g_mutex_lock (&preferences->priv->commit_lock);

	g_hash_table_replace (
		preferences->priv->commit_sources,
		e_source_dup_uid (source),
		e_weak_ref_new (source));

	if (preferences->priv->commit_timeout_id > 0) {
		g_source_remove (preferences->priv->commit_timeout_id);
		preferences->priv->commit_timeout_id = 0;
	}

	if (start_timeout) {
		if (!preferences->priv->toplevel) {
			GtkWidget *toplevel;

			toplevel = gtk_widget_get_toplevel (GTK_WIDGET (preferences));

			if (toplevel) {
				g_object_weak_ref (G_OBJECT (toplevel),
					(GWeakNotify) g_nullify_pointer, &preferences->priv->toplevel);

				preferences->priv->toplevel_notify_id = g_signal_connect (
					toplevel, "notify::visible",
					G_CALLBACK (proxy_preferences_toplevel_notify_visible_cb), preferences);

				preferences->priv->toplevel = toplevel;

				if (!gtk_widget_is_visible (toplevel)) {
					start_timeout = FALSE;
					commit_now = TRUE;
				}
			}
		}
	}

	if (start_timeout) {
		preferences->priv->commit_timeout_id =
			e_named_timeout_add_seconds (
				COMMIT_DELAY_SECS,
				proxy_preferences_commit_timeout_cb,
				preferences);
	}

	g_mutex_unlock (&preferences->priv->commit_lock);

	if (commit_now)
		e_proxy_preferences_submit (preferences);
}

static GList *
proxy_preferences_commit_claim (EProxyPreferences *preferences)
{
	GQueue queue = G_QUEUE_INIT;
	GList *list, *link;

	g_mutex_lock (&preferences->priv->commit_lock);

	if (preferences->priv->commit_timeout_id > 0) {
		g_source_remove (preferences->priv->commit_timeout_id);
		preferences->priv->commit_timeout_id = 0;
	}

	/* Returns a list of GWeakRefs which may hold an ESource. */
	list = g_hash_table_get_values (preferences->priv->commit_sources);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source;

		source = g_weak_ref_get ((GWeakRef *) link->data);
		if (source != NULL)
			g_queue_push_tail (&queue, source);
	}

	g_list_free (list);

	g_hash_table_remove_all (preferences->priv->commit_sources);

	g_mutex_unlock (&preferences->priv->commit_lock);

	return g_queue_peek_head_link (&queue);
}

static gboolean
proxy_preferences_activate_link_cb (GtkLinkButton *button,
                                    EProxyPreferences *preferences)
{
	EProxySelector *selector;

	selector = E_PROXY_SELECTOR (preferences->priv->proxy_selector);

	if (e_proxy_preferences_get_show_advanced (preferences)) {
		/* Basic mode always shows the built-in proxy profile. */
		e_proxy_preferences_set_show_advanced (preferences, FALSE);
		e_proxy_selector_set_selected (selector, NULL);
	} else {
		e_proxy_preferences_set_show_advanced (preferences, TRUE);
	}

	return TRUE;
}

static gboolean
proxy_preferences_switch_to_label (GBinding *binding,
                                   const GValue *source_value,
                                   GValue *target_value,
                                   gpointer user_data)
{
	const gchar *string;

	if (g_value_get_boolean (source_value))
		string = _("Switch to Basic Proxy Preferences");
	else
		string = _("Switch to Advanced Proxy Preferences");

	g_value_set_string (target_value, string);

	return TRUE;
}

static gboolean
proxy_preferences_source_to_display_name (GBinding *binding,
                                          const GValue *source_value,
                                          GValue *target_value,
                                          gpointer user_data)
{
	ESource *source;
	gchar *display_name;

	source = g_value_get_object (source_value);
	g_return_val_if_fail (source != NULL, FALSE);

	display_name = e_source_dup_display_name (source);
	g_value_take_string (target_value, display_name);

	return TRUE;
}

static void
proxy_preferences_write_done_cb (GObject *source_object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	ESource *source;
	EProxyPreferences *preferences;
	GError *error = NULL;

	source = E_SOURCE (source_object);
	preferences = E_PROXY_PREFERENCES (user_data);

	e_source_write_finish (source, result, &error);

	/* FIXME Display the error in an alert sink. */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_object_unref (preferences);
}

static void
proxy_preferences_commit_changes (EProxyPreferences *preferences)
{
	GList *list, *link;

	list = proxy_preferences_commit_claim (preferences);

	for (link = list; link != NULL; link = g_list_next (link)) {
		e_source_write (
			E_SOURCE (link->data), NULL,
			proxy_preferences_write_done_cb,
			g_object_ref (preferences));
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
proxy_preferences_toplevel_notify_visible_cb (GtkWidget *widget,
					      GParamSpec *param,
					      EProxyPreferences *preferences)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (E_IS_PROXY_PREFERENCES (preferences));

	/* The toplevel widget was hidden, save anything pending immediately */
	if (!gtk_widget_is_visible (widget))
		e_proxy_preferences_submit (preferences);
}

static void
proxy_preferences_source_changed_cb (ESourceRegistry *registry,
                                     ESource *source,
                                     EProxyPreferences *preferences)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_PROXY))
		proxy_preferences_commit_stash (preferences, source, TRUE);
}

static void
proxy_preferences_set_registry (EProxyPreferences *preferences,
                                ESourceRegistry *registry)
{
	gulong handler_id;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (preferences->priv->registry == NULL);

	preferences->priv->registry = g_object_ref (registry);

	handler_id = g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (proxy_preferences_source_changed_cb), preferences);
	preferences->priv->source_changed_handler_id = handler_id;
}

static void
proxy_preferences_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			proxy_preferences_set_registry (
				E_PROXY_PREFERENCES (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_ADVANCED:
			e_proxy_preferences_set_show_advanced (
				E_PROXY_PREFERENCES (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_preferences_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_proxy_preferences_get_registry (
				E_PROXY_PREFERENCES (object)));
			return;

		case PROP_SHOW_ADVANCED:
			g_value_set_boolean (
				value,
				e_proxy_preferences_get_show_advanced (
				E_PROXY_PREFERENCES (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_preferences_dispose (GObject *object)
{
	EProxyPreferences *self = E_PROXY_PREFERENCES (object);

	if (self->priv->toplevel) {
		g_object_weak_unref (G_OBJECT (self->priv->toplevel),
			(GWeakNotify) g_nullify_pointer, &self->priv->toplevel);

		if (self->priv->toplevel_notify_id) {
			g_signal_handler_disconnect (self->priv->toplevel, self->priv->toplevel_notify_id);
			self->priv->toplevel_notify_id = 0;
		}

		self->priv->toplevel = NULL;
	}

	if (self->priv->source_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_changed_handler_id);
		self->priv->source_changed_handler_id = 0;
	}

	if (self->priv->commit_timeout_id > 0) {
		g_source_remove (self->priv->commit_timeout_id);
		self->priv->commit_timeout_id = 0;

		/* Make sure the changes are committed, or at least its write invoked */
		proxy_preferences_commit_changes (self);
	}

	g_clear_object (&self->priv->registry);

	g_hash_table_remove_all (self->priv->commit_sources);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_proxy_preferences_parent_class)->dispose (object);
}

static void
proxy_preferences_finalize (GObject *object)
{
	EProxyPreferences *self = E_PROXY_PREFERENCES (object);

	g_mutex_clear (&self->priv->commit_lock);
	g_hash_table_destroy (self->priv->commit_sources);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_proxy_preferences_parent_class)->finalize (object);
}

static void
proxy_preferences_constructed (GObject *object)
{
	EProxyPreferences *preferences;
	ESourceRegistry *registry;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *container2;
	PangoAttribute *attr;
	PangoAttrList *attr_list;
	GList *list;
	const gchar *extension_name;
	gboolean show_advanced;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_proxy_preferences_parent_class)->constructed (object);

	preferences = E_PROXY_PREFERENCES (object);
	registry = e_proxy_preferences_get_registry (preferences);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (preferences), GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing (GTK_BOX (preferences), 12);

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 12);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 12);
	gtk_box_pack_start (GTK_BOX (preferences), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_proxy_selector_new (registry);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_widget_set_size_request (widget, 200, -1);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 3);
	preferences->priv->proxy_selector = widget;  /* do not reference */

	e_binding_bind_property (
		preferences, "show-advanced",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	attr_list = pango_attr_list_new ();
	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	pango_attr_list_insert (attr_list, attr);

	widget = gtk_label_new ("");
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		preferences->priv->proxy_selector, "selected",
		widget, "label",
		G_BINDING_SYNC_CREATE,
		proxy_preferences_source_to_display_name,
		NULL, NULL, NULL);

	pango_attr_list_unref (attr_list);

	widget = e_proxy_editor_new (registry);
	gtk_widget_set_margin_start (widget, 12);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	preferences->priv->proxy_editor = widget;  /* do not reference */
	gtk_widget_show (widget);

	e_binding_bind_property (
		preferences->priv->proxy_selector, "selected",
		widget, "source",
		G_BINDING_SYNC_CREATE);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_margin_start (widget, 12);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);

	e_binding_bind_property (
		preferences, "show-advanced",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	container = widget;

	widget = gtk_label_new (
		_("Apply custom proxy settings to these accounts:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container2 = widget;

	widget = e_proxy_link_selector_new (registry);
	gtk_container_add (GTK_CONTAINER (container2), widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		preferences->priv->proxy_selector, "selected",
		widget, "target-source",
		G_BINDING_SYNC_CREATE);

	/* This is bound to the GtkBox created above. */
	e_binding_bind_property (
		widget, "show-toggles",
		container, "visible",
		G_BINDING_SYNC_CREATE);

	widget = gtk_link_button_new ("");
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_tooltip_markup (
		widget, _(
		"<b>Advanced Proxy Preferences</b> lets you "
		"define alternate network proxies and apply "
		"them to specific accounts"));
	gtk_box_pack_start (GTK_BOX (preferences), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		preferences, "show-advanced",
		widget, "label",
		G_BINDING_SYNC_CREATE,
		proxy_preferences_switch_to_label,
		NULL, NULL, NULL);

	e_binding_bind_property (
		preferences, "show-advanced",
		widget, "has-tooltip",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	g_signal_connect (
		widget, "activate-link",
		G_CALLBACK (proxy_preferences_activate_link_cb),
		preferences);

	extension_name = E_SOURCE_EXTENSION_PROXY;
	list = e_source_registry_list_sources (registry, extension_name);
	show_advanced = (g_list_length (list) > 1);
	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Switch to advanced mode if there are multiple proxy profiles. */
	e_proxy_preferences_set_show_advanced (preferences, show_advanced);
}

static void
e_proxy_preferences_class_init (EProxyPreferencesClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = proxy_preferences_set_property;
	object_class->get_property = proxy_preferences_get_property;
	object_class->dispose = proxy_preferences_dispose;
	object_class->finalize = proxy_preferences_finalize;
	object_class->constructed = proxy_preferences_constructed;

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
		PROP_SHOW_ADVANCED,
		g_param_spec_boolean (
			"show-advanced",
			"Show Advanced",
			"Show advanced proxy preferences",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_proxy_preferences_init (EProxyPreferences *preferences)
{
	GHashTable *commit_sources;

	/* Keys are UIDs, values are allocated GWeakRefs. */
	commit_sources = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) e_weak_ref_free);

	preferences->priv = e_proxy_preferences_get_instance_private (preferences);

	g_mutex_init (&preferences->priv->commit_lock);
	preferences->priv->commit_sources = commit_sources;
}

/**
 * e_proxy_preferences_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EProxyPreferences widget using #ESource instances in
 * @registry.
 *
 * Returns: a new #EProxyPreferences
 **/
GtkWidget *
e_proxy_preferences_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_PROXY_PREFERENCES,
		"registry", registry, NULL);
}

/**
 * e_proxy_preferences_submit:
 * @preferences: an #EProxyPreferences
 *
 * Writes the displayed proxy profile details to the #ESource being edited,
 * and submits the changes to the registry service asynchronously.
 *
 * Normally changes are submitted to the registry service automatically
 * after a brief delay, but changes may sometimes need to be submitted
 * explicitly such as when the top-level window is closing.
 **/
void
e_proxy_preferences_submit (EProxyPreferences *preferences)
{
	EProxyEditor *proxy_editor;
	ESource *source;

	g_return_if_fail (E_IS_PROXY_PREFERENCES (preferences));

	proxy_editor = E_PROXY_EDITOR (preferences->priv->proxy_editor);

	/* Save user changes to the proxy source. */
	e_proxy_editor_save (proxy_editor);

	/* This part normally happens from a "source-changed"
	 * signal handler, but we can't wait for that here. */
	source = e_proxy_editor_ref_source (proxy_editor);
	proxy_preferences_commit_stash (preferences, source, FALSE);
	g_object_unref (source);

	/* Commit any pending changes immediately. */
	proxy_preferences_commit_changes (preferences);
}

/**
 * e_proxy_preferences_get_registry:
 * @preferences: an #EProxyPreferences
 *
 * Returns the #ESourceRegistry passed to e_proxy_preferences_new().
 *
 * Returns: an #ESourceRegistry
 **/
ESourceRegistry *
e_proxy_preferences_get_registry (EProxyPreferences *preferences)
{
	g_return_val_if_fail (E_IS_PROXY_PREFERENCES (preferences), NULL);

	return preferences->priv->registry;
}

/**
 * e_proxy_preferences_get_show_advanced:
 * @preferences: an #EProxyPreferences
 *
 * Returns whether @preferences is currently in advanced mode.
 *
 * Returns: whether advanced proxy preferences are visible
 **/
gboolean
e_proxy_preferences_get_show_advanced (EProxyPreferences *preferences)
{
	g_return_val_if_fail (E_IS_PROXY_PREFERENCES (preferences), FALSE);

	return preferences->priv->show_advanced;
}

/**
 * e_proxy_preferences_set_show_advanced:
 * @preferences: an #EProxyPreferences
 * @show_advanced: whether to show advanced proxy preferences
 *
 * Switches @preferences to advanced mode if @show_advanced is %TRUE,
 * or to basic mode if @show_advanced is %FALSE.
 **/
void
e_proxy_preferences_set_show_advanced (EProxyPreferences *preferences,
                                       gboolean show_advanced)
{
	g_return_if_fail (E_IS_PROXY_PREFERENCES (preferences));

	if (show_advanced == preferences->priv->show_advanced)
		return;

	preferences->priv->show_advanced = show_advanced;

	g_object_notify (G_OBJECT (preferences), "show-advanced");
}

