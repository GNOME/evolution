/*
 * EventEditor widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */

#include <gnome.h>
#include "calendar.h"
#include "eventedit.h"
#include "main.h"

static void event_editor_init          (EventEditor *ee);
GtkWindow *parent_class;

guint
event_editor_get_type (void)
{
	static guint event_editor_type = 0;
	
	if(!event_editor_type) {
		GtkTypeInfo event_editor_info = {
			"EventEditor",
			sizeof(EventEditor),
			sizeof(EventEditorClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) event_editor_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		event_editor_type = gtk_type_unique (gtk_window_get_type (), &event_editor_info);
		parent_class = gtk_type_class (gtk_window_get_type ());
	}
	return event_editor_type;
}

/*
 * when the start time is changed, this adjusts the end time. 
 */
static void
adjust_end_time (GtkWidget *widget, EventEditor *ee)
{
	struct tm *tm;
	time_t start_t;

	start_t = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->start_time));
	tm = localtime (&start_t);
	if (tm->tm_hour < 22)
		tm->tm_hour++;
	gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->end_time), mktime (tm));
}

GtkWidget *
adjust (GtkWidget *w, gfloat x, gfloat y, gfloat xs, gfloat ys)
{
	GtkWidget *a = gtk_alignment_new (x, y, xs, ys);
	
	gtk_container_add (GTK_CONTAINER (a), w);
	return a;
}

static GtkWidget *
event_editor_setup_time_frame (EventEditor *ee)
{
	GtkWidget *frame;
	GtkWidget *start_time, *end_time, *allday, *recur;
	GtkTable  *t;
	
	frame = gtk_frame_new (_("Time"));
	t = GTK_TABLE (ee->general_time_table = gtk_table_new (1, 1, 0));
	gtk_container_add (GTK_CONTAINER (frame), ee->general_time_table);
	
	ee->start_time = start_time = gnome_date_edit_new (ee->ical->dtstart);
	ee->end_time   = end_time   = gnome_date_edit_new (ee->ical->dtend);
	gnome_date_edit_set_popup_range ((GnomeDateEdit *) start_time, day_begin, day_end);
	gnome_date_edit_set_popup_range ((GnomeDateEdit *) end_time,   day_begin, day_end);
	gtk_signal_connect (GTK_OBJECT (start_time), "time_changed",
			    GTK_SIGNAL_FUNC (adjust_end_time), ee);
	gtk_table_attach (t, gtk_label_new (_("Start time")), 1, 2, 1, 2, 0, 0, 0, 0);
	gtk_table_attach (t, gtk_label_new (_("End time")), 1, 2, 2, 3, 0, 0, 0, 0);
	
	gtk_table_attach (t, start_time, 2, 3, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (t, end_time, 2, 3, 2, 3,   GTK_EXPAND | GTK_FILL, 0, 0, 0);

	allday = gtk_check_button_new_with_label (_("All day event"));
	gtk_table_attach (t, allday, 3, 4, 1, 2, 0, 0, 0, 0);

	recur  = gtk_check_button_new_with_label (_("Recurring event"));
	gtk_table_attach (t, recur, 3, 4, 2, 3, 0, 0, 0, 0);

	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	return frame;
}

static GtkWidget *
ee_single_alarm (char *text, CalendarAlarm **alarm, void *x)
{
	GtkWidget *hbox;
	GtkWidget *check;
	GtkWidget *entry;

	hbox = gtk_hbox_new (0, 0);
	
	check = gtk_check_button_new_with_label (text);
	entry = gtk_entry_new ();
	gtk_widget_set_usize (entry, 40, 0);
	gtk_box_pack_start (GTK_BOX (hbox), check, 0, 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), entry, 0, 0, 0);

	return hbox;
}

#define FX GTK_FILL | GTK_EXPAND
static GtkWidget *
ee_alarm_widgets (EventEditor *ee)
{
	GtkWidget *table, *aalarm, *dalarm, *palarm, *malarm, *mailto, *mailte, *l;
	
	l = gtk_frame_new (_("Alarm"));
	
	table = gtk_table_new (1, 1, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), 3);
	gtk_container_add (GTK_CONTAINER (l), table);
	
	mailto  = gtk_label_new (_("Mail to:"));
	mailte  = gtk_entry_new ();
	dalarm  = gtk_check_button_new_with_label (_("Display"));
	aalarm  = gtk_check_button_new_with_label (_("Audio"));
	palarm  = gtk_check_button_new_with_label (_("Program"));
	malarm  = gtk_check_button_new_with_label (_("Mail"));

	gtk_table_attach (GTK_TABLE (table), dalarm, 2, 3, 1, 2, FX, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), aalarm, 2, 3, 2, 3, FX, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), palarm, 2, 3, 3, 4, FX, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), malarm, 2, 3, 4, 5, FX, 0, 0, 0);
#if 0
	gtk_table_attach (GTK_TABLE (table), mailto, 1, 2, 5, 6, 0, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), mailte, 1, 2, 5, 6, 0, 0, 0, 0);
#endif
	return l;
}

static void
ee_ok (GtkWidget *widget, EventEditor *ee)
{
}

