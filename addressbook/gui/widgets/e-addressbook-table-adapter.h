/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _EAB_TABLE_ADAPTER_H_
#define _EAB_TABLE_ADAPTER_H_

#include <table/e-table-model.h>
#include <libebook/e-book.h>
#include <libebook/e-book-view.h>

#define E_TYPE_AB_TABLE_ADAPTER                 (eab_table_adapter_get_type ())
#define EAB_TABLE_ADAPTER(o)                    (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_AB_TABLE_ADAPTER, EAddressbookTableAdapter))
#define EAB_TABLE_ADAPTER_CLASS(k)              (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_AB_TABLE_ADAPTER, EAddressbookTableAdapterClass))
#define E_IS_ADDRESSBOOK_TABLE_ADAPTER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_AB_TABLE_ADAPTER))
#define E_IS_ADDRESSBOOK_TABLE_ADAPTER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_AB_TABLE_ADAPTER))

typedef struct _EAddressbookTableAdapter EAddressbookTableAdapter;
typedef struct _EAddressbookTableAdapterPrivate EAddressbookTableAdapterPrivate;
typedef struct _EAddressbookTableAdapterClass EAddressbookTableAdapterClass;

struct _EAddressbookTableAdapter {
	ETableModel parent;

	EAddressbookTableAdapterPrivate *priv;
};


struct _EAddressbookTableAdapterClass {
	ETableModelClass parent_class;
};


GType        eab_table_adapter_get_type (void);
void         eab_table_adapter_construct (EAddressbookTableAdapter *adapter,
					  EABModel *model);
ETableModel *eab_table_adapter_new (EABModel *model);

#endif /* _EAB_TABLE_ADAPTER_H_ */
