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


#ifndef _SCORE_RULE_H
#define _SCORE_RULE_H

#include "filter-rule.h"

#define SCORE_TYPE_RULE            (score_rule_get_type ())
#define SCORE_RULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCORE_TYPE_RULE, ScoreRule))
#define SCORE_RULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SCORE_TYPE_RULE, ScoreRuleClass))
#define IS_SCORE_RULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCORE_TYPE_RULE))
#define IS_SCORE_RULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SCORE_TYPE_RULE))
#define SCORE_RULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SCORE_TYPE_RULE, ScoreRuleClass))

typedef struct _ScoreRule ScoreRule;
typedef struct _ScoreRuleClass ScoreRuleClass;

struct _ScoreRule {
	FilterRule parent_object;
	
	int score;
};

struct _ScoreRuleClass {
	FilterRuleClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};


GType score_rule_get_type (void);

ScoreRule *score_rule_new (void);

/* methods */

#endif /* ! _SCORE_RULE_H */
