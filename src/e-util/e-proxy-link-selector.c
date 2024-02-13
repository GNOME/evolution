/*
 * e-proxy-link-selector.c
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
 * SECTION: e-proxy-link-selector
 * @include: e-util/e-util.h
 * @short_description: Link accounts to a proxy profile
 *
 * #EProxyLinkSelector shows all network-based accounts in a tree view,
 * with a checkbox next to each account.  The checkbox allows users to
 * choose between linking the account to a pre-determined user-defined
 * proxy profile, or to the built-in default proxy profile.
 **/

#include "evolution-config.h"

#include "e-proxy-link-selector.h"

struct _EProxyLinkSelectorPrivate {
	ESource *target_source;
	ESource *fallback_source;
};

enum {
	PROP_0,
	PROP_TARGET_SOURCE
};

G_DEFINE_TYPE_WITH_PRIVATE (EProxyLinkSelector, e_proxy_link_selector, E_TYPE_SOURCE_SELECTOR)

static gboolean
proxy_link_selector_target_source_to_show_toggles (GBinding *binding,
                                                   const GValue *source_value,
                                                   GValue *target_value,
                                                   gpointer user_data)
{
	ESource *target_source;
	ESource *fallback_source;
	gboolean show_toggles;

	fallback_source = E_SOURCE (user_data);
	target_source = g_value_get_object (source_value);
	show_toggles = !e_source_equal (target_source, fallback_source);
	g_value_set_boolean (target_value, show_toggles);

	return TRUE;
}

