/*
 * EventEditor widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors: Miguel de Icaza (miguel@kernel.org)
 *          Federico Mena (quartic@gimp.org)
 */
#include <config.h>
#include <gnome.h>
#include <string.h>
#include "calendar.h"
#include "eventedit.h"
#include "main.h"
#include "timeutil.h"


static void event_editor_class_init (EventEditorClass *class);
static void event_editor_init       (EventEditor      *ee);
static void event_editor_destroy    (GtkObject        *object);

/* Note: do not i18n these strings, they are part of the vCalendar protocol */
static char *class_names [] = { "PUBLIC", "PRIVATE", "CONFIDENTIAL" };

static GnomeDialogClass *parent_class;

struct numbered_item {
	char *text;
	int num;
};


guint
event_editor_get_type (void)
{
	static guint event_editor_type = 0;
	
	if(!event_editor_type) {
		GtkTypeInfo event_editor_info = {
			"EventEditor",
			sizeof(EventEditor),
			sizeof(EventEditorClass),
			(GtkClassInitFunc) event_editor_class_init,
			(GtkObjectInitFunc) event_editor_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		event_editor_type = gtk_type_unique (gnome_dialog_get_type (), &event_editor_info);
	}
	return event_editor_type;
}

static void
event_editor_class_init (EventEditorClass *class)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (gnome_dialog_get_type ());
	object_class = (GtkObjectClass*) class;
	object_class->destroy = event_editor_destroy;
}

GtkWidget *
adjust (GtkWidget *w, gfloat x, gfloat y, gfloat xs, gfloat ys)
{
	GtkWidget *a = gtk_alignment_new (x, y, xs, ys);
	
	gtk_container_add (GTK_CONTAINER (a), w);
	return a;
}

static GtkWidget *
make_spin_button (int val, int low, int high)
{
	GtkAdjustment *adj;
	GtkWidget *spin;

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (val, low, high, 1, 10, 10));
	spin = gtk_spin_button_new (adj, 0.5, 0);
	gtk_widget_set_usize (spin, 60, 0);

	return spin;
}

/*
 * Checks if the day range occupies all the day, and if so, check the
 * box accordingly
 */
static void
ee_check_all_day (EventEditor *ee)
{
	time_t ev_start, ev_end;

	ev_start = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->start_time));
	ev_end   = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->end_time)); 
	
	if (get_time_t_hour (ev_start) <= day_begin && get_time_t_hour (ev_end) >= day_end)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (ee->general_allday), 1);
	else
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (ee->general_allday), 0);
}

/*
 * Callback: checks that the dates are start < end
 */
static void
check_dates (GnomeDateEdit *gde, EventEditor *ee)
{
	time_t start, end;
	struct tm tm_start, tm_end;

	start = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->start_time));
	end = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->end_time));

	if (start > end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (gde) == ee->start_time) {
			tm_end.tm_year = tm_start.tm_year;
			tm_end.tm_mon  = tm_start.tm_mon;
			tm_end.tm_mday = tm_start.tm_mday;

			gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->end_time), mktime (&tm_end));
		} else if (GTK_WIDGET (gde) == ee->end_time) {
			tm_start.tm_year = tm_end.tm_year;
			tm_start.tm_mon  = tm_end.tm_mon;
			tm_start.tm_mday = tm_end.tm_mday;

			gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->start_time), mktime (&tm_start));
		}
	}
}

/*
 * Callback: checks that start_time < end_time and whether the
 * selected hour range spans all of the day
 */
static void
check_times (GnomeDateEdit *gde, EventEditor *ee)
{
	time_t start, end;
	struct tm tm_start, tm_end;

	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_flush ();

	start = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->start_time));
	end = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->end_time));

	if (start >= end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (gde) == ee->start_time) {
			tm_end.tm_min  = tm_start.tm_min;
			tm_end.tm_sec  = tm_start.tm_sec;

			tm_end.tm_hour = tm_start.tm_hour + 1;

			if (tm_end.tm_hour >= 24) {
				tm_end.tm_hour = 24; /* mktime() will bump the day */
				tm_end.tm_min = 0;
				tm_end.tm_sec = 0;
			}

			gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->end_time), mktime (&tm_end));
		} else if (GTK_WIDGET (gde) == ee->end_time) {
			tm_start.tm_min  = tm_end.tm_min;
			tm_start.tm_sec  = tm_end.tm_sec;

			tm_start.tm_hour = tm_end.tm_hour - 1;

			if (tm_start.tm_hour < 0) {
				tm_start.tm_hour = 0;
				tm_start.tm_min = 0;
				tm_start.tm_min = 0;
			}

			gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->start_time), mktime (&tm_start));
		}
	}

	/* Check whether the event spans the whole day */

	ee_check_all_day (ee);
}

/*
 * Callback: all day event box clicked
 */
static void
set_all_day (GtkToggleButton *toggle, EventEditor *ee)
{
	struct tm *tm;
	time_t start_t;

	start_t = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->start_time));
	tm = localtime (&start_t);
	tm->tm_hour = day_begin;
	tm->tm_min  = 0;
	tm->tm_sec  = 0;
	gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->start_time), mktime (tm));
	
	if (toggle->active)
		tm->tm_hour = day_end;
	else
		tm->tm_hour++;
	
	gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->end_time), mktime (tm));
}

