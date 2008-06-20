/* Evolution Accessibility: ea-expander.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Boby Wang <boby.wang@sun.com> Sun Microsystem Inc., 2006
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
