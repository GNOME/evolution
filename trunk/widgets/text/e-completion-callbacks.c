/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-completion-callbacks.c - A callback based ECompletion.
 * Copyright 2003
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

#include <config.h>

#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "e-util/e-util.h"

#include "e-completion-callbacks.h"

static void e_completion_callbacks_class_init (ECompletionCallbacksClass *klass);
static void e_completion_callbacks_init       (ECompletionCallbacks *complete);

static void     callbacks_request_completion    (ECompletion *comp, const gchar *search_text, gint pos, gint limit);
static void     callbacks_end_completion        (ECompletion *comp);

#define PARENT_TYPE E_COMPLETION_TYPE
static ECompletionClass *parent_class;



E_MAKE_TYPE (e_completion_callbacks,
	     "ECompletionCallbacks",
	     ECompletionCallbacks,
	     e_completion_callbacks_class_init,
	     e_completion_callbacks_init,
	     PARENT_TYPE)

static void
e_completion_callbacks_class_init (ECompletionCallbacksClass *klass)
{
	ECompletionClass *comp_class = (ECompletionClass *) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	comp_class->request_completion = callbacks_request_completion;
	comp_class->end_completion = callbacks_end_completion;
}

static void
e_completion_callbacks_init (ECompletionCallbacks *complete)
{
}

static void
callbacks_request_completion (ECompletion *comp, const gchar *search_text, gint pos, gint limit)
{
  ECompletionCallbacks *cc = E_COMPLETION_CALLBACKS (comp);

  cc->request_completion (cc, search_text, pos, limit, cc->data);
}

static void
callbacks_end_completion (ECompletion *comp)
{
  ECompletionCallbacks *cc = E_COMPLETION_CALLBACKS (comp);

  cc->end_completion (cc, cc->data);
}

ECompletionCallbacks*
e_completion_callbacks_new (ECompletionCallbacksRequestCompletionFn request_completion,
			    ECompletionCallbacksEndCompletionFn end_completion,
			    gpointer data)
{
  ECompletionCallbacks *cc;

  g_return_val_if_fail (request_completion != NULL, NULL);
  g_return_val_if_fail (end_completion != NULL, NULL);

  cc = gtk_type_new (E_COMPLETION_CALLBACKS_TYPE);

  cc->request_completion = request_completion;
  cc->end_completion = end_completion;
  cc->data = data;

  return cc;
}
