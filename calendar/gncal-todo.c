/* To-do widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#include <config.h>
#include <string.h>
#include <gnome.h>
#include "gncal-todo.h"
#include "main.h"
#include "popup-menu.h"


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
ok_button (GtkWidget *widget, GnomeDialog *dialog)
{
	iCalObject *ico;
	GncalTodo *todo;
	GtkEntry *entry;

	ico = gtk_object_get_user_data (GTK_OBJECT (dialog));

	todo = GNCAL_TODO (gtk_object_get_data (GTK_OBJECT (dialog), "gncal_todo"));
	entry = GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (dialog), "summary_entry"));

	if (ico->summary)
		g_free (ico->summary);

	ico->summary = g_strdup (gtk_entry_get_text (entry));
	ico->user_data = NULL;

	if (ico->new) {
		gnome_calendar_add_object (todo->calendar, ico);
		ico->new = FALSE;
	} else
		gnome_calendar_object_changed (todo->calendar, ico, CHANGE_ALL); /* ok, summary only... */

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
cancel_button (GtkWidget *widget, GnomeDialog *dialog)
{
	iCalObject *ico;

	ico = gtk_object_get_user_data (GTK_OBJECT (dialog));

	ico->user_data = NULL;

	if (ico->new)
		ical_object_destroy (ico);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gint
delete_event (GtkWidget *widget, GdkEvent *event, GnomeDialog *dialog)
{
	cancel_button (NULL, dialog);
	return TRUE;
}

static void
simple_todo_editor (GncalTodo *todo, iCalObject *ico)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *w;
	GtkWidget *entry;

	dialog = gnome_dialog_new (ico->new ? _("Create to-do item") : _("Edit to-do item"),
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (hbox), 4);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	w = gtk_label_new (_("Summary:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), ico->summary);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	gtk_widget_show (entry);

	ico->user_data = dialog;

	gtk_object_set_user_data (GTK_OBJECT (dialog), ico);

	gtk_object_set_data (GTK_OBJECT (dialog), "gncal_todo", todo);
	gtk_object_set_data (GTK_OBJECT (dialog), "summary_entry", entry);

	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0, (GtkSignalFunc) ok_button, dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 1, (GtkSignalFunc) cancel_button, dialog);
	gtk_signal_connect (GTK_OBJECT (dialog), "delete_event",
			    (GtkSignalFunc) delete_event,
			    dialog);

	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);

	gtk_window_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_widget_show (dialog);
	gtk_widget_grab_focus (entry);
}

static iCalObject *
get_clist_selected_ico (GtkCList *clist)
{
	gint sel;

	if (!clist->selection)
		return NULL;

	sel = (gint) clist->selection->data;

	return gtk_clist_get_row_data (clist, sel);
}

static void
add_todo (GncalTodo *todo)
{
	iCalObject *ico;

	ico = ical_new ("", user_name, "");
	ico->type = ICAL_TODO;
	ico->new = TRUE;

	simple_todo_editor (todo, ico);
}

static void
edit_todo (GncalTodo *todo)
{
	simple_todo_editor (todo, get_clist_selected_ico (todo->clist));
}

static void
delete_todo (GncalTodo *todo)
{
	gnome_calendar_remove_object (todo->calendar, get_clist_selected_ico (todo->clist));
}

static void
add_activated (GtkWidget *widget, GncalTodo *todo)
{
	add_todo (todo);
}

static void
edit_activated (GtkWidget *widget, GncalTodo *todo)
{
	edit_todo (todo);
}

static void
delete_activated (GtkWidget *widget, GncalTodo *todo)
{
	delete_todo (todo);
}

static void
clist_row_selected (GtkCList *clist, gint row, gint column, GdkEventButton *event, GncalTodo *todo)
{
	static struct menu_item items[] = {
		{ N_("Add to-do item..."), (GtkSignalFunc) add_activated, NULL, TRUE },
		{ N_("Edit this item..."), (GtkSignalFunc) edit_activated, NULL, TRUE },
		{ N_("Delete this item"), (GtkSignalFunc) delete_activated, NULL, TRUE }
	};

	int i;

	if (!event)
		return;

	switch (event->button) {
	case 1:
		if (event->type == GDK_2BUTTON_PRESS)
			edit_todo (todo);
		break;

	case 3:
		for (i = 0; i < (sizeof (items) / sizeof (items[0])); i++)
			items[i].data = todo;

		popup_menu (items, sizeof (items) / sizeof (items[0]), event);
		break;

	default:
		break;
	}
}

static void
gncal_todo_init (GncalTodo *todo)
{
	GtkWidget *w;
	GtkWidget *hbox;

	gtk_box_set_spacing (GTK_BOX (todo), 4);

	/* Label */

	w = gtk_label_new (_("To-do list"));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (todo), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Clist */

	w = gtk_clist_new (1);
	todo->clist = GTK_CLIST (w);

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
			    (GtkSignalFunc) add_activated,
			    todo);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);
	gtk_widget_show (w);

	/* Edit */

	w = gtk_button_new_with_label (_("Edit..."));
	todo->edit_button = w;
	gtk_widget_set_sensitive (w, FALSE);
	gtk_signal_connect (GTK_OBJECT (w), "clicked",
			    (GtkSignalFunc) edit_activated,
			    todo);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);
	gtk_widget_show (w);

	/* Delete */

	w = gtk_button_new_with_label (_("Delete"));
	todo->delete_button = w;
	gtk_widget_set_sensitive (w, FALSE);
	gtk_signal_connect (GTK_OBJECT (w), "clicked",
			    (GtkSignalFunc) delete_activated,
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
	char *text[1] = { ico->summary };
	iCalObject *row_ico;

	if (ico->priority == 0)
		i = gtk_clist_append (todo->clist, text); /* items with undefined priority go to the end of the list */
	else {

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