static GtkWidget *
event_editor_setup_time_frame (EventEditor *ee)
{
	GtkWidget *frame;
	GtkWidget *start_time, *end_time;
	GtkTable  *t;
	
	frame = gtk_frame_new (_("Time"));
	t = GTK_TABLE (ee->general_time_table = gtk_table_new (1, 1, 0));
	gtk_container_border_width (GTK_CONTAINER (t), 4);
	gtk_table_set_row_spacings (t, 4);
	gtk_table_set_col_spacings (t, 4);
	gtk_container_add (GTK_CONTAINER (frame), ee->general_time_table);

	/* 1. Start time */
	if (ee->ical->dtstart == 0){
		ee->ical->dtstart = time (NULL);
		ee->ical->dtend   = time_add_minutes (ee->ical->dtstart, 30);
	}
	ee->start_time = start_time = gnome_date_edit_new (ee->ical->dtstart, TRUE);
	gnome_date_edit_set_popup_range ((GnomeDateEdit *) start_time, day_begin, day_end);
	gtk_signal_connect (GTK_OBJECT (start_time), "date_changed",
			    GTK_SIGNAL_FUNC (check_dates), ee);
	gtk_signal_connect (GTK_OBJECT (start_time), "time_changed",
			    GTK_SIGNAL_FUNC (check_times), ee);
	gtk_table_attach (t, gtk_label_new (_("Start time:")), 1, 2, 1, 2,
			  GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_table_attach (t, start_time, 2, 3, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);

	/* 2. End time */
	ee->end_time   = end_time   = gnome_date_edit_new (ee->ical->dtend, TRUE);
	gnome_date_edit_set_popup_range ((GnomeDateEdit *) end_time,   day_begin, day_end);
	gtk_signal_connect (GTK_OBJECT (end_time), "date_changed",
			    GTK_SIGNAL_FUNC (check_dates), ee);
	gtk_signal_connect (GTK_OBJECT (end_time), "time_changed",
			    GTK_SIGNAL_FUNC (check_times), ee);
	gtk_table_attach (t, gtk_label_new (_("End time:")), 1, 2, 2, 3,
			  GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_table_attach (t, end_time, 2, 3, 2, 3,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);

	/* 3. All day checkbox */
	ee->general_allday = gtk_check_button_new_with_label (_("All day event"));
	gtk_signal_connect (GTK_OBJECT (ee->general_allday), "toggled",
			    GTK_SIGNAL_FUNC (set_all_day), ee);
	gtk_table_attach (t, ee->general_allday, 3, 4, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  4, 0);
	ee_check_all_day (ee);

	return frame;
}

static GtkWidget *
timesel_new (void)
{
	GtkWidget *menu, *option_menu;
	char *items [] = { N_("Minutes"), N_("Hours"), N_("Days") };
	int i;
	
	option_menu = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	for (i = 0; i < 3; i++){
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (_(items [i]));
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
	return option_menu;
}

/*
 * Set the sensitive state depending on whether the alarm enabled flag.
 */
static void
ee_alarm_setting (CalendarAlarm *alarm, int sensitive)
{
	gtk_widget_set_sensitive (GTK_WIDGET (alarm->w_count), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (alarm->w_timesel), sensitive);

	if (alarm->type == ALARM_PROGRAM || alarm->type == ALARM_MAIL){
		gtk_widget_set_sensitive (GTK_WIDGET (alarm->w_entry), sensitive);
		gtk_widget_set_sensitive (GTK_WIDGET (alarm->w_label), sensitive);
	}
}

static void
alarm_toggle (GtkToggleButton *toggle, CalendarAlarm *alarm)
{
	ee_alarm_setting (alarm, toggle->active);
}

#define FXS (GTK_FILL | GTK_EXPAND | GTK_SHRINK)
#define FS  (GTK_FILL | GTK_SHRINK)

static void
ee_create_ae (GtkTable *table, char *str, CalendarAlarm *alarm, enum AlarmType type, int y)
{
	GtkWidget *entry;

	alarm->w_enabled = gtk_check_button_new_with_label (str);
	gtk_signal_connect (GTK_OBJECT (alarm->w_enabled), "toggled",
			    GTK_SIGNAL_FUNC (alarm_toggle), alarm);
	gtk_table_attach (table, alarm->w_enabled, 0, 1, y, y+1, FS, FS, 0, 0);

	alarm->w_count = make_spin_button (alarm->count, 0, 10000);
	gtk_table_attach (table, alarm->w_count, 1, 2, y, y+1, FS, FS, 0, 0);
	
	alarm->w_timesel = timesel_new ();
	gtk_option_menu_set_history (GTK_OPTION_MENU (alarm->w_timesel), alarm->units);
	gtk_table_attach (table, alarm->w_timesel, 2, 3, y, y+1, FS, FS, 0, 0);
	
	switch (type){
	case ALARM_MAIL:
		alarm->w_label = gtk_label_new (_("Mail to:"));
		gtk_misc_set_alignment (GTK_MISC (alarm->w_label), 1.0, 0.5);	
		gtk_table_attach (table, alarm->w_label, 3, 4, y, y+1, FS, FS, 0, 0);
		alarm->w_entry = gtk_entry_new ();
		gtk_table_attach (table, alarm->w_entry, 4, 5, y, y+1, FXS, FS, 0, 0);
		gtk_entry_set_text (GTK_ENTRY (alarm->w_entry), alarm->data ? alarm->data : "");
		break;

	case ALARM_PROGRAM:
		alarm->w_label = gtk_label_new (_("Run program:"));
		gtk_misc_set_alignment (GTK_MISC (alarm->w_label), 1.0, 0.5);	
		gtk_table_attach (table, alarm->w_label, 3, 4, y, y+1, FS, FS, 0, 0);
		alarm->w_entry = gnome_file_entry_new ("alarm-program", _("Select program to run at alarm time"));
		entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (alarm->w_entry));
		gtk_entry_set_text (GTK_ENTRY (entry), alarm->data ? alarm->data : "");
		gtk_table_attach (table, alarm->w_entry, 4, 5, y, y+1, FXS, FS, 0, 0);
		break;

	default:
		break;
	}
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (alarm->w_enabled), alarm->enabled);
	ee_alarm_setting (alarm, alarm->enabled);
}

static GtkWidget *
ee_alarm_widgets (EventEditor *ee)
{
	GtkWidget *table, *mailto, *mailte, *l;
	
	l = gtk_frame_new (_("Alarms"));
	
	table = gtk_table_new (1, 1, 0);
	gtk_container_border_width (GTK_CONTAINER (table), 4);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
	gtk_container_add (GTK_CONTAINER (l), table);
	
	mailto  = gtk_label_new (_("Mail to:"));
	mailte  = gtk_entry_new ();

	ee_create_ae (GTK_TABLE (table), _("Display"), &ee->ical->dalarm, ALARM_DISPLAY, 1);
	ee_create_ae (GTK_TABLE (table), _("Audio"),   &ee->ical->aalarm, ALARM_AUDIO, 2);
	ee_create_ae (GTK_TABLE (table), _("Program"), &ee->ical->palarm, ALARM_PROGRAM, 3);
	ee_create_ae (GTK_TABLE (table), _("Mail"),    &ee->ical->malarm, ALARM_MAIL, 4);
	
	return l;
}

static GtkWidget *
ee_classification_widgets (EventEditor *ee)
{
	GtkWidget *rpub, *rpriv, *rconf;
	GtkWidget *frame, *hbox;

	frame = gtk_frame_new (_("Classification"));

	hbox  = gtk_hbox_new (TRUE, 0);
	gtk_container_border_width (GTK_CONTAINER (hbox), 4);
	gtk_container_add (GTK_CONTAINER (frame), hbox);
	
	rpub  = gtk_radio_button_new_with_label (NULL, _("Public"));
	rpriv = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rpub), _("Private"));
	rconf  = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rpub), _("Confidential"));

	gtk_box_pack_start (GTK_BOX (hbox), rpub, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), rpriv, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), rconf, FALSE, FALSE, 0);

	if (strcmp (ee->ical->class, class_names[0]))
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (rpub), TRUE);
	else if (strcmp (ee->ical->class, class_names[1]))
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (rpriv), TRUE);
	else if (strcmp (ee->ical->class, class_names[2]))
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (rconf), TRUE);

	ee->general_radios = rpub;

	return frame;
}

