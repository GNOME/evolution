/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef __E_PANED_H__
#define __E_PANED_H__

#include <gdk/gdk.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkvpaned.h>


#define E_PANED_TYPE                  (e_paned_get_type ())
#define E_PANED(obj)                  (GTK_CHECK_CAST ((obj), E_PANED_TYPE, EPaned))
#define E_PANED_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), E_PANED_TYPE, EPanedClass))
#define E_IS_PANED(obj)               (GTK_CHECK_TYPE ((obj), E_PANED_TYPE))
#define E_IS_PANED_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), E_PANED_TYPE))


typedef struct _EPaned       EPaned;
typedef struct _EPanedClass  EPanedClass;

	
struct _EPaned
{
	GtkFrame container;

	GList *children;
	GtkPaned *toplevel_paned;
	gboolean horizontal;
};

struct _EPanedClass
{
	GtkFrameClass parent_class;
};

void       e_paned_insert    (EPaned *paned, int pos, GtkWidget *child,
			      int requested_size);
void       e_paned_remove    (EPaned *paned, GtkWidget *child);

GtkWidget *e_paned_new       (gboolean horizontal);
void       e_paned_construct (EPaned *e_paned, gboolean horizontal);
GtkType    e_paned_get_type  (void);

#endif /* __E_PANED_H__ */
