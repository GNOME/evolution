/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#ifndef _FILTER_DRIVER_H
#define _FILTER_DRIVER_H

#include <gtk/gtk.h>
#include <camel/camel-session.h>
#include <camel/camel-folder.h>

#define FILTER_DRIVER(obj)         GTK_CHECK_CAST (obj, filter_driver_get_type (), FilterDriver)
#define FILTER_DRIVER_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_driver_get_type (), FilterDriverClass)
#define IS_FILTER_DRIVER(obj)      GTK_CHECK_TYPE (obj, filter_driver_get_type ())

typedef struct _FilterDriver      FilterDriver;
typedef struct _FilterDriverClass FilterDriverClass;

struct _FilterDriver {
	GtkObject parent;

	struct _FilterDriverPrivate *priv;

	CamelSession *session;
};

struct _FilterDriverClass {
	GtkObjectClass parent_class;
};

guint		filter_driver_get_type	(void);
FilterDriver      *filter_driver_new	(void);

void filter_driver_set_session(FilterDriver *, CamelSession *);
int filter_driver_set_rules(FilterDriver *, const char *description, const char *filter);
void filter_driver_set_global(FilterDriver *, const char *name, const char *value);

/* apply rules to a folder, unmatched messages goto inbox, if not NULL */
int filter_driver_run(FilterDriver *d, CamelFolder *source, CamelFolder *inbox);

#endif /* ! _FILTER_DRIVER_H */
