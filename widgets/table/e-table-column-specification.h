/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_COLUMN_SPECIFICATION_H_
#define _E_TABLE_COLUMN_SPECIFICATION_H_

#include <gtk/gtkobject.h>
#include <gnome-xml/tree.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_TABLE_COLUMN_SPECIFICATION_TYPE        (e_table_column_specification_get_type ())
#define E_TABLE_COLUMN_SPECIFICATION(o)          (GTK_CHECK_CAST ((o), E_TABLE_COLUMN_SPECIFICATION_TYPE, ETableColumnSpecification))
#define E_TABLE_COLUMN_SPECIFICATION_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_COLUMN_SPECIFICATION_TYPE, ETableColumnSpecificationClass))
#define E_IS_TABLE_COLUMN_SPECIFICATION(o)       (GTK_CHECK_TYPE ((o), E_TABLE_COLUMN_SPECIFICATION_TYPE))
#define E_IS_TABLE_COLUMN_SPECIFICATION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_COLUMN_SPECIFICATION_TYPE))

typedef struct {
	GtkObject base;
	int model_col;
	char *title;
	char *pixbuf;

	double expansion;
	int minimum_width;
	guint resizable : 1;

	char *cell;
	char *compare;
} ETableColumnSpecification;

typedef struct {
	GtkObjectClass parent_class;
} ETableColumnSpecificationClass;

GtkType                    e_table_column_specification_get_type        (void);

ETableColumnSpecification *e_table_column_specification_new             (void);

void                       e_table_column_specification_load_from_node  (ETableColumnSpecification *state,
									 const xmlNode             *node);
xmlNode                   *e_table_column_specification_save_to_node    (ETableColumnSpecification *state,
									 xmlNode                   *parent);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_COLUMN_SPECIFICATION_H_ */
