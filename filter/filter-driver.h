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
#include <mail/mail-threads.h>

#include "filter-context.h"

#define FILTER_DRIVER(obj)         GTK_CHECK_CAST (obj, filter_driver_get_type (), FilterDriver)
#define FILTER_DRIVER_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_driver_get_type (), FilterDriverClass)
#define IS_FILTER_DRIVER(obj)      GTK_CHECK_TYPE (obj, filter_driver_get_type ())

typedef struct _FilterDriver      FilterDriver;
typedef struct _FilterDriverClass FilterDriverClass;

struct _FilterDriver {
	GtkObject parent;

	struct _FilterDriverPrivate *priv;
};

struct _FilterDriverClass {
	GtkObjectClass parent_class;
};

typedef CamelFolder * (*FilterGetFolderFunc) (FilterDriver *, const char *uri, void *data);

guint         filter_driver_get_type (void);
FilterDriver  *filter_driver_new     (FilterContext *ctx, FilterGetFolderFunc fetcher, void *data);

/*void filter_driver_set_global(FilterDriver *, const char *name, const char *value);*/

/* filter a message - returns TRUE if the message was filtered into some location other than inbox */
gboolean filter_driver_run (FilterDriver *driver, CamelMimeMessage *message, CamelMessageInfo *info,
			    CamelFolder *inbox, enum _filter_source_t sourcetype,
			    gpointer unhook_func, gpointer unhook_data,
			    gboolean self_destruct, CamelException *ex);

#if 0
/* generate the search query/action string for a filter option */
void filter_driver_expand_option (FilterDriver *d, GString *s, GString *action, struct filter_option *op);

/* get info about rules (options) */
int filter_driver_rule_count (FilterDriver *d);
struct filter_option *filter_driver_rule_get (FilterDriver *d, int n);
#endif

#endif /* ! _FILTER_DRIVER_H */
