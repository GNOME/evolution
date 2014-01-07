/*
 * e-composer-activity.c
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

#include "e-composer-private.h"

#define E_COMPOSER_ACTIVITY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_ACTIVITY, EComposerActivityPrivate))

struct _EComposerActivityPrivate {
	EMsgComposer *composer;
	gboolean saved_editable;
};

enum {
	PROP_0,
	PROP_COMPOSER
};

G_DEFINE_TYPE (
	EComposerActivity,
	e_composer_activity,
	E_TYPE_ACTIVITY)

static void
composer_activity_lock_interface (EComposerActivity *activity)
{
	GtkActionGroup *action_group;
	EMsgComposer *composer;
	EWebViewGtkHTML *web_view;
	gboolean editable;

	composer = e_composer_activity_get_composer (activity);

	web_view = e_msg_composer_get_web_view (composer);
	editable = e_web_view_gtkhtml_get_editable (web_view);
	e_web_view_gtkhtml_set_editable (web_view, FALSE);
	activity->priv->saved_editable = editable;

	action_group = composer->priv->async_actions;
	gtk_action_group_set_sensitive (action_group, FALSE);
}

static void
composer_activity_unlock_interface (EComposerActivity *activity)
{
	GtkActionGroup *action_group;
	EMsgComposer *composer;
	EWebViewGtkHTML *web_view;
	gboolean editable;

	composer = e_composer_activity_get_composer (activity);

	editable = activity->priv->saved_editable;
	web_view = e_msg_composer_get_web_view (composer);
	e_web_view_gtkhtml_set_editable (web_view, editable);

	action_group = composer->priv->async_actions;
	gtk_action_group_set_sensitive (action_group, TRUE);
}

static void
composer_activity_set_composer (EComposerActivity *activity,
                                EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (activity->priv->composer == NULL);

	activity->priv->composer = g_object_ref (composer);

	composer_activity_lock_interface (activity);
}

static void
composer_activity_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPOSER:
			composer_activity_set_composer (
				E_COMPOSER_ACTIVITY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_activity_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPOSER:
			g_value_set_object (
				value, e_composer_activity_get_composer (
				E_COMPOSER_ACTIVITY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_activity_dispose (GObject *object)
{
	EComposerActivity *activity;

	activity = E_COMPOSER_ACTIVITY (object);

	if (activity->priv->composer != NULL) {
		composer_activity_unlock_interface (activity);
		g_object_unref (activity->priv->composer);
		activity->priv->composer = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_activity_parent_class)->dispose (object);
}

static void
e_composer_activity_class_init (EComposerActivityClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EComposerActivityPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = composer_activity_set_property;
	object_class->get_property = composer_activity_get_property;
	object_class->dispose = composer_activity_dispose;

	g_object_class_install_property (
		object_class,
		PROP_COMPOSER,
		g_param_spec_object (
			"composer",
			NULL,
			NULL,
			E_TYPE_MSG_COMPOSER,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_composer_activity_init (EComposerActivity *activity)
{
	activity->priv = E_COMPOSER_ACTIVITY_GET_PRIVATE (activity);
}

EActivity *
e_composer_activity_new (EMsgComposer *composer)
{
	return g_object_new (
		E_TYPE_COMPOSER_ACTIVITY,
		"composer", composer, NULL);
}

EMsgComposer *
e_composer_activity_get_composer (EComposerActivity *activity)
{
	g_return_val_if_fail (E_IS_COMPOSER_ACTIVITY (activity), NULL);

	return activity->priv->composer;
}
