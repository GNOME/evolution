/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#ifndef _SUBSCRIBE_CONTROL_H_
#define _SUBSCRIBE_CONTROL_H_

#include "mail-types.h"
#include <gtk/gtktable.h>
#include <gal/e-table/e-tree-model.h>
#include <bonobo/bonobo-ui-compat.h>
#include <bonobo/bonobo-property-bag.h>
#include "shell/Evolution.h"


#define SUBSCRIBE_CONTROL_TYPE        (subscribe_control_get_type ())
#define SUBSCRIBE_CONTROL(o)          (GTK_CHECK_CAST ((o), SUBSCRIBE_CONTROL_TYPE, SubscribeControl))
#define SUBSCRIBE_CONTROL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), SUBSCRIBE_CONTROL_TYPE, SubscribeControlClass))
#define IS_SUBSCRIBE_CONTROL(o)       (GTK_CHECK_TYPE ((o), SUBSCRIBE_CONTROL_TYPE))
#define IS_SUBSCRIBE_CONTROL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), SUBSCRIBE_CONTROL_TYPE))

struct  _SubscribeControl {
	GtkObject parent;

	BonoboUIHandler   *uih;

	GtkWidget *app;
	
	GtkWidget         *storage_set_view;
	GtkWidget         *hpaned;
	GtkWidget         *table;
	GtkWidget         *description;
	GtkWidget         *etable;
	ETreeModel        *model;
	ETreePath         *root;
};


typedef struct {
	GtkObjectClass parent_class;
} SubscribeControlClass;

GtkType    subscribe_control_get_type             (void);
GtkWidget *subscribe_control_new                  (void);

#endif /* _SUBSCRIBE_CONTROL_H_ */
