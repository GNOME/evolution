/* Week view composite widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Arturo Espinosa <arturo@nuclecu.unam.mx>
 * 
 * Heavily based on Federico Mena's week view.
 * 
 */

#include "gncal-year-view.h"
#include "timeutil.h"

static void gncal_year_view_init (GncalYearView *yview);

static void
select_day(GtkWidget *widget, gpointer data)
{
	int i;
	
	GncalYearView *yview;
	
	yview = GNCAL_YEAR_VIEW(data);
	
	for (i = 0; i < 12; i++)
	  gtk_signal_handler_block(GTK_OBJECT(yview->calendar[i]),
				   yview->handler[i]);
	
	for (i = 0; i < 12; i++)
	  if (GTK_CALENDAR(yview->calendar[i]) != GTK_CALENDAR(widget))
	    gtk_calendar_select_day(GTK_CALENDAR(yview->calendar[i]), 0);
					      
	for (i = 0; i < 12; i++)
	  gtk_signal_handler_unblock(GTK_OBJECT(yview->calendar[i]),
				   yview->handler[i]);
}

guint
gncal_year_view_get_type (void)
{
        static guint year_view_type = 0;

	if (!year_view_type) {
		GtkTypeInfo year_view_info = {
			"GncalYearView",
			sizeof (GncalYearView),
			sizeof (GncalYearViewClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) gncal_year_view_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		year_view_type = gtk_type_unique (gtk_table_get_type (), 
						  &year_view_info);
	}

	return year_view_type;
}

static void
gncal_year_view_init (GncalYearView *yview)
{
	int i;
	
	for (i = 0; i < 12; i++) {
		yview->calendar[i] = NULL;
		yview->handler [i] = 0;
	}
	
	yview->year_label = NULL;
	yview->year = 0;
}

GtkWidget *
gncal_year_view_new (GnomeCalendar *calendar, time_t date)
{
	struct tm my_tm = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	char monthbuff[40];
	GncalYearView *yview;
	GtkWidget *frame, *vbox, *label;
	struct tm *tmptm;
	int i, x, y;

	yview = gtk_type_new (gncal_year_view_get_type ());

	tmptm = localtime(&date);
	yview->year = tmptm->tm_year;
	yview->gcal = calendar;
	my_tm.tm_year = tmptm->tm_year;
	yview->year_label = gtk_label_new("");
	gtk_table_attach (GTK_TABLE (yview), 
			  GTK_WIDGET (yview->year_label),
			  1, 2,
			  0, 1,
			  0, 0, 0, 5);
	gtk_widget_show(GTK_WIDGET(yview->year_label));
	
	for (x = 0; x < 3; x++)
	  for (y = 0; y < 4; y++) {
		  
		  i = y * 3 + x;
		  
		  yview->calendar[i] = gtk_calendar_new();
		  gtk_calendar_display_options(GTK_CALENDAR(yview->calendar[i]), GTK_CALENDAR_SHOW_DAY_NAMES);
		  frame = gtk_frame_new(NULL);
		  vbox = gtk_vbox_new(0,0);
		
		  yview->handler[i] = 
		    gtk_signal_connect(GTK_OBJECT(yview->calendar[i]),
				       "day_selected", 
				       GTK_SIGNAL_FUNC(select_day),
				       (gpointer *) yview);
		  
		  my_tm.tm_mon = i;
		  strftime(monthbuff, 40, "%B", &my_tm);
		  label = gtk_label_new(monthbuff);
		
		  gtk_container_add(GTK_CONTAINER(frame), vbox);
		  gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 0);
		  gtk_box_pack_start(GTK_BOX(vbox), yview->calendar[i], 0, 0, 0);
		  
		  gtk_table_attach (GTK_TABLE (yview), 
				    GTK_WIDGET (frame),
				    x, x + 1,
				    y + 1, y + 2,
				    0, 0, 0, 0);

		  gtk_widget_show (frame);
		  gtk_widget_show (vbox);
		  gtk_widget_show (GTK_WIDGET (yview->calendar[i]));
	  }

	gncal_year_view_set (yview, date);
	
	return GTK_WIDGET (yview);
}

void gncal_year_view_set (GncalYearView *yview, time_t date)
{
	int i;
	char buff[10];
	struct tm *tmptm;

	tmptm = localtime(&date);
	yview->year = tmptm->tm_year;

	snprintf(buff, 10, "%d", yview->year + 1900);
	gtk_label_set(GTK_LABEL(yview->year_label), buff);
	
	for (i = 0; i < 12; i++) {
		gtk_calendar_select_month (GTK_CALENDAR(yview->calendar[i]), i, yview->year);
	}
}

static void
year_view_mark_day (iCalObject *ical, time_t start, time_t end, void *closure)
{
	GncalYearView *yview = (GncalYearView *) closure;
	struct tm *tm_s;
	int days, day;
	
	tm_s = localtime (&start);
	days = difftime (end, start) / (60*60*24);

	for (day = 0; day <= days; day++){
		time_t new = mktime (tm_s);
		struct tm *tm_day;
		
		tm_day = localtime (&new);
		gtk_calendar_mark_day (GTK_CALENDAR (yview->calendar [tm_day->tm_mon]),
				       tm_day->tm_mday);
		tm_s->tm_mday++;
	}
}

void
gncal_year_view_update (GncalYearView *yview, iCalObject *ico, int flags)
{
	g_return_if_fail (yview != NULL);
	g_return_if_fail (GNCAL_IS_YEAR_VIEW (yview));

	/* If only the summary changed, we dont care */
	if ((flags & CHANGE_SUMMARY) == flags)
		return;

	if (flags & CHANGE_NEW){
		time_t year_begin, year_end;
		GList  *l, *nl;
		
		year_begin = time_year_begin (yview->year);
		year_end   = time_year_end   (yview->year);
		
		l = g_list_append (NULL, ico);
		nl = calendar_get_objects_in_range (l, year_begin, year_end, NULL);
		if (nl){
			ical_foreach (nl, year_view_mark_day, yview);
			g_list_free (nl);
		}
		g_list_free (l);
	}
}