static void
proxy_link_selector_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TARGET_SOURCE:
			e_proxy_link_selector_set_target_source (
				E_PROXY_LINK_SELECTOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_link_selector_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TARGET_SOURCE:
			g_value_take_object (
				value,
				e_proxy_link_selector_ref_target_source (
				E_PROXY_LINK_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_link_selector_dispose (GObject *object)
{
	EProxyLinkSelector *self = E_PROXY_LINK_SELECTOR (object);

	g_clear_object (&self->priv->target_source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_proxy_link_selector_parent_class)->dispose (object);
}

static void
proxy_link_selector_constructed (GObject *object)
{
	EProxyLinkSelector *self = E_PROXY_LINK_SELECTOR (object);
	ESourceSelector *selector;
	ESourceRegistry *registry;
	ESource *builtin_proxy;

	selector = E_SOURCE_SELECTOR (object);
	registry = e_source_selector_get_registry (selector);

	/* Set the target and fallback sources before chaining up. */

	builtin_proxy = e_source_registry_ref_builtin_proxy (registry);
	g_return_if_fail (builtin_proxy != NULL);

	self->priv->target_source = g_object_ref (builtin_proxy);
	self->priv->fallback_source = g_object_ref (builtin_proxy);

	g_object_unref (builtin_proxy);

	/* Hide toggle buttons when the target source is the same as
	 * the fallback source since toggling the buttons would have
	 * no effect in that particular case. */
	e_binding_bind_property_full (
		selector, "target-source",
		selector, "show-toggles",
		G_BINDING_SYNC_CREATE,
		proxy_link_selector_target_source_to_show_toggles,
		NULL,
		g_object_ref (self->priv->fallback_source),
		(GDestroyNotify) g_object_unref);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_proxy_link_selector_parent_class)->constructed (object);

	/* This triggers a model rebuild, so chain up first. */
	e_source_selector_set_show_icons (selector, TRUE);
}

static gboolean
proxy_link_selector_get_source_selected (ESourceSelector *selector,
                                         ESource *source)
{
	EProxyLinkSelector *link_selector;
	ESourceAuthentication *extension;
	const gchar *extension_name;
	const gchar *target_uid;
	gboolean selected = FALSE;
	gchar *uid;

	link_selector = E_PROXY_LINK_SELECTOR (selector);

	/* Make sure this source has an Authentication extension. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_AUTHENTICATION (extension), FALSE);

	uid = e_source_authentication_dup_proxy_uid (extension);
	target_uid = e_source_get_uid (link_selector->priv->target_source);
	selected = (g_strcmp0 (uid, target_uid) == 0);
	g_free (uid);

	return selected;
}

static gboolean
proxy_link_selector_set_source_selected (ESourceSelector *selector,
                                         ESource *source,
                                         gboolean selected)
{
	EProxyLinkSelector *link_selector;
	ESourceAuthentication *extension;
	ESource *target_source;
	const gchar *extension_name;
	const gchar *new_target_uid;
	const gchar *old_target_uid;

	link_selector = E_PROXY_LINK_SELECTOR (selector);

	/* Make sure this source has an Authentication extension. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_AUTHENTICATION (extension), FALSE);

	if (selected)
		target_source = link_selector->priv->target_source;
	else
		target_source = link_selector->priv->fallback_source;

	new_target_uid = e_source_get_uid (target_source);
	old_target_uid = e_source_authentication_get_proxy_uid (extension);

	if (g_strcmp0 (new_target_uid, old_target_uid) != 0) {
		e_source_authentication_set_proxy_uid (
			extension, new_target_uid);
		e_source_selector_queue_write (selector, source);

		return TRUE;
	}

	return FALSE;
}

static void
e_proxy_link_selector_class_init (EProxyLinkSelectorClass *class)
{
	GObjectClass *object_class;
	ESourceSelectorClass *source_selector_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = proxy_link_selector_set_property;
	object_class->get_property = proxy_link_selector_get_property;
	object_class->dispose = proxy_link_selector_dispose;
	object_class->constructed = proxy_link_selector_constructed;

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->get_source_selected =
				proxy_link_selector_get_source_selected;
	source_selector_class->set_source_selected =
				proxy_link_selector_set_source_selected;

	g_object_class_install_property (
		object_class,
		PROP_TARGET_SOURCE,
		g_param_spec_object (
			"target-source",
			"Target Source",
			"The data source to link to "
			"when the checkbox is active",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_proxy_link_selector_init (EProxyLinkSelector *selector)
{
	selector->priv = e_proxy_link_selector_get_instance_private (selector);
}

/**
 * e_proxy_link_selector_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EProxyLinkSelector using #ESource instances in @registry.
 *
 * Returns: a new #EProxyLinkSelector
 **/
GtkWidget *
e_proxy_link_selector_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_PROXY_LINK_SELECTOR,
		"extension-name", E_SOURCE_EXTENSION_AUTHENTICATION,
		"registry", registry, NULL);
}

/**
 * e_proxy_link_selector_ref_target_source:
 * @selector: an #EProxyLinkSelector
 *
 * Returns the target network proxy profile #ESource.
 *
 * See e_proxy_link_selector_set_target_source() for further details.
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #ESource
 **/
ESource *
e_proxy_link_selector_ref_target_source (EProxyLinkSelector *selector)
{
	g_return_val_if_fail (E_IS_PROXY_LINK_SELECTOR (selector), NULL);

	return g_object_ref (selector->priv->target_source);
}

/**
 * e_proxy_link_selector_set_target_source:
 * @selector: an #EProxyLinkSelector
 * @target_source: an #ESource
 *
 * Sets the target network proxy profile #ESource.
 *
 * Checking the box next to an account name in @selector will link the
 * account to @target_source.  The account will then use @target_source
 * as its #GProxyResolver when connecting to a remote host.
 *
 * As a special case, if @target_source refers to the built-in network
 * proxy profile, then @selector will hide its checkboxes since they would
 * otherwise link accounts to the same #ESource when checked or unchecked.
 **/
void
e_proxy_link_selector_set_target_source (EProxyLinkSelector *selector,
                                         ESource *target_source)
{
	g_return_if_fail (E_IS_PROXY_LINK_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (target_source));

	if (target_source == selector->priv->target_source)
		return;

	g_clear_object (&selector->priv->target_source);
	selector->priv->target_source = g_object_ref (target_source);

	g_object_notify (G_OBJECT (selector), "target-source");

	e_source_selector_update_all_rows (E_SOURCE_SELECTOR (selector));
}

