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

#ifndef _FILTER_RULE_H
#define _FILTER_RULE_H

#include <gtk/gtk.h>

#include "filter-part.h"

#define FILTER_RULE(obj)	GTK_CHECK_CAST (obj, filter_rule_get_type (), FilterRule)
#define FILTER_RULE_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_rule_get_type (), FilterRuleClass)
#define IS_FILTER_RULE(obj)      GTK_CHECK_TYPE (obj, filter_rule_get_type ())

typedef struct _FilterRule	FilterRule;
typedef struct _FilterRuleClass	FilterRuleClass;

struct _RuleContext;

enum _filter_grouping_t {
	FILTER_GROUP_ALL,	/* all rules must match */
	FILTER_GROUP_ANY	/* any rule must match */
};

enum _filter_source_t {
	FILTER_SOURCE_INCOMING, /* performed on incoming email */
	FILTER_SOURCE_DEMAND,   /* performed on the selected folder
				 * when the user asks for it */
	FILTER_SOURCE_OUTGOING  /* performed on outgoing mail */
};

struct _FilterRule {
	GtkObject parent;
	struct _FilterRulePrivate *priv;
	
	char *name;
	
	enum _filter_grouping_t grouping;
	enum _filter_source_t source;
	GList *parts;
};

struct _FilterRuleClass {
	GtkObjectClass parent_class;
	
	/* virtual methods */
	xmlNodePtr (*xml_encode)(FilterRule *);
	int (*xml_decode)(FilterRule *, xmlNodePtr, struct _RuleContext *);
	
	void (*build_code)(FilterRule *, GString *out);
	
	GtkWidget *(*get_widget)(FilterRule *fr, struct _RuleContext *f);
	
	/* signals */
};

guint		filter_rule_get_type	(void);
FilterRule	*filter_rule_new	(void);

/* methods */
void		filter_rule_set_name	(FilterRule *fr, const char *name);

xmlNodePtr	filter_rule_xml_encode	(FilterRule *fr);
int		filter_rule_xml_decode	(FilterRule *fr, xmlNodePtr node, struct _RuleContext *f);

void		filter_rule_add_part	(FilterRule *fr, FilterPart *fp);
void		filter_rule_remove_part	(FilterRule *fr, FilterPart *fp);
void		filter_rule_replace_part(FilterRule *fr, FilterPart *fp, FilterPart *new);

GtkWidget	*filter_rule_get_widget	(FilterRule *fr, struct _RuleContext *f);

void		filter_rule_build_code	(FilterRule *fr, GString *out);
/*
void		filter_rule_build_action(FilterRule *fr, GString *out);
*/

/* static functions */
FilterRule	*filter_rule_next_list		(GList *l, FilterRule *last);
FilterRule	*filter_rule_find_list		(GList *l, const char *name);

#endif /* ! _FILTER_RULE_H */