/*
 * Retrieves the information from the CalendarAlarm widgets and stores them
 * on the CalendarAlarm generic values
 */
void
ee_store_alarm (CalendarAlarm *alarm, enum AlarmType type)
{
	GtkWidget *item;
	GtkMenu   *menu;
	GList     *child;
	int idx;
	
	if (alarm->data){
		g_free (alarm->data);
		alarm->data = 0;
	}
	
	alarm->enabled = GTK_TOGGLE_BUTTON (alarm->w_enabled)->active;

	if (!alarm->enabled)
		return;
	
	if (type == ALARM_PROGRAM)
		alarm->data = g_strdup (gtk_entry_get_text (GTK_ENTRY (gnome_file_entry_gtk_entry (alarm->w_entry))));
	if (type == ALARM_MAIL)
		alarm->data = g_strdup (gtk_entry_get_text (GTK_ENTRY (alarm->w_entry)));

	/* Find out the index */
	menu = GTK_MENU (GTK_OPTION_MENU (alarm->w_timesel)->menu);
	
	item = gtk_menu_get_active (menu);
	
	for (idx = 0, child = GTK_MENU_SHELL (menu)->children; child->data != item; child = child->next)
		idx++;
	
	alarm->units = idx;
	alarm->count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (alarm->w_count));
}

static void
ee_store_general_values_to_ical (EventEditor *ee)
{
	GtkRadioButton *radio = GTK_RADIO_BUTTON (ee->general_radios);
	iCalObject *ical = ee->ical;
	GSList *list = radio->group;
	int idx;
	time_t now;

	now = time (NULL);
	ical->dtstart = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->start_time));
	ical->dtend   = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->end_time));

	if (ical->summary)
		g_free (ical->summary);

	ical->summary = gtk_editable_get_chars (GTK_EDITABLE (ee->general_summary), 0, -1);

	ee_store_alarm (&ical->dalarm, ALARM_DISPLAY);
	ee_store_alarm (&ical->aalarm, ALARM_AUDIO);
	ee_store_alarm (&ical->palarm, ALARM_PROGRAM);
	ee_store_alarm (&ical->malarm, ALARM_MAIL);

	for (idx = 0; list; list = list->next) {
		if (GTK_TOGGLE_BUTTON (list->data)->active)
			break;
		idx++;
	}
	g_free (ical->class);
	ical->class = g_strdup (class_names [idx]);
}

static int
option_menu_active_number (GtkWidget *omenu)
{
	GtkWidget *menu;
	GtkWidget *item;
	struct numbered_item *ni;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (omenu));
	item = gtk_menu_get_active (GTK_MENU (menu));

	ni = gtk_object_get_user_data (GTK_OBJECT (item));

	return ni->num;
}

