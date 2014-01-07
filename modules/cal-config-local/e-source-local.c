/*
 * e-source-local.c
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

#include "e-source-local.h"

#define E_SOURCE_LOCAL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SOURCE_LOCAL, ESourceLocalPrivate))

struct _ESourceLocalPrivate {
	GMutex property_lock;
	GFile *custom_file;
};

enum {
	PROP_0,
	PROP_CUSTOM_FILE
};

G_DEFINE_DYNAMIC_TYPE (
	ESourceLocal,
	e_source_local,
	E_TYPE_SOURCE_EXTENSION)

static void
source_local_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CUSTOM_FILE:
			e_source_local_set_custom_file (
				E_SOURCE_LOCAL (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_local_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CUSTOM_FILE:
			g_value_take_object (
				value,
				e_source_local_dup_custom_file (
				E_SOURCE_LOCAL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_local_dispose (GObject *object)
{
	ESourceLocalPrivate *priv;

	priv = E_SOURCE_LOCAL_GET_PRIVATE (object);

	if (priv->custom_file != NULL) {
		g_object_unref (priv->custom_file);
		priv->custom_file = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_source_local_parent_class)->dispose (object);
}

static void
source_local_finalize (GObject *object)
{
	ESourceLocalPrivate *priv;

	priv = E_SOURCE_LOCAL_GET_PRIVATE (object);

	g_mutex_clear (&priv->property_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_source_local_parent_class)->finalize (object);
}

static void
e_source_local_class_init (ESourceLocalClass *class)
{
	GObjectClass *object_class;
	ESourceExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESourceLocalPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = source_local_set_property;
	object_class->get_property = source_local_get_property;
	object_class->dispose = source_local_dispose;
	object_class->finalize = source_local_finalize;

	extension_class = E_SOURCE_EXTENSION_CLASS (class);
	extension_class->name = E_SOURCE_EXTENSION_LOCAL_BACKEND;

	g_object_class_install_property (
		object_class,
		PROP_CUSTOM_FILE,
		g_param_spec_object (
			"custom-file",
			"Custom File",
			"Custom iCalendar file",
			G_TYPE_FILE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			E_SOURCE_PARAM_SETTING));
}

static void
e_source_local_class_finalize (ESourceLocalClass *class)
{
}

static void
e_source_local_init (ESourceLocal *extension)
{
	extension->priv = E_SOURCE_LOCAL_GET_PRIVATE (extension);
	g_mutex_init (&extension->priv->property_lock);
}

void
e_source_local_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_source_local_register_type (type_module);
}

GFile *
e_source_local_get_custom_file (ESourceLocal *extension)
{
	g_return_val_if_fail (E_IS_SOURCE_LOCAL (extension), NULL);

	return extension->priv->custom_file;
}

GFile *
e_source_local_dup_custom_file (ESourceLocal *extension)
{
	GFile *protected;
	GFile *duplicate;

	g_return_val_if_fail (E_IS_SOURCE_LOCAL (extension), NULL);

	g_mutex_lock (&extension->priv->property_lock);

	protected = e_source_local_get_custom_file (extension);
	duplicate = (protected != NULL) ? g_file_dup (protected) : NULL;

	g_mutex_unlock (&extension->priv->property_lock);

	return duplicate;
}

void
e_source_local_set_custom_file (ESourceLocal *extension,
                                GFile *custom_file)
{
	g_return_if_fail (E_IS_SOURCE_LOCAL (extension));

	if (custom_file != NULL) {
		g_return_if_fail (G_IS_FILE (custom_file));
		g_object_ref (custom_file);
	}

	g_mutex_lock (&extension->priv->property_lock);

	if (extension->priv->custom_file != NULL)
		g_object_unref (extension->priv->custom_file);

	extension->priv->custom_file = custom_file;

	g_mutex_unlock (&extension->priv->property_lock);

	g_object_notify (G_OBJECT (extension), "custom-file");
}

