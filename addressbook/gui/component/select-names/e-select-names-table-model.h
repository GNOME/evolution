/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 */

#ifndef __E_SELECT_NAMES_TABLE_MODEL_H__
#define __E_SELECT_NAMES_TABLE_MODEL_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "e-select-names-model.h"
#include <widgets/e-table/e-table-model.h>

#define E_TYPE_SELECT_NAMES_TABLE_MODEL            (e_select_names_table_model_get_type ())
#define E_SELECT_NAMES_TABLE_MODEL(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_SELECT_NAMES_TABLE_MODEL, ESelectNamesTableModel))
#define E_SELECT_NAMES_TABLE_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_TABLE_MODEL, ESelectNamesTableModelClass))
#define E_IS_SELECT_NAMES_TABLE_MODEL(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_SELECT_NAMES_TABLE_MODEL))
#define E_IS_SELECT_NAMES_TABLE_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_SELECT_NAMES_TABLE_MODEL))

typedef struct _ESelectNamesTableModel ESelectNamesTableModel;
typedef struct _ESelectNamesTableModelClass ESelectNamesTableModelClass;

struct _ESelectNamesTableModel {
	ETableModel parent;

	ESelectNamesModel *source;
};

struct _ESelectNamesTableModelClass {
	ETableModel parent_class;
};

ETableModel *e_select_names_table_model_new  (ESelectNamesModel *source);

/* Standard Gtk function */			      
GtkType     e_select_names_table_model_get_type (void);

#endif /* ! __E_SELECT_NAMES_TABLE_MODEL_H__ */