static int
ee_store_recur_rule_to_ical (EventEditor *ee)
{
	iCalObject *ical;
	int i, j;
	GSList *list;

	ical = ee->ical;

	for (i = 0, list = ee->recur_rr_group; list; i++, list = list->next)
		if (GTK_TOGGLE_BUTTON (list->data)->active)
			break;

	i = g_slist_length (ee->recur_rr_group) - i - 1; /* buttons are stored in reverse order of insertion */

	/* None selected, no rule to be stored */
	if (i == 0)
		return 0;

	if (!ical->recur)
		ical->recur = g_new0 (Recurrence, 1);
	
	switch (i) {
	case 1:
		/* Daily */
		ical->recur->type = RECUR_DAILY;
		ical->recur->interval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ee->recur_rr_day_period));
		break;

	case 2:
		/* Weekly */
		ical->recur->type = RECUR_WEEKLY;
		ical->recur->interval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ee->recur_rr_week_period));
		ical->recur->weekday = 0;

		for (j = 0; j < 7; j++)
			if (GTK_TOGGLE_BUTTON (ee->recur_rr_week_days[j])->active) {
				if (j == 6)
					ical->recur->weekday |= 1 << 0; /* sunday is at bit 0 */
				else
					ical->recur->weekday |= 1 << (j + 1);
			}

		break;

	case 3:
		/* Monthly */

		if (GTK_WIDGET_SENSITIVE (ee->recur_rr_month_date)) {
			/* by day */

			ical->recur->type = RECUR_MONTHLY_BY_DAY;
			ical->recur->u.month_day =
				gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ee->recur_rr_month_date));
		} else {
			/* by position */

			ical->recur->type = RECUR_MONTHLY_BY_POS;

			ical->recur->u.month_pos = option_menu_active_number (ee->recur_rr_month_day);
			ical->recur->u.month_day = option_menu_active_number (ee->recur_rr_month_weekday);
		}

		ical->recur->interval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ee->recur_rr_month_period));

		break;

	case 4:
		/* Yearly */
		ical->recur->type = RECUR_YEARLY_BY_DAY;
		/* FIXME: need to specify anything else?  I am assuming the code will look at the dtstart
		 * to figure out when to recur. - Federico
		 */
		break;

	default:
		g_assert_not_reached ();
	}
	return 1;
}

static void
ee_store_recur_end_to_ical (EventEditor *ee)
{
	iCalObject *ical;
	GSList *list;
	int i;

	/* Ending date of recurrence */

	ical = ee->ical;

	for (i = 0, list = ee->recur_ed_group; list; i++, list = list->next)
		if (GTK_TOGGLE_BUTTON (list->data)->active)
			break;

	i = g_slist_length (ee->recur_ed_group) - i - 1; /* the list is stored in reverse order of insertion */

	switch (i) {
	case 0:
		/* repeat forever */
		ical->recur->_enddate = 0;
		ical->recur->enddate = 0;
		ical->recur->duration = 0;
		break;

	case 1:
		/* end date */
		ical->recur->_enddate = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->recur_ed_end_on));
		ical->recur->enddate = ical->recur->enddate;
		ical->recur->duration = 0;
		break;

	case 2:
		/* end after n occurrences */
		ical->recur->duration = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ee->recur_ed_end_after));
		ical_object_compute_end (ical);
		break;

	default:
		g_assert_not_reached ();
		break;
	}
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
ee_store_recur_exceptions_to_ical (EventEditor *ee)
{
	iCalObject *ical;
	GtkCList *clist;
	int i;
	time_t *t;

	ical = ee->ical;
	clist = GTK_CLIST (ee->recur_ex_clist);

	free_exdate (ical);

	for (i = 0; i < clist->rows; i++) {
		t = gtk_clist_get_row_data (clist, i);
		ical->exdate = g_list_prepend (ical->exdate, t);
	}
}

static void
ee_store_recur_values_to_ical (EventEditor *ee)
{
	if (ee_store_recur_rule_to_ical (ee)){
		ee_store_recur_exceptions_to_ical (ee);
		ee_store_recur_end_to_ical (ee);
	} else if (ee->ical->recur) {
		g_free (ee->ical->recur);
		ee->ical->recur = NULL;
		if (ee->ical->exdate){
			GList *l = ee->ical->exdate;

			for (; l; l = l->next)
				g_free (l->data);
			g_list_free (l);
		}
	}
}

/*
 * Retrieves all of the information from the different widgets and updates
 * the iCalObject accordingly.
 */
static void
ee_store_dlg_values_to_ical (EventEditor *ee)
{
	time_t now;

	ee_store_general_values_to_ical (ee);
	ee_store_recur_values_to_ical (ee);

	now = time (NULL);

	/* FIXME: This is not entirely correct; we should check if the values actually changed */
	ee->ical->last_mod = now;

	if (ee->ical->new)
		ee->ical->created = now;
}

static void
ee_ok (GtkWidget *widget, EventEditor *ee)
{
	ee_store_dlg_values_to_ical (ee);

	if (ee->ical->new)
		gnome_calendar_add_object (ee->gnome_cal, ee->ical);
	else
		gnome_calendar_object_changed (ee->gnome_cal, ee->ical, CHANGE_ALL);

	ee->ical->new = 0;
}

static void
ee_cancel (GtkWidget *widget, EventEditor *ee)
{
	if (ee->ical->new) {
		ical_object_destroy (ee->ical);
		ee->ical = NULL;
	}

}

static void
ee_create_buttons (EventEditor *ee)
{
        gnome_dialog_append_buttons(GNOME_DIALOG(ee), 
				    GNOME_STOCK_BUTTON_OK,
				    GNOME_STOCK_BUTTON_CANCEL, NULL);

	gnome_dialog_button_connect (GNOME_DIALOG (ee), 0, GTK_SIGNAL_FUNC(ee_ok), ee);
	gnome_dialog_button_connect (GNOME_DIALOG (ee), 1, GTK_SIGNAL_FUNC(ee_cancel), ee);
	
	return;
}

/*
 * Load the contents in a delayed fashion, as the GtkText widget needs it
 */
static void
ee_fill_summary (GtkWidget *widget, EventEditor *ee)
{
	int pos = 0;

	gtk_editable_insert_text (GTK_EDITABLE (ee->general_summary), ee->ical->summary,
				  strlen (ee->ical->summary), &pos);
	gtk_text_thaw (GTK_TEXT (ee->general_summary));
}

enum {
	OWNER_LINE,
	DESC_LINE,
	SUMMARY_LINE,
	TIME_LINE,
	ALARM_LINE,
	CLASS_LINE
};

