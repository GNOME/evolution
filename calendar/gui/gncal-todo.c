/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* To-do widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#include <config.h>
#include <string.h>
#include <gnome.h>
#include <cal-client/cal-client.h>
#include <cal-util/timeutil.h>
#include "event-editor.h"
#include "gncal-todo.h"
#include "calendar-commands.h"
#include "popup-menu.h"

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
	*date.value = icaltimetype_from_timet (d, 1);
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
	g_slist_append (l, text);
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
	d = time_from_icaltimetype (*date.value);
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

static CalComponent *
get_clist_selected_comp (GtkCList *clist)
{
	gint sel;

	if (!clist->selection)
		return NULL;

	sel = GPOINTER_TO_INT(clist->selection->data);

	return gtk_clist_get_row_data (clist, sel);
}

static void
add_todo (GncalTodo *todo)
{
	CalComponent *comp;

	comp = cal_component_new ();

#if 0
	gncal_todo_edit (todo, comp);
#endif
	gtk_object_unref (GTK_OBJECT (comp));
}

static void
edit_todo (GncalTodo *todo)
{
	CalComponent *comp;

	comp = get_clist_selected_comp (todo->clist);

#if 0
	gncal_todo_edit (todo, comp);
#endif
}

static void
delete_todo (GncalTodo *todo)
{
	CalComponent *comp;
	const char *uid;

	comp = get_clist_selected_comp (todo->clist);
	cal_component_get_uid (comp, &uid);
	
	if (!cal_client_remove_object (todo->calendar->client, uid))
		g_message ("delete_todo(): Could not remove the object!");
}

static void
add_activated (GtkWidget *widget, GncalTodo *todo)
{
	GtkWidget *w;

	while ((w = gtk_grab_get_current ()) != NULL)
		gtk_grab_remove (w);
	
	add_todo (todo);
}

