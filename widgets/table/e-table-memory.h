/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_MEMORY_H_
#define _E_TABLE_MEMORY_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gal/e-table/e-table-model.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_TABLE_MEMORY_TYPE        (e_table_memory_get_type ())
#define E_TABLE_MEMORY(o)          (GTK_CHECK_CAST ((o), E_TABLE_MEMORY_TYPE, ETableMemory))
#define E_TABLE_MEMORY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_MEMORY_TYPE, ETableMemoryClass))
#define E_IS_TABLE_MEMORY(o)       (GTK_CHECK_TYPE ((o), E_TABLE_MEMORY_TYPE))
#define E_IS_TABLE_MEMORY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_MEMORY_TYPE))

typedef struct ETableMemory ETableMemory;
typedef struct ETableMemoryPriv ETableMemoryPriv;
typedef struct ETableMemoryClass ETableMemoryClass;

struct ETableMemory {
	ETableModel base;
	ETableMemoryPriv *priv;
};

struct ETableMemoryClass {
	ETableModelClass parent_class;
};


GtkType       e_table_memory_get_type     (void);
void          e_table_memory_construct    (ETableMemory *etable);
ETableMemory *e_table_memory_new          (void);

/* row operations */
void          e_table_memory_insert  (ETableMemory *etable,
				      int           row,
				      gpointer      data);
gpointer      e_table_memory_remove  (ETableMemory *etable,
				      int           row);
void          e_table_memory_clear   (ETableMemory *etable);

/* Freeze and thaw */
void          e_table_memory_freeze       (ETableMemory *etable);
void          e_table_memory_thaw         (ETableMemory *etable);
gpointer      e_table_memory_get_data     (ETableMemory *etm,
					   int           row);
void          e_table_memory_set_data     (ETableMemory *etm,
					   int           row,
					   gpointer      data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_MEMORY_H */
