/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <gnome.h>
#include <cal-util/timeutil.h>
#include <gui/event-editor-utils.h>
#include <gui/event-editor.h>

typedef struct {
	GladeXML *gui;
	GtkWidget *dialog;
	GnomeCalendar *gnome_cal;
	iCalObject *ical;
} EventEditorDialog;


extern int day_begin, day_end;
extern char *user_name;
extern int am_pm_flag;
extern int week_starts_on_monday;


static void append_exception (EventEditorDialog *dialog, time_t t);
static void check_all_day (EventEditorDialog *dialog);
static void alarm_toggle (GtkToggleButton *toggle, EventEditorDialog *dialog);

static void
fill_in_dialog_from_ical (EventEditorDialog *dialog)
{
	iCalObject *ical = dialog->ical;
	GladeXML *gui = dialog->gui;
	GList *list;
	GtkWidget *alarm_display, *alarm_program, *alarm_audio, *alarm_mail;

	store_to_editable (gui, "general-owner",
			   dialog->ical->organizer->addr ?
			   dialog->ical->organizer->addr : _("?"));

	store_to_editable (gui, "general-summary", ical->summary);

	/* start and end time */
	store_to_gnome_dateedit (gui, "start-time", ical->dtstart);
	store_to_gnome_dateedit (gui, "end-time", ical->dtend);

	check_all_day (dialog);

	/* alarms */
	alarm_display = glade_xml_get_widget (dialog->gui, "alarm-display");
	alarm_program = glade_xml_get_widget (dialog->gui, "alarm-program");
	alarm_audio = glade_xml_get_widget (dialog->gui, "alarm-audio");
	alarm_mail = glade_xml_get_widget (dialog->gui, "alarm-mail");

	store_to_toggle (gui, "alarm-display", ical->dalarm.enabled);
	store_to_toggle (gui, "alarm-program", ical->palarm.enabled);
	store_to_toggle (gui, "alarm-audio", ical->aalarm.enabled);
	store_to_toggle (gui, "alarm-mail", ical->malarm.enabled);
	alarm_toggle (GTK_TOGGLE_BUTTON (alarm_display), dialog);
	alarm_toggle (GTK_TOGGLE_BUTTON (alarm_program), dialog);
	alarm_toggle (GTK_TOGGLE_BUTTON (alarm_audio), dialog);
	alarm_toggle (GTK_TOGGLE_BUTTON (alarm_mail), dialog);
	gtk_signal_connect (GTK_OBJECT (alarm_display), "toggled", GTK_SIGNAL_FUNC (alarm_toggle), dialog);
	gtk_signal_connect (GTK_OBJECT (alarm_program), "toggled", GTK_SIGNAL_FUNC (alarm_toggle), dialog);
	gtk_signal_connect (GTK_OBJECT (alarm_audio), "toggled", GTK_SIGNAL_FUNC (alarm_toggle), dialog);
	gtk_signal_connect (GTK_OBJECT (alarm_mail), "toggled", GTK_SIGNAL_FUNC (alarm_toggle), dialog);

	/* alarm counts */
	store_to_spin (gui, "alarm-display-amount", ical->dalarm.count);
	store_to_spin (gui, "alarm-audio-amount", ical->aalarm.count);
	store_to_spin (gui, "alarm-program-amount", ical->palarm.count);
	store_to_spin (gui, "alarm-mail-amount", ical->malarm.count);

	store_to_alarm_unit (gui, "alarm-display-unit", ical->dalarm.units);
	store_to_alarm_unit (gui, "alarm-audio-unit", ical->aalarm.units);
	store_to_alarm_unit (gui, "alarm-program-unit", ical->palarm.units);
	store_to_alarm_unit (gui, "alarm-mail-unit", ical->malarm.units);

	store_to_editable (gui, "run-program", ical->palarm.data);
	store_to_editable (gui, "mail-to", ical->malarm.data);

	/* classification */
	if (strcmp (ical->class, "PUBLIC") == 0)
	    	store_to_toggle (gui, "classification-public", TRUE);
	if (strcmp (ical->class, "PRIVATE") == 0)
	    	store_to_toggle (gui, "classification-private", TRUE);
	if (strcmp (ical->class, "CONFIDENTIAL") == 0)
		store_to_toggle (gui, "classification-confidential", TRUE);

	/* recurrence rules */

	if (! ical->recur)
		return;

	switch (ical->recur->type) {
	case RECUR_DAILY:
		store_to_toggle (gui, "recurrence-rule-daily", TRUE);
		store_to_spin (gui, "recurrence-rule-daily-days", ical->recur->interval);
		break;
	case RECUR_WEEKLY:
		store_to_toggle (gui, "recurrence-rule-weekly", TRUE);
		store_to_spin (gui, "recurrence-rule-weekly-weeks", ical->recur->interval);
		if (ical->recur->weekday & (1 << 0))
			store_to_toggle (gui, "recurrence-rule-weekly-sun", TRUE);
		if (ical->recur->weekday & (1 << 1))
			store_to_toggle (gui, "recurrence-rule-weekly-mon", TRUE);
		if (ical->recur->weekday & (1 << 2))
			store_to_toggle (gui, "recurrence-rule-weekly-tue", TRUE);
		if (ical->recur->weekday & (1 << 3))
			store_to_toggle (gui, "recurrence-rule-weekly-wed", TRUE);
		if (ical->recur->weekday & (1 << 4))
			store_to_toggle (gui, "recurrence-rule-weekly-thu", TRUE);
		if (ical->recur->weekday & (1 << 5))
			store_to_toggle (gui, "recurrence-rule-weekly-fri", TRUE);
		if (ical->recur->weekday & (1 << 6))
			store_to_toggle (gui, "recurrence-rule-weekly-sat", TRUE);
		break;
	case RECUR_MONTHLY_BY_DAY:
		store_to_toggle (gui, "recurrence-rule-monthly", TRUE);
		store_to_toggle (gui, "recurrence-rule-monthly-on-day", TRUE);
		store_to_spin (gui, "recurrence-rule-monthly-day", ical->recur->u.month_day);
		store_to_spin (gui, "recurrence-rule-monthly-every-n-months", ical->recur->interval);
		break;
	case RECUR_MONTHLY_BY_POS:
		store_to_toggle (gui, "recurrence-rule-monthly", TRUE);
		store_to_toggle (gui, "recurrence-rule-monthly-weekday", TRUE);
		store_to_option (gui, "recurrence-rule-monthly-week", ical->recur->u.month_pos);
		store_to_option (gui, "recurrence-rule-monthly-day", ical->recur->weekday);
		store_to_spin (gui, "recurrence-rule-monthly-every-n-months", ical->recur->interval);
		break;
	case RECUR_YEARLY_BY_DAY:
	case RECUR_YEARLY_BY_MONTH:
		store_to_toggle (gui, "recurrence-rule-yearly", TRUE);
		store_to_spin (gui, "recurrence-rule-yearly-every-n-years", ical->recur->interval);
		break;
	}


	if (ical->recur->_enddate == 0) {
		if (ical->recur->duration == 0)
			store_to_toggle (gui, "recurrence-ending-date-repeat-forever", TRUE);
		else {
			store_to_toggle (gui, "recurrence-ending-date-end-after", TRUE);
			store_to_spin (gui, "recurrence-ending-date-end-after-count", ical->recur->duration);
		}
	} else {
		store_to_toggle (gui, "recurrence-ending-date-end-on", TRUE);
		/* Shorten by one day, as we store end-on date a day ahead */
		/* FIX ME is this correct? */
		store_to_gnome_dateedit (gui, "recurrence-ending-date-end-on-date", ical->recur->enddate - 86400);
	}

	/* fill the exceptions list */
	for (list = ical->exdate; list; list = list->next)
		append_exception (dialog, *((time_t *) list->data));
}


