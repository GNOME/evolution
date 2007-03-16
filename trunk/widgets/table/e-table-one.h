/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-one.h
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

#ifndef _E_TABLE_ONE_H_
#define _E_TABLE_ONE_H_

#include <table/e-table-model.h>

G_BEGIN_DECLS

#define E_TABLE_ONE_TYPE        (e_table_one_get_type ())
#define E_TABLE_ONE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_ONE_TYPE, ETableOne))
#define E_TABLE_ONE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_ONE_TYPE, ETableOneClass))
#define E_IS_TABLE_ONE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_ONE_TYPE))
#define E_IS_TABLE_ONE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_ONE_TYPE))
#define E_TABLE_ONE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TABLE_ONE_TYPE, ETableOneClass))

typedef struct {
	ETableModel   parent;
		    
	ETableModel  *source;
	void        **data;
} ETableOne;

typedef struct {
	ETableModelClass parent_class;
} ETableOneClass;

GType e_table_one_get_type (void);

ETableModel *e_table_one_new (ETableModel *source);
void         e_table_one_commit (ETableOne *one);

G_END_DECLS

#endif /* _E_TABLE_ONE_H_ */

