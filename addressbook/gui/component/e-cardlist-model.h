/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_CARDLIST_MODEL_H_
#define _E_CARDLIST_MODEL_H_

#include "e-table-model.h"
#include <ebook/e-book.h>
#include <ebook/e-book-view.h>
#include <ebook/e-card-simple.h>

#define E_CARDLIST_MODEL_TYPE        (e_cardlist_model_get_type ())
#define E_CARDLIST_MODEL(o)          (GTK_CHECK_CAST ((o), E_CARDLIST_MODEL_TYPE, ECardlistModel))
#define E_CARDLIST_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CARDLIST_MODEL_TYPE, ECardlistModelClass))
#define E_IS_CARDLIST_MODEL(o)       (GTK_CHECK_TYPE ((o), E_CARDLIST_MODEL_TYPE))
#define E_IS_CARDLIST_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CARDLIST_MODEL_TYPE))

typedef struct {
	ETableModel parent;

	/* item specific fields */
	ECardSimple **data;
	int data_count;
} ECardlistModel;


typedef struct {
	ETableModelClass parent_class;
} ECardlistModelClass;


GtkType e_cardlist_model_get_type (void);
ETableModel *e_cardlist_model_new (void);

/* Returns object with an extra ref count. */
ECard *e_cardlist_model_get  (ECardlistModel  *model,
			      int              row);
void e_cardlist_model_add    (ECardlistModel  *model,
			      ECard          **card,
			      int              count);
void e_cardlist_model_remove (ECardlistModel  *model,
			      const char      *id);

#endif /* _E_CARDLIST_MODEL_H_ */
