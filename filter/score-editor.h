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


#ifndef _SCORE_EDITOR_H
#define _SCORE_EDITOR_H

#include "rule-editor.h"
#include "score-context.h"

#define SCORE_TYPE_EDITOR            (score_editor_get_type ())
#define SCORE_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCORE_TYPE_EDITOR, ScoreEditor))
#define SCORE_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SCORE_TYPE_EDITOR, ScoreEditorClass))
#define IS_SCORE_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCORE_TYPE_EDITOR))
#define IS_SCORE_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SCORE_TYPE_EDITOR))
#define SCORE_EDITOR_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), SCORE_TYPE_EDITOR, ScoreEditorClass))

typedef struct _ScoreEditor ScoreEditor;
typedef struct _ScoreEditorClass ScoreEditorClass;

struct _ScoreEditor {
	RuleEditor parent_object;
	
};

struct _ScoreEditorClass {
	RuleEditorClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};


GType score_editor_get_type (void);

ScoreEditor *score_editor_new (ScoreContext *sc);

#endif /* ! _SCORE_EDITOR_H */
