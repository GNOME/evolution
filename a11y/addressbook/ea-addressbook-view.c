/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * ea-addressbook-view.c
 * Copyright (C) 2000  Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author:  Leon Zhang <leon.zhang@sun.com> Sun Microsystem Inc., 2003
 */

#include <config.h>

#include <libgnome/gnome-i18n.h>
#include "ea-addressbook-view.h"

static G_CONST_RETURN gchar* ea_ab_view_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_ab_view_get_description (AtkObject *accessible);

static void ea_ab_view_class_init (EABViewClass *klass);

static gpointer parent_class = NULL;

GType
ea_ab_view_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static  GTypeInfo tinfo =  {
			sizeof (EABViewClass),
			(GBaseInitFunc) NULL,  /* base_init */
			(GBaseFinalizeFunc) NULL,  /* base_finalize */
			(GClassInitFunc) ea_ab_view_class_init,
			(GClassFinalizeFunc) NULL, /* class_finalize */
			NULL,	/* class_data */
			sizeof (EaABView),
			0,	/* n_preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL	/* value table */
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (GailWidget, in this case) */

		factory = atk_registry_get_factory (atk_get_default_registry (),
						    GTK_TYPE_EVENT_BOX);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type,
						"EaABView", &tinfo, 0);
	}

	return type;
}

static void
ea_ab_view_class_init (EABViewClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_ab_view_get_name;
	class->get_description = ea_ab_view_get_description;
}

static G_CONST_RETURN gchar*
ea_ab_view_get_name (AtkObject *accessible)
{
	g_return_val_if_fail (EA_IS_AB_VIEW(accessible), NULL);
	if (accessible->name)
		return accessible->name;

	return _("evolution addressbook");
}

static G_CONST_RETURN gchar*
ea_ab_view_get_description (AtkObject *accessible)
{
	if (accessible->description)
		return accessible->description;

	return _("evolution addressbook");
}

AtkObject* 
ea_ab_view_new (GObject *obj)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail(obj != NULL, NULL);
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW(obj), NULL);

	object = g_object_new (EA_TYPE_AB_VIEW, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, obj);
	accessible->role = ATK_ROLE_CANVAS;

	return accessible;
}