static void
fill_in_dialog_from_defaults (EventEditorDialog *dialog)
{
	time_t now = time (NULL);
	time_t soon = time_add_minutes (now, 30);

	store_to_editable (dialog->gui, "general-owner", "?");

	/* start and end time */
	store_to_gnome_dateedit (dialog->gui, "start-time", now);
	store_to_gnome_dateedit (dialog->gui, "end-time", soon);
}


static void
free_exdate (iCalObject *ical)
{
	GList *list;

	if (!ical->exdate)
		return;

	for (list = ical->exdate; list; list = list->next)
		g_free (list->data);

	g_list_free (ical->exdate);
	ical->exdate = NULL;
}


static void
dialog_to_ical (EventEditorDialog *dialog)
{
	iCalObject *ical = dialog->ical;
	gboolean all_day_event;
	GladeXML *gui = dialog->gui;

	/* general event information */

	if (ical->summary)
		g_free (ical->summary);
	ical->summary  = extract_from_editable (gui, "general-summary");

	ical->dtstart = extract_from_gnome_dateedit (gui, "start-time");
	ical->dtend = extract_from_gnome_dateedit (gui, "end-time");

	all_day_event = extract_from_toggle (gui, "all-day-event");

	ical->dalarm.enabled = extract_from_toggle (gui, "alarm-display");
	ical->aalarm.enabled = extract_from_toggle (gui, "alarm-program");
	ical->palarm.enabled = extract_from_toggle (gui, "alarm-audio");
	ical->malarm.enabled = extract_from_toggle (gui, "alarm-mail");

	ical->dalarm.count = extract_from_spin (gui, "alarm-display-amount");
	ical->aalarm.count = extract_from_spin (gui, "alarm-audio-amount");
	ical->palarm.count = extract_from_spin (gui, "alarm-program-amount");
	ical->malarm.count = extract_from_spin (gui, "alarm-mail-amount");

	ical->dalarm.units = extract_from_alarm_unit (gui, "alarm-display-unit");
	ical->aalarm.units = extract_from_alarm_unit (gui, "alarm-audio-unit");
	ical->palarm.units = extract_from_alarm_unit (gui, "alarm-program-unit");
	ical->malarm.units = extract_from_alarm_unit (gui, "alarm-mail-unit");

	ical->palarm.data = g_strdup (extract_from_editable (gui, "run-program"));
	ical->malarm.data = g_strdup (extract_from_editable (gui, "mail-to"));

	if (extract_from_toggle (gui, "classification-public"))
		ical->class = g_strdup ("PUBLIC");
	else if (extract_from_toggle (gui, "classification-private"))
		ical->class = g_strdup ("PRIVATE");
	else /* "classification-confidential" */
		ical->class = g_strdup ("CONFIDENTIAL");


	/* recurrence information */

	if (extract_from_toggle (gui, "recurrence-rule-none"))
		return; /* done */
	if (!ical->recur)
		ical->recur = g_new0 (Recurrence, 1);

	if (extract_from_toggle (gui, "recurrence-rule-daily")) {
		ical->recur->type = RECUR_DAILY;
		ical->recur->interval = extract_from_spin (gui, "recurrence-rule-daily-days");
	}
	if (extract_from_toggle (gui, "recurrence-rule-weekly")) {
		ical->recur->type = RECUR_WEEKLY;
		ical->recur->interval = extract_from_spin (gui, "recurrence-rule-weekly-weeks");
		ical->recur->weekday = 0;

		if (extract_from_toggle (gui, "recurrence-rule-weekly-sun"))
			ical->recur->weekday |= 1 << 0;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-mon"))
			ical->recur->weekday |= 1 << 1;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-tue"))
			ical->recur->weekday |= 1 << 2;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-wed"))
			ical->recur->weekday |= 1 << 3;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-thu"))
			ical->recur->weekday |= 1 << 4;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-fri"))
			ical->recur->weekday |= 1 << 5;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-sat"))
			ical->recur->weekday |= 1 << 6;
	}
	if (extract_from_toggle (gui, "recurrence-rule-monthly")) {
		if (extract_from_toggle (gui, "recurrence-rule-monthly-on-day")) {
			/* by day of in the month (ex: the 5th) */
			ical->recur->type = RECUR_MONTHLY_BY_DAY;
			ical->recur->u.month_day = extract_from_spin (gui, "recurrence-rule-monthly-day");

		}
		else {  /* "recurrence-rule-monthly-weekday" is TRUE */ 
			/* by position on the calender (ex: 2nd monday) */
			ical->recur->type = RECUR_MONTHLY_BY_POS;
			ical->recur->u.month_pos = extract_from_option (gui, "recurrence-rule-monthly-week");
			ical->recur->weekday = extract_from_option (gui, "recurrence-rule-monthly-day");
		}
		ical->recur->interval = extract_from_spin (gui, "recurrence-rule-monthly-every-n-months");
	}
	if (extract_from_toggle (gui, "recurrence-rule-yearly")) {
		ical->recur->type = RECUR_YEARLY_BY_DAY;
		ical->recur->interval = extract_from_spin (gui, "recurrence-rule-yearly-every-n-years");
		/* FIXME: need to specify anything else?  I am assuming the code will look at the dtstart
		 * to figure out when to recur. - Federico
		 */
	}

	/* recurrence ending date */
	if (extract_from_toggle (gui, "recurrence-ending-date-repeat-forever")) {
		ical->recur->_enddate = 0;
		ical->recur->enddate = 0;
		ical->recur->duration = 0;
	}
	if (extract_from_toggle (gui, "recurrence-ending-date-end-on")) {
		/* Also here, to ensure that the event is used, we add 86400 secs to get 
		   get next day, in accordance to the RFC */
		ical->recur->_enddate = extract_from_gnome_dateedit (gui, "recurrence-ending-date-end-on-date") + 86400;
		ical->recur->enddate = ical->recur->_enddate;
		ical->recur->duration = 0;
	}
	if (extract_from_toggle (gui, "recurrence-ending-date-end-after")) {
		ical->recur->duration = extract_from_spin (gui, "recurrence-ending-date-end-after-count");
		ical_object_compute_end (ical);
	}


	/* get exceptions from clist into ical->exdate */
	{
		int i;
		time_t *t;
		GtkCList *exception_list = GTK_CLIST (glade_xml_get_widget (dialog->gui, "recurrence-exceptions-list"));

		free_exdate (ical);

		for (i = 0; i < exception_list->rows; i++) {
			t = gtk_clist_get_row_data (exception_list, i);
			ical->exdate = g_list_prepend (ical->exdate, t);
		}
	}
}


