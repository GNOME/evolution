/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-text-model.h
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

#ifndef E_TABLE_TEXT_MODEL_H
#define E_TABLE_TEXT_MODEL_H

#include <text/e-text-model.h>
#include <table/e-table-model.h>

G_BEGIN_DECLS

#define E_TYPE_TABLE_TEXT_MODEL            (e_table_text_model_get_type ())
#define E_TABLE_TEXT_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TABLE_TEXT_MODEL, ETableTextModel))
#define E_TABLE_TEXT_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_TABLE_TEXT_MODEL, ETableTextModelClass))
#define E_IS_TABLE_TEXT_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TABLE_TEXT_MODEL))
#define E_IS_TABLE_TEXT_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_TABLE_TEXT_MODEL))

typedef struct _ETableTextModel ETableTextModel;
typedef struct _ETableTextModelClass ETableTextModelClass;

struct _ETableTextModel {
	ETextModel parent;

	ETableModel *model;
	int row;
	int model_col;

	int cell_changed_signal_id;
	int row_changed_signal_id;
};

struct _ETableTextModelClass {
	ETextModelClass parent_class;

};


/* Standard Gtk function */
GtkType e_table_text_model_get_type (void);
ETableTextModel *e_table_text_model_new (ETableModel *table_model, int row, int model_col);

G_END_DECLS

#endif
