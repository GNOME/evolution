/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * ECompletionView - A text completion selection widget
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Author: Jon Trowbridge <trow@ximian.com>
 * Adapted from code by Miguel de Icaza <miguel@ximian.com>
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


#ifndef E_COMPLETION_VIEW_H
#define E_COMPLETION_VIEW_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtk.h>
#include <gal/e-table/e-table.h>
#include "e-completion.h"

BEGIN_GNOME_DECLS

#define E_COMPLETION_VIEW_TYPE        (e_completion_view_get_type ())
#define E_COMPLETION_VIEW(o)          (GTK_CHECK_CAST ((o), E_COMPLETION_VIEW_TYPE, ECompletionView))
#define E_COMPLETION_VIEW_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), E_COMPLETION_VIEW_TYPE, ECompletionViewClass))
#define E_IS_COMPLETION_VIEW(o)       (GTK_CHECK_TYPE ((o), E_COMPLETION_VIEW_TYPE))
#define E_IS_COMPLETION_VIEW_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_COMPLETION_VIEW_TYPE))

typedef struct _ECompletionView ECompletionView;
typedef struct _ECompletionViewClass ECompletionViewClass;

struct _ECompletionView {
	GtkVBox parent;
	
	ETableModel *model;
	GtkWidget *table;

	ECompletion *completion;
	guint begin_signal_id;
	guint comp_signal_id;
	guint restart_signal_id;
	guint cancel_signal_id;
	guint end_signal_id;

	GtkWidget *key_widget;
	guint key_signal_id;

	gint complete_key;
	gint uncomplete_key;

	GList *choices;
	gint choice_count;
	gboolean have_all_choices;

	gboolean editable;
	gint selection;
};

struct _ECompletionViewClass {
	GtkVBoxClass parent_class;

	/* Signals */
	void (*nonempty) (ECompletionView *cv);
	void (*added)    (ECompletionView *cv);
	void (*full)     (ECompletionView *cv);
	void (*browse)   (ECompletionView *cv, const gchar *text);
	void (*unbrowse) (ECompletionView *cv);
	void (*activate) (ECompletionView *cv, const gchar *text, gpointer extra_data);
};

GtkType    e_completion_view_get_type     (void);

void       e_completion_view_construct    (ECompletionView *cv, ECompletion *completion);
GtkWidget *e_completion_view_new          (ECompletion *completion);

void       e_completion_view_connect_keys (ECompletionView *cv, GtkWidget *w);

void       e_completion_view_set_complete_key   (ECompletionView *cv, gint keyval);
void       e_completion_view_set_uncomplete_key (ECompletionView *cv, gint keyval);

void       e_completion_view_set_width    (ECompletionView *cv, gint width);
void       e_completion_view_set_editable (ECompletionView *cv, gboolean);

END_GNOME_DECLS


#endif /* E_COMPLETION_H */