static void
ee_ok (GtkWidget *widget, EventEditorDialog *dialog)
{
	dialog_to_ical (dialog);

	if (dialog->ical->new)
		gnome_calendar_add_object (dialog->gnome_cal, dialog->ical);
	else
		gnome_calendar_object_changed (dialog->gnome_cal,
					       dialog->ical,
					       CHANGE_ALL);
	dialog->ical->new = 0;
}


static void
ee_cancel (GtkWidget *widget, EventEditorDialog *dialog)
{
	if (dialog->ical) {
		ical_object_unref (dialog->ical);
		dialog->ical = NULL;
	}
}


static void
alarm_toggle (GtkToggleButton *toggle, EventEditorDialog *dialog)
{
	GtkWidget *alarm_display = glade_xml_get_widget (dialog->gui, "alarm-display");
	GtkWidget *alarm_program = glade_xml_get_widget (dialog->gui, "alarm-program");
	GtkWidget *alarm_audio = glade_xml_get_widget (dialog->gui, "alarm-audio");
	GtkWidget *alarm_mail = glade_xml_get_widget (dialog->gui, "alarm-mail");
	GtkWidget *alarm_amount, *alarm_unit;

	if (GTK_WIDGET (toggle) == alarm_display) {
		alarm_amount = glade_xml_get_widget (dialog->gui, "alarm-display-amount");
		alarm_unit = glade_xml_get_widget (dialog->gui, "alarm-display-unit");
	}
	if (GTK_WIDGET (toggle) == alarm_audio) {
		alarm_amount = glade_xml_get_widget (dialog->gui, "alarm-audio-amount");
		alarm_unit = glade_xml_get_widget (dialog->gui, "alarm-audio-unit");
	}
	if (GTK_WIDGET (toggle) == alarm_program) {
		GtkWidget *run_program;
		alarm_amount = glade_xml_get_widget (dialog->gui, "alarm-program-amount");
		alarm_unit = glade_xml_get_widget (dialog->gui, "alarm-program-unit");
		run_program = glade_xml_get_widget (dialog->gui, "run-program-file-entry");
		gtk_widget_set_sensitive (run_program, toggle->active);
	}
	if (GTK_WIDGET (toggle) == alarm_mail) {
		GtkWidget *mail_to;
		alarm_amount = glade_xml_get_widget (dialog->gui, "alarm-mail-amount");
		alarm_unit = glade_xml_get_widget (dialog->gui, "alarm-mail-unit");
		mail_to = glade_xml_get_widget (dialog->gui, "mail-to");
		gtk_widget_set_sensitive (mail_to, toggle->active);
	}

	gtk_widget_set_sensitive (alarm_amount, toggle->active);
	gtk_widget_set_sensitive (alarm_unit, toggle->active);
}



