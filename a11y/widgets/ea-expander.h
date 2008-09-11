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
 *		Boby Wang <boby.wang@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _EA_EXPANDER_H_
#define _EA_EXPANDER_H_

#include <gtk/gtk.h>
#include <misc/e-expander.h>

#define EA_TYPE_EXPANDER           (ea_expander_get_type ())
#define EA_EXPANDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_EXPANDER, EaExpander))
#define EA_EXPANDER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass,), EA_TYPE_EXPANDER, EaExpanderClass))
#define EA_IS_EXPANDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_EXPANDER))
#define EA_IS_EXPANDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_EXPANDER_CLASS))

typedef struct _EaExpander EaExpander;
typedef struct _EaExpanderClass EaExpanderClass;

struct _EaExpander {
	GtkAccessible object;
};

struct _EaExpanderClass {
	GtkAccessibleClass parent_class;
};

/* Standard Glib function */
GType ea_expander_get_type (void);
AtkObject* ea_expander_new (GtkWidget *expander);

#endif /* ! _EA_EXPANDER_H_ */