/* Create/setup the general page */
static void
ee_init_general_page (EventEditor *ee)
{
	GtkWidget *l;
	GtkWidget *hbox;

	ee->general_table = gtk_table_new (1, 1, FALSE);
	gtk_container_border_width (GTK_CONTAINER (ee->general_table), 4);
	gtk_table_set_row_spacings (GTK_TABLE (ee->general_table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (ee->general_table), 4);
	gtk_notebook_append_page (GTK_NOTEBOOK (ee->notebook), GTK_WIDGET (ee->general_table),
				  gtk_label_new (_("General")));

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_table_attach (GTK_TABLE (ee->general_table), hbox,
			  0, 1, OWNER_LINE, OWNER_LINE + 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 4);
	
	l = gtk_label_new (_("Owner:"));
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

	ee->general_owner = gtk_label_new (ee->ical->organizer ? ee->ical->organizer : _("?"));
	gtk_misc_set_alignment (GTK_MISC (ee->general_owner), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), ee->general_owner, TRUE, TRUE, 4);

	l = gtk_label_new (_("Summary:"));
	gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (ee->general_table), l,
			  0, 1, DESC_LINE, DESC_LINE + 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	
	ee->general_summary = gtk_text_new (NULL, NULL);
	gtk_text_freeze (GTK_TEXT (ee->general_summary));
	gtk_signal_connect (GTK_OBJECT (ee->general_summary), "realize",
			    GTK_SIGNAL_FUNC (ee_fill_summary), ee);
	gtk_widget_set_usize (ee->general_summary, 0, 60);
	gtk_text_set_editable (GTK_TEXT (ee->general_summary), 1);
	gtk_table_attach (GTK_TABLE (ee->general_table), ee->general_summary,
			  0, 1, SUMMARY_LINE, SUMMARY_LINE+1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);

	l = ee_alarm_widgets (ee);
	gtk_table_attach (GTK_TABLE (ee->general_table), l,
			  0, 1, ALARM_LINE, ALARM_LINE + 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);

	l = event_editor_setup_time_frame (ee);
	gtk_table_attach (GTK_TABLE (ee->general_table), l,
			  0, 1, TIME_LINE, TIME_LINE + 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);

	l = ee_classification_widgets (ee);
	gtk_table_attach (GTK_TABLE (ee->general_table), l,
			  0, 1, CLASS_LINE, CLASS_LINE + 1,
			  GTK_EXPAND | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
}

static void
ee_init_summary_page (EventEditor *ee)
{
}

struct {
	char *name;
} recurrence_types [] = {
	{ N_("None") },
	{ N_("Daily") },
	{ N_("Weekly") },
	{ N_("Monthly") },
	{ N_("Yearly") },
	{ 0 }
};

static void
recurrence_toggled (GtkRadioButton *radio, EventEditor *ee)
{
	GSList *list = ee->recur_rr_group;
	int which;

	if (!GTK_TOGGLE_BUTTON (radio)->active)
		return;

	for (which = 0; list; list = list->next, which++) {
		if (list->data == radio) {
			gtk_notebook_set_page (GTK_NOTEBOOK (ee->recur_rr_notebook), 4 - which);
			return;
		}
	}
}

static struct numbered_item weekday_positions[] = {
	{ N_("1st"), 1 },
	{ N_("2nd"), 2 },
	{ N_("3rd"), 3 },
	{ N_("4th"), 4 },
	{ N_("5th"), 5 },
	{ 0 }
};

static struct numbered_item weekday_names[] = {
	{ N_("Monday"),    1 },
	{ N_("Tuesday"),   2 },
	{ N_("Wednesday"), 3 },
	{ N_("Thursday"),  4 },
	{ N_("Friday"),    5 },
	{ N_("Saturday"),  6 },
	{ N_("Sunday"),    0 }, /* on the spec, Sunday is zero */
	{ 0 }
};

static GtkWidget *
make_numbered_menu (struct numbered_item *items, int sel)
{
	GtkWidget *option_menu, *menu;
	int i;

	option_menu = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	for (i = 0; items[i].text; i++) {
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (_(items[i].text));
		gtk_object_set_user_data (GTK_OBJECT (item), &items[i]);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), sel);

	return option_menu;
}

static void
month_sensitize (EventEditor *ee, int state)
{
	gtk_widget_set_sensitive (ee->recur_rr_month_date, state);
	gtk_widget_set_sensitive (ee->recur_rr_month_date_label, state);
	
	gtk_widget_set_sensitive (ee->recur_rr_month_day,  !state);
	gtk_widget_set_sensitive (ee->recur_rr_month_weekday,  !state);
}

static void
recur_month_enable_date (GtkToggleButton *button, EventEditor *ee)
{
	month_sensitize (ee, button->active);
}

static void
desensitize_on_toggle (GtkToggleButton *toggle, gpointer data)
{
	gtk_widget_set_sensitive (GTK_WIDGET (data), !toggle->active);
}