/*
 * Checks if the day range occupies all the day, and if so, check the
 * box accordingly
 */
static void
check_all_day (EventEditorDialog *dialog)
{
	time_t ev_start = extract_from_gnome_dateedit (dialog->gui, "start-time");
	time_t ev_end = extract_from_gnome_dateedit (dialog->gui, "end-time");

	/* all day event checkbox */
	if (get_time_t_hour (ev_start) <= day_begin &&
	    get_time_t_hour (ev_end) >= day_end)
		store_to_toggle (dialog->gui, "all-day-event", TRUE);
	else
		store_to_toggle (dialog->gui, "all-day-event", FALSE);
}


/*
 * Callback: all day event box clicked
 */
static void
set_all_day (GtkToggleButton *toggle, EventEditorDialog *dialog)
{
	struct tm tm;
	time_t start_t;

	start_t = extract_from_gnome_dateedit (dialog->gui, "start-time");
	tm = *localtime (&start_t);
	tm.tm_hour = day_begin;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	store_to_gnome_dateedit (dialog->gui, "start-time", mktime (&tm));
	
	if (toggle->active)
		tm.tm_hour = day_end;
	else
		tm.tm_hour++;
	
	store_to_gnome_dateedit (dialog->gui, "end-time", mktime (&tm));
}


