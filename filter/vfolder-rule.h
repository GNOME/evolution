/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#ifndef _VFOLDER_RULE_H
#define _VFOLDER_RULE_H

#include <gtk/gtk.h>
#include "filter-rule.h"

#define VFOLDER_RULE(obj)	GTK_CHECK_CAST (obj, vfolder_rule_get_type (), VfolderRule)
#define VFOLDER_RULE_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, vfolder_rule_get_type (), VfolderRuleClass)
#define IS_VFOLDER_RULE(obj)      GTK_CHECK_TYPE (obj, vfolder_rule_get_type ())

typedef struct _VfolderRule	VfolderRule;
typedef struct _VfolderRuleClass	VfolderRuleClass;

struct _VfolderRule {
	FilterRule parent;
	struct _VfolderRulePrivate *priv;

	GList *sources;		/* uri's of the source folders */
};

struct _VfolderRuleClass {
	FilterRuleClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		vfolder_rule_get_type	(void);
VfolderRule	*vfolder_rule_new	(void);

/* methods */
void		vfolder_rule_add_source		(VfolderRule *vr, const char *uri);
void		vfolder_rule_remove_source	(VfolderRule *vr, const char *uri);
const char	*vfolder_rule_find_source	(VfolderRule *vr, const char *uri);
const char	*vfolder_rule_next_source	(VfolderRule *vr, const char *last);

#endif /* ! _VFOLDER_RULE_H */

