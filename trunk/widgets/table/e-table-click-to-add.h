/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-click-to-add.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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

#ifndef _E_TABLE_CLICK_TO_ADD_H_
#define _E_TABLE_CLICK_TO_ADD_H_

#include <libxml/tree.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-table-header.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-item.h>
#include <table/e-table-selection-model.h>

G_BEGIN_DECLS

#define E_TABLE_CLICK_TO_ADD_TYPE        (e_table_click_to_add_get_type ())
#define E_TABLE_CLICK_TO_ADD(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_CLICK_TO_ADD_TYPE, ETableClickToAdd))
#define E_TABLE_CLICK_TO_ADD_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_CLICK_TO_ADD_TYPE, ETableClickToAddClass))
#define E_IS_TABLE_CLICK_TO_ADD(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_CLICK_TO_ADD_TYPE))
#define E_IS_TABLE_CLICK_TO_ADD_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_CLICK_TO_ADD_TYPE))

typedef struct {
	GnomeCanvasGroup  parent;

	ETableModel      *one;    /* The ETableOne. */

	ETableModel      *model;  /* The backend model. */
	ETableHeader     *eth;    /* This is just to give to the ETableItem. */

	char             *message;

	GnomeCanvasItem  *row;    /* If row is NULL, we're sitting with no data and a "Click here" message. */
	GnomeCanvasItem  *text;   /* If text is NULL, row shouldn't be. */
	GnomeCanvasItem  *rect;   /* What the heck.  Why not. */
	
	gdouble           width;
	gdouble           height;

	ETableSelectionModel *selection;
} ETableClickToAdd;

typedef struct {
	GnomeCanvasGroupClass parent_class;

	/*
	 * signals
	 */
	void (*cursor_change) (ETableClickToAdd *etcta, gint row, gint col);
	void (*style_set) (ETableClickToAdd *etcta, GtkStyle *previous_style);
} ETableClickToAddClass;

GType      e_table_click_to_add_get_type (void);

void       e_table_click_to_add_commit (ETableClickToAdd *etcta);

G_END_DECLS

#endif /* _E_TABLE_CLICK_TO_ADD_H_ */