/*
 * Callback: checks that the dates are start < end
 */
static void
check_dates (GnomeDateEdit *gde, EventEditorDialog *dialog)
{
	time_t start, end;
	struct tm tm_start, tm_end;
	GtkWidget *start_time = glade_xml_get_widget (dialog->gui, "start-time");
	GtkWidget *end_time = glade_xml_get_widget (dialog->gui, "end-time");


	//start = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->start_time));
	start = extract_from_gnome_dateedit (dialog->gui, "start-time");
	//end = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->end_time));
	end = extract_from_gnome_dateedit (dialog->gui, "end-time");

	if (start > end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (gde) == start_time) {
			tm_end.tm_year = tm_start.tm_year;
			tm_end.tm_mon  = tm_start.tm_mon;
			tm_end.tm_mday = tm_start.tm_mday;

			gnome_date_edit_set_time (GNOME_DATE_EDIT (end_time), mktime (&tm_end));
		} else if (GTK_WIDGET (gde) == end_time) {
			tm_start.tm_year = tm_end.tm_year;
			tm_start.tm_mon  = tm_end.tm_mon;
			tm_start.tm_mday = tm_end.tm_mday;

			//gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->start_time), mktime (&tm_start));
		}
	}
}


/*
 * Callback: checks that start_time < end_time and whether the
 * selected hour range spans all of the day
 */
