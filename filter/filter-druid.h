/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _FILTER_DRUID_H
#define _FILTER_DRUID_H

#include <gtk/gtk.h>

#include "filter-xml.h"

#define FILTER_DRUID(obj)         GTK_CHECK_CAST (obj, filter_druid_get_type (), FilterDruid)
#define FILTER_DRUID_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_druid_get_type (), FilterDruidClass)
#define IS_FILTER_DRUID(obj)      GTK_CHECK_TYPE (obj, filter_druid_get_type ())

typedef struct _FilterDruid      FilterDruid;
typedef struct _FilterDruidClass FilterDruidClass;

struct _FilterDruid {
	GnomeDialog parent;

	GList *options;		/* all options */
	GList *rules;		/* all rules */
	GList *user;		/* current user options */

	struct filter_option *option_current;

	struct _FilterDruidPrivate *priv;
};

struct _FilterDruidClass {
	GnomeDialogClass parent_class;
};

guint		filter_druid_get_type	(void);
FilterDruid      *filter_druid_new	(void);

/* Hmm, glists suck, no typesafety */
void		filter_druid_set_rules(FilterDruid *f, GList *options, GList *rules, struct filter_option *userrule);

#endif /* ! _FILTER_DRUID_H */
