/* Week view composite widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Arturo Espinosa <arturo@nuclecu.unam.mx>
 * 
 * Heavily based on Federico Mena's week view.
 * 
 */

#include <time.h>

#include "gncal-year-view.h"

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
}

GtkWidget *
gncal_year_view_new (int year)
{
	struct tm my_tm = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	char monthbuff[40];
	GncalYearView *yview;
	GtkWidget *frame, *vbox, *label;
	int i, x, y;

	yview = gtk_type_new (gncal_year_view_get_type ());

	gtk_table_set_homogeneous (GTK_TABLE (yview), TRUE);

	yview->year = year;

	for (x = 0; x < 3; x++)
	  for (y = 0; y < 4; y++) {
		  
		  i = y * 3 + x;
		  
		  yview->calendar[i] = gtk_calendar_new();
		  frame = gtk_frame_new(NULL);
		  vbox = gtk_vbox_new(0,0);
		
		  yview->handler[i] = 
		    gtk_signal_connect(GTK_OBJECT(yview->calendar[i]),
				       "day_selected", 
				       GTK_SIGNAL_FUNC(select_day),
				       (gpointer *) yview);
		  
		  my_tm.tm_mon = i;
		  my_tm.tm_year = year;
		  strftime(monthbuff, sizeof (monthbuff)-1, "%B", &my_tm);
		  label = gtk_label_new(monthbuff);
		
		  gtk_container_add(GTK_CONTAINER(frame), vbox);
		  gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 0);
		  gtk_box_pack_start(GTK_BOX(vbox), yview->calendar[i], 0, 0, 0);
		  
		  gtk_table_attach (GTK_TABLE (yview), 
				    GTK_WIDGET (vbox),
				    x, x + 1,
				    y, y + 1,
				    0, 0, 0, 0);

		  gtk_widget_show (frame);
		  gtk_widget_show (vbox);
		  gtk_widget_show (GTK_WIDGET (yview->calendar[i]));
	  }

	gncal_year_view_set (yview, year);
	
	return GTK_WIDGET (yview);
}

void gncal_year_view_set (GncalYearView *yview, int year)
{
	int i;
	
	for (i = 0; i < 12; i++) {
		yview->year = year;
		gtk_calendar_select_month (GTK_CALENDAR(yview->calendar[i]), i + 1, year);
	}
}
