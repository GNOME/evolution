/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-completion-match.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Jon Trowbridge <trow@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __E_COMPLETION_MATCH_H__
#define __E_COMPLETION_MATCH_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef struct _ECompletionMatch ECompletionMatch;

struct _ECompletionMatch {
	gchar *match_text; /* in utf8 */
	gchar *menu_text;  /* in utf8 */
	double score;
	gint sort_major;
	gint sort_minor;
	gpointer user_data;

	gint ref;
	void (*destroy) (ECompletionMatch *);
};

typedef void (*ECompletionMatchFn) (ECompletionMatch *, gpointer closure);

void              e_completion_match_construct     (ECompletionMatch *);
void              e_completion_match_ref           (ECompletionMatch *);
void              e_completion_match_unref         (ECompletionMatch *);

void              e_completion_match_set_text       (ECompletionMatch *, const gchar *match_text, const gchar *label_text);
const gchar      *e_completion_match_get_match_text (ECompletionMatch *);
const gchar      *e_completion_match_get_menu_text  (ECompletionMatch *);

gint              e_completion_match_compare        (const ECompletionMatch *, const ECompletionMatch *);
gint              e_completion_match_compare_alpha  (const ECompletionMatch *, const ECompletionMatch *);

ECompletionMatch *e_completion_match_new            (const gchar *match_text, const gchar *menu_text, double score);




G_END_DECLS

#endif /* __E_COMPLETION_MATCH_H__ */

