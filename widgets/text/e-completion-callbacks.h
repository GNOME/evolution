/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-completion-callback.h - A callback based completion object.
 * Copyright 2003, Ximian, Inc.
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
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

#ifndef E_COMPLETION_CALLBACKS_H
#define E_COMPLETION_CALLBACKS_H

#include <gtk/gtkobject.h>
#include "e-completion.h"

G_BEGIN_DECLS

#define E_COMPLETION_CALLBACKS_TYPE        (e_completion_callbacks_get_type ())
#define E_COMPLETION_CALLBACKS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_COMPLETION_CALLBACKS_TYPE, ECompletionCallbacks))
#define E_COMPLETION_CALLBACKS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_COMPLETION_CALLBACKS_TYPE, ECompletionCallbacksClass))
#define E_IS_COMPLETION_CALLBACKS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_COMPLETION_CALLBACKS_TYPE))
#define E_IS_COMPLETION_CALLBACKS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_COMPLETION_CALLBACKS_TYPE))

typedef struct _ECompletionCallbacks ECompletionCallbacks;
typedef struct _ECompletionCallbacksClass ECompletionCallbacksClass;
struct _ECompletionCallbacksPrivate;

typedef void (*ECompletionCallbacksRequestCompletionFn) (ECompletionCallbacks *comp, const gchar *search_text, gint pos, gint limit, gpointer data);
typedef void (*ECompletionCallbacksEndCompletionFn) (ECompletionCallbacks *comp, gpointer data);

struct _ECompletionCallbacks {
	ECompletion parent;

	ECompletionCallbacksRequestCompletionFn request_completion;
	ECompletionCallbacksEndCompletionFn end_completion;

	gpointer data;
};

struct _ECompletionCallbacksClass {
	ECompletionClass parent_class;
};

GtkType      e_completion_callbacks_get_type (void);

ECompletionCallbacks* e_completion_callbacks_new (ECompletionCallbacksRequestCompletionFn request_completion,
					 ECompletionCallbacksEndCompletionFn end_completion,
					 gpointer data);

G_END_DECLS


#endif /* E_COMPLETION_CALLBACKS_H */

