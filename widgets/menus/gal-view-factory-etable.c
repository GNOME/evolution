/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"

#include "gal-view-etable.h"
#include "gal-view-factory-etable.h"

#define GAL_VIEW_FACTORY_ETABLE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), GAL_TYPE_VIEW_FACTORY_ETABLE, GalViewFactoryEtablePrivate))

struct _GalViewFactoryEtablePrivate {
	ETableSpecification *specification;
};

enum {
	PROP_0,
	PROP_SPECIFICATION
};

G_DEFINE_TYPE (
	GalViewFactoryEtable,
	gal_view_factory_etable, GAL_TYPE_VIEW_FACTORY)

static void
view_factory_etable_set_specification (GalViewFactoryEtable *factory,
                                       ETableSpecification *specification)
{
	g_return_if_fail (factory->priv->specification == NULL);
	g_return_if_fail (E_IS_TABLE_SPECIFICATION (specification));

	factory->priv->specification = g_object_ref (specification);
}

static void
view_factory_etable_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SPECIFICATION:
			view_factory_etable_set_specification (
				GAL_VIEW_FACTORY_ETABLE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
view_factory_etable_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SPECIFICATION:
			g_value_set_object (
				value,
				gal_view_factory_etable_get_specification (
				GAL_VIEW_FACTORY_ETABLE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
view_factory_etable_dispose (GObject *object)
{
	GalViewFactoryEtablePrivate *priv;

	priv = GAL_VIEW_FACTORY_ETABLE_GET_PRIVATE (object);

	if (priv->specification != NULL) {
		g_object_unref (priv->specification);
		priv->specification = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (gal_view_factory_etable_parent_class)->dispose (object);
}

static const gchar *
view_factory_etable_get_title (GalViewFactory *factory)
{
	return _("Table");
}

static const gchar *
view_factory_etable_get_type_code (GalViewFactory *factory)
{
	return "etable";
}

static GalView *
view_factory_etable_new_view (GalViewFactory *factory,
                              const gchar *name)
{
	GalViewFactoryEtablePrivate *priv;

	priv = GAL_VIEW_FACTORY_ETABLE_GET_PRIVATE (factory);

	return gal_view_etable_new (priv->specification, name);
}

static void
gal_view_factory_etable_class_init (GalViewFactoryEtableClass *class)
{
	GObjectClass *object_class;
	GalViewFactoryClass *view_factory_class;

	g_type_class_add_private (class, sizeof (GalViewFactoryEtablePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = view_factory_etable_set_property;
	object_class->get_property = view_factory_etable_get_property;
	object_class->dispose = view_factory_etable_dispose;

	view_factory_class = GAL_VIEW_FACTORY_CLASS (class);
	view_factory_class->get_title = view_factory_etable_get_title;
	view_factory_class->get_type_code = view_factory_etable_get_type_code;
	view_factory_class->new_view = view_factory_etable_new_view;

	g_object_class_install_property (
		object_class,
		PROP_SPECIFICATION,
		g_param_spec_object (
			"specification",
			NULL,
			NULL,
			E_TYPE_TABLE_SPECIFICATION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
gal_view_factory_etable_init (GalViewFactoryEtable *factory)
{
	factory->priv = GAL_VIEW_FACTORY_ETABLE_GET_PRIVATE (factory);
}

/**
 * gal_view_etable_new:
 * @specification: The spec to create GalViewEtables based upon.
 *
 * A new GalViewFactory for creating ETable views.  Create one of
 * these and pass it to GalViewCollection for use.
 *
 * Returns: The new GalViewFactoryEtable.
 */
GalViewFactory *
gal_view_factory_etable_new (ETableSpecification *specification)
{
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	return g_object_new (
		GAL_TYPE_VIEW_FACTORY_ETABLE,
		"specification", specification, NULL);
}

ETableSpecification *
gal_view_factory_etable_get_specification (GalViewFactoryEtable *factory)
{
	g_return_val_if_fail (GAL_IS_VIEW_FACTORY_ETABLE (factory), NULL);

	return factory->priv->specification;
}
