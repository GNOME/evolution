/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#ifndef _SUBSCRIBE_CONTROL_H_
#define _SUBSCRIBE_CONTROL_H_

#include "mail-types.h"
#include <gtk/gtktable.h>
#include <gal/e-table/e-tree-model.h>
#include <bonobo/bonobo-ui-handler.h>
#include <bonobo/bonobo-property-bag.h>
#include "shell/Evolution.h"


#define SUBSCRIBE_CONTROL_TYPE        (subscribe_control_get_type ())
#define SUBSCRIBE_CONTROL(o)          (GTK_CHECK_CAST ((o), SUBSCRIBE_CONTROL_TYPE, SubscribeControl))
#define SUBSCRIBE_CONTROL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), SUBSCRIBE_CONTROL_TYPE, SubscribeControlClass))
#define IS_SUBSCRIBE_CONTROL(o)       (GTK_CHECK_TYPE ((o), SUBSCRIBE_CONTROL_TYPE))
#define IS_SUBSCRIBE_CONTROL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), SUBSCRIBE_CONTROL_TYPE))

struct  _SubscribeControl {
	GtkTable parent;
	
	BonoboPropertyBag *properties;
	
	Evolution_Shell shell;

	/* This is a kludge for the toolbar problem. */
	int serial;

	/*
	 * The current URI being displayed by the SubscribeControl
	 */
	char        *uri;
	gboolean    is_news;

	GtkWidget         *description;
	GtkWidget         *table;
	ETreeModel        *model;
	ETreePath         *root;
};


typedef struct {
	GtkTableClass parent_class;
} SubscribeControlClass;

GtkType    subscribe_control_get_type             (void);
GtkWidget *subscribe_control_new                  (const Evolution_Shell  shell);

gboolean   subscribe_control_set_uri              (SubscribeControl      *subscribe_control,
						   const char            *uri);

/* menu/toolbar callbacks */
void subscribe_select_all   (BonoboUIHandler *uih, void *user_data, const char *path);
void subscribe_unselect_all (BonoboUIHandler *uih, void *user_data, const char *path);
void subscribe_folder       (GtkWidget *widget, gpointer user_data);
void unsubscribe_folder     (GtkWidget *widget, gpointer user_data);
void subscribe_refresh_list (GtkWidget *widget, gpointer user_data);
void subscribe_search       (GtkWidget *widget, gpointer user_data);

#endif /* _SUBSCRIBE_CONTROL_H_ */
