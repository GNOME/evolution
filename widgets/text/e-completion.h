/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* ECompletion - A base class for text completion.
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Author: Miguel de Icaza <miguel@ximian.com>
 * Adapted by Jon Trowbridge <trow@ximian.com>
 *
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef E_COMPLETION_H
#define E_COMPLETION_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtkobject.h>

BEGIN_GNOME_DECLS

#define E_COMPLETION_TYPE        (e_completion_get_type ())
#define E_COMPLETION(o)          (GTK_CHECK_CAST ((o), E_COMPLETION_TYPE, ECompletion))
#define E_COMPLETION_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), E_COMPLETION_TYPE, ECompletionClass))
#define E_IS_COMPLETION(o)       (GTK_CHECK_TYPE ((o), E_COMPLETION_TYPE))
#define E_IS_COMPLETION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_COMPLETION_TYPE))

typedef struct _ECompletion ECompletion;
typedef struct _ECompletionClass ECompletionClass;
struct _ECompletionPrivate;

typedef void (*ECompletionBeginFn) (ECompletion *, const gchar *text, gint pos, gint limit, gpointer user_data);
typedef void (*ECompletionEndFn)   (ECompletion *, gboolean finished, gpointer user_data);
typedef void (*ECompletionMatchFn) (const gchar *text, double score, gpointer extra_data, gpointer user_data);

struct _ECompletion {
	GtkObject parent;

	struct _ECompletionPrivate *priv;
};

struct _ECompletionClass {
	GtkObjectClass parent_class;

	/* Signals */
	void     (*begin_completion)   (ECompletion *comp);
	void     (*completion)         (ECompletion *comp, const gchar *text, gpointer extra_data);
	void     (*restart_completion) (ECompletion *comp);
	void     (*cancel_completion)  (ECompletion *comp);
	void     (*end_completion)     (ECompletion *comp);
};

GtkType      e_completion_get_type (void);

void         e_completion_begin_search  (ECompletion *comp, const gchar *text, gint pos, gint limit);
void         e_completion_cancel_search (ECompletion *comp);

gboolean     e_completion_searching       (ECompletion *comp);
const gchar *e_completion_search_text     (ECompletion *comp);
gint         e_completion_search_text_pos (ECompletion *comp);
gint         e_completion_match_count     (ECompletion *comp);
void         e_completion_foreach_match   (ECompletion *comp, ECompletionMatchFn fn, gpointer user_data);
gpointer     e_completion_find_extra_data (ECompletion *comp, const gchar *text);

void         e_completion_construct    (ECompletion *comp, ECompletionBeginFn, ECompletionEndFn, gpointer user_data);
ECompletion *e_completion_new          (ECompletionBeginFn, ECompletionEndFn, gpointer user_data);



/* These functions should only be called by derived classes or search callbacks,
   or very bad things might happen. */

void         e_completion_found_match      (ECompletion *comp, const gchar *completion_text);
void         e_completion_found_match_full (ECompletion *comp, const gchar *completion_text, double score,
					    gpointer extra_data, GtkDestroyNotify extra_data_destructor);
void         e_completion_end_search       (ECompletion *comp);

END_GNOME_DECLS


#endif /* E_COMPLETION_H */
