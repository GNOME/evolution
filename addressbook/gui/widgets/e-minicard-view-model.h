/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_MINICARD_VIEW_MODEL_H_
#define _E_MINICARD_VIEW_MODEL_H_

#include <gal/widgets/e-reflow-model.h>
#include <gal/widgets/e-selection-model.h>
#include "e-minicard.h"
#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/backend/ebook/e-book-view.h"
#include "addressbook/backend/ebook/e-card.h"

#define E_MINICARD_VIEW_MODEL_TYPE        (e_minicard_view_model_get_type ())
#define E_MINICARD_VIEW_MODEL(o)          (GTK_CHECK_CAST ((o), E_MINICARD_VIEW_MODEL_TYPE, EMinicardViewModel))
#define E_MINICARD_VIEW_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_MINICARD_VIEW_MODEL_TYPE, EMinicardViewModelClass))
#define E_IS_MINICARD_VIEW_MODEL(o)       (GTK_CHECK_TYPE ((o), E_MINICARD_VIEW_MODEL_TYPE))
#define E_IS_MINICARD_VIEW_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_MINICARD_VIEW_MODEL_TYPE))

/* Virtual Column list:
   0   Email
   1   Full Name
   2   Street
   3   Phone
*/

typedef struct _EMinicardViewModel EMinicardViewModel;
typedef struct _EMinicardViewModelClass EMinicardViewModelClass;

struct _EMinicardViewModel {
	EReflowModel parent;

	/* item specific fields */
	EBook *book;
	char *query;
	EBookView *book_view;

	int get_view_idle;

	ECard **data;
	int data_count;
	int allocated_count;

	int create_card_id, remove_card_id, modify_card_id, status_message_id;

	guint editable : 1;
	guint first_get_view : 1;
};


struct _EMinicardViewModelClass {
	EReflowModelClass parent_class;

	/*
	 * Signals
	 */
	void (*status_message) (EMinicardViewModel *model, const gchar *message);
	gint (* drag_begin) (EMinicardViewModel *model, GdkEvent *event);
};


GtkType       e_minicard_view_model_get_type     (void);
EReflowModel *e_minicard_view_model_new          (void);

/* Returns object with ref count of 1. */
ECard        *e_minicard_view_model_get_card     (EMinicardViewModel *model,
						  int                 row);
void          e_minicard_view_model_stop         (EMinicardViewModel *model);
gint          e_minicard_view_model_right_click  (EMinicardViewModel *emvm,
						  GdkEvent           *event,
						  ESelectionModel    *selection);

#endif /* _E_MINICARD_VIEW_MODEL_H_ */
