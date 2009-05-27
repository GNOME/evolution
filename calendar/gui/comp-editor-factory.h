/*
 * Evolution calendar - Component editor factory object
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef COMP_EDITOR_FACTORY_H
#define COMP_EDITOR_FACTORY_H

#include <bonobo/bonobo-object.h>
#include "evolution-calendar.h"



#define TYPE_COMP_EDITOR_FACTORY            (comp_editor_factory_get_type ())
#define COMP_EDITOR_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_COMP_EDITOR_FACTORY,	\
					     CompEditorFactory))
#define COMP_EDITOR_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),			\
					     TYPE_COMP_EDITOR_FACTORY, CompEditorFactoryClass))
#define IS_COMP_EDITOR_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_COMP_EDITOR_FACTORY))
#define IS_COMP_EDITOR_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_COMP_EDITOR_FACTORY))

typedef struct CompEditorFactoryPrivate CompEditorFactoryPrivate;

typedef struct {
	BonoboObject object;

	/* Private data */
	CompEditorFactoryPrivate *priv;
} CompEditorFactory;

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_CompEditorFactory__epv epv;
} CompEditorFactoryClass;

GType comp_editor_factory_get_type (void);

CompEditorFactory *comp_editor_factory_new (void);



#endif