static void
ee_rp_init_rule (EventEditor *ee)
{
	static char *day_names [] = { N_("Mon"), N_("Tue"), N_("Wed"), N_("Thu"), N_("Fri"), N_("Sat"), N_("Sun") };
	GtkWidget   *r, *re, *r1, *f, *vbox, *hbox, *b, *week_hbox, *week_day, *w;
	GtkWidget   *none, *daily, *weekly, *monthly, *yearly;
	GtkNotebook *notebook;
	GSList      *group;
	int          i, page, day_period, week_period, month_period, year_period;
	int          week_vector, default_day, def_pos, def_off;
	time_t       now;
	struct tm   *tm;

	now = time (NULL);
	tm = localtime (&now);

	f = gtk_frame_new (_("Recurrence rule"));

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (hbox), 4);
	gtk_container_add (GTK_CONTAINER (f), hbox);

	vbox = gtk_vbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), gtk_vseparator_new (), FALSE, FALSE, 0);

	ee->recur_rr_notebook = gtk_notebook_new ();
	notebook = GTK_NOTEBOOK (ee->recur_rr_notebook);
	gtk_box_pack_start (GTK_BOX (hbox), ee->recur_rr_notebook, TRUE, TRUE, 0);

	day_period   = 1;
	week_period  = 1;
	month_period = 1;
	year_period  = 1;

	/* Default to today */

	week_vector = 1 << tm->tm_wday;
	default_day = tm->tm_mday - 1;
	def_pos = 0;
	def_off = 0;

	/* Determine which should be the default selection */

	page = 0;
	if (ee->ical->recur) {
		enum RecurType type = ee->ical->recur->type;
		int interval = ee->ical->recur->interval;

		switch (type) {
		case RECUR_DAILY:
			page = 1;
			day_period = interval;
			break;

		case RECUR_WEEKLY:
			page = 2;
			week_period = interval;
			week_vector = ee->ical->recur->weekday;
			break;

		case RECUR_MONTHLY_BY_POS:
			page = 3;
			month_period = interval;
			def_pos = ee->ical->recur->u.month_pos;
			default_day = ee->ical->recur->u.month_day;
			break;

		case RECUR_MONTHLY_BY_DAY:
			page = 3;
			month_period = interval;
			default_day = ee->ical->recur->u.month_day;
			break;

		case RECUR_YEARLY_BY_MONTH:
			page = 4;
			year_period  = interval;
			break;

		case RECUR_YEARLY_BY_DAY:
			page = 4;
			year_period  = interval;
			break;
		}
	} else
		page = 0;

	/* The recurrency selector */

	for (i = 0, group = NULL; recurrence_types [i].name; i++) {
		r = gtk_radio_button_new_with_label (group, _(recurrence_types [i].name));
		group = gtk_radio_button_group (GTK_RADIO_BUTTON (r));

		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (r), i == page);
		gtk_signal_connect (GTK_OBJECT (r), "toggled", GTK_SIGNAL_FUNC (recurrence_toggled), ee);
		gtk_box_pack_start (GTK_BOX (vbox), r, FALSE, FALSE, 0);

		if (i == 0)
			gtk_signal_connect (GTK_OBJECT (r), "toggled",
					    (GtkSignalFunc) desensitize_on_toggle,
					    ee->recur_hbox);
	}

	ee->recur_rr_group = group;

	/* 0. No recurrence */
	none = gtk_label_new ("");
	
	/* 1. The daily recurrence */

	daily = gtk_vbox_new (FALSE, 0);

	b = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (daily), b, FALSE, FALSE, 0);

	ee->recur_rr_day_period = make_spin_button (day_period, 1, 10000);
	gtk_box_pack_start (GTK_BOX (b), gtk_label_new (_("Every")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (b), ee->recur_rr_day_period, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (b), gtk_label_new (_("day(s)")), FALSE, FALSE, 0);

	/* 2. The weekly recurrence */

	weekly = gtk_vbox_new (FALSE, 4);

	week_hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (weekly), week_hbox, FALSE, FALSE, 0);

	/* 2.1 The week period selector */

	ee->recur_rr_week_period = make_spin_button (week_period, 1, 10000);
	gtk_box_pack_start (GTK_BOX (week_hbox), gtk_label_new (_("Every")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (week_hbox), ee->recur_rr_week_period, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (week_hbox), gtk_label_new (_("week(s)")), FALSE, FALSE, 0);

	/* 2.2 The week day selector */

	week_day = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (weekly), week_day, FALSE, FALSE, 0);

	for (i = 0; i < 7; i++) {
		ee->recur_rr_week_days [i] = gtk_check_button_new_with_label (_(day_names [i]));
		gtk_box_pack_start (GTK_BOX (week_day), ee->recur_rr_week_days [i], FALSE, FALSE, 0);

		if (week_vector & (1 << ((i + 1) % 7))) /* on the spec, Sunday is 0 */
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (ee->recur_rr_week_days [i]), TRUE);
	}

	/* 3. The monthly recurrence */

	monthly = gtk_table_new (0, 0, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (monthly), 4);
	gtk_table_set_col_spacings (GTK_TABLE (monthly), 4);

	re = gtk_radio_button_new_with_label (NULL, _("Recur on the"));
	ee->recur_rr_month_date = make_spin_button (default_day, 1, 31);
	ee->recur_rr_month_date_label = w = gtk_label_new (_("th day of the month"));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (monthly), re,
			  0, 1, 0, 1, FS, FS, 0, 0);
	gtk_table_attach (GTK_TABLE (monthly), ee->recur_rr_month_date,
			  1, 2, 0, 1, FS, FS, 0, 0);
	gtk_table_attach (GTK_TABLE (monthly), w, 
			  2, 3, 0, 1, FS, FS, 0, 0);
	gtk_signal_connect (GTK_OBJECT (re), "toggled", GTK_SIGNAL_FUNC (recur_month_enable_date), ee);

	r1 = gtk_radio_button_new_with_label (gtk_radio_button_group (GTK_RADIO_BUTTON (re)), _("Recur on the"));
	ee->recur_rr_month_day = make_numbered_menu (weekday_positions, def_pos);
	ee->recur_rr_month_weekday = make_numbered_menu (weekday_names, default_day);
	gtk_table_attach (GTK_TABLE (monthly), r1,
			  0, 1, 1, 2, FS, FS, 0, 0);
	gtk_table_attach (GTK_TABLE (monthly), ee->recur_rr_month_day,
			  1, 2, 1, 2, FS, FS, 0, 0);
	gtk_table_attach (GTK_TABLE (monthly), ee->recur_rr_month_weekday,
			  2, 3, 1, 2, FS, FS, 0, 0);

	gtk_table_attach (GTK_TABLE (monthly), gtk_label_new (_("Every")),
			  3, 4, 0, 2, FS, FS, 0, 0);
	ee->recur_rr_month_period = make_spin_button (month_period, 1, 10000);
	gtk_table_attach (GTK_TABLE (monthly), ee->recur_rr_month_period,
			  4, 5, 0, 2, FS, FS, 0, 0);
	gtk_table_attach (GTK_TABLE (monthly), gtk_label_new (_("month(s)")),
			  5, 6, 0, 2, FS, FS, 0, 0);

	if (ee->ical->recur) {
		if (ee->ical->recur->type == RECUR_MONTHLY_BY_POS)
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (r1), 1);
	} else
		recur_month_enable_date (GTK_TOGGLE_BUTTON (re), ee);

	/* 4. The yearly recurrence */

	yearly = gtk_vbox_new (FALSE, 0);

	b = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (yearly), b, FALSE, FALSE, 0);

	ee->recur_rr_year_period = make_spin_button (year_period, 1, 10000);
	gtk_box_pack_start (GTK_BOX (b), gtk_label_new (_("Every")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (b), ee->recur_rr_year_period, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (b), gtk_label_new (_("year(s)")), FALSE, FALSE, 0);

	/* Finish setting this up */

	gtk_notebook_append_page (notebook, none, gtk_label_new (""));
	gtk_notebook_append_page (notebook, daily, gtk_label_new (""));
	gtk_notebook_append_page (notebook, weekly, gtk_label_new (""));
	gtk_notebook_append_page (notebook, monthly, gtk_label_new (""));
	gtk_notebook_append_page (notebook, yearly, gtk_label_new (""));
	gtk_notebook_set_show_tabs (notebook, FALSE);
	gtk_notebook_set_show_border (notebook, FALSE);

	gtk_notebook_set_page (notebook, page);

	/* Attach to the main box */

	gtk_box_pack_start (GTK_BOX (ee->recur_vbox), f, FALSE, FALSE, 0);
}

