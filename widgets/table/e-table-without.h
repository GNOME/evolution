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
typedef void *(*ETableWithoutGetKeyFunc)       (ETableModel *source,
						int          row,
						void        *closure);
typedef void *(*ETableWithoutDuplicateKeyFunc) (const void  *key,
						void        *closure);
typedef void  (*ETableWithoutFreeKeyFunc)      (void        *key,
						void        *closure);

typedef struct {
	ETableSubset base;

	ETableWithoutPrivate *priv;
} ETableWithout;

typedef struct {
	ETableSubsetClass parent_class;
	
} ETableWithoutClass;
GtkType      e_table_without_get_type   (void);
ETableModel *e_table_without_new        (ETableModel                   *source,
					 GHashFunc                      hash_func,
					 GCompareFunc                   compare_func,
					 ETableWithoutGetKeyFunc        get_key_func,
					 ETableWithoutDuplicateKeyFunc  duplicate_key_func,
					 ETableWithoutFreeKeyFunc       free_gotten_key_func,
					 ETableWithoutFreeKeyFunc       free_duplicated_key_func,
					 void                          *closure);
ETableModel *e_table_without_construct  (ETableWithout                 *etw,
					 ETableModel                   *source,
					 GHashFunc                      hash_func,
					 GCompareFunc                   compare_func,
					 ETableWithoutGetKeyFunc        get_key_func,
					 ETableWithoutDuplicateKeyFunc  duplicate_key_func,
					 ETableWithoutFreeKeyFunc       free_gotten_key_func,
					 ETableWithoutFreeKeyFunc       free_duplicated_key_func,
					 void                          *closure);
void         e_table_without_hide       (ETableWithout                 *etw,
					 void                          *key);
void         e_table_without_hide_adopt (ETableWithout                 *etw,
					 void                          *key);
void         e_table_without_show       (ETableWithout                 *etw,
					 void                          *key);
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_TABLE_WITHOUT_H_ */