static void
check_times (GnomeDateEdit *gde, EventEditorDialog *dialog)
{
	time_t start, end;
	struct tm tm_start, tm_end;
	GtkWidget *start_time = glade_xml_get_widget (dialog->gui, "start-time");
	GtkWidget *end_time = glade_xml_get_widget (dialog->gui, "end-time");


	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_flush ();

	start = gnome_date_edit_get_date (GNOME_DATE_EDIT (start_time));
	end = gnome_date_edit_get_date (GNOME_DATE_EDIT (end_time));

	if (start >= end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (gde) == start_time) {
			tm_end.tm_min  = tm_start.tm_min;
			tm_end.tm_sec  = tm_start.tm_sec;

			tm_end.tm_hour = tm_start.tm_hour + 1;

			if (tm_end.tm_hour >= 24) {
				tm_end.tm_hour = 24; /* mktime() will bump the day */
				tm_end.tm_min = 0;
				tm_end.tm_sec = 0;
			}

			gnome_date_edit_set_time (GNOME_DATE_EDIT (end_time), mktime (&tm_end));
		} else if (GTK_WIDGET (gde) == end_time) {
			tm_start.tm_min  = tm_end.tm_min;
			tm_start.tm_sec  = tm_end.tm_sec;

			tm_start.tm_hour = tm_end.tm_hour - 1;

			if (tm_start.tm_hour < 0) {
				tm_start.tm_hour = 0;
				tm_start.tm_min = 0;
				tm_start.tm_min = 0;
			}

			gnome_date_edit_set_time (GNOME_DATE_EDIT (start_time), mktime (&tm_start));
		}
	}

	/* Check whether the event spans the whole day */

	check_all_day (dialog);
}


static void
recurrence_toggled (GtkWidget *radio, EventEditorDialog *dialog)
{
	GtkWidget *recurrence_rule_notebook = glade_xml_get_widget (dialog->gui, "recurrence-rule-notebook");

	GtkWidget *recurrence_rule_none = glade_xml_get_widget (dialog->gui, "recurrence-rule-none");
	GtkWidget *recurrence_rule_daily = glade_xml_get_widget (dialog->gui, "recurrence-rule-daily");
	GtkWidget *recurrence_rule_weekly = glade_xml_get_widget (dialog->gui, "recurrence-rule-weekly");
	GtkWidget *recurrence_rule_monthly = glade_xml_get_widget (dialog->gui, "recurrence-rule-monthly");
	GtkWidget *recurrence_rule_yearly = glade_xml_get_widget (dialog->gui, "recurrence-rule-yearly");

	if (radio == recurrence_rule_none)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 0);
	if (radio == recurrence_rule_daily)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 1);
	if (radio == recurrence_rule_weekly)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 2);
	if (radio == recurrence_rule_monthly)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 3);
	if (radio == recurrence_rule_yearly)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 4);
}


static char *
get_exception_string (time_t t)
{
	static char buf[256];

	strftime (buf, sizeof(buf), _("%a %b %d %Y"), localtime (&t));
	return buf;
}


static void
append_exception (EventEditorDialog *dialog, time_t t)
{
	time_t *tt;
	char   *c[1];
	int     i;
	GtkCList *exception_list = GTK_CLIST (glade_xml_get_widget (dialog->gui, "recurrence-exceptions-list"));

	c[0] = get_exception_string (t);

	tt = g_new (time_t, 1);
	*tt = t;

	i = gtk_clist_append (exception_list, c);
	gtk_clist_set_row_data (exception_list, i, tt);
	gtk_clist_select_row (exception_list, i, 0);

	//gtk_widget_set_sensitive (ee->recur_ex_vbox, TRUE);
}


