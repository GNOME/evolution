/* To-do widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#include <string.h>
#include "gncal-todo.h"


static void gncal_todo_init (GncalTodo *todo);


guint
gncal_todo_get_type (void)
{
	static guint todo_type = 0;

	if (!todo_type) {
		GtkTypeInfo todo_info = {
			"GncalTodo",
			sizeof (GncalTodo),
			sizeof (GncalTodoClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) gncal_todo_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		todo_type = gtk_type_unique (gtk_vbox_get_type (), &todo_info);
	}

	return todo_type;
}

static void
add_todo (GncalTodo *todo)
{
	/* FIXME */
}

static void
edit_todo (GncalTodo *todo)
{
	/* FIXME */
}

static void
delete_todo (GncalTodo *todo)
{
	/* FIXME */
}

static void
clist_row_selected (GtkCList *clist, gint row, gint column, GdkEventButton *event, GncalTodo *todo)
{
	if (!event)
		return;

	switch (event->button) {
	case 1:
		if (event->type == GDK_2BUTTON_PRESS)
			edit_todo (todo);
		break;

	case 3:
		/* FIXME: popup menu */
		break;

	default:
		break;
	}
}

static void
add_button_clicked (GtkWidget *widget, GncalTodo *todo)
{
	add_todo (todo);
}

static void
edit_button_clicked (GtkWidget *widget, GncalTodo *todo)
{
	edit_todo (todo);
}

static void
delete_button_clicked (GtkWidget *widget, GncalTodo *todo)
{
	delete_todo (todo);
}

static void
gncal_todo_init (GncalTodo *todo)
{
	char *titles[] = { _("Done"), _("Pri"), _("Summary") };
	GtkWidget *w;
	GtkWidget *hbox;

	gtk_box_set_spacing (GTK_BOX (todo), 4);

	/* Label */

	w = gtk_label_new (_("To-do list"));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (todo), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Clist */

	w = gtk_clist_new_with_titles (3, titles);
	todo->clist = GTK_CLIST (w);

	gtk_clist_set_column_width (todo->clist, 0, 30); /* eek */
	gtk_clist_set_column_width (todo->clist, 1, 20); /* eek */
	gtk_clist_set_policy (todo->clist, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_clist_set_selection_mode (todo->clist, GTK_SELECTION_BROWSE);

	gtk_signal_connect (GTK_OBJECT (todo->clist), "select_row",
			    (GtkSignalFunc) clist_row_selected,
			    todo);

	gtk_box_pack_start (GTK_BOX (todo), w, TRUE, TRUE, 0);
	gtk_widget_show (w);

	/* Box for buttons */

	hbox = gtk_hbox_new (TRUE, 4);
	gtk_box_pack_start (GTK_BOX (todo), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	/* Add */

	w = gtk_button_new_with_label (_("Add..."));
	gtk_signal_connect (GTK_OBJECT (w), "clicked",
			    (GtkSignalFunc) add_button_clicked,
			    todo);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);
	gtk_widget_show (w);

	/* Edit */

	w = gtk_button_new_with_label (_("Edit..."));
	todo->edit_button = w;
	gtk_widget_set_sensitive (w, FALSE);
	gtk_signal_connect (GTK_OBJECT (w), "clicked",
			    (GtkSignalFunc) edit_button_clicked,
			    todo);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);
	gtk_widget_show (w);

	/* Delete */

	w = gtk_button_new_with_label (_("Delete"));
	todo->delete_button = w;
	gtk_widget_set_sensitive (w, FALSE);
	gtk_signal_connect (GTK_OBJECT (w), "clicked",
			    (GtkSignalFunc) delete_button_clicked,
			    todo);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);
	gtk_widget_show (w);
}

GtkWidget *
gncal_todo_new (GnomeCalendar *calendar)
{
	GncalTodo *todo;

	g_return_val_if_fail (calendar != NULL, NULL);

	todo = gtk_type_new (gncal_todo_get_type ());

	todo->calendar = calendar;

	gncal_todo_update (todo, NULL, 0);

	return GTK_WIDGET (todo);
}

static void
insert_in_clist (GncalTodo *todo, iCalObject *ico)
{
	int i;
	char buf[20];
	char *text[3] = { NULL, NULL, ico->summary };
	iCalObject *row_ico;

	if (ico->priority == 0) {
		strcpy (buf, "?"); /* undefined priority */
		text[1] = buf;

		i = gtk_clist_append (todo->clist, text);
	} else {
		sprintf (buf, "%d", ico->priority);
		text[1] = buf;

		/* Find proper place in clist to insert object.  Objects are sorted by priority. */

		for (i = 0; i < todo->clist->rows; i++) {
			row_ico = gtk_clist_get_row_data (todo->clist, i);

			if (ico->priority >= row_ico->priority)
				break;
		}

		gtk_clist_insert (todo->clist, i, text);
	}

	/* Set the appropriate "done" icon and hook the object to the row */

	gtk_clist_set_row_data (todo->clist, i, ico);
}

void
gncal_todo_update (GncalTodo *todo, iCalObject *ico, int flags)
{
	GList *list;

	g_return_if_fail (todo != NULL);
	g_return_if_fail (GNCAL_IS_TODO (todo));

	gtk_clist_freeze (todo->clist);

	gtk_clist_clear (todo->clist);

	for (list = todo->calendar->cal->todo; list; list = list->next)
		insert_in_clist (todo, list->data);

	gtk_clist_thaw (todo->clist);

	gtk_widget_set_sensitive (todo->edit_button, (todo->clist->rows > 0));
	gtk_widget_set_sensitive (todo->delete_button, (todo->clist->rows > 0));
}
