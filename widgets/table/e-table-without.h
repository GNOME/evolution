/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_WITHOUT_H_
#define _E_TABLE_WITHOUT_H_

#include <gtk/gtkobject.h>
#include <gal/e-table/e-table-subset.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TABLE_WITHOUT_TYPE        (e_table_without_get_type ())
#define E_TABLE_WITHOUT(o)          (GTK_CHECK_CAST ((o), E_TABLE_WITHOUT_TYPE, ETableWithout))
#define E_TABLE_WITHOUT_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_WITHOUT_TYPE, ETableWithoutClass))
#define E_IS_TABLE_WITHOUT(o)       (GTK_CHECK_TYPE ((o), E_TABLE_WITHOUT_TYPE))
#define E_IS_TABLE_WITHOUT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_WITHOUT_TYPE))

typedef struct _ETableWithoutPrivate ETableWithoutPrivate;

typedef struct {
	ETableSubset base;

	ETableWithoutPrivate *priv;
} ETableWithout;

typedef struct {
	ETableSubsetClass parent_class;
	
} ETableWithoutClass;

GtkType      e_table_without_get_type   (void);
ETableModel *e_table_without_new        (ETableModel   *etm);
ETableModel *e_table_without_construct  (ETableWithout *etw,
					 ETableModel   *source);
void         e_table_without_add        (ETableWithout *etw,
					 void          *key);
void         e_table_without_remove     (ETableWithout *etw,
					 void          *key);
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_TABLE_WITHOUT_H_ */

