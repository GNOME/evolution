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

#ifndef __E_SUMMARY_TABLE_H__
#define __E_SUMMARY_TABLE_H__

#include <gtk/gtkvbox.h>
#include <glib.h>
#include <gal/e-table/e-tree-memory.h>

#define E_SUMMARY_TABLE_TYPE (e_summary_table_get_type ())
#define E_SUMMARY_TABLE(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TABLE_TYPE, ESummaryTable))
#define E_SUMMARY_TABLE_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TABLE_TYPE, ESummaryTableClass))
#define IS_E_SUMMARY_TABLE(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_TABLE_TYPE))
#define IS_E_SUMMARY_TABLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_SUMMARY_TABLE_TYPE))

typedef struct _ESummaryTablePrivate ESummaryTablePrivate;
typedef struct _ESummaryTableClass ESummaryTableClass;
typedef struct _ESummaryTable ESummaryTable;

typedef struct _ESummaryTableModelEntry {
	ETreePath path;

	char *location;
	
	gboolean editable;
	gboolean removable;

	gboolean shown;
	char *name;
} ESummaryTableModelEntry;

struct _ESummaryTable {
	GtkVBox  parent;

	GHashTable *model;
	ESummaryTablePrivate *priv;
};

struct _ESummaryTableClass {
	GtkVBoxClass parent_class;

	void (* item_changed) (ESummaryTable *table,
			       ETreePath path);
};

GtkType e_summary_table_get_type (void);
GtkWidget *e_summary_table_new (GHashTable *model);
ETreePath e_summary_table_add_node (ESummaryTable *est,
				    ETreePath path,
				    int position,
				    gpointer node_data);
guint e_summary_table_get_num_children (ESummaryTable *est,
					ETreePath path);
#endif
