/* To-do widget for gncal
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena <federico@helixcode.com>
 */

#include <config.h>
#include <gnome.h>
#include "event-editor.h"
#include "gncal-todo.h"

int todo_show_due_date = 0;
int todo_show_priority = 0;
int todo_show_time_remaining = 0;

int todo_item_dstatus_highlight_overdue = 0;
int todo_item_dstatus_highlight_due_today = 0;
int todo_item_dstatus_highlight_not_due_yet = 0;


char *todo_overdue_font_text;
gint todo_current_sort_column = 0;
gint todo_current_sort_type = GTK_SORT_ASCENDING;

gboolean todo_style_changed =0;
gboolean todo_list_autoresize = 1;
gboolean todo_list_redraw_in_progess = 0;


static void
ok_button (GtkWidget *widget, GnomeDialog *dialog)
{
	CalComponent *comp;
	CalClient *cal_client;
	GnomeDateEdit *due_date;
	GtkEditable *entry;
	GtkSpinButton *priority;
	GtkText *comment;
	CalComponentText *text = g_new0 (CalComponentText, 1);
	CalComponentDateTime date;
	GSList *l;
	gchar *t;
	time_t d;
	int p;

	comp = gtk_object_get_user_data (GTK_OBJECT (dialog));

	cal_client = (CalClient*) (gtk_object_get_data (GTK_OBJECT (dialog), "cal_client"));

	/* Due date */
	due_date = GNOME_DATE_EDIT (gtk_object_get_data(GTK_OBJECT(dialog), "due_date"));
	d = gnome_date_edit_get_date (due_date);
	date.value = g_new0 (struct icaltimetype, 1);
	*date.value = icaltime_from_timet (d, 1, TRUE);
	cal_component_set_dtend (comp, &date);

	/* Summary */
	entry = GTK_EDITABLE (gtk_object_get_data (GTK_OBJECT (dialog), "summary_entry"));
	t = gtk_editable_get_chars (entry, 0, -1);
	text->value = t;
	cal_component_set_summary (comp, text);
	g_free (t);

	/* Priority */
	priority = GTK_SPIN_BUTTON (gtk_object_get_data(GTK_OBJECT(dialog), "priority"));
	p = gtk_spin_button_get_value_as_int (priority);
	cal_component_set_priority (comp, &p);

	/* Comment */
	cal_component_get_comment_list (comp, &l);
	comment = GTK_TEXT(gtk_object_get_data (GTK_OBJECT(dialog), "comment"));
	t = gtk_editable_get_chars (entry, 0, -1);
	text->value = t;
	l = g_slist_append (l, text);
	cal_component_set_comment_list (comp, l);
	cal_component_free_text_list (l);

	if (!cal_client_update_object (cal_client, comp))
		g_message ("ok_button(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
cancel_button (GtkWidget *widget, GnomeDialog *dialog)
{
	CalComponent *comp;

	comp = gtk_object_get_user_data (GTK_OBJECT (dialog));

	gtk_object_unref (GTK_OBJECT (comp));

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gint
delete_event (GtkWidget *widget, GdkEvent *event, GnomeDialog *dialog)
{
	cancel_button (NULL, dialog);
	return TRUE;
}

/* I've hacked this so we can use it separate from the rest of GncalTodo.
   This whole file will go once we've got the new editor working. */
void
gncal_todo_edit (CalClient *client, CalComponent *comp)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *due_box;
	GtkWidget *due_label;
	GtkWidget *due_entry;
	GtkWidget *comment_box;
	GtkWidget *comment_label;
	GtkWidget *comment_text;
	GtkWidget *comment_internal_box;
	GtkWidget *comment_sep;
	GtkWidget *w;
	GtkWidget *pri_box;
	GtkWidget *pri_label;
	GtkWidget *pri_spin;
	GtkObject *pri_adj;
	GtkWidget *entry;
	gboolean new;
	CalComponentText text;
	CalComponentDateTime date;
	GSList *l;
	time_t d;
	gint *p;

	new = (CAL_COMPONENT_NO_TYPE == cal_component_get_vtype (comp));
	if (new)
		cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);

	dialog = gnome_dialog_new (new ? _("Create to-do item") : _("Edit to-do item"),
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
#if 0
	gnome_dialog_set_parent (GNOME_DIALOG (dialog),
	  GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (todo->calendar))));
#endif
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (hbox), 4);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);


	due_box = gtk_hbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (due_box), 4);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), due_box, FALSE, FALSE, 0);
	gtk_widget_show (due_box);

	pri_box = gtk_hbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (pri_box), 4);
	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)->vbox), pri_box, FALSE, FALSE, 0);
	gtk_widget_show (pri_box);

	comment_box = gtk_hbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (comment_box), 4);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), comment_box, FALSE, FALSE, 0);
	gtk_widget_show (comment_box);

	comment_internal_box = gtk_vbox_new(FALSE,2);
	gtk_container_border_width (GTK_CONTAINER (comment_internal_box), 4);

	gtk_box_pack_start (GTK_BOX (comment_box), comment_internal_box, TRUE, TRUE, 0);
	gtk_widget_show (comment_internal_box);

	w = gtk_label_new (_("Summary:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	entry = gtk_entry_new ();
	cal_component_get_summary (comp, &text);
	gtk_entry_set_text (GTK_ENTRY (entry), text.value);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	gtk_widget_show (entry);


	due_label = gtk_label_new (_("Due Date:"));
	gtk_box_pack_start (GTK_BOX (due_box), due_label, FALSE, FALSE, 0);
	gtk_widget_show (due_label);

	due_entry = gtk_entry_new ();
	cal_component_get_dtend (comp, &date);
	d = icaltime_as_timet (*date.value);
	due_entry = date_edit_new (d, TRUE);
	gtk_box_pack_start (GTK_BOX (due_box), due_entry, TRUE, TRUE, 0);
	gtk_widget_show (due_entry);

	pri_label = gtk_label_new (_("Priority:"));
	gtk_box_pack_start (GTK_BOX (pri_box), pri_label, FALSE, FALSE, 0);
	gtk_widget_show (pri_label);

	pri_adj = gtk_adjustment_new (5.0, 1.0, 9.0, 1.0, 3.0, 0.0);
	pri_spin = gtk_spin_button_new (GTK_ADJUSTMENT(pri_adj), 0.0, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pri_spin), TRUE);
	gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (pri_spin), FALSE);
	gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (pri_spin), FALSE);
	cal_component_get_priority (comp, &p);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pri_spin), (gfloat) *p);
	gtk_box_pack_start (GTK_BOX (pri_box), pri_spin, FALSE, FALSE, 0);
	gtk_widget_show (pri_spin);

	comment_sep = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (comment_box), comment_sep, FALSE, FALSE, 0);
	gtk_widget_show(comment_sep);

	comment_label = gtk_label_new (_("Item Comments:"));
	gtk_label_set_justify(GTK_LABEL(comment_label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (comment_internal_box), comment_label, TRUE, TRUE, 0);
	gtk_widget_show (comment_label);

	comment_text = gtk_text_new (NULL, NULL);
	gtk_text_set_editable (GTK_TEXT (comment_text), TRUE);
	gtk_text_set_word_wrap( GTK_TEXT(comment_text), TRUE);
	gtk_text_freeze(GTK_TEXT(comment_text));
#ifndef NO_WARNINGS
#warning "FIX ME"
#endif
	/* Need to handle multiple comments */
	cal_component_get_comment_list (comp, &l);
	if (l) {
		CalComponentText text = *(CalComponentText*)l->data;

		gtk_text_insert(GTK_TEXT(comment_text), NULL, NULL, NULL,
				text.value, strlen(text.value));
	}
	cal_component_free_text_list (l);
	gtk_text_thaw(GTK_TEXT(comment_text));
	gtk_box_pack_start (GTK_BOX (comment_internal_box), comment_text, FALSE, TRUE, 0);
	gtk_widget_show (comment_text);

	gtk_object_set_user_data (GTK_OBJECT (dialog), comp);
	gtk_object_ref (GTK_OBJECT (comp));

	gtk_object_set_data (GTK_OBJECT (dialog), "cal_client", client);
	gtk_object_set_data (GTK_OBJECT (dialog), "summary_entry", entry);
	gtk_object_set_data (GTK_OBJECT (dialog), "due_date", due_entry);
	gtk_object_set_data (GTK_OBJECT (dialog), "priority", pri_spin);
	gtk_object_set_data (GTK_OBJECT (dialog), "comment", comment_text);

	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0, (GtkSignalFunc) ok_button, dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 1, (GtkSignalFunc) cancel_button, dialog);

	gtk_signal_connect (GTK_OBJECT (dialog), "delete_event",
			    (GtkSignalFunc) delete_event,
			    dialog);

	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
	gnome_dialog_editable_enters (GNOME_DIALOG (dialog), GTK_EDITABLE(entry));

	gtk_window_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_widget_show (dialog);
	gtk_widget_grab_focus (entry);
}