static void
recurrence_exception_added (GtkWidget *widget, EventEditorDialog *dialog)
{
	//GtkWidget *exception_date = glade_xml_get_widget (dialog->gui, "recurrence-exceptions-date");
	time_t t = extract_from_gnome_dateedit (dialog->gui, "recurrence-exceptions-date");
	append_exception (dialog, t);
}


static void
recurrence_exception_deleted (GtkWidget *widget, EventEditorDialog *dialog)
{
	GtkCList *clist;
	int sel, length;

	clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, "recurrence-exceptions-list"));
	if (! clist->selection)
		return;
	sel = GPOINTER_TO_INT(clist->selection->data);

	g_free (gtk_clist_get_row_data (clist, sel)); /* free the time_t stored there */

	gtk_clist_remove (clist, sel);
	length = g_list_length(clist->row_list);
	if (sel >= length)
		sel--;
	gtk_clist_select_row (clist, sel, 0);

	//if (clist->rows == 0)
	//	gtk_widget_set_sensitive (ee->recur_ex_vbox, FALSE);
}


static void
recurrence_exception_changed (GtkWidget *widget, EventEditorDialog *dialog)
{
	GtkCList *clist;
	time_t *t;
	int sel;

	clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, "recurrence-exceptions-list"));
	if (! clist->selection)
		return;

	sel = GPOINTER_TO_INT(clist->selection->data);

	t = gtk_clist_get_row_data (clist, sel);
	*t = extract_from_gnome_dateedit (dialog->gui, "recurrence-exceptions-date");

	gtk_clist_set_text (clist, sel, 0, get_exception_string (*t));
}



GtkWidget *event_editor_new (GnomeCalendar *gcal, iCalObject *ical)
{
	EventEditorDialog *dialog = g_new0 (EventEditorDialog, 1);

	dialog->ical = ical;
	dialog->gnome_cal = gcal;

	printf ("glade_xml_new (%s, NULL)\n",
		EVOLUTION_GLADEDIR "/event-editor-dialog.glade");

	dialog->gui = glade_xml_new (EVOLUTION_GLADEDIR
				     "/event-editor-dialog.glade",
				     NULL);

	dialog->dialog = glade_xml_get_widget (dialog->gui,
					       "event-editor-dialog");

	gnome_dialog_button_connect (GNOME_DIALOG (dialog->dialog),
				     0, GTK_SIGNAL_FUNC(ee_ok), dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog->dialog),
				     1, GTK_SIGNAL_FUNC(ee_cancel), dialog);


	{
		GtkWidget *start_time = glade_xml_get_widget (dialog->gui, "start-time");
		GtkWidget *end_time = glade_xml_get_widget (dialog->gui, "end-time");

		gtk_signal_connect (GTK_OBJECT (start_time), "date_changed",
				    GTK_SIGNAL_FUNC (check_dates), dialog);
		gtk_signal_connect (GTK_OBJECT (start_time), "time_changed",
				    GTK_SIGNAL_FUNC (check_times), dialog);

		gtk_signal_connect (GTK_OBJECT (end_time), "date_changed",
				    GTK_SIGNAL_FUNC (check_dates), dialog);
		gtk_signal_connect (GTK_OBJECT (end_time), "time_changed",
				    GTK_SIGNAL_FUNC (check_times), dialog);
	}


	{
		GtkWidget *all_day_checkbox = glade_xml_get_widget (dialog->gui, "all-day-event");
		gtk_signal_connect (GTK_OBJECT (all_day_checkbox), "toggled", GTK_SIGNAL_FUNC (set_all_day), dialog);
	}


	{
		GtkWidget *recurrence_rule_none = glade_xml_get_widget (dialog->gui, "recurrence-rule-none");
		GtkWidget *recurrence_rule_daily = glade_xml_get_widget (dialog->gui, "recurrence-rule-daily");
		GtkWidget *recurrence_rule_weekly = glade_xml_get_widget (dialog->gui, "recurrence-rule-weekly");
		GtkWidget *recurrence_rule_monthly = glade_xml_get_widget (dialog->gui, "recurrence-rule-monthly");
		GtkWidget *recurrence_rule_yearly = glade_xml_get_widget (dialog->gui, "recurrence-rule-yearly");

		gtk_signal_connect (GTK_OBJECT (recurrence_rule_none), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_rule_daily), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_rule_weekly), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_rule_monthly), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_rule_yearly), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		
	}


	{
		GtkWidget *recurrence_exception_add = glade_xml_get_widget (dialog->gui, "recurrence-exceptions-add");
		GtkWidget *recurrence_exception_delete = glade_xml_get_widget (dialog->gui, "recurrence-exceptions-delete");
		GtkWidget *recurrence_exception_change = glade_xml_get_widget (dialog->gui, "recurrence-exceptions-change");

		gtk_signal_connect (GTK_OBJECT (recurrence_exception_add), "clicked",
				    GTK_SIGNAL_FUNC (recurrence_exception_added), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_exception_delete), "clicked",
				    GTK_SIGNAL_FUNC (recurrence_exception_deleted), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_exception_change), "clicked",
				    GTK_SIGNAL_FUNC (recurrence_exception_changed), dialog);
	}



	if (ical->new)
		fill_in_dialog_from_defaults (dialog);
	else
		fill_in_dialog_from_ical (dialog);
	

	gnome_dialog_run (GNOME_DIALOG(dialog->dialog));

	gnome_dialog_close (GNOME_DIALOG(dialog->dialog));

	return dialog->dialog;
}