static void
ee_cancel (GtkWidget *widget, EventEditor *ee)
{
}

static GtkWidget *
ee_create_buttons (EventEditor *ee)
{
	GtkWidget *box = gtk_hbox_new (1, 5);
	GtkWidget *ok, *cancel;

	ok     = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
	cancel = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);

	gtk_box_pack_start (GTK_BOX (box), ok, 0, 0, 5);
	gtk_box_pack_start (GTK_BOX (box), cancel, 0, 0, 5);

	gtk_signal_connect (GTK_OBJECT (ok),     "clicked", GTK_SIGNAL_FUNC(ee_ok), ee);
	gtk_signal_connect (GTK_OBJECT (cancel), "clicked", GTK_SIGNAL_FUNC(ee_cancel), ee);
	
	return box;
}

/*
 * Load the contents in a delayed fashion, as the GtkText widget needs it
 */
static void
ee_fill_summary (GtkWidget *widget, EventEditor *ee)
{
	int pos = 0;
	
	gtk_text_freeze (GTK_TEXT (ee->general_summary));
	gtk_editable_insert_text (GTK_EDITABLE (ee->general_summary), ee->ical->summary,
				  strlen (ee->ical->summary), &pos);
	gtk_text_thaw (GTK_TEXT (ee->general_summary));
}

enum {
	OWNER_LINE,
	DESC_LINE,
	SUMMARY_LINE,
	TIME_LINE = 4,
	ALARM_LINE
};

#define LABEL_SPAN 2

static void
event_editor_init_widgets (EventEditor *ee)
{
	GtkWidget *frame, *l;
	
	ee->hbox = gtk_vbox_new (0, 0);
	gtk_container_add (GTK_CONTAINER (ee), ee->hbox);
	gtk_container_border_width (GTK_CONTAINER (ee), 5);
	
	ee->notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (ee->hbox), ee->notebook, 1, 1, 0);
	
	ee->general_table = (GtkTable *) gtk_table_new (1, 1, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (ee->notebook), GTK_WIDGET (ee->general_table),
				  gtk_label_new (_("General")));
	
	l = adjust (gtk_label_new (_("Owner:")), 1.0, 0.5, 1.0, 1.0);
	gtk_table_attach (ee->general_table, l,
			  1, LABEL_SPAN, OWNER_LINE, OWNER_LINE + 1, GTK_FILL|GTK_EXPAND, 0, 0, 6);

	ee->general_owner = gtk_label_new (ee->ical->organizer);
	gtk_table_attach (ee->general_table, ee->general_owner,
			  LABEL_SPAN, LABEL_SPAN + 1, OWNER_LINE, OWNER_LINE + 1, GTK_FILL|GTK_EXPAND, 0, 0, 0);

	l = gtk_label_new (_("Description:"));
	gtk_table_attach (ee->general_table, l,
			  1, LABEL_SPAN, DESC_LINE, DESC_LINE + 1, GTK_FILL|GTK_EXPAND, 0, 0, 0);
	
	ee->general_summary = gtk_text_new (NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (ee->general_summary), "realize",
			    GTK_SIGNAL_FUNC (ee_fill_summary), ee);
	gtk_widget_set_usize (ee->general_summary, 0, 60);
	gtk_text_set_editable (GTK_TEXT (ee->general_summary), 1);
	gtk_table_attach (ee->general_table, ee->general_summary,
			  1, 40, SUMMARY_LINE, SUMMARY_LINE+1, GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 6, 0);

	frame = event_editor_setup_time_frame (ee);
	gtk_table_attach (ee->general_table, frame,
			  1, 40, TIME_LINE + 2, TIME_LINE + 3,
			  GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	l = ee_alarm_widgets (ee);
	gtk_table_attach (ee->general_table, l,
			  1, 40, ALARM_LINE, ALARM_LINE + 1,
			  0, 0, 0, 0);
	
	/* Separator */
	gtk_box_pack_start (GTK_BOX (ee->hbox), gtk_hseparator_new (), 1, 1, 0);

	/* Buttons */
	gtk_box_pack_start (GTK_BOX (ee->hbox), ee_create_buttons (ee), 0, 0, 5);
	
	/* We show all of the contained widgets */
	gtk_widget_show_all (GTK_WIDGET (ee));
	/* And we hide the toplevel, to be consistent with the rest of Gtk */
	gtk_widget_hide (GTK_WIDGET (ee));
}

static void
event_editor_init (EventEditor *ee)
{
	ee->ical = 0;
}

GtkWidget *
event_editor_new (iCalObject *ical)
{
	GtkWidget *retval;
	EventEditor *ee;
	
	retval = gtk_type_new (event_editor_get_type ());
	ee = EVENT_EDITOR (retval);
	
	if (ical == 0){
		ee->new_ical = 1;
		ical = ical_new ("Test Comment", user_name, "Test Summary");
	} else
		ee->new_ical = 0;
	
	ee->ical = ical;
	event_editor_init_widgets (ee);
	
	return retval;
}

/*
 * New event:  Create iCal, edit, check result: Ok: insert;  Cancel: destroy iCal
 * Edit event: fetch  iCal, edit, check result: Ok: remove from calendar, add to calendar; Cancel: nothing
 */
