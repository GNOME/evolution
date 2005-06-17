/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-completion-view.h - A text completion selection widget
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

#ifndef E_COMPLETION_VIEW_H
#define E_COMPLETION_VIEW_H

#include <gtk/gtk.h>
#include <table/e-table.h>
#include "e-completion.h"

G_BEGIN_DECLS

#define E_COMPLETION_VIEW_TYPE        (e_completion_view_get_type ())
#define E_COMPLETION_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_COMPLETION_VIEW_TYPE, ECompletionView))
#define E_COMPLETION_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_COMPLETION_VIEW_TYPE, ECompletionViewClass))
#define E_IS_COMPLETION_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_COMPLETION_VIEW_TYPE))
#define E_IS_COMPLETION_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_COMPLETION_VIEW_TYPE))

typedef struct _ECompletionView ECompletionView;
typedef struct _ECompletionViewClass ECompletionViewClass;

struct _ECompletionView {
	GtkEventBox parent;
	
	ETableModel *model;
	GtkWidget *table;

	GPtrArray *choices;

	ECompletion *completion;
	guint begin_signal_id;
	guint comp_signal_id;
	guint end_signal_id;

	GtkWidget *key_widget;
	guint key_signal_id;

	gint complete_key;
	gint uncomplete_key;

	gboolean have_all_choices;

	gboolean editable;
	gint selection;
       
	gint border_width;
};

struct _ECompletionViewClass {
	GtkEventBoxClass parent_class;

	/* Signals */
	void (*nonempty) (ECompletionView *cv);
	void (*added)    (ECompletionView *cv);
	void (*full)     (ECompletionView *cv);
	void (*browse)   (ECompletionView *cv, ECompletionMatch *match);
	void (*unbrowse) (ECompletionView *cv);
	void (*activate) (ECompletionView *cv, ECompletionMatch *match);
};

GtkType    e_completion_view_get_type     (void);

void       e_completion_view_construct    (ECompletionView *cv, ECompletion *completion);
GtkWidget *e_completion_view_new          (ECompletion *completion);

void       e_completion_view_connect_keys (ECompletionView *cv, GtkWidget *w);

void       e_completion_view_set_complete_key   (ECompletionView *cv, gint keyval);
void       e_completion_view_set_uncomplete_key (ECompletionView *cv, gint keyval);

void       e_completion_view_set_width    (ECompletionView *cv, gint width);
void       e_completion_view_set_editable (ECompletionView *cv, gboolean);

G_END_DECLS


#endif /* E_COMPLETION_H */
