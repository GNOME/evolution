/* To-do widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#ifndef GNCAL_TODO_H
#define GNCAL_TODO_H

#include <gtk/gtkclist.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include "gnome-cal.h"


BEGIN_GNOME_DECLS


#define GNCAL_TODO(obj)         GTK_CHECK_CAST (obj, gncal_todo_get_type (), GncalTodo)
#define GNCAL_TODO_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gncal_todo_get_type (), GncalTodoClass)
#define GNCAL_IS_TODO(obj)      GTK_CHECK_TYPE (obj, gncal_todo_get_type ())


typedef struct _GncalTodo GncalTodo;
typedef struct _GncalTodoClass GncalTodoClass;

struct _GncalTodo {
	GtkVBox vbox;

	GnomeCalendar *calendar;	/* the calendar we are associated to */

	GtkCList *clist;

	GtkWidget *edit_button;
	GtkWidget *delete_button;
              GSList *data_ptrs;
  
 
};

struct _GncalTodoClass {
	GtkVBoxClass parent_class;
};


guint      gncal_todo_get_type (void);
GtkWidget *gncal_todo_new      (GnomeCalendar *calendar);

void       gncal_todo_update   (GncalTodo *todo, iCalObject *ico, int flags);


END_GNOME_DECLS

#endif
