/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Iain Holmes <iain@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __E_SUMMARY_SHOWN_H__
#define __E_SUMMARY_SHOWN_H__

#include <gtk/gtkhbox.h>
#include <glib.h>
#include <gal/e-table/e-tree-memory.h>

#define E_SUMMARY_SHOWN_TYPE (e_summary_shown_get_type ())
#define E_SUMMARY_SHOWN(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_SHOWN_TYPE, ESummaryShown))
#define E_SUMMARY_SHOWN_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_SHOWN_TYPE, ESummaryShownClass))
#define IS_E_SUMMARY_SHOWN(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_SHOWN_TYPE))
#define IS_E_SUMMARY_SHOWN_CLASS(klass) (GTK_CHECK_TYPE ((klass), E_SUMMARY_SHOWN_TYPE))

typedef struct _ESummaryShownPrivate ESummaryShownPrivate;
typedef struct _ESummaryShownClass ESummaryShownClass;
typedef struct _ESummaryShown ESummaryShown;

typedef struct _ESummaryShownModelEntry {
	ETreePath path;
	char *name;
	char *location;

	gboolean showable;
	int ref_count;
} ESummaryShownModelEntry;

struct _ESummaryShown {
	GtkHBox parent;

	GHashTable *all_model;
	GHashTable *shown_model;
	ESummaryShownPrivate *priv;
};

struct _ESummaryShownClass {
	GtkHBoxClass parent_class;

	void (* item_changed) (ESummaryShown *shown);
};

GtkType e_summary_shown_get_type (void);
GtkWidget *e_summary_shown_new (void);
ETreePath e_summary_shown_add_node (ESummaryShown *shown,
				    gboolean all,
				    ESummaryShownModelEntry *entry,
				    ETreePath parent,
				    gpointer data);
void e_summary_shown_remove_node (ESummaryShown *shown,
				  gboolean all,
				  ESummaryShownModelEntry *entry);
#endif
