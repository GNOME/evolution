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

#ifndef _SCORE_RULE_H
#define _SCORE_RULE_H

#include <gtk/gtk.h>

#include "filter-rule.h"

#define SCORE_RULE(obj)	GTK_CHECK_CAST (obj, score_rule_get_type (), ScoreRule)
#define SCORE_RULE_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, score_rule_get_type (), ScoreRuleClass)
#define IS_SCORE_RULE(obj)      GTK_CHECK_TYPE (obj, score_rule_get_type ())

typedef struct _ScoreRule	ScoreRule;
typedef struct _ScoreRuleClass	ScoreRuleClass;

struct _ScoreRule {
	FilterRule parent;
	struct _ScoreRulePrivate *priv;

	int score;
};

struct _ScoreRuleClass {
	FilterRuleClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		score_rule_get_type	(void);
ScoreRule	*score_rule_new	(void);

/* methods */

#endif /* ! _SCORE_RULE_H */