static void
sensitize_on_toggle (GtkToggleButton *toggle, gpointer data)
{
	gtk_widget_set_sensitive (GTK_WIDGET (data), toggle->active);
}

static void
ee_rp_init_ending_date (EventEditor *ee)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GSList    *group;
	GtkWidget *radio0, *radio1, *radio2;
	GtkWidget *hbox;
	GtkWidget *ihbox;
	GtkWidget *widget;
	time_t     enddate;
	int        repeat;

	frame = gtk_frame_new (_("Ending date"));

	vbox = gtk_vbox_new (TRUE, 4);
	gtk_container_border_width (GTK_CONTAINER (vbox), 4);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	group = NULL;

	/* repeat forever */

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	radio0 = gtk_radio_button_new_with_label (group, _("Repeat forever"));
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio0));
	gtk_box_pack_start (GTK_BOX (hbox), radio0, FALSE, FALSE, 0);

	/* end on date */

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	radio1 = gtk_radio_button_new_with_label (group, _("End on"));
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio1));
	gtk_box_pack_start (GTK_BOX (hbox), radio1, FALSE, FALSE, 0);

	ihbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_set_sensitive (ihbox, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), ihbox, FALSE, FALSE, 0);

	if (ee->ical->recur)
		enddate = ee->ical->recur->enddate;
	else
		enddate = ee->ical->dtend;

	ee->recur_ed_end_on = widget = gnome_date_edit_new (enddate, FALSE);
	gtk_box_pack_start (GTK_BOX (ihbox), widget, FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (radio1), "toggled",
			    (GtkSignalFunc) sensitize_on_toggle,
			    ihbox);

	/* end after n occurrences */

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	radio2 = gtk_radio_button_new_with_label (group, _("End after"));
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio2));
	gtk_box_pack_start (GTK_BOX (hbox), radio2, FALSE, FALSE, 0);

	ihbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_set_sensitive (ihbox, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), ihbox, FALSE, FALSE, 0);

	if (ee->ical->recur && ee->ical->recur->duration)
		repeat = ee->ical->recur->duration;
	else
		repeat = 2;

	ee->recur_ed_end_after = widget = make_spin_button (repeat, 1, 10000);
	gtk_box_pack_start (GTK_BOX (ihbox), widget, FALSE, FALSE, 0);

	widget = gtk_label_new (_("occurrence(s)"));
	gtk_box_pack_start (GTK_BOX (ihbox), widget, FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (radio2), "toggled",
			    (GtkSignalFunc) sensitize_on_toggle,
			    ihbox);

	/* Activate appropriate item */

	if (ee->ical->recur) {
		if (ee->ical->recur->_enddate == 0) {
			if (ee->ical->recur->duration == 0)
				gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (radio0), TRUE);
			else {
				gtk_spin_button_set_value (GTK_SPIN_BUTTON (ee->recur_ed_end_after),
							   ee->ical->recur->duration);
				gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (radio2), TRUE);
			}
		} else {
			gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->recur_ed_end_on), ee->ical->recur->enddate);
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (radio1), TRUE);
		}
	}

	/* Done, add to main table */

	ee->recur_ed_group = group;

	gtk_box_pack_start (GTK_BOX (ee->recur_hbox), frame, FALSE, FALSE, 0);
}

static char *
get_exception_string (time_t t)
{
	static char buf[256];

	strftime (buf, sizeof(buf), "%a %b %d %Y", localtime (&t)); /* FIXME: how to i18n this? */
	return buf;
}

