/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#ifndef _EM_FILTER_RULE_H
#define _EM_FILTER_RULE_H

#include "filter/filter-rule.h"

#define EM_FILTER_RULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), em_filter_rule_get_type(), EMFilterRule))
#define EM_FILTER_RULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), em_filter_rule_get_type(), EMFilterRuleClass))
#define EM_IS_FILTER_RULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), em_filter_rule_get_type()))
#define EM_IS_FILTER_RULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), em_filter_rule_get_type()))
#define EM_FILTER_RULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), em_filter_rule_get_type(), EMFilterRuleClass))

typedef struct _EMFilterRule EMFilterRule;
typedef struct _EMFilterRuleClass EMFilterRuleClass;

struct _EMFilterRule {
	FilterRule parent_object;
	
	GList *actions;
};

struct _EMFilterRuleClass {
	FilterRuleClass parent_class;
};

GType           em_filter_rule_get_type (void);
EMFilterRule   *em_filter_rule_new      (void);

/* methods */
void            em_filter_rule_add_action     (EMFilterRule *fr, FilterPart *fp);
void            em_filter_rule_remove_action  (EMFilterRule *fr, FilterPart *fp);
void            em_filter_rule_replace_action (EMFilterRule *fr, FilterPart *fp, FilterPart *new);

void            em_filter_rule_build_action   (EMFilterRule *fr, GString *out);

#endif /* ! _EM_FILTER_RULE_H */
