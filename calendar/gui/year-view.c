/* Year view display for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Arturo Espinosa <arturo@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <libgnomeui/gnome-canvas-text.h>
#include "year-view.h"
#include "main.h"


#define HEAD_SPACING 4		/* Spacing between year heading and months */
#define TITLE_SPACING 2		/* Spacing between title and calendar */
#define SPACING 4		/* Spacing between months */


static void year_view_class_init    (YearViewClass  *class);
static void year_view_init          (YearView       *yv);
static void year_view_size_request  (GtkWidget      *widget,
				     GtkRequisition *requisition);
static void year_view_size_allocate (GtkWidget      *widget,
				     GtkAllocation  *allocation);


static GnomeCanvas *parent_class;


GtkType
year_view_get_type (void)
{
	static GtkType year_view_type = 0;

	if (!year_view_type) {
		GtkTypeInfo year_view_info = {
			"YearView",
			sizeof (YearView),
			sizeof (YearViewClass),
			(GtkClassInitFunc) year_view_class_init,
			(GtkObjectInitFunc) year_view_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		year_view_type = gtk_type_unique (gnome_canvas_get_type (), &year_view_info);
	}

	return year_view_type;
}

static void
year_view_class_init (YearViewClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gnome_canvas_get_type ());

	widget_class->size_request = year_view_size_request;
	widget_class->size_allocate = year_view_size_allocate;
}

static void
year_view_init (YearView *yv)
{
	int i;
	char buf[100];
	struct tm tm;

	memset (&tm, 0, sizeof (tm));

	/* Heading */

	yv->heading = gnome_canvas_item_new (GNOME_CANVAS_GROUP (yv->canvas.root),
					     gnome_canvas_text_get_type (),
					     "anchor", GTK_ANCHOR_N,
					     "font", "-*-helvetica-bold-r-normal--14-*-*-*-*-*-iso8859-1",
					     "fill_color", "black",
					     NULL);

	/* Months */

	for (i = 0; i < 12; i++) {
		/* Title */

		strftime (buf, 100, "%B", &tm);
		tm.tm_mon++;

		yv->titles[i] = gnome_canvas_item_new (GNOME_CANVAS_GROUP (yv->canvas.root),
						       gnome_canvas_text_get_type (),
						       "text", buf,
						       "anchor", GTK_ANCHOR_N,
						       "font", "-*-helvetica-bold-r-normal--12-*-*-*-*-*-iso8859-1",
						       "fill_color", "black",
						       NULL);

		/* Month item */

		yv->mitems[i] = gnome_month_item_new (GNOME_CANVAS_GROUP (yv->canvas.root));
		gnome_canvas_item_set (yv->mitems[i],
				       "anchor", GTK_ANCHOR_NW,
				       "start_on_monday", week_starts_on_monday,
				       "heading_color", "white",
				       NULL);
	}
}

GtkWidget *
year_view_new (GnomeCalendar *calendar, time_t year)
{
	YearView *yv;

	g_return_val_if_fail (calendar != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (calendar), NULL);

	yv = gtk_type_new (year_view_get_type ());
	yv->calendar = calendar;

	year_view_set (yv, year);
	return GTK_WIDGET (yv);
}

static void
year_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	YearView *yv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_YEAR_VIEW (widget));
	g_return_if_fail (requisition != NULL);

	yv = YEAR_VIEW (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_request)
		(* GTK_WIDGET_CLASS (parent_class)->size_request) (widget, requisition);

	requisition->width = 200;
	requisition->height = 150;
}

static void
year_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	YearView *yv;
	double width, height;
	double mwidth, mheight;
	double h_yofs;
	double m_yofs;
	double x, y;
	int i;
	GtkArg arg;
	GdkFont *head_font, *title_font;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_YEAR_VIEW (widget));
	g_return_if_fail (allocation != NULL);

	yv = YEAR_VIEW (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (yv), 0, 0, allocation->width, allocation->height);

	arg.name = "font_gdk";
	gtk_object_getv (GTK_OBJECT (yv->heading), 1, &arg);
	head_font = GTK_VALUE_BOXED (arg);

	arg.name = "font_gdk";
	gtk_object_getv (GTK_OBJECT (yv->titles[0]), 1, &arg);
	title_font = GTK_VALUE_BOXED (arg);

	/* Adjust heading */

	gnome_canvas_item_set (yv->heading,
			       "x", (double) allocation->width / 2.0,
			       "y", (double) HEAD_SPACING,
			       NULL);

	/* Adjust months */

	h_yofs = 2 * HEAD_SPACING + head_font->ascent + head_font->descent;
	m_yofs = SPACING + title_font->ascent + title_font->descent;

	width = (allocation->width + SPACING) / 3.0;
	height = (allocation->height - h_yofs + SPACING) / 4.0;

	mwidth = (allocation->width - 2 * SPACING) / 3.0;
	mheight = (allocation->height - h_yofs - 3 * SPACING - 4 * m_yofs) / 4.0;

	for (i = 0; i < 12; i++) {
		x = (i % 3) * width;
		y = (i / 3) * height + h_yofs;

		/* Title */

		gnome_canvas_item_set (yv->titles[i],
				       "x", x + width / 2.0,
				       "y", y,
				       NULL);

		/* Month item */

		gnome_canvas_item_set (yv->mitems[i],
				       "x", x,
				       "y", y + m_yofs,
				       "width", mwidth,
				       "height", mheight,
				       NULL);
	}
}

