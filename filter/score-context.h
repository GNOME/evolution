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

#ifndef _SCORE_CONTEXT_H
#define _SCORE_CONTEXT_H

#include "rule-context.h"

#define SCORE_TYPE_CONTEXT            (score_context_get_type ())
#define SCORE_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCORE_TYPE_CONTEXT, ScoreContext))
#define SCORE_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SCORE_TYPE_CONTEXT, ScoreContextClass))
#define IS_SCORE_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCORE_TYPE_CONTEXT))
#define IS_SCORE_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SCORE_TYPE_CONTEXT))
#define SCORE_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SCORE_TYPE_CONTEXT, ScoreContextClass))

typedef struct _ScoreContext ScoreContext;
typedef struct _ScoreContextClass ScoreContextClass;

struct _ScoreContext {
	RuleContext parent_object;
	
};

struct _ScoreContextClass {
	RuleContextClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};


GType score_context_get_type (void);

ScoreContext *score_context_new (void);

/* methods */

#endif /* ! _SCORE_CONTEXT_H */