void event_editor_new_whole_day (GnomeCalendar *owner, time_t day)
{
	struct tm tm;
	iCalObject *ico;
	GtkWidget *ee;

	g_return_if_fail (owner != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (owner));

	ico = ical_new ("", user_name, "");
	ico->new = TRUE;

	tm = *localtime (&day);

	/* Set the start time of the event to the beginning of the day */

	tm.tm_hour = day_begin;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	ico->dtstart = mktime (&tm);

	/* Set the end time of the event to the end of the day */

	tm.tm_hour = day_end;
	ico->dtend = mktime (&tm);

	/* Launch the event editor */

	ee = event_editor_new (owner, ico);
}



GtkWidget *make_date_edit (void)
{
	return date_edit_new (time (NULL), FALSE);
}


GtkWidget *make_date_edit_with_time (void)
{
	return date_edit_new (time (NULL), TRUE);
}

 
GtkWidget *date_edit_new (time_t the_time, int show_time)
{
	return gnome_date_edit_new_flags (the_time,
				  ((show_time ? GNOME_DATE_EDIT_SHOW_TIME : 0)
				   | (am_pm_flag ? 0 : GNOME_DATE_EDIT_24_HR)
				   | (week_starts_on_monday
				      ? GNOME_DATE_EDIT_WEEK_STARTS_ON_MONDAY
				      : 0)));
}



GtkWidget *
make_spin_button (int val, int low, int high)
{
	GtkAdjustment *adj;
	GtkWidget *spin;

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (val, low, high, 1, 10, 10));
	spin = gtk_spin_button_new (adj, 0.5, 0);
	gtk_widget_set_usize (spin, 60, 0);

	return spin;
}


/* todo

   build some of the recur stuff by hand to take into account
   the start-on-monday preference?

   get the apply button to work right

   make the properties stuff unglobal

   figure out why alarm units aren't sticking between edits

   closing the dialog window with the wm caused a crash
   Gtk-WARNING **: invalid cast from `(unknown)' to `GnomeDialog'
   on line 669:  gnome_dialog_close (GNOME_DIALOG(dialog->dialog));
 */