void
year_view_update (YearView *yv, iCalObject *object, int flags)
{
	g_return_if_fail (yv != NULL);
	g_return_if_fail (IS_YEAR_VIEW (yv));

	/* FIXME */
}

void
year_view_set (YearView *yv, time_t year)
{
	struct tm tm;
	int i;
	char buf[100];

	g_return_if_fail (yv != NULL);
	g_return_if_fail (IS_YEAR_VIEW (yv));

	tm = *localtime (&year);

	/* Heading */

	sprintf (buf, "%d", tm.tm_year + 1900);
	gnome_canvas_item_set (yv->heading,
			       "text", buf,
			       NULL);

	/* Months */

	for (i = 0; i < 12; i++)
		gnome_canvas_item_set (yv->mitems[i],
				       "year", tm.tm_year + 1900,
				       "month", i,
				       NULL);

	/* FIXME: update events */
}

void
year_view_time_format_changed (YearView *yv)
{
	int i;

	g_return_if_fail (yv != NULL);
	g_return_if_fail (IS_YEAR_VIEW (yv));

	for (i = 0; i < 12; i++)
		gnome_canvas_item_set (yv->mitems[i],
				       "start_on_monday", week_starts_on_monday,
				       NULL);

	/* FIXME: update events */
}














#if 0

#include "gncal-year-view.h"
#include "calendar.h"
#include "timeutil.h"

static void gncal_year_view_init (GncalYearView *yview);

static void
double_click(GtkCalendar *gc, GncalYearView *yview)
{
	struct tm tm;
	time_t t;

	tm.tm_mday = gc->selected_day;
	tm.tm_mon  = gc->month;
	tm.tm_year = gc->year - 1900;
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_isdst  = -1;
	t = mktime (&tm);

	gnome_calendar_dayjump (yview->gcal, t);
}
	
static void
do_nothing(GtkCalendarClass *c)
{
}

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
	
	yview->gcal = NULL;
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
		  gtk_calendar_display_options(GTK_CALENDAR(yview->calendar[i]), 
					       GTK_CALENDAR_SHOW_DAY_NAMES |
					       GTK_CALENDAR_NO_MONTH_CHANGE);
		  frame = gtk_frame_new(NULL);
		  vbox = gtk_vbox_new(0,0);
		
		  yview->handler[i] = 
		    gtk_signal_connect(GTK_OBJECT(yview->calendar[i]), "day_selected", 
				       GTK_SIGNAL_FUNC(select_day), (gpointer *) yview);
		  
		  gtk_signal_connect(GTK_OBJECT(yview->calendar[i]), "day_selected_double_click",
				     GTK_SIGNAL_FUNC(double_click), (gpointer *) yview);

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

static void
year_view_mark_day (iCalObject *ical, time_t start, time_t end, void *closure)
{
	GncalYearView *yview = (GncalYearView *) closure;
	struct tm tm_s;
	time_t t, day_end;

	tm_s = *localtime (&start);
	day_end = time_end_of_day (end);

	for (t = start; t <= day_end; t+= 60*60*24){
		time_t new = mktime (&tm_s);
		struct tm tm_day;
		
		tm_day = *localtime (&new);
		gtk_calendar_mark_day (GTK_CALENDAR (yview->calendar [tm_day.tm_mon]),
				       tm_day.tm_mday);
		tm_s.tm_mday++;
	}
}

static void
gncal_year_view_set_year (GncalYearView *yview, int year)
{
	time_t year_begin, year_end;
	char buff[20];
	GList  *l;
	int i;

	if (!yview->gcal->cal)
		return;
	
	snprintf(buff, 20, "%d", yview->year + 1900);
	gtk_label_set(GTK_LABEL(yview->year_label), buff);

	for (i = 0; i < 12; i++) {
		gtk_calendar_freeze (GTK_CALENDAR (yview->calendar [i]));
		gtk_calendar_select_month (GTK_CALENDAR(yview->calendar[i]), i, yview->year + 1900);
		gtk_calendar_clear_marks (GTK_CALENDAR (yview->calendar[i]));
	}
	
	year_begin = time_year_begin (yview->year);
	year_end   = time_year_end   (yview->year);

	l = calendar_get_events_in_range (yview->gcal->cal, year_begin, year_end);
	for (; l; l = l->next){
		CalendarObject *co = l->data;

		year_view_mark_day (co->ico, co->ev_start, co->ev_end, yview);
	}
	for (i = 0; i < 12; i++) 
		gtk_calendar_thaw (GTK_CALENDAR (yview->calendar [i]));

	calendar_destroy_event_list (l);
}

void
gncal_year_view_set (GncalYearView *yview, time_t date)
{
	struct tm *tmptm;

	tmptm = localtime(&date);
	yview->year = tmptm->tm_year;

	gncal_year_view_set_year (yview, yview->year);
}

void
gncal_year_view_update (GncalYearView *yview, iCalObject *ico, int flags)
{
	g_return_if_fail (yview != NULL);
	g_return_if_fail (GNCAL_IS_YEAR_VIEW (yview));

	/* If only the summary changed, we dont care */
	if (flags && (flags & CHANGE_SUMMARY) == flags)
		return;

	gncal_year_view_set_year (yview, yview->year);
}

#endif
