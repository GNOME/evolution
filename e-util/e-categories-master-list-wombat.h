/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_CATEGORIES_MASTER_LIST_WOMBAT_H_
#define _E_CATEGORIES_MASTER_LIST_WOMBAT_H_

#include <gal/widgets/e-categories-master-list-array.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_CATEGORIES_MASTER_LIST_WOMBAT_TYPE        (e_categories_master_list_wombat_get_type ())
#define E_CATEGORIES_MASTER_LIST_WOMBAT(o)          (GTK_CHECK_CAST ((o), E_CATEGORIES_MASTER_LIST_WOMBAT_TYPE, ECategoriesMasterListWombat))
#define E_CATEGORIES_MASTER_LIST_WOMBAT_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CATEGORIES_MASTER_LIST_WOMBAT_TYPE, ECategoriesMasterListWombatClass))
#define E_IS_CATEGORIES_MASTER_LIST_WOMBAT(o)       (GTK_CHECK_TYPE ((o), E_CATEGORIES_MASTER_LIST_WOMBAT_TYPE))
#define E_IS_CATEGORIES_MASTER_LIST_WOMBAT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CATEGORIES_MASTER_LIST_WOMBAT_TYPE))

typedef struct _ECategoriesMasterListWombatPriv ECategoriesMasterListWombatPriv;

typedef struct {
	ECategoriesMasterListArray       base;
	ECategoriesMasterListWombatPriv *priv;
} ECategoriesMasterListWombat;

typedef struct {
	ECategoriesMasterListArrayClass parent_class;
} ECategoriesMasterListWombatClass;

GtkType                e_categories_master_list_wombat_get_type  (void);
ECategoriesMasterList *e_categories_master_list_wombat_new       (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_CATEGORIES_MASTER_LIST_WOMBAT_H_ */
