/*
 * gal-view.c
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

#include "gal-view.h"

struct _GalViewPrivate {
	gchar *title;
};

enum {
	PROP_0,
	PROP_TITLE
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GalView, gal_view, G_TYPE_OBJECT)

static void
view_set_property (GObject *object,
                   guint property_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TITLE:
			gal_view_set_title (
				GAL_VIEW (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
view_get_property (GObject *object,
                   guint property_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TITLE:
			g_value_set_string (
				value, gal_view_get_title (
				GAL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
view_finalize (GObject *object)
{
	GalView *self = GAL_VIEW (object);

	g_free (self->priv->title);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (gal_view_parent_class)->finalize (object);
}

static void
view_load (GalView *view,
           const gchar *filename)
{
}

static void
view_save (GalView *view,
           const gchar *filename)
{
}

static GalView *
view_clone (GalView *view)
{
	const gchar *title;

	title = gal_view_get_title (view);

	return g_object_new (
		G_OBJECT_TYPE (view),
		"title", title, NULL);
}

static void
gal_view_class_init (GalViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = view_set_property;
	object_class->get_property = view_get_property;
	object_class->finalize = view_finalize;

	class->load = view_load;
	class->save = view_save;
	class->clone = view_clone;

	g_object_class_install_property (
		object_class,
		PROP_TITLE,
		g_param_spec_string (
			"title",
			"Title",
			"View Title",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GalViewClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
gal_view_init (GalView *view)
{
	view->priv = gal_view_get_instance_private (view);
}

/**
 * gal_view_load
 * @view: The view to load to
 * @filename: The file to load from
 */
void
gal_view_load (GalView *view,
               const gchar *filename)
{
	GalViewClass *class;

	g_return_if_fail (GAL_IS_VIEW (view));
	g_return_if_fail (filename != NULL);

	class = GAL_VIEW_GET_CLASS (view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->load != NULL);

	class->load (view, filename);
}

/**
 * gal_view_save
 * @view: The view to save
 * @filename: The file to save to
 */
void
gal_view_save (GalView *view,
               const gchar *filename)
{
	GalViewClass *class;

	g_return_if_fail (GAL_IS_VIEW (view));
	g_return_if_fail (filename != NULL);

	class = GAL_VIEW_GET_CLASS (view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->save != NULL);

	class->save (view, filename);
}

/**
 * gal_view_get_title
 * @view: The view to query.
 *
 * Returns: The title of the view.
 */
const gchar *
gal_view_get_title (GalView *view)
{
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	return view->priv->title;
}

/**
 * gal_view_set_title
 * @view: The view to set.
 * @title: The new title value.
 */
void
gal_view_set_title (GalView *view,
                    const gchar *title)
{
	g_return_if_fail (GAL_IS_VIEW (view));

	if (g_strcmp0 (title, view->priv->title) == 0)
		return;

	g_free (view->priv->title);
	view->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (view), "title");
}

/**
 * gal_view_clone
 * @view: The view to clone.
 *
 * Returns: The clone.
 */
GalView *
gal_view_clone (GalView *view)
{
	GalViewClass *class;

	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	class = GAL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->clone != NULL, NULL);

	return class->clone (view);
}

/**
 * gal_view_changed
 * @view: The view that changed.
 */
void
gal_view_changed (GalView *view)
{
	g_return_if_fail (GAL_IS_VIEW (view));

	g_signal_emit (view, signals[CHANGED], 0);
}

