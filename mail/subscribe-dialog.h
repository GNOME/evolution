/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#ifndef _SUBSCRIBE_DIALOG_H_
#define _SUBSCRIBE_DIALOG_H_

#include "mail-types.h"
#include "camel.h"
#include <gtk/gtktable.h>
#include <gal/e-table/e-tree-model.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-compat.h>
#include <bonobo/bonobo-property-bag.h>
#include "shell/Evolution.h"
#include "shell/evolution-storage-set-view-listener.h"

#define SUBSCRIBE_DIALOG_TYPE        (subscribe_dialog_get_type ())
#define SUBSCRIBE_DIALOG(o)          (GTK_CHECK_CAST ((o), SUBSCRIBE_DIALOG_TYPE, SubscribeDialog))
#define SUBSCRIBE_DIALOG_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), SUBSCRIBE_DIALOG_TYPE, SubscribeDialogClass))
#define IS_SUBSCRIBE_DIALOG(o)       (GTK_CHECK_TYPE ((o), SUBSCRIBE_DIALOG_TYPE))
#define IS_SUBSCRIBE_DIALOG_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), SUBSCRIBE_DIALOG_TYPE))

struct  _SubscribeDialog {
	GtkObject parent;

	Evolution_Shell           shell;

	GtkWidget                *app;

	GtkWidget         	 *hpaned;
	GtkWidget         	 *table;
	GtkWidget         	 *description;

	GtkWidget                *store_etable;
	ETableModel              *store_model;

	GtkWidget         	 *folder_etable;
	ETreeModel        	 *folder_model;
	ETreePath         	 *folder_root;

	CamelStore               *store;
	CamelFolderInfo          *folder_info;

	GList                    *store_list;
};


typedef struct {
	GtkObjectClass parent_class;
} SubscribeDialogClass;

GtkType    subscribe_dialog_get_type             (void);
GtkWidget *subscribe_dialog_new                  (Evolution_Shell shell);

#endif /* _SUBSCRIBE_DIALOG_H_ */
