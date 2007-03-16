/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_ADDRESSBOOK_TREEVIEW_ADAPTER_H_
#define _E_ADDRESSBOOK_TREEVIEW_ADAPTER_H_

#include <gtk/gtktreemodel.h>
#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/backend/ebook/e-book-view.h"
#include "addressbook/backend/ebook/e-card-simple.h"

#define E_TYPE_ADDRESSBOOK_TREEVIEW_ADAPTER        (e_addressbook_treeview_adapter_get_type ())
#define E_ADDRESSBOOK_TREEVIEW_ADAPTER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_ADDRESSBOOK_TREEVIEW_ADAPTER, EAddressbookTreeViewAdapter))
#define E_ADDRESSBOOK_TREEVIEW_ADAPTER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_ADDRESSBOOK_TREEVIEW_ADAPTER, EAddressbookTreeViewAdapterClass))
#define E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_ADDRESSBOOK_TREEVIEW_ADAPTER))
#define E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_ADDRESSBOOK_TREEVIEW_ADAPTER))

/* Virtual Column list:
   0   Email
   1   Full Name
   2   Street
   3   Phone
*/

typedef struct _EAddressbookTreeViewAdapter EAddressbookTreeViewAdapter;
typedef struct _EAddressbookTreeViewAdapterPrivate EAddressbookTreeViewAdapterPrivate;
typedef struct _EAddressbookTreeViewAdapterClass EAddressbookTreeViewAdapterClass;

struct _EAddressbookTreeViewAdapter {
	GObject parent;

	EAddressbookTreeViewAdapterPrivate *priv;
};


struct _EAddressbookTreeViewAdapterClass {
	GObjectClass parent_class;
};


GType         e_addressbook_treeview_adapter_get_type (void);
void          e_addressbook_treeview_adapter_construct (EAddressbookTreeViewAdapter *adapter,
							EAddressbookModel *model);
GtkTreeModel *e_addressbook_treeview_adapter_new (EAddressbookModel *model);

#endif /* _E_ADDRESSBOOK_TABLE_ADAPTER_H_ */
