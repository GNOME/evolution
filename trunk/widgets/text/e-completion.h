/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-completion.h - A base class for text completion.
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *   Adapted by Jon Trowbridge <trow@ximian.com>
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

#ifndef E_COMPLETION_H
#define E_COMPLETION_H

#include <gtk/gtkobject.h>
#include "e-completion-match.h"

G_BEGIN_DECLS

#define E_COMPLETION_TYPE        (e_completion_get_type ())
#define E_COMPLETION(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_COMPLETION_TYPE, ECompletion))
#define E_COMPLETION_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_COMPLETION_TYPE, ECompletionClass))
#define E_IS_COMPLETION(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_COMPLETION_TYPE))
#define E_IS_COMPLETION_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_COMPLETION_TYPE))

typedef struct _ECompletion ECompletion;
typedef struct _ECompletionClass ECompletionClass;
struct _ECompletionPrivate;

typedef gboolean (*ECompletionRefineFn) (ECompletion *, ECompletionMatch *, const gchar *search_text, gint pos);

struct _ECompletion {
	GtkObject parent;

	struct _ECompletionPrivate *priv;
};

struct _ECompletionClass {
	GtkObjectClass parent_class;

	/* virtual functions */
	void     (*request_completion)  (ECompletion *comp, const gchar *search_text, gint pos, gint limit);
	void     (*end_completion)      (ECompletion *comp);

	/* Signals */
	void     (*completion_started)  (ECompletion *comp, const gchar *search_text, gint pos, gint limit);

	void     (*completion_found)    (ECompletion *comp, ECompletionMatch *match);

	void     (*completion_finished) (ECompletion *comp);
};

GtkType      e_completion_get_type (void);

void         e_completion_begin_search    (ECompletion *comp, const gchar *text, gint pos, gint limit);

gboolean     e_completion_searching       (ECompletion *comp);
gboolean     e_completion_refining        (ECompletion *comp);
const gchar *e_completion_search_text     (ECompletion *comp);
gint         e_completion_search_text_pos (ECompletion *comp);
gint         e_completion_match_count     (ECompletion *comp);
void         e_completion_foreach_match   (ECompletion *comp, ECompletionMatchFn fn, gpointer user_data);

ECompletion *e_completion_new (void);



/* These functions should only be called by derived classes or search callbacks,
   or very bad things might happen. */

void         e_completion_found_match (ECompletion *comp, ECompletionMatch *);
void         e_completion_end_search  (ECompletion *comp);

G_END_DECLS


#endif /* E_COMPLETION_H */

