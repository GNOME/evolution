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

#ifndef _EM_VFOLDER_RULE_H
#define _EM_VFOLDER_RULE_H

#include "filter/filter-rule.h"

#define EM_VFOLDER_RULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), em_vfolder_rule_get_type(), EMVFolderRule))
#define EM_VFOLDER_RULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), em_vfolder_rule_get_type(), EMVFolderRuleClass))
#define EM_IS_VFOLDER_RULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), em_vfolder_rule_get_type()))
#define EM_IS_VFOLDER_RULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), em_vfolder_rule_get_type()))
#define EM_VFOLDER_RULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), em_vfolder_rule_get_type(), EMVFolderRuleClass))

/* perhaps should be bits? */
enum _em_vfolder_rule_with_t {
	EM_VFOLDER_RULE_WITH_SPECIFIC,
	EM_VFOLDER_RULE_WITH_LOCAL,
	EM_VFOLDER_RULE_WITH_REMOTE_ACTIVE,
	EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE,
};

typedef struct _EMVFolderRule EMVFolderRule;
typedef struct _EMVFolderRuleClass EMVFolderRuleClass;

typedef enum _em_vfolder_rule_with_t em_vfolder_rule_with_t;

struct _EMVFolderRule {
	FilterRule rule;
	
	em_vfolder_rule_with_t with;
	GList *sources;		/* uri's of the source folders */
};

struct _EMVFolderRuleClass {
	FilterRuleClass parent_class;
};

GType        em_vfolder_rule_get_type (void);
EMVFolderRule *em_vfolder_rule_new      (void);

/* methods */
void         em_vfolder_rule_add_source    (EMVFolderRule *vr, const char *uri);
void         em_vfolder_rule_remove_source (EMVFolderRule *vr, const char *uri);
const char  *em_vfolder_rule_find_source   (EMVFolderRule *vr, const char *uri);
const char  *em_vfolder_rule_next_source   (EMVFolderRule *vr, const char *last);

#endif /* ! _EM_VFOLDER_RULE_H */