static void
edit_activated (GtkWidget *widget, GncalTodo *todo)
{
	GtkWidget *w;

	while ((w = gtk_grab_get_current ()) != NULL)
		gtk_grab_remove (w);
	
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

	gtk_widget_set_sensitive (todo->edit_button, (todo->clist->selection != NULL));
	gtk_widget_set_sensitive (todo->delete_button, (todo->clist->selection != NULL));

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

/*
 * once we get a call back stating that a column
 * has been resized never ever automatically resize again
 */
static void
column_resized (GtkWidget *widget, GncalTodo *todo)
{
	/* disabling autoresize of columns */
	if (todo_list_autoresize && !todo_list_redraw_in_progess){
		todo_list_autoresize = 0;
	}
}

/*
 * restore the previously set settings for sorting the 
 * todo list
 */
static void
init_column_sorting (GtkCList *clist)
{

	/* due date isn't shown so we can't sort by it */
	if (todo_current_sort_column == 1 && ! todo_show_due_date) 
		todo_current_sort_column = 0;
	
	clist->sort_type = todo_current_sort_type;
	clist->sort_column = todo_current_sort_column;
	
	gtk_clist_set_sort_column (clist, todo_current_sort_column);
	gtk_clist_sort (clist);
}

static void 
todo_click_column (GtkCList *clist, gint column, gpointer data)
{
	if (column == clist->sort_column)
	{
		if (clist->sort_type == GTK_SORT_ASCENDING) {
			clist->sort_type = GTK_SORT_DESCENDING;
			todo_current_sort_type = GTK_SORT_DESCENDING;
		} else {
			clist->sort_type = GTK_SORT_ASCENDING;
			todo_current_sort_type = GTK_SORT_ASCENDING;
		}
	}
	else {
		gtk_clist_set_sort_column (clist, column);
		todo_current_sort_column = column;
	}
	
	gtk_clist_sort (clist); 
  
	/*
	 * save the sorting preferences cause I hate to have the user
	 * click twice
	 */

	gnome_config_set_int("/calendar/Todo/sort_column", todo_current_sort_column);
	gnome_config_set_int("/calendar/Todo/sort_type", todo_current_sort_type);
	gnome_config_sync();
}

static void
gncal_todo_init (GncalTodo *todo)
{
	GtkWidget *w;
	GtkWidget *sw;
	GtkWidget *hbox;
	gchar *titles[4] = {
	    N_("Summary"),
	    N_("Due Date"),
	    N_("Priority"),
	    N_("Time Left")
	};
	char *tmp[4];
	tmp[0] = _(titles[0]);
	tmp[1] = _(titles[1]);
	tmp[2] = _(titles[2]);
	tmp[3] = _(titles[3]);

	gtk_box_set_spacing (GTK_BOX (todo), 4);

	/* Label */

	w = gtk_label_new (_("To-do list"));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (todo), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Clist */

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (todo), sw, TRUE, TRUE, 0);
	gtk_widget_show (sw);


	w = gtk_clist_new_with_titles(4, tmp);

	todo->clist = GTK_CLIST (w);
	gtk_clist_set_selection_mode (todo->clist, GTK_SELECTION_BROWSE);

	gtk_signal_connect (GTK_OBJECT (todo->clist), "select_row",
			    (GtkSignalFunc) clist_row_selected,
			    todo);
	gtk_clist_set_button_actions (todo->clist, 2, GTK_BUTTON_SELECTS);
	gtk_signal_connect (GTK_OBJECT (todo->clist), "resize_column",
			    (GtkSignalFunc) column_resized,
			    todo);
	gtk_signal_connect (GTK_OBJECT (todo->clist), "click_column",
			  (GtkSignalFunc) todo_click_column, NULL);

	gtk_container_add (GTK_CONTAINER (sw), w);
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

static char *
convert_time_t_to_char (time_t t)
{
	char buf[100];
	struct tm tm;

	tm = *localtime (&t);
	strftime(buf, sizeof (buf), "%m/%d/%Y", &tm);

	return g_strdup (buf);
}


enum todo_styles {
	TODO_STYLE_OVERDUE,
	TODO_STYLE_DUE_TODAY,
	TODO_STYLE_NOT_DUE
};


enum todo_status {
	TODO_ITEM_DSTATUS_NOT_DUE_YET,
	TODO_ITEM_DSTATUS_DUE_TODAY,
	TODO_ITEM_DSTATUS_OVERDUE,
	TODO_ITEM_DSTATUS_LAST_DUE_STATUS
};
typedef enum todo_status todo_status;

static GtkStyle *
make_todo_style(GncalTodo *todo, todo_status style_type) 
{
	GtkStyle *style = NULL;
	GdkColor style_color;
	int color_prop = 0;
	switch(style_type) {
	case TODO_ITEM_DSTATUS_NOT_DUE_YET:
	  color_prop = COLOR_PROP_TODO_NOT_DUE_YET;
	  break;
	case TODO_ITEM_DSTATUS_DUE_TODAY:
	  color_prop = COLOR_PROP_TODO_DUE_TODAY;
	  break;
	case TODO_ITEM_DSTATUS_OVERDUE:
	  color_prop = COLOR_PROP_TODO_OVERDUE;
	  break;
	case TODO_ITEM_DSTATUS_LAST_DUE_STATUS:
	}
	
	style_color.red   = color_props[color_prop].r;
	style_color.green = color_props[color_prop].g;
	style_color.blue  = color_props[color_prop].b;
	
	style = gtk_style_copy (GTK_WIDGET (todo->clist)->style);
	style->base[GTK_STATE_NORMAL] = style_color;
	return style;
}


static
todo_status todo_item_due_status(time_t *todo_due_time) {
	struct tm due_tm_time;
	struct tm current_time;
	struct tm *temp_tm;
	time_t current_time_val = time(NULL);
	temp_tm = localtime(todo_due_time);
	/* make a copy so it dosen't get over written */
	memcpy(&due_tm_time, temp_tm, sizeof(struct tm));
	
	
	temp_tm = localtime(&current_time_val);
	memcpy(&current_time, temp_tm, sizeof(struct tm));
	
	if(due_tm_time.tm_mon == current_time.tm_mon &&
	   due_tm_time.tm_mday == current_time.tm_mday &&
	   due_tm_time.tm_year == current_time.tm_year) {
		return TODO_ITEM_DSTATUS_DUE_TODAY;
	}
	
	if((*todo_due_time) < current_time_val) {
		return TODO_ITEM_DSTATUS_OVERDUE;
	}
	
	return TODO_ITEM_DSTATUS_NOT_DUE_YET;
}


enum todo_remaining_time_form {
	TODO_ITEM_REMAINING_WEEKS,
	TODO_ITEM_REMAINING_DAYS,
	TODO_ITEM_REMAINING_HOURS,
	TODO_ITEM_REMAINING_MINUTES,
	TODO_ITEM_REMAINING_SECONDS
};
typedef enum todo_remaining_time_form todo_remaining_time_form;


static void
insert_in_clist (GncalTodo *todo, CalComponent *comp)
{
	int i;
	CalComponentText t;
	CalComponentDateTime date;
	char *text[4];
	char time_remaining_buffer[100];
	time_t time_remain;
	todo_remaining_time_form time_remaining_form;
	int sec_in_week = 3600*7*24;
	int sec_in_day = 3600*24;
	int sec_in_hour = 3600;
	int sec_in_minute = 60;
	int weeks = 0;
	int days = 0;
	int hours = 0;
	int minutes = 0;
	int seconds = 0; 
	int *p;
	time_t d;
	
	/* an array for the styles of items */
	static GtkStyle *dstatus_styles[TODO_ITEM_DSTATUS_LAST_DUE_STATUS];
	/* we want to remake the styles when the status is changed,
	   also we need to check for the null value in the pointer so we init them
	   at startup */
	if (todo_style_changed || !dstatus_styles[TODO_ITEM_DSTATUS_NOT_DUE_YET]) {
		g_free(dstatus_styles[TODO_ITEM_DSTATUS_NOT_DUE_YET]);
		g_free(dstatus_styles[TODO_ITEM_DSTATUS_OVERDUE]);
		g_free(dstatus_styles[TODO_ITEM_DSTATUS_DUE_TODAY]);
		
		dstatus_styles[TODO_ITEM_DSTATUS_NOT_DUE_YET] = make_todo_style(todo, TODO_ITEM_DSTATUS_NOT_DUE_YET);
		dstatus_styles[TODO_ITEM_DSTATUS_OVERDUE] = make_todo_style(todo, TODO_ITEM_DSTATUS_OVERDUE);
		dstatus_styles[TODO_ITEM_DSTATUS_DUE_TODAY] = make_todo_style(todo, TODO_ITEM_DSTATUS_DUE_TODAY);
		
		todo_style_changed = 0;
	}

	cal_component_get_summary (comp, &t);
	text[0] =  g_strdup (t.value);

	if(todo_show_time_remaining) {
	  memset(time_remaining_buffer, 0, 100);
	  /* we need to make a string that represents the amount of time remaining
	     before this task is due */
	  
	  /* for right now all I'll do is up to the hours. */
	  cal_component_get_dtend (comp, &date);
	  time_remain = time_from_icaltimetype (*date.value) - time (NULL);
	  if(time_remain < 0) {
	    text[3] = "Overdue!";
	  }
	  else {

	    /* lets determine a decent denomination to display */
	    if(time_remain / (sec_in_week)) 
	      {
	    	/* we have weeks available */
	    	time_remaining_form = TODO_ITEM_REMAINING_WEEKS;
		weeks = time_remain / sec_in_week;
		days = (time_remain % (sec_in_week))/sec_in_day;
	      }
	    else if(time_remain / (sec_in_day)) 
	      {
		/* we have days available */
	    	time_remaining_form = TODO_ITEM_REMAINING_DAYS;
		days = time_remain / sec_in_day;
		hours = (time_remain % sec_in_day)/sec_in_hour;
	      }
	    else if(time_remain / (sec_in_hour)) 
	      {
		/* we have hours available */
	    	time_remaining_form = TODO_ITEM_REMAINING_HOURS;
		hours = time_remain /sec_in_hour;
		minutes = (time_remain % sec_in_hour) / sec_in_minute;
	      }
	    else if(time_remain / sec_in_minute) 
	      {
	    	time_remaining_form = TODO_ITEM_REMAINING_MINUTES;
		minutes = time_remain / sec_in_minute;
		seconds = time_remain % sec_in_minute;
	      }
	    else 
	      {
	    	time_remaining_form = TODO_ITEM_REMAINING_SECONDS;
		seconds = time_remain;
	      }

	    switch(time_remaining_form) 
	      {
	      case TODO_ITEM_REMAINING_WEEKS:
		snprintf(time_remaining_buffer, 100, "%d %s %d %s", weeks,
			 (weeks > 1) ? _("Weeks") : _("Week"),
			 days, (days > 1) ? _("Days") : _("Day"));
		break;
	      case TODO_ITEM_REMAINING_DAYS:
		snprintf(time_remaining_buffer, 100, "%d %s %d %s", days,
			 (days > 1) ? _("Days") : _("Day"),
			 hours, (hours > 1) ? _("Hours") : _("Hour"));
		break;
	      case TODO_ITEM_REMAINING_HOURS:
		snprintf(time_remaining_buffer, 100, "%d %s %d %s", hours,
			 (hours > 1) ? _("Hours") : _("Hour"),
			 minutes, (minutes > 1) ? _("Minutes") : _("Minute"));
		break;
	      case TODO_ITEM_REMAINING_MINUTES:
		snprintf(time_remaining_buffer, 100, "%d %s %d %s", minutes,
			 (minutes > 1) ? _("Minutes") : _("Minute"),
			 seconds, (seconds > 1) ? _("Seconds") : _("Second"));
		break;
	      case TODO_ITEM_REMAINING_SECONDS:
		snprintf(time_remaining_buffer, 100, "%d %s", seconds,
			 (seconds > 1) ? _("Seconds") : _("Second")); 
		break;
	      }
	    text[3] = g_strdup(time_remaining_buffer);
	    todo->data_ptrs = g_slist_append(todo->data_ptrs, text[3]);
	  }

	}
	else {
	  text[3] = "Loose penguini!";
	}
	/*
	 * right now column 0 will be the summary
	 * and column 1 will be the due date. 
	 * WISH:  this should be able to be changed on the fly
	 */

	cal_component_get_dtend (comp, &date);
	d = time_from_icaltimetype (*date.value);
	if(todo_show_due_date)
	  {
	    text[1] = convert_time_t_to_char (d);
	    /* Append the data's pointer so later it can be properly freed */
	    todo->data_ptrs = g_slist_append (todo->data_ptrs, text[1]);
	  }
	else
		text[1] = NULL;

	cal_component_get_priority (comp, &p);
	if(p && todo_show_priority)
	  {
	    text[2] = g_strdup_printf ("%d", *p);
	    todo->data_ptrs = g_slist_append (todo->data_ptrs, text[2]);
	  }
	else
	  text[2] = NULL;

	i = gtk_clist_append (todo->clist, text);
	
	gtk_clist_set_row_data_full (todo->clist, i, comp,
				     (GtkDestroyNotify) gtk_object_ref);
	gtk_object_ref (GTK_OBJECT (comp));

	/*
	 * determine if the task is overdue..
	 * if so mark with the apropriate style
	 */

	switch(todo_item_due_status(&d)) {
	case TODO_ITEM_DSTATUS_NOT_DUE_YET:
	  if(todo_item_dstatus_highlight_not_due_yet) 
	    {
	      gtk_clist_set_row_style(todo->clist, i, dstatus_styles[TODO_ITEM_DSTATUS_NOT_DUE_YET]);
	    }
	  break;
	case TODO_ITEM_DSTATUS_DUE_TODAY:
	  if(todo_item_dstatus_highlight_due_today)
	    {
	      gtk_clist_set_row_style(todo->clist, i, dstatus_styles[TODO_ITEM_DSTATUS_DUE_TODAY]);
	    }
	  break;
	case TODO_ITEM_DSTATUS_OVERDUE:
	  if(todo_item_dstatus_highlight_overdue) 
	    {
	      gtk_clist_set_row_style(todo->clist, i, dstatus_styles[TODO_ITEM_DSTATUS_OVERDUE]);
	    }
	  break;
	case TODO_ITEM_DSTATUS_LAST_DUE_STATUS:
	}
	
	/* keep the list in order */
	gtk_clist_sort (todo->clist); 
}


void
gncal_todo_update (GncalTodo *todo, CalComponent *comp, int flags)
{
	GSList *current_list;
	CalClientGetStatus status;
	GList *l, *uids;

	g_return_if_fail (todo != NULL);
	g_return_if_fail (GNCAL_IS_TODO (todo));
	
	/*
	 * shut down the resize handler cause we are playing with the list. 
	 * In otherwords turn off the event handler
	 */
	todo_list_redraw_in_progess =1;
	
	/* freeze the list */
	gtk_clist_freeze (todo->clist);
	init_column_sorting (todo->clist);

	/*
	 * before here we have to free some of the memory that
	 * stores the due date, or else we have a memory leak. 
	 * luckily all of the pointers are stored in todo->data_ptrs;
	 */

	/* check on the columns that we should display */
	/* check for due date */

	if(todo_show_due_date) {
	  gtk_clist_set_column_visibility (todo->clist, 1, 1);
	}
	else {
	  gtk_clist_set_column_visibility (todo->clist, 1, 0);
	}

	if(todo_show_time_remaining) {
	  gtk_clist_set_column_visibility (todo->clist, 3, 1);
	}
	else {
	  gtk_clist_set_column_visibility (todo->clist, 3, 0);
	}
				       

	if(todo_show_priority)
	  gtk_clist_set_column_visibility (todo->clist, 2, 1);
	else
	  gtk_clist_set_column_visibility (todo->clist, 2, 0);
	
	/* free the memory locations that were used in the previous display */
	for (current_list = todo->data_ptrs;
	     current_list != NULL;
	     current_list = g_slist_next(current_list)){
		g_free(current_list->data);
	}

	/* free the list and clear out the pointer */
	g_slist_free(todo->data_ptrs);
	todo->data_ptrs = NULL;

	gtk_clist_clear (todo->clist);


	uids = cal_client_get_uids (todo->calendar->client, CAL_COMPONENT_TODO);
	for (l = uids; l; l = l->next){
		char *uid = l->data;
		CalComponent *comp;

		status = cal_client_get_object (todo->calendar->client, uid,
						&comp);

		if (status == CAL_CLIENT_GET_SUCCESS) {
			insert_in_clist (todo, comp);
			gtk_object_unref (GTK_OBJECT (comp));
		}
#ifndef NO_WARNINGS
#warning "FIX ME"
#endif
		/* else? */
		g_free (uid);
	}
	g_list_free (uids);

	/* if we are autoresizing then do it now */
	if(todo_list_autoresize && todo->clist->rows != 0) 
		gtk_clist_columns_autosize (todo->clist);

	gtk_clist_thaw (todo->clist);
	
	gtk_widget_set_sensitive (todo->edit_button,
				  (todo->clist->selection != NULL));
	gtk_widget_set_sensitive (todo->delete_button,
				  (todo->clist->selection != NULL));
	todo_list_redraw_in_progess = 0;
}











