/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_ADDRESSBOOK_REFLOW_ADAPTER_H_
#define _E_ADDRESSBOOK_REFLOW_ADAPTER_H_

#include <gal/widgets/e-reflow-model.h>
#include <gal/widgets/e-selection-model.h>
#include "e-addressbook-model.h"
#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/backend/ebook/e-book-view.h"
#include "addressbook/backend/ebook/e-card.h"

#define E_ADDRESSBOOK_REFLOW_ADAPTER_TYPE        (e_addressbook_reflow_adapter_get_type ())
#define E_ADDRESSBOOK_REFLOW_ADAPTER(o)          (GTK_CHECK_CAST ((o), E_ADDRESSBOOK_REFLOW_ADAPTER_TYPE, EAddressbookReflowAdapter))
#define E_ADDRESSBOOK_REFLOW_ADAPTER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_ADDRESSBOOK_REFLOW_ADAPTER_TYPE, EAddressbookReflowAdapterClass))
#define E_IS_ADDRESSBOOK_REFLOW_ADAPTER(o)       (GTK_CHECK_TYPE ((o), E_ADDRESSBOOK_REFLOW_ADAPTER_TYPE))
#define E_IS_ADDRESSBOOK_REFLOW_ADAPTER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_ADDRESSBOOK_REFLOW_ADAPTER_TYPE))

typedef struct _EAddressbookReflowAdapter EAddressbookReflowAdapter;
typedef struct _EAddressbookReflowAdapterPrivate EAddressbookReflowAdapterPrivate;
typedef struct _EAddressbookReflowAdapterClass EAddressbookReflowAdapterClass;

struct _EAddressbookReflowAdapter {
	EReflowModel parent;

	EAddressbookReflowAdapterPrivate *priv;
};


struct _EAddressbookReflowAdapterClass {
	EReflowModelClass parent_class;

	/*
	 * Signals
	 */
	gint (* drag_begin) (EAddressbookReflowAdapter *adapter, GdkEvent *event);
};


GtkType       e_addressbook_reflow_adapter_get_type     (void);
void          e_addressbook_reflow_adapter_construct    (EAddressbookReflowAdapter *adapter,
							 EAddressbookModel *model);
EReflowModel *e_addressbook_reflow_adapter_new          (EAddressbookModel *model);

/* Returns object with ref count of 1. */
ECard        *e_addressbook_reflow_adapter_get_card     (EAddressbookReflowAdapter *adapter,
							 int                 index);
gint          e_addressbook_reflow_adapter_right_click  (EAddressbookReflowAdapter *emvm,
							 GdkEvent           *event,
							 ESelectionModel    *selection);

#endif /* _E_ADDRESSBOOK_REFLOW_ADAPTER_H_ */
