/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_ONE_H_
#define _E_TABLE_ONE_H_

#include "e-table-model.h"

#define E_TABLE_ONE_TYPE        (e_table_one_get_type ())
#define E_TABLE_ONE(o)          (GTK_CHECK_CAST ((o), E_TABLE_ONE_TYPE, ETableOne))
#define E_TABLE_ONE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_ONE_TYPE, ETableOneClass))
#define E_IS_TABLE_ONE(o)       (GTK_CHECK_TYPE ((o), E_TABLE_ONE_TYPE))
#define E_IS_TABLE_ONE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_ONE_TYPE))

typedef struct {
	ETableModel   parent;
		    
	ETableModel  *source;
	void        **data;
} ETableOne;

typedef struct {
	ETableModelClass parent_class;
} ETableOneClass;

GtkType e_table_one_get_type (void);

ETableModel *e_table_one_new (ETableModel *source);
gint         e_table_one_commit (ETableOne *one);

#endif /* _E_TABLE_ONE_H_ */

