/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Author: NotZed <notzed@ximian.com>
 *          Jeffrey Stedfast <fejj@ximian.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef _VFOLDER_RULE_H
#define _VFOLDER_RULE_H

#include "filter-rule.h"

#define VFOLDER_TYPE_RULE            (vfolder_rule_get_type ())
#define VFOLDER_RULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VFOLDER_TYPE_RULE, VfolderRule))
#define VFOLDER_RULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VFOLDER_TYPE_RULE, VfolderRuleClass))
#define IS_VFOLDER_RULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VFOLDER_TYPE_RULE))
#define IS_VFOLDER_RULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VFOLDER_TYPE_RULE))
#define VFOLDER_RULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VFOLDER_TYPE_RULE, VfolderRuleClass))

/* perhaps should be bits? */
enum _vfolder_rule_with_t {
	VFOLDER_RULE_WITH_SPECIFIC,
	VFOLDER_RULE_WITH_LOCAL,
	VFOLDER_RULE_WITH_REMOTE_ACTIVE,
	VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE,
};

typedef struct _VfolderRule VfolderRule;
typedef struct _VfolderRuleClass VfolderRuleClass;

typedef enum _vfolder_rule_with_t vfolder_rule_with_t;

struct _VfolderRule {
	FilterRule rule;
	
	vfolder_rule_with_t with;
	GList *sources;		/* uri's of the source folders */
};

struct _VfolderRuleClass {
	FilterRuleClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};

GType        vfolder_rule_get_type (void);
VfolderRule *vfolder_rule_new      (void);

/* methods */
void         vfolder_rule_add_source    (VfolderRule *vr, const char *uri);
void         vfolder_rule_remove_source (VfolderRule *vr, const char *uri);
const char  *vfolder_rule_find_source   (VfolderRule *vr, const char *uri);
const char  *vfolder_rule_next_source   (VfolderRule *vr, const char *last);

#endif /* ! _VFOLDER_RULE_H */