static void
append_exception (EventEditor *ee, time_t t)
{
	time_t *tt;
	char   *c[1];
	int     i;

	c[0] = get_exception_string (t);

	tt = g_new (time_t, 1);
	*tt = t;

	i = gtk_clist_append (GTK_CLIST (ee->recur_ex_clist), c);
	gtk_clist_set_row_data (GTK_CLIST (ee->recur_ex_clist), i, tt);

	gtk_widget_set_sensitive (ee->recur_ex_vbox, TRUE);
}

static void
fill_exception_clist (EventEditor *ee)
{
	GList  *list;

	for (list = ee->ical->exdate; list; list = list->next)
		append_exception (ee, *((time_t *) list->data));
}

static void
add_exception (GtkWidget *widget, EventEditor *ee)
{
	time_t t;

	t = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->recur_ex_date));
	append_exception (ee, t);
}

static void
change_exception (GtkWidget *widget, EventEditor *ee)
{
	GtkCList *clist;
	time_t *t;
	int sel;

	clist = GTK_CLIST (ee->recur_ex_clist);
	sel = (gint) clist->selection->data;

	t = gtk_clist_get_row_data (clist, sel);
	*t = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->recur_ex_date));

	gtk_clist_set_text (clist, sel, 0, get_exception_string (*t));
}

static void
delete_exception (GtkWidget *widget, EventEditor *ee)
{
	GtkCList *clist;
	int sel;

	clist = GTK_CLIST (ee->recur_ex_clist);
	sel = (gint) clist->selection->data;

	g_free (gtk_clist_get_row_data (clist, sel)); /* free the time_t stored there */

	gtk_clist_remove (clist, sel);

	if (clist->rows == 0)
		gtk_widget_set_sensitive (ee->recur_ex_vbox, FALSE);
}

static void
ee_rp_init_exceptions (EventEditor *ee)
{
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *ivbox;
	GtkWidget *widget;

	frame = gtk_frame_new (_("Exceptions"));

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (hbox), 4);
	gtk_container_add (GTK_CONTAINER (frame), hbox);

	vbox = gtk_vbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	ee->recur_ex_date = widget = gnome_date_edit_new (time (NULL), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	widget = gtk_button_new_with_label (_("Add exception"));
	gtk_signal_connect (GTK_OBJECT (widget), "clicked",
			    (GtkSignalFunc) add_exception,
			    ee);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	ee->recur_ex_vbox = ivbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_set_sensitive (ivbox, FALSE); /* at first there are no items to change or delete */
	gtk_box_pack_start (GTK_BOX (vbox), ivbox, FALSE, FALSE, 0);
	
	widget = gtk_button_new_with_label (_("Change selected"));
	gtk_signal_connect (GTK_OBJECT (widget), "clicked",
			    (GtkSignalFunc) change_exception,
			    ee);
	gtk_box_pack_start (GTK_BOX (ivbox), widget, FALSE, FALSE, 0);

	widget = gtk_button_new_with_label (_("Delete selected"));
	gtk_signal_connect (GTK_OBJECT (widget), "clicked",
			    (GtkSignalFunc) delete_exception,
			    ee);
	gtk_box_pack_start (GTK_BOX (ivbox), widget, FALSE, FALSE, 0);

	ee->recur_ex_clist = widget = gtk_clist_new (1);
	gtk_clist_set_selection_mode (GTK_CLIST (widget), GTK_SELECTION_BROWSE);
	gtk_clist_set_policy (GTK_CLIST (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	fill_exception_clist (ee);
	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);

	/* Done, add to main table */

	gtk_box_pack_start (GTK_BOX (ee->recur_hbox), frame, TRUE, TRUE, 0);
}

static void
ee_init_recurrence_page (EventEditor *ee)
{
	ee->recur_vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (ee->recur_vbox), 4);

	ee->recur_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_set_sensitive (ee->recur_hbox, FALSE);

	ee->recur_page_label = gtk_label_new (_("Recurrence"));

	gtk_notebook_append_page (GTK_NOTEBOOK (ee->notebook), ee->recur_vbox,
				  ee->recur_page_label);

	ee_rp_init_rule (ee);

	/* pack here so that the box gets inserted after the recurrence rule frame */
	gtk_box_pack_start (GTK_BOX (ee->recur_vbox), ee->recur_hbox, FALSE, FALSE, 0);

	ee_rp_init_ending_date (ee);
	ee_rp_init_exceptions (ee);
}

static void
event_editor_init_widgets (EventEditor *ee)
{	
	ee->notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG(ee)->vbox), ee->notebook, 1, 1, 0);

	/* Init the various configuration pages */
	ee_init_general_page (ee);
	ee_init_summary_page (ee);
	ee_init_recurrence_page (ee);
       
	/* Buttons */
	ee_create_buttons(ee);
	
	/* We show all of the contained widgets */
	gtk_widget_show_all (GTK_BIN (ee)->child);
}

static void
event_editor_init (EventEditor *ee)
{
	ee->ical = 0;
	gnome_dialog_set_destroy(GNOME_DIALOG(ee), TRUE);
}

static void
event_editor_destroy (GtkObject *object)
{
	EventEditor *ee;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (object));

	ee = EVENT_EDITOR (object);

	if (ee->ical)
		ee->ical->user_data = NULL; /* we are no longer editing it */
}

GtkWidget *
event_editor_new (GnomeCalendar *gcal, iCalObject *ical)
{
	GtkWidget *retval;
	EventEditor *ee;

	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_flush ();

	retval = gtk_type_new (event_editor_get_type ());
	ee = EVENT_EDITOR (retval);
	
	if (ical == 0){
		ical = ical_new ("", user_name, "");
		ical->new     = 1;
	}

	ical->user_data = ee; /* so that the world can know we are editing it */

	ee->ical = ical;
	ee->gnome_cal = gcal;
	event_editor_init_widgets (ee);
	
	return retval;
}
