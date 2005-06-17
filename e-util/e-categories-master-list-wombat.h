/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_CATEGORIES_MASTER_LIST_WOMBAT_H_
#define _E_CATEGORIES_MASTER_LIST_WOMBAT_H_

#include <widgets/misc/e-categories-master-list-array.h>

G_BEGIN_DECLS

#define E_TYPE_CATEGORIES_MASTER_LIST_WOMBAT        (e_categories_master_list_wombat_get_type ())
#define E_CATEGORIES_MASTER_LIST_WOMBAT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_CATEGORIES_MASTER_LIST_WOMBAT, ECategoriesMasterListWombat))
#define E_CATEGORIES_MASTER_LIST_WOMBAT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_CATEGORIES_MASTER_LIST_WOMBAT, ECategoriesMasterListWombatClass))
#define E_IS_CATEGORIES_MASTER_LIST_WOMBAT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_CATEGORIES_MASTER_LIST_WOMBAT))
#define E_IS_CATEGORIES_MASTER_LIST_WOMBAT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_CATEGORIES_MASTER_LIST_WOMBAT))

typedef struct _ECategoriesMasterListWombatPriv ECategoriesMasterListWombatPriv;

typedef struct {
	ECategoriesMasterListArray       base;
	ECategoriesMasterListWombatPriv *priv;
} ECategoriesMasterListWombat;

typedef struct {
	ECategoriesMasterListArrayClass parent_class;
} ECategoriesMasterListWombatClass;

GType                  e_categories_master_list_wombat_get_type  (void);
ECategoriesMasterList *e_categories_master_list_wombat_new       (void);

G_END_DECLS

#endif /* _E_CATEGORIES_MASTER_LIST_WOMBAT_H_ */
