/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Print support
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *          Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-paper.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-paper-selector.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-copies.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>
#include <libgnomeprint/gnome-print-preview.h>
#include <libgnomeprint/gnome-printer-dialog.h>
#include <e-util/e-dialog-widgets.h>
#include <e-util/e-time-utils.h>
#include <gal/widgets/e-unicode.h>
#include <cal-util/timeutil.h>
#include "calendar-commands.h"
#include "calendar-config.h"
#include "e-day-view.h"
#include "e-day-view-layout.h"
#include "e-week-view.h"
#include "e-week-view-layout.h"
#include "gnome-cal.h"
#include "print.h"



/*
 * Note that most dimensions are in points (1/72 of an inch) since that is
 * what gnome-print uses.
 */

/* The font size to use for normal text. */
#define DAY_NORMAL_FONT_SIZE	12
#define WEEK_NORMAL_FONT_SIZE	12
#define MONTH_NORMAL_FONT_SIZE	8

/* The height of the header bar across the top of the Day, Week & Month views,
   which contains the dates shown and the 2 small calendar months. */
#define HEADER_HEIGHT		80

/* The width of the small calendar months, the space from the right edge of
   the header rectangle, and the space between the months. */
#define SMALL_MONTH_WIDTH	80
#define SMALL_MONTH_PAD		4
#define SMALL_MONTH_SPACING	12

/* The minimum number of rows we leave space for for the long events in the
   day view. */
#define DAY_VIEW_MIN_ROWS_IN_TOP_DISPLAY	2

/* The row height for long events in the day view. */
#define DAY_VIEW_ROW_HEIGHT		20

/* The minutes per row in the day view printout. */
#define DAY_VIEW_MINS_PER_ROW		30

#define DAY_VIEW_ROWS			((60 / DAY_VIEW_MINS_PER_ROW) * 24)

/* The width of the column with all the times in it. */
#define DAY_VIEW_TIME_COLUMN_WIDTH	36

/* The space on the right of each event. */
#define DAY_VIEW_EVENT_X_PAD		8


/* copied from gnome-month-item.c  this should be shared?? */

/* Number of days in a month, for normal and leap years */
static const int days_in_month[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

/* The weird month of September 1752, where 3 Sep through 13 Sep were eliminated due to the
 * Gregorian reformation.
 */
static const int sept_1752[42] = {
	 0,  0,  1,  2, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30,
	 0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0
};

#define REFORMATION_DAY 639787		/* First day of the reformation, counted from 1 Jan 1 */
#define MISSING_DAYS 11			/* They corrected out 11 days */
#define THURSDAY 4			/* First day of reformation */
#define SATURDAY 6			/* Offset value; 1 Jan 1 was a Saturday */
#define SEPT_1752_START 2		/* Start day within month */
#define SEPT_1752_END 20		/* End day within month */


struct pdinfo
{
	gint days_shown;
	time_t day_starts[E_DAY_VIEW_MAX_DAYS + 1];

	GArray *long_events;
	GArray *events[E_DAY_VIEW_MAX_DAYS];

	gint start_hour;
	gint end_hour;
	gint start_minute_offset;
	gint end_minute_offset;
	gint rows;
	gint mins_per_row;
	gint8 cols_per_row[DAY_VIEW_ROWS];
	gboolean use_24_hour_format;
};

struct psinfo
{
	gint days_shown;
	time_t day_starts[E_WEEK_VIEW_MAX_WEEKS * 7 + 1];

	GArray *events;

	int rows_per_cell;
	int rows_per_compressed_cell;
	int display_start_weekday;
	gboolean multi_week_view;
	int weeks_shown;
	int month;
	gboolean compress_weekend;
	gboolean use_24_hour_format;
	double row_height;
	double header_row_height;
};

struct ptinfo
{
	GList *todos;
};

struct einfo
{
	char *text;
	time_t start;
	time_t end;
	int count;
};

static const GnomePaper *paper_info = NULL;

/* Returns the number of leap years since year 1 up to (but not including) the specified year */
static int
leap_years_up_to (int year)
{
	return (year / 4					/* trivial leapness */
		- ((year > 1700) ? (year / 100 - 17) : 0)	/* minus centuries since 1700 */
		+ ((year > 1600) ? ((year - 1600) / 400) : 0));	/* plus centuries since 1700 divisible by 400 */
}

/* Returns whether the specified year is a leap year */
static int
is_leap_year (int year)
{
	if (year <= 1752)
		return !(year % 4);
	else
		return (!(year % 4) && (year % 100)) || !(year % 400);
}

/* Returns the 1-based day number within the year of the specified date */
static int
day_in_year (int day, int month, int year)
{
	int is_leap, i;

	is_leap = is_leap_year (year);

	for (i = 0; i < month; i++)
		day += days_in_month [is_leap][i];

	return day;
}

/* Returns the day of the week (zero-based, zero is Sunday) for the specified date.  For the days
 * that were removed on the Gregorian reformation, it returns Thursday.
 */
static int
day_in_week (int day, int month, int year)
{
	int n;

	n = (year - 1) * 365 + leap_years_up_to (year - 1) + day_in_year (day, month, year);

	if (n < REFORMATION_DAY)
		return (n - 1 + SATURDAY) % 7;

	if (n >= (REFORMATION_DAY + MISSING_DAYS))
		return (n - 1 + SATURDAY - MISSING_DAYS) % 7;

	return THURSDAY;
}

/* Fills the 42-element days array with the day numbers for the specified month.  Slots outside the
 * bounds of the month are filled with zeros.  The starting and ending indexes of the days are
 * returned in the start and end arguments.
 */
static void
build_month (int month, int year, int *days, int *start, int *end)
{
	int i;
	int d_month, d_week, week_start_day;

	/* Note that months are zero-based, so September is month 8 */

	if ((year == 1752) && (month == 8)) {
		memcpy (days, sept_1752, 42 * sizeof (int));

		if (start)
			*start = SEPT_1752_START;

		if (end)
			*end = SEPT_1752_END;

		return;
	}

	for (i = 0; i < 42; i++)
		days[i] = 0;

	d_month = days_in_month[is_leap_year (year)][month];
	/* Get the start weekday in the month, 0=Sun to 6=Sat. */
	d_week = day_in_week (1, month, year);

	/* Get the configuration setting specifying which weekday we put on
	   the left column, 0=Sun to 6=Sat. */
	week_start_day = calendar_config_get_week_start_day ();

	/* Figure out which square we want to put the 1 in. */
	d_week = (d_week + 7 - week_start_day) % 7;

	for (i = 0; i < d_month; i++)
		days[d_week + i] = i + 1;

	if (start)
		*start = d_week;

	if (end)
		*end = d_week + d_month - 1;
}


enum align_box {
	ALIGN_LEFT=1,
	ALIGN_RIGHT,
	ALIGN_CENTER,
	ALIGN_BORDER= 1<<8
};


/* Prints a rectangle, with or without a border, filled or outline, and
   possibly with triangular arrows at one or both horizontal edges.
   width      = width of border, -ve means no border.
   fillcolor = shade of fill,   -ve means no fill.
   left_triangle_width, right_triangle_width = width from edge of rectangle to
          point of triangle, or -ve for no triangle. */
static void
print_border_with_triangles (GnomePrintContext *pc,
			     double l, double r, double t, double b,
			     double width, double fillcolor,
			     double left_triangle_width,
			     double right_triangle_width)
{
	gnome_print_gsave (pc);

	/* Fill in the interior of the rectangle, if desired. */
	if (fillcolor >= 0.0) {
		gnome_print_moveto (pc, l, t);
		if (left_triangle_width > 0.0)
			gnome_print_lineto (pc, l - left_triangle_width,
					    (t + b) / 2);
		gnome_print_lineto (pc, l, b);
		gnome_print_lineto (pc, r, b);
		if (right_triangle_width > 0.0)
			gnome_print_lineto (pc, r + right_triangle_width,
					    (t + b) / 2);
		gnome_print_lineto (pc, r, t);
		gnome_print_closepath (pc);
		gnome_print_setrgbcolor (pc, fillcolor, fillcolor,
					 fillcolor);
		gnome_print_fill (pc);
	}

	/* Draw the outline, if desired. */
	if (width >= 0.0) {
		gnome_print_moveto (pc, l, t);
		if (left_triangle_width > 0.0)
			gnome_print_lineto (pc, l - left_triangle_width,
					    (t + b) / 2);
		gnome_print_lineto (pc, l, b);
		gnome_print_lineto (pc, r, b);
		if (right_triangle_width > 0.0)
			gnome_print_lineto (pc, r + right_triangle_width,
					    (t + b) / 2);
		gnome_print_lineto (pc, r, t);
		gnome_print_closepath (pc);
		gnome_print_setrgbcolor (pc, 0, 0, 0);
		gnome_print_setlinewidth (pc, width);
		gnome_print_stroke (pc);
	}

	gnome_print_grestore (pc);
}


/* Prints a rectangle, with or without a border, and filled or outline.
   width      = width of border, -ve means no border.
   fillcolor = shade of fill,   -ve means no fill. */
static void
print_border (GnomePrintContext *pc,
	      double l, double r, double t, double b,
	      double width, double fillcolor)
{
	print_border_with_triangles (pc, l, r, t, b, width, fillcolor,
				     -1.0, -1.0);
}


/* Prints 1 line of aligned text in a box. It is centered vertically, and
   the horizontal alignment can be either ALIGN_LEFT, ALIGN_RIGHT, or
   ALIGN_CENTER. */
static void
print_text(GnomePrintContext *pc, GnomeFont *font, const char *text,
	   enum align_box align, double l, double r, double t, double b)
{
	double w, x, y;

	gnome_print_gsave (pc);

	w = gnome_font_get_width_string (font, text);

	switch (align & 3) {
	case ALIGN_LEFT:
	default:
		x = l;
		break;
	case ALIGN_RIGHT:
		x = r - w;
		break;
	case ALIGN_CENTER:
		x = l + ((r - l) - w) / 2;
		break;
	}

	/* Make sure we don't go off the left edge. */
	x = MAX (l, x);

	/* Calculate the top of where the text should be. */
	y = (t + b + gnome_font_get_size (font)) / 2;

	/* Now calculate the baseline. */
	y -= gnome_font_get_ascender (font) + 1;

	/* Set a clipping rectangle. */
	gnome_print_moveto (pc, l, t);
	gnome_print_lineto (pc, r, t);
	gnome_print_lineto (pc, r, b);
	gnome_print_lineto (pc, l, b);
	gnome_print_closepath (pc);
	gnome_print_clip (pc);

	gnome_print_newpath (pc);
	gnome_print_moveto (pc, x, y);
	gnome_print_setfont (pc, font);
	gnome_print_setrgbcolor (pc, 0, 0, 0);
	gnome_print_show (pc, text);

	gnome_print_grestore (pc);
}

/* gets/frees the font for you, as a bold font */
static void
print_text_size(GnomePrintContext *pc, double size, const char *text,
		enum align_box align, double l, double r, double t, double b)
{
	GnomeFont *font;

	font = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 0, size);
	print_text(pc, font, text, align, l, r, t, b);
	gtk_object_unref (GTK_OBJECT (font));
}

static void
titled_box (GnomePrintContext *pc, const char *text, GnomeFont *font,
	    enum align_box align, double *l, double *r, double *t, double *b,
	    double linewidth)
{
	double size;

	size = gnome_font_get_size (font);

	if (align & ALIGN_BORDER) {
		print_border (pc, *l, *r, *t, *t - size * 1.4,
			      linewidth, 0.9);
		print_border (pc, *l, *r, *t - size * 1.4, *b,
			      linewidth, -1.0);

		*l += 2;
		*r -= 2;
		*b += 2;
	}

	print_text (pc, font, text, align, *l, *r, *t, *t - size * 1.4);
	*t -= size * 1.4;
}

enum datefmt {
	DATE_MONTH	= 1 << 0,
	DATE_DAY	= 1 << 1,
	DATE_DAYNAME	= 1 << 2,
	DATE_YEAR	= 1 << 3
};

static char *days[] = {
	N_("1st"), N_("2nd"), N_("3rd"), N_("4th"), N_("5th"),
	N_("6th"), N_("7th"), N_("8th"), N_("9th"), N_("10th"),
	N_("11th"), N_("12th"), N_("13th"), N_("14th"), N_("15th"),
	N_("16th"), N_("17th"), N_("18th"), N_("19th"), N_("20th"),
	N_("21st"), N_("22nd"), N_("23rd"), N_("24th"), N_("25th"),
	N_("26th"), N_("27th"), N_("28th"), N_("29th"),	N_("30th"),
	N_("31st")
};

/*
  format the date 'nicely' and consistently for various headers
*/
static char *
format_date(time_t time, int flags, char *buffer, int bufflen)
{
	char fmt[64];
	struct tm tm;

	tm = *localtime(&time);
	fmt[0] = 0;
	if (flags & DATE_DAYNAME) {
		strcat(fmt, "%A");
	}
	if (flags & DATE_DAY) {
		if (flags & DATE_DAYNAME)
			strcat(fmt, " ");
		strcat(fmt, gettext(days[tm.tm_mday-1]));
	}
	if (flags & DATE_MONTH) {
		if (flags & (DATE_DAY|DATE_DAYNAME))
			strcat(fmt, " ");
		strcat(fmt, "%B");
		if ((flags & (DATE_DAY|DATE_YEAR)) == (DATE_DAY|DATE_YEAR))
			strcat(fmt, ",");
	}
	if (flags & DATE_YEAR) {
		if (flags & (DATE_DAY|DATE_DAYNAME|DATE_MONTH))
			strcat(fmt, " ");
		strcat(fmt, "%Y");
	}
	strftime(buffer, bufflen, fmt, &tm);
	return buffer;
}


/*
  print out the month small, embolden any days with events.
*/
static void
print_month_small (GnomePrintContext *pc, GnomeCalendar *gcal, time_t month,
		   double left, double right, double top, double bottom,
		   int titleflags, time_t greystart, time_t greyend,
		   int bordertitle)
{
	CalClient *client;
	GnomeFont *font, *font_bold, *font_normal;
	time_t now, next;
	int x, y;
	int days[42];
	int day, weekday, week_start_day;
	char buf[100];
	struct tm tm;
	double font_size, max_font_size;
	double header_size, col_width, row_height, text_xpad, w;
	double cell_top, cell_bottom, cell_left, cell_right, text_right;
	char *daynames[] = { N_("Su"), N_("Mo"), N_("Tu"), N_("We"),
			     N_("Th"), N_("Fr"), N_("Sa") };

	/* Print the title, e.g. 'June 2001', in the top 16% of the area. */
	format_date (month, titleflags, buf, 100);
	header_size = (top - bottom) * 0.16;
	font = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, FALSE,
				       header_size * 0.8);
	if (bordertitle) {
		print_border (pc, left, right, top, top - header_size,
			      1.0, 0.9);
	}
	print_text (pc, font, buf, ALIGN_CENTER, left, right,
		    top - header_size * 0.1, top - header_size);
	gtk_object_unref (GTK_OBJECT (font));

	top -= header_size;

	client = gnome_calendar_get_cal_client (gcal);

	col_width = (right - left) / 7;

	/* The top row with the day abbreviations gets an extra bit of
	   vertical space around it. */
	row_height = (top - bottom) / 7.4;

	/* First we need to calculate a reasonable font size. We start with a
	   rough guess of just under the height of each row. */
	font_size = row_height * 0.9;

	/* Check that it isn't going to be too wide. The characters are about
	   twice as high as they are wide, but we need to fit two characters
	   into each cell, so we don't want to go over col_width. */
	max_font_size = col_width * 0.65;

	font_size = MIN (font_size, max_font_size);


	/* get month days */
        tm = *localtime (&month);
	build_month (tm.tm_mon, tm.tm_year + 1900, days, 0, 0);

	font_normal = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0,
					      font_size);
	font_bold = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 0,
					    font_size);

	/* Get a reasonable estimate of the largest number we will need,
	   and use it to calculate the offset from the right edge of the
	   cell that we should put the numbers. */
	w = gnome_font_get_width_string (font_bold, "23");
	text_xpad = (col_width - w) / 2;

	gnome_print_setrgbcolor (pc, 0, 0, 0);

	/* Print the abbreviated day names across the top in bold. */
	week_start_day = calendar_config_get_week_start_day ();
	weekday = week_start_day;
	for (x = 0; x < 7; x++) {
		print_text (pc, font_bold, _(daynames[weekday]), ALIGN_CENTER,
			    left + x * col_width, left + (x + 1) * col_width,
			    top, top - row_height * 1.4);
		weekday = (weekday + 1) % 7;
	}

	top -= row_height * 1.4;

	now = time_month_begin (month);
	for (y = 0; y < 6; y++) {

		cell_top = top - y * row_height;
		cell_bottom = cell_top - row_height;

		for (x = 0; x < 7; x++) {

			cell_left = left + x * col_width;
			/* We add a 0.05 to make sure the cells meet up with
			   each other. Otherwise you sometimes get lines
			   between them which looks bad. Maybe I'm not using
			   coords in the way gnome-print expects. */
			cell_right = cell_left + col_width + 0.05;
			text_right = cell_right - text_xpad;

			day = days[y * 7 + x];
			if (day != 0) {
				GList *uids;

				sprintf (buf, "%d", day);

				/* this is a slow messy way to do this ... but easy ... */
				uids = cal_client_get_objects_in_range (client,
									CALOBJ_TYPE_EVENT,
									now, time_day_end (now));
				font = uids ? font_bold : font_normal;
				cal_obj_uid_list_free (uids);

				next = time_add_day (now, 1);
				if ((now >= greystart && now < greyend)
				    || (greystart >= now && greystart < next)) {
					print_border (pc,
						      cell_left, cell_right,
						      cell_top, cell_bottom,
						      -1.0, 0.75);
				}
				print_text (pc, font, buf, ALIGN_RIGHT,
					    cell_left, text_right,
					    cell_top, cell_bottom);

				now = next;
			}
		}
	}
	gtk_object_unref (GTK_OBJECT (font_normal));
	gtk_object_unref (GTK_OBJECT (font_bold));
}



/* wraps text into the print context, not taking up more than its allowed space */
static double
bound_text(GnomePrintContext *pc, GnomeFont *font, const char *text,
	   double left, double right, double top, double bottom, double indent)
{
	double maxwidth = right-left;
	double width;
	const char *p;
	char *wordstart;
	int c;
	char *outbuffer, *o, *outbuffendmarker;
	int outbufflen;
	int dump=0;
	int first=1;

	g_return_val_if_fail(text!=NULL, top);

	if (top<bottom) {
		/* too much to fit in appointment printout */
		return top;
	}

	outbufflen = 1024;
	outbuffer = g_malloc(outbufflen);
	outbuffendmarker = outbuffer+outbufflen-2;

	top -= gnome_font_get_size (font);
	gnome_print_setfont (pc, font);

	width=0;
	p = text;
	wordstart = outbuffer;
	o = outbuffer;
	while ((c=*p)) {
		if (c=='\n') {
			wordstart=o;
			dump=1;
		} else {
			/* grow output buffer if required */
			if (o>=outbuffendmarker) {
				char *newbuf;
				outbufflen*=2;
				newbuf = g_realloc(outbuffer, outbufflen);
				o = newbuf+(o-outbuffer);
				wordstart = newbuf+(o-outbuffer);
				outbuffer = newbuf;
				outbuffendmarker = outbuffer+outbufflen-2;
			}
			*o++=c;
			if (c==' ')
				wordstart = o;
			width+=gnome_font_get_glyph_width(font, gnome_font_lookup_default (font, c));
			if (width>maxwidth)
				dump=1;
			else
				dump=0;
		}
		if (dump) {
			if (wordstart==outbuffer)
				wordstart=o;
			c=*wordstart;
			*wordstart=0;
			gnome_print_moveto(pc, left, top);
			gnome_print_show(pc, outbuffer);
			*wordstart=c;
			memcpy(outbuffer, wordstart, o-wordstart);
			width = gnome_font_get_width_string_n(font, outbuffer, o-wordstart);
			o=outbuffer+(o-wordstart);
			wordstart = outbuffer;
			top -= gnome_font_get_size (font);
			if (top<bottom) {
				/* too much to fit, drop the rest */
				g_free(outbuffer);
				return top;
			}
			if (first) {
				left += indent;
				maxwidth -= indent;
				first=0;
			}
		}
		p++;
	}
	if (dump==0) {
		*o=0;
		gnome_print_moveto(pc, left, top);
		gnome_print_show(pc, outbuffer);
		top -= gnome_font_get_size (font);
	}
	g_free(outbuffer);
	return top;
}


/* Draw the borders, lines, and times down the left of the day view. */
static void
print_day_background (GnomePrintContext *pc, GnomeCalendar *gcal,
		      time_t whence, struct pdinfo *pdi,
		      double left, double right, double top, double bottom)
{
	GnomeFont *font_hour, *font_minute;
	double yinc, y;
	double width = DAY_VIEW_TIME_COLUMN_WIDTH;
	double font_size, max_font_size, hour_font_size, minute_font_size;
	char buf[20], *minute;
	gboolean use_24_hour;
	int i, hour, row;

	/* Fill the time column in light-gray. */
	print_border (pc, left, left + width, top, bottom, -1.0, 0.9);

	/* Draw the border around the entire view. */
	gnome_print_setrgbcolor (pc, 0, 0, 0);
	print_border (pc, left, right, top, bottom, 1.0, -1.0);

	/* Draw the vertical line on the right of the time column. */
	gnome_print_setlinewidth (pc, 0.0);
	gnome_print_moveto (pc, left + width, bottom);
	gnome_print_lineto (pc, left + width, top);
	gnome_print_stroke (pc);

	/* Calculate the row height. */
	yinc = (top - bottom) / (pdi->end_hour - pdi->start_hour);

	/* Get the 2 fonts we need. */
	font_size = yinc * 0.6;
	max_font_size = width * 0.5;
	font_size = MIN (font_size, max_font_size);
	font_hour    = gnome_font_new_closest ("Times", GNOME_FONT_BOLD,
					       0, font_size);
	hour_font_size = gnome_font_get_size (font_hour);

	font_size = yinc * 0.33;
	max_font_size = width * 0.25;
	font_size = MIN (font_size, max_font_size);
	font_minute  = gnome_font_new_closest ("Times", GNOME_FONT_BOOK,
					       0, font_size);
	minute_font_size = gnome_font_get_size (font_minute);

	use_24_hour = calendar_config_get_24_hour_format ();

	row = 0;
	for (i = pdi->start_hour; i < pdi->end_hour; i++) {
		y = top - yinc * (row + 1);
		gnome_print_setrgbcolor (pc, 0, 0, 0);

		if (use_24_hour) {
			hour = i;
			minute = "00";
		} else {
			if (i < 12)
				minute = _("am");
		  else
			  minute = _("pm");

			hour = i % 12;
			if (hour == 0)
				hour = 12;
		}

		/* the hour label/minute */
		sprintf (buf, "%d", hour);
		print_text (pc, font_hour, buf, ALIGN_RIGHT,
			    left, left + width * 0.58,
			    y + yinc - 4, y + yinc - 4 - hour_font_size);
		print_text (pc, font_minute, minute, ALIGN_RIGHT,
			    left, left + width - 3,
			    y + yinc - 3, y + yinc - 3 - minute_font_size);

		/* Draw the horizontal line between hours, across the entire
		   width of the day view. */
		gnome_print_moveto (pc, left, y);
		gnome_print_lineto (pc, right, y);
		gnome_print_stroke (pc);

		/* Draw the horizontal line for the 1/2-hours, across the
		   entire width except for part of the time column. */
		gnome_print_moveto (pc, left + width * 0.6, y + yinc / 2);
		gnome_print_lineto (pc, right, y + yinc / 2);
		gnome_print_stroke (pc);

		row++;
	}

	gtk_object_unref (GTK_OBJECT (font_hour));
	gtk_object_unref (GTK_OBJECT (font_minute));
}


/* This adds one event to the view, adding it to the appropriate array. */
static gint
print_day_add_event (CalComponent *comp,
		     time_t	    start,
		     time_t	    end,
		     gint	    days_shown,
		     time_t	   *day_starts,		
		     GArray	   *long_events,
		     GArray	  **events)

{
	EDayViewEvent event;
	gint day, offset;
	struct tm start_tm, end_tm;

#if 0
	g_print ("Day view lower: %s", ctime (&day_starts[0]));
	g_print ("Day view upper: %s", ctime (&day_starts[days_shown]));
	g_print ("Event start: %s", ctime (&start));
	g_print ("Event end  : %s\n", ctime (&end));
#endif

	/* Check that the event times are valid. */
	g_return_val_if_fail (start <= end, -1);
	g_return_val_if_fail (start < day_starts[days_shown], -1);
	g_return_val_if_fail (end > day_starts[0], -1);

	start_tm = *(localtime (&start));
	end_tm = *(localtime (&end));

	event.comp = comp;
	gtk_object_ref (GTK_OBJECT (comp));
	event.start = start;
	event.end = end;
	event.canvas_item = NULL;

	/* Calculate the start & end minute, relative to the top of the
	   display. */
	/*offset = day_view->first_hour_shown * 60
	  + day_view->first_minute_shown;*/
	offset = 0;
	event.start_minute = start_tm.tm_hour * 60 + start_tm.tm_min - offset;
	event.end_minute = end_tm.tm_hour * 60 + end_tm.tm_min - offset;

	event.start_row_or_col = -1;
	event.num_columns = -1;

	/* Find out which array to add the event to. */
	for (day = 0; day < days_shown; day++) {
		if (start >= day_starts[day] && end <= day_starts[day + 1]) {

			/* Special case for when the appointment ends at
			   midnight, i.e. the start of the next day. */
			if (end == day_starts[day + 1]) {

				/* If the event last the entire day, then we
				   skip it here so it gets added to the top
				   canvas. */
				if (start == day_starts[day])
				    break;

				event.end_minute = 24 * 60;
			}

			g_array_append_val (events[day], event);
			return day;
		}
	}

	/* The event wasn't within one day so it must be a long event,
	   i.e. shown in the top canvas. */
	g_array_append_val (long_events, event);
	return E_DAY_VIEW_LONG_EVENT;
}


static gboolean
print_day_details_cb (CalComponent *comp, time_t istart, time_t iend,
		      gpointer data)
{
	struct pdinfo *pdi = (struct pdinfo *)data;

	print_day_add_event (comp, istart, iend,
			     pdi->days_shown, pdi->day_starts,
			     pdi->long_events, pdi->events);

	return TRUE;
}


static void
free_event_array (GArray *array)
{
	EDayViewEvent *event;
	gint event_num;

	for (event_num = 0; event_num < array->len; event_num++) {
		event = &g_array_index (array, EDayViewEvent, event_num);
		if (event->canvas_item)
			gtk_object_destroy (GTK_OBJECT (event->canvas_item));
		gtk_object_unref (GTK_OBJECT (event->comp));
	}

	g_array_set_size (array, 0);
}


static void
print_day_long_event (GnomePrintContext *pc, GnomeFont *font,
		      double left, double right, double top, double bottom,
		      double row_height, EDayViewEvent *event,
		      struct pdinfo *pdi)
{
	CalComponentText summary;
	double x1, x2, y1, y2;
	double left_triangle_width = -1.0, right_triangle_width = -1.0;
	char *text;
	char buffer[32];
	struct tm date_tm;

	/* If the event starts before the first day being printed, draw a
	   triangle. (Note that I am assuming we are just showing 1 day at
	   the moment.) */
	if (event->start < pdi->day_starts[0])
		left_triangle_width = 4;

	/* If the event ends after the last day being printed, draw a
	   triangle. */
	if (event->end > pdi->day_starts[1])
		right_triangle_width = 4;

	x1 = left + 10;
	x2 = right - 10;
	y1 = top - event->start_row_or_col * row_height - 4;
	y2 = y1 - row_height + 4;
	print_border_with_triangles (pc, x1, x2, y1, y2, 0.0, 0.95,
				     left_triangle_width,
				     right_triangle_width);

	/* If the event starts after the first day being printed, we need to
	   print the start time. */
	if (event->start > pdi->day_starts[0]) {
		date_tm.tm_year = 2001;
		date_tm.tm_mon = 0;
		date_tm.tm_mday = 1;
		date_tm.tm_hour = event->start_minute / 60;
		date_tm.tm_min = event->start_minute % 60;
		date_tm.tm_sec = 0;
		date_tm.tm_isdst = -1;

		e_time_format_time (&date_tm, pdi->use_24_hour_format, FALSE,
				    buffer, sizeof (buffer));

		x1 += 4;
		print_text (pc, font, buffer, ALIGN_LEFT, x1, x2, y1, y2);
		x1 += gnome_font_get_width_string (font, buffer);
	}

	/* If the event ends before the end of the last day being printed,
	   we need to print the end time. */
	if (event->end < pdi->day_starts[1]) {
		date_tm.tm_year = 2001;
		date_tm.tm_mon = 0;
		date_tm.tm_mday = 1;
		date_tm.tm_hour = event->end_minute / 60;
		date_tm.tm_min = event->end_minute % 60;
		date_tm.tm_sec = 0;
		date_tm.tm_isdst = -1;

		e_time_format_time (&date_tm, pdi->use_24_hour_format, FALSE,
				    buffer, sizeof (buffer));

		x2 -= 4;
		print_text (pc, font, buffer, ALIGN_RIGHT, x1, x2, y1, y2);
		x2 -= gnome_font_get_width_string (font, buffer);
	}

	/* Print the text. */
	cal_component_get_summary (event->comp, &summary);
	text = summary.value ? (char*) summary.value : "";

	x1 += 4;
	x2 -= 4;
	print_text (pc, font, text, ALIGN_CENTER, x1, x2, y1, y2);
}


static void
print_day_event (GnomePrintContext *pc, GnomeFont *font,
		 double left, double right, double top, double bottom,
		 EDayViewEvent *event, struct pdinfo *pdi)
{
	CalComponentText summary;
	double x1, x2, y1, y2, col_width, row_height;
	int start_offset, end_offset, start_row, end_row;
	char *text, start_buffer[32], end_buffer[32];
	gboolean display_times = FALSE, free_text = FALSE;
	struct tm date_tm;

	if ((event->start_minute >= pdi->end_minute_offset)
	    || (event->end_minute <= pdi->start_minute_offset))
		return;

	start_offset = event->start_minute - pdi->start_minute_offset;
	end_offset = event->end_minute - pdi->start_minute_offset;

	start_row = start_offset / pdi->mins_per_row;
	start_row = MAX (0, start_row);
	end_row = (end_offset - 1) / pdi->mins_per_row;
	end_row = MIN (pdi->rows - 1, end_row);
	col_width = (right - left) / pdi->cols_per_row[event->start_minute / pdi->mins_per_row];

	if (start_offset != start_row * pdi->mins_per_row
	    || end_offset != (end_row + 1) * pdi->mins_per_row)
		display_times = TRUE;

	x1 = left + event->start_row_or_col * col_width;
	x2 = x1 + event->num_columns * col_width - DAY_VIEW_EVENT_X_PAD;

	row_height = (top - bottom) / pdi->rows;
	y1 = top - start_row * row_height;
	y2 = top - (end_row + 1) * row_height;

#if 0
	g_print ("Event: %g,%g %g,%g\n  row_height: %g start_row: %i top: %g rows: %i\n",
		 x1, y1, x2, y2, row_height, start_row, top, pdi->rows);
#endif

	print_border (pc, x1, x2, y1, y2, 1.0, 0.95);

	cal_component_get_summary (event->comp, &summary);
	text = summary.value ? (char*) summary.value : "";


	if (display_times) {
		date_tm.tm_year = 2001;
		date_tm.tm_mon = 0;
		date_tm.tm_mday = 1;
		date_tm.tm_hour = event->start_minute / 60;
		date_tm.tm_min = event->start_minute % 60;
		date_tm.tm_sec = 0;
		date_tm.tm_isdst = -1;

		e_time_format_time (&date_tm, pdi->use_24_hour_format, FALSE,
				    start_buffer, sizeof (start_buffer));

		date_tm.tm_hour = event->end_minute / 60;
		date_tm.tm_min = event->end_minute % 60;

		e_time_format_time (&date_tm, pdi->use_24_hour_format, FALSE,
				    end_buffer, sizeof (end_buffer));

		text = g_strdup_printf ("%s - %s %s ", start_buffer,
					end_buffer, text);

		free_text = TRUE;
	}

	bound_text (pc, font, text, x1 + 2, x2 - 2, y1, y2, 0);

	if (free_text)
		g_free (text);
}


static void
print_day_details (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		   double left, double right, double top, double bottom)
{
	CalClient *client;
	EDayViewEvent *event;
	GnomeFont *font;
	time_t start, end;
	struct pdinfo pdi;
	gint rows_in_top_display, i;
	double font_size, max_font_size;

	start = time_day_begin (whence);
	end = time_day_end (start);

	pdi.days_shown = 1;
	pdi.day_starts[0] = start;
	pdi.day_starts[1] = end;
	pdi.long_events = g_array_new (FALSE, FALSE, sizeof (EDayViewEvent));
	pdi.events[0] = g_array_new (FALSE, FALSE, sizeof (EDayViewEvent));
	pdi.start_hour = calendar_config_get_day_start_hour ();
	pdi.end_hour = calendar_config_get_day_end_hour ();
	if (calendar_config_get_day_end_minute () != 0)
		pdi.end_hour++;
	pdi.rows = (pdi.end_hour - pdi.start_hour) * 2;
	pdi.mins_per_row = 30;
	pdi.start_minute_offset = pdi.start_hour * 60;
	pdi.end_minute_offset = pdi.end_hour * 60;
	pdi.use_24_hour_format = calendar_config_get_24_hour_format ();

	/* Get the events from the server. */
	client = gnome_calendar_get_cal_client (gcal);
	cal_client_generate_instances (client, CALOBJ_TYPE_EVENT, start, end,
				       print_day_details_cb, &pdi);
	qsort (pdi.long_events->data, pdi.long_events->len,
	       sizeof (EDayViewEvent), e_day_view_event_sort_func);
	qsort (pdi.events[0]->data, pdi.events[0]->len,
	       sizeof (EDayViewEvent), e_day_view_event_sort_func);

	/* Lay them out the long events, across the top of the page. */
	e_day_view_layout_long_events (pdi.long_events, pdi.days_shown,
				       pdi.day_starts, &rows_in_top_display);

	/* Print the long events. */
	font = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0, 12);
	for (i = 0; i < pdi.long_events->len; i++) {
		event = &g_array_index (pdi.long_events, EDayViewEvent, i);
		print_day_long_event (pc, font, left, right, top, bottom,
				      DAY_VIEW_ROW_HEIGHT, event, &pdi);
	}
	gtk_object_unref (GTK_OBJECT (font));

	/* We always leave space for DAY_VIEW_MIN_ROWS_IN_TOP_DISPLAY in the
	   top display, but we may have more rows than that, in which case
	   the main display area will be compressed. */
	rows_in_top_display = MAX (rows_in_top_display,
				   DAY_VIEW_MIN_ROWS_IN_TOP_DISPLAY);

	/* Draw the border around the long events. */
	gnome_print_setrgbcolor (pc, 0, 0, 0);
	print_border (pc, left, right,
		      top, top - rows_in_top_display * DAY_VIEW_ROW_HEIGHT - 4,
		      1.0, -1.0);

	/* Adjust the area containing the main display. */
	top -= rows_in_top_display * DAY_VIEW_ROW_HEIGHT + 4;

	/* Draw the borders, lines, and times down the left. */
	print_day_background (pc, gcal, whence, &pdi,
			      left, right, top, bottom);

	/* Now adjust to get rid of the time column. */
	left += DAY_VIEW_TIME_COLUMN_WIDTH;

	/* Lay out the short events, within the day. */
	e_day_view_layout_day_events (pdi.events[0], DAY_VIEW_ROWS,
				      DAY_VIEW_MINS_PER_ROW, pdi.cols_per_row);

	/* Print the short events. */
	max_font_size = ((top - bottom) / pdi.rows) - 4;
	font_size = MIN (DAY_NORMAL_FONT_SIZE, max_font_size);
	font = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0, font_size);
	for (i = 0; i < pdi.events[0]->len; i++) {
		event = &g_array_index (pdi.events[0], EDayViewEvent, i);
		print_day_event (pc, font, left, right, top, bottom,
				 event, &pdi);
	}
	gtk_object_unref (GTK_OBJECT (font));

	/* Free everything. */
	free_event_array (pdi.long_events);
	g_array_free (pdi.long_events, TRUE);
	free_event_array (pdi.events[0]);
	g_array_free (pdi.events[0], TRUE);
}


/*
 * Print Day Summary
 */
#if 0
#define TIME_FMT "%X"
#else
#define TIME_FMT "%l:%M%p"
#endif

#if 0
static gboolean
print_day_summary_cb (CalComponent *comp, time_t istart, time_t iend, gpointer data)
{
	CalComponentText text;
	struct psinfo *psi = (struct psinfo *)data;
	struct einfo *ei;

	ei = g_new (struct einfo, 1);

	cal_component_get_summary (comp, &text);
	ei->text = g_strdup (text.value);

	ei->start = istart;
	ei->end = iend;
	ei->count = 0;

	psi->events = g_list_append (psi->events, ei);

	return TRUE;
}

static void
print_day_summary (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		   double left, double right, double top, double bottom,
		   double size, int totime, int titleformat)
{
	CalClient *client;
	struct psinfo psi;
	time_t start, end;
	GList *l;
	GnomeFont *font_summary;
	double y, yend, x, xend, inc, incsmall;
	char buf[100];
	double margin;
	struct tm tm;

	client = gnome_calendar_get_cal_client (gcal);

	/* fill static detail */
	font_summary = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0, size);

	gnome_print_setfont (pc, font_summary);

	start = time_day_begin(whence);
	end = time_day_end(start);

	tm = *localtime(&start);

	format_date(start, titleformat, buf, 100);
	titled_box (pc, buf, font_summary, ALIGN_RIGHT | ALIGN_BORDER,
		    &left, &right, &top, &bottom, 0.0);

	psi.events = NULL;

	cal_client_generate_instances (client, CALOBJ_TYPE_EVENT, start, end,
				       print_day_summary_cb, &psi);
	inc = size*0.3;
	incsmall = size*0.2;

	y = top-inc;
	yend = bottom-incsmall;

	/* do a good rough approximation of the 'widest' time */
	tm.tm_year = 2000;
	tm.tm_mon = 12;
	tm.tm_mday = 22;
	tm.tm_sec = 22;
	tm.tm_min = 22;
	tm.tm_hour = 23;
	strftime(buf, 100, TIME_FMT, &tm);
	margin = gnome_font_get_width_string(font_summary, buf);

	for (l = psi.events; l; l = l->next) {
		struct einfo *ei = (struct einfo *)l->data;

		x = left + incsmall;
		xend = right - inc;

		if (y - gnome_font_get_size (font_summary) < bottom)
			break;

		tm = *localtime (&ei->start);
		strftime (buf, 100, TIME_FMT, &tm);
		gnome_print_moveto (pc, x + (margin
					     - gnome_font_get_width_string (font_summary, buf)),
				    y - gnome_font_get_size (font_summary));
		gnome_print_show (pc, buf);

		if (totime) {
			tm = *localtime (&ei->end);
			strftime (buf, 100, TIME_FMT, &tm);
			gnome_print_moveto (pc,
					    (x + margin + inc
					     + (margin
						- gnome_font_get_width_string (font_summary, buf))),
					    y - gnome_font_get_size (font_summary));
			gnome_print_show (pc, buf);

			y = bound_text (pc, font_summary, ei->text,
					x + margin * 2 + inc * 2, xend,
					y, yend, 0);
		} else {
			/* we also indent back after each time is printed */
			y = bound_text (pc, font_summary, ei->text,
					x + margin + inc, xend,
					y, yend, -margin + inc);
		}

		y += gnome_font_get_size (font_summary) - inc;

		g_free (ei);
	}
	g_list_free (psi.events);

	gtk_object_unref (GTK_OBJECT (font_summary));
}
#endif

/* This adds one event to the view, adding it to the appropriate array. */
static gboolean
print_week_summary_cb (CalComponent *comp,
		       time_t	  start,
		       time_t	  end,
		       gpointer	  data)

{
	EWeekViewEvent event;
	struct tm start_tm, end_tm;

	struct psinfo *psi = (struct psinfo *)data;

	/* Check that the event times are valid. */

#if 0
	g_print ("View start:%li end:%li  Event start:%li end:%li\n",
		 psi->day_starts[0], psi->day_starts[psi->days_shown],
		 start, end);
#endif

	g_return_val_if_fail (start <= end, TRUE);
	g_return_val_if_fail (start < psi->day_starts[psi->days_shown], TRUE);
	g_return_val_if_fail (end > psi->day_starts[0], TRUE);

	start_tm = *(localtime (&start));
	end_tm = *(localtime (&end));

	event.comp = comp;
	gtk_object_ref (GTK_OBJECT (event.comp));
	event.start = start;
	event.end = end;
	event.spans_index = 0;
	event.num_spans = 0;

	event.start_minute = start_tm.tm_hour * 60 + start_tm.tm_min;
	event.end_minute = end_tm.tm_hour * 60 + end_tm.tm_min;
	if (event.end_minute == 0 && start != end)
		event.end_minute = 24 * 60;

	g_array_append_val (psi->events, event);

	return TRUE;
}


/* Returns TRUE if the event is a one-day event (i.e. not a long event). */
static gboolean
print_is_one_day_week_event (EWeekViewEvent *event,
			     EWeekViewEventSpan *span,
			     time_t *day_starts)
{
	if (event->start == day_starts[span->start_day]
	    && event->end == day_starts[span->start_day + 1])
		return FALSE;

	if (span->num_days == 1
	    && event->start >= day_starts[span->start_day]
	    && event->end <= day_starts[span->start_day + 1])
		return TRUE;

	return FALSE;
}


static void
print_week_long_event (GnomePrintContext *pc, GnomeFont *font,
		       struct psinfo *psi,
		       double x1, double x2, double y1, double y2,
		       EWeekViewEvent *event, EWeekViewEventSpan *span,
		       char *text)
{
	double left_triangle_width = -1.0, right_triangle_width = -1.0;
	struct tm date_tm;
	char buffer[32];

	/* If the event starts before the first day of the span, draw a
	   triangle to indicate it continues. */
	if (event->start < psi->day_starts[span->start_day])
		left_triangle_width = 4;

	/* If the event ends after the last day of the span, draw a
	   triangle. */
	if (event->end > psi->day_starts[span->start_day + span->num_days])
		right_triangle_width = 4;

	print_border_with_triangles (pc, x1, x2, y1, y2, 0.5, 0.9,
				     left_triangle_width,
				     right_triangle_width);

	/* If the event starts after the first day being printed, we need to
	   print the start time. */
	if (event->start > psi->day_starts[span->start_day]) {
		date_tm.tm_year = 2001;
		date_tm.tm_mon = 0;
		date_tm.tm_mday = 1;
		date_tm.tm_hour = event->start_minute / 60;
		date_tm.tm_min = event->start_minute % 60;
		date_tm.tm_sec = 0;
		date_tm.tm_isdst = -1;

		e_time_format_time (&date_tm, psi->use_24_hour_format, FALSE,
				    buffer, sizeof (buffer));

		x1 += 4;
		print_text (pc, font, buffer, ALIGN_LEFT, x1, x2, y1, y2);
		x1 += gnome_font_get_width_string (font, buffer);
	}

	/* If the event ends before the end of the last day being printed,
	   we need to print the end time. */
	if (event->end < psi->day_starts[span->start_day + span->num_days]) {
		date_tm.tm_year = 2001;
		date_tm.tm_mon = 0;
		date_tm.tm_mday = 1;
		date_tm.tm_hour = event->end_minute / 60;
		date_tm.tm_min = event->end_minute % 60;
		date_tm.tm_sec = 0;
		date_tm.tm_isdst = -1;

		e_time_format_time (&date_tm, psi->use_24_hour_format, FALSE,
				    buffer, sizeof (buffer));

		x2 -= 4;
		print_text (pc, font, buffer, ALIGN_RIGHT, x1, x2, y1, y2);
		x2 -= gnome_font_get_width_string (font, buffer);
	}

	x1 += 4;
	x2 -= 4;
	print_text (pc, font, text, ALIGN_CENTER, x1, x2, y1, y2);
}


static void
print_week_day_event (GnomePrintContext *pc, GnomeFont *font,
		      struct psinfo *psi,
		      double x1, double x2, double y1, double y2,
		      EWeekViewEvent *event, EWeekViewEventSpan *span,
		      char *text)
{
	struct tm date_tm;
	char buffer[32];

	date_tm.tm_year = 2001;
	date_tm.tm_mon = 0;
	date_tm.tm_mday = 1;
	date_tm.tm_hour = event->start_minute / 60;
	date_tm.tm_min = event->start_minute % 60;
	date_tm.tm_sec = 0;
	date_tm.tm_isdst = -1;

	e_time_format_time (&date_tm, psi->use_24_hour_format, FALSE,
			    buffer, sizeof (buffer));

	print_text (pc, font, buffer, ALIGN_LEFT, x1, x2, y1, y2);
	x1 += gnome_font_get_width_string (font, buffer);

	x1 += 4;
	print_text (pc, font, text, ALIGN_LEFT, x1, x2, y1, y2);
}


static void
print_week_event (GnomePrintContext *pc, GnomeFont *font,
		  struct psinfo *psi,
		  double left, double top,
		  double cell_width, double cell_height,
		  EWeekViewEvent *event, GArray *spans)
{
	EWeekViewEventSpan *span;
	gint span_num;
	CalComponentText summary;
	char *text;
	int num_days, start_x, start_y, start_h, end_x, end_y, end_h;
	double x1, x2, y1, y2;

	cal_component_get_summary (event->comp, &summary);
	text = summary.value ? (char*) summary.value : "";

	for (span_num = 0; span_num < event->num_spans; span_num++) {
		span = &g_array_index (spans, EWeekViewEventSpan,
				       event->spans_index + span_num);

		if (e_week_view_layout_get_span_position
		    (event, span,
		     psi->rows_per_cell,
		     psi->rows_per_compressed_cell,
		     psi->display_start_weekday,
		     psi->multi_week_view,
		     psi->compress_weekend,
		     &num_days)) {

			e_week_view_layout_get_day_position
				(span->start_day,
				 psi->multi_week_view,
				 psi->weeks_shown,
				 psi->display_start_weekday,
				 psi->compress_weekend,
				 &start_x, &start_y, &start_h);

			if (num_days == 1) {
				end_x = start_x;
				end_y = start_y;
				end_h = start_h;
			} else {
				e_week_view_layout_get_day_position
					(span->start_day + num_days - 1,
					 psi->multi_week_view,
					 psi->weeks_shown,
					 psi->display_start_weekday,
					 psi->compress_weekend,
					 &end_x, &end_y, &end_h);
			}

			x1 = left + start_x * cell_width + 6;
			x2 = left + (end_x + 1) * cell_width - 6;
			y1 = top - start_y * cell_height
				- psi->header_row_height
				- span->row * psi->row_height;
			y2 = y1 - psi->row_height * 0.9;

			if (print_is_one_day_week_event (event, span,
							 psi->day_starts)) {
				print_week_day_event (pc, font, psi,
						      x1, x2, y1, y2,
						      event, span, text);
			} else {
				print_week_long_event (pc, font, psi,
						       x1, x2, y1, y2,
						       event, span, text);
			}
		}
	}
}


static void
print_week_view_background (GnomePrintContext *pc, GnomeFont *font,
			    struct psinfo *psi,
			    double left, double top,
			    double cell_width, double cell_height)
{
	int day, day_x, day_y, day_h;
	double x1, x2, y1, y2, font_size, fillcolor;
	struct tm tm;
	char *format_string, buffer[128];

	font_size = gnome_font_get_size (font);

	for (day = 0; day < psi->days_shown; day++) {
		e_week_view_layout_get_day_position
			(day, psi->multi_week_view, psi->weeks_shown,
			 psi->display_start_weekday, psi->compress_weekend,
			 &day_x, &day_y, &day_h);

		x1 = left + day_x * cell_width;
		x2 = left + (day_x + 1) * cell_width;
		y1 = top - day_y * cell_height;
		y2 = y1 - day_h * cell_height;

		tm = *localtime (&psi->day_starts[day]);

		/* In the month view we draw a grey background for the end
		   of the previous month and the start of the following. */
		fillcolor = -1.0;
		if (psi->multi_week_view && (tm.tm_mon != psi->month))
			fillcolor = 0.9;

		print_border (pc, x1, x2, y1, y2, 1.0, fillcolor);

		if (psi->multi_week_view) {
			if (tm.tm_mday == 1)
				format_string = _("%d %B");
			else
				format_string = "%d";
		} else {
			gnome_print_moveto (pc, x1 + 0.1 * cell_width,
					    y1 - psi->header_row_height + 3);
			gnome_print_lineto (pc, x2,
					    y1 - psi->header_row_height + 3);
			gnome_print_setrgbcolor (pc, 0, 0, 0);
			gnome_print_setlinewidth (pc, 0.0);
			gnome_print_stroke (pc);

			/* strftime format %A = full weekday name, %d = day of
			   month, %B = full month name. You can change the
			   order but don't change the specifiers or add
			   anything. */
			format_string = _("%A %d %B");

		}

		strftime (buffer, sizeof (buffer), format_string, &tm);
		print_text (pc, font, buffer, ALIGN_RIGHT,
			    x1, x2 - 4, y1 - 2, y1 - 2 - font_size);
	}
}



static void
print_week_summary (GnomePrintContext *pc, GnomeCalendar *gcal,
		    time_t whence, gboolean multi_week_view, int weeks_shown,
		    int month, double font_size,
		    double left, double right, double top, double bottom)
{
	CalClient *client;
	EWeekViewEvent *event;
	struct psinfo psi;
	time_t day_start;
	gint rows_per_day[E_WEEK_VIEW_MAX_WEEKS * 7], day, event_num;
	GArray *spans;
	GnomeFont *font;
	double cell_width, cell_height;

	psi.days_shown = weeks_shown * 7;
	psi.events = g_array_new (FALSE, FALSE, sizeof (EWeekViewEvent));
	psi.multi_week_view = multi_week_view;
	psi.weeks_shown = weeks_shown;
	psi.month = month;

	/* Get a few config settings. */
	psi.compress_weekend = calendar_config_get_compress_weekend ();
	psi.use_24_hour_format = calendar_config_get_24_hour_format ();

	/* We convert this from (0 = Sun, 6 = Sat) to (0 = Mon, 6 = Sun). */
	psi.display_start_weekday = calendar_config_get_week_start_day ();
	psi.display_start_weekday = (psi.display_start_weekday + 6) % 7;

	/* If weekends are compressed then we can't start on a Sunday. */
	if (psi.compress_weekend && psi.display_start_weekday == 6)
		psi.display_start_weekday = 5;

	day_start = time_day_begin (whence);
	for (day = 0; day <= psi.days_shown; day++) {
		psi.day_starts[day] = day_start;
		day_start = time_add_day (day_start, 1);
	}

	/* Get the events from the server. */
	client = gnome_calendar_get_cal_client (gcal);
	cal_client_generate_instances (client, CALOBJ_TYPE_EVENT,
				       psi.day_starts[0],
				       psi.day_starts[psi.days_shown],
				       print_week_summary_cb, &psi);
	qsort (psi.events->data, psi.events->len,
	       sizeof (EWeekViewEvent), e_week_view_event_sort_func);

	/* Layout the events. */
	spans = e_week_view_layout_events (psi.events, NULL,
					   psi.multi_week_view,
					   psi.weeks_shown,
					   psi.compress_weekend,
					   psi.display_start_weekday,
					   psi.day_starts, rows_per_day);

	/* Calculate the size of the cells. */
	if (multi_week_view) {
		cell_width = (right - left) / (psi.compress_weekend ? 6 : 7);
		cell_height = (top - bottom) / (weeks_shown * 2);
	} else {
		cell_width = (right - left) / 2;
		cell_height = (top - bottom) / 6;
	}

	/* Calculate the row height, using the normal font and with room for
	   space or a rectangle around it. */
	psi.row_height = font_size * 1.2;
	psi.header_row_height = font_size * 1.5;

	/* Calculate how many rows we can fit into each type of cell. */
	psi.rows_per_cell = ((cell_height * 2) - psi.header_row_height)
		/ psi.row_height;
	psi.rows_per_compressed_cell = (cell_height - psi.header_row_height)
		/ psi.row_height;

	font = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0, font_size);

	/* Draw the grid and the day names/numbers. */
	print_week_view_background (pc, font, &psi, left, top,
				    cell_width, cell_height);

	/* Print the events. */
	for (event_num = 0; event_num < psi.events->len; event_num++) {
		event = &g_array_index (psi.events, EWeekViewEvent, event_num);
		print_week_event (pc, font, &psi, left, top,
				  cell_width, cell_height, event, spans);
	}

	gtk_object_unref (GTK_OBJECT (font));

	/* Free everything. */
	for (event_num = 0; event_num < psi.events->len; event_num++) {
		event = &g_array_index (psi.events, EWeekViewEvent, event_num);
		gtk_object_unref (GTK_OBJECT (event->comp));
	}
	g_array_free (psi.events, TRUE);
	g_array_free (spans, TRUE);
}


static void
print_year_summary (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		    double left, double right, double top, double bottom,
		    int morerows)
{
	double row_height, col_width, l, r, t, b;
	time_t now;
	int col, row, rows, cols;

	l = left;
	t = top;

	/* If morerows is set we do 4 rows and 3 columns instead of 3 rows and
	   4 columns. This is useful if we switch paper orientation. */
	if (morerows) {
		rows = 4;
		cols = 3;
	} else {
		rows = 3;
		cols = 4;
	}

	row_height = (top - bottom) / rows;
	col_width = (right - left) / cols;
	r = l + col_width;
	b = top - row_height;
	now = time_year_begin (whence);

	for (row = 0; row < rows; row++) {
		t = top - row_height * row;
		b = t - row_height;
		for (col = 0; col < cols; col++) {
			l = left + col_width * col;
			r = l + col_width;
			print_month_small (pc, gcal, now,
					   l + 8, r - 8, t - 8, b + 8,
					   DATE_MONTH, 0, 0, TRUE);
			now = time_add_month (now, 1);
		}
	}
}

static void
print_month_summary (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		     double left, double right, double top, double bottom)
{
	time_t date;
	struct tm tm;
	char buffer[100];
	GnomeFont *font;
	gboolean compress_weekend;
	int columns, col, weekday, len, month;
	double font_size, cell_width, x1, x2, y1, y2;

	weekday = calendar_config_get_week_start_day ();
	compress_weekend = calendar_config_get_compress_weekend ();

	/* Remember which month we want. */
	tm = *localtime (&whence);
	month = tm.tm_mon;

	/* Find the start of the month, and then the start of the week on
	   or before that day. */
	date = time_month_begin (whence);
	date = time_week_begin (date, weekday);

	/* If weekends are compressed then we can't start on a Sunday. */
	if (compress_weekend && weekday == 0)
		date = time_add_day (date, -1);

        tm = *localtime (&date);

	/* do day names ... */
	font = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 0,
				       MONTH_NORMAL_FONT_SIZE);
	font_size = gnome_font_get_size (font);
	gnome_print_setfont (pc, font);

	columns = compress_weekend ? 6 : 7;
	cell_width = (right - left) / columns;
	y1 = top;
	y2 = top - font_size * 1.5;

	for (col = 0; col < columns; col++) {
		if (tm.tm_wday == 6 && compress_weekend) {
			strftime (buffer, sizeof (buffer), "%a/", &tm);
			len = strlen (buffer);
			tm.tm_mday++;
			mktime (&tm);
			strftime (buffer + len, sizeof (buffer) - len,
				  "%a", &tm);
		} else {
			strftime (buffer, sizeof (buffer), "%A", &tm);
		}

		x1 = left + cell_width * col;
		x2 = x1 + cell_width;

		print_border (pc, x1, x2, y1, y2, 1.0, -1.0);
		print_text (pc, font, buffer, ALIGN_CENTER, x1, x2, y1, y2);

		tm.tm_mday++;
		mktime (&tm);
	}
	gtk_object_unref (GTK_OBJECT (font));

	top = y2;
	print_week_summary (pc, gcal, date, TRUE, 6, month,
			    MONTH_NORMAL_FONT_SIZE,
			    left, right, top, bottom);
}

static void
print_todo_details (GnomePrintContext *pc, GnomeCalendar *gcal,
		    time_t start, time_t end,
		    double left, double right, double top, double bottom)
{
	CalClient *client;
	GList *uids;
	GList *l;
	GnomeFont *font_summary;
	double y, yend, x, xend;
	struct icaltimetype *tt;

	client = gnome_calendar_get_task_pad_cal_client (gcal);

	font_summary = gnome_font_new_closest ("Times", GNOME_FONT_BOOK,
					       0, 10);

	gnome_print_setrgbcolor (pc, 0, 0, 0);
	gnome_print_setlinewidth (pc, 0.0);

	titled_box (pc, _("Tasks"), font_summary, ALIGN_CENTER | ALIGN_BORDER,
		    &left, &right, &top, &bottom, 1.0);

	y = top - 3;
	yend = bottom - 2;

	uids = cal_client_get_uids (client, CALOBJ_TYPE_TODO);

	for (l = uids; l; l = l->next) {
		char *uid;
		CalComponent *comp;
		CalClientGetStatus status;
		CalComponentText summary;

		uid = l->data;

		status = cal_client_get_object (client, uid, &comp);

		switch (status) {
		case CAL_CLIENT_GET_SUCCESS:
			break;

		case CAL_CLIENT_GET_NOT_FOUND:
			/* Nothing: the object may have been removed from the server */
			continue;

		case CAL_CLIENT_GET_SYNTAX_ERROR:
			g_message ("print_todo_details(): Syntax error while getting object `%s'",
				   uid);
			continue;

		default:
			g_assert_not_reached ();
		}

		cal_component_get_summary (comp, &summary);

		if (!summary.value)
			continue;

		x = left;
		xend = right - 2;

		if (y < bottom)
			break;

		/* Print the box to put the tick in. */
		print_border (pc, x + 2, x + 8, y - 3, y - 11, 0.0, -1.0);

		/* If the task is complete, print a tick in the box. */
		cal_component_get_completed (comp, &tt);
		if (tt) {
			cal_component_free_icaltimetype (tt);

			gnome_print_setrgbcolor (pc, 0, 0, 0);
			gnome_print_setlinewidth (pc, 1.0);
			gnome_print_moveto (pc, x + 3, y - 8);
			gnome_print_lineto (pc, x + 5, y - 10);
			gnome_print_lineto (pc, x + 7, y - 3.5);
			gnome_print_stroke (pc);
		}

		y = bound_text (pc, font_summary, summary.value,
				x + 10, xend, y, yend, 0);
		y += gnome_font_get_size (font_summary) - 6;
		gnome_print_moveto (pc, x, y);
		gnome_print_lineto (pc, xend, y);
		gnome_print_stroke (pc);
		y -= 3;
	}

	cal_obj_uid_list_free (uids);

	gtk_object_unref (GTK_OBJECT (font_summary));
}


/* Value for the PrintView enum */
static const int print_view_map[] = {
	PRINT_VIEW_DAY,
	PRINT_VIEW_WEEK,
	PRINT_VIEW_MONTH,
	PRINT_VIEW_YEAR,
	-1
};

/* Creates the range selector widget for printing a calendar */
static GtkWidget *
range_selector_new (GtkWidget *dialog, time_t at, int *view)
{
	GtkWidget *box;
	GtkWidget *radio;
	GSList *group;
	char text[1024];
	char str1[512];
	char str2[512];
	struct tm tm;
	time_t week_begin, week_end;
	struct tm week_begin_tm, week_end_tm;
	gint week_start_day;

	box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);

	tm = *localtime (&at);

	/* Day */

	strftime (text, sizeof (text), _("Current day (%a %b %d %Y)"), &tm);
	radio = gtk_radio_button_new_with_label (NULL, text);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
	gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

	/* Week */

	week_start_day = calendar_config_get_week_start_day ();
	week_begin = time_week_begin (at, week_start_day);
	/* If the week starts on a Sunday, we have to show the Saturday first,
	   since the weekend is compressed. */
	if (week_start_day == 0)
		week_begin = time_add_day (week_begin, -1);
	week_end = time_add_day (week_end, 6);

	week_begin_tm = *localtime (&week_begin);
	week_end_tm = *localtime (&week_end);

	if (week_begin_tm.tm_mon == week_end_tm.tm_mon) {
		strftime (str1, sizeof (str1), _("%a %b %d"), &week_begin_tm);
		strftime (str2, sizeof (str2), _("%a %d %Y"), &week_end_tm);
	} else {
		if (week_begin_tm.tm_year == week_end_tm.tm_year) {
			strftime (str1, sizeof (str1), _("%a %b %d"), &week_begin_tm);
			strftime (str2, sizeof (str2), _("%a %b %d %Y"), &week_end_tm);
		} else {
			strftime (str1, sizeof (str1), _("%a %b %d %Y"), &week_begin_tm);
			strftime (str2, sizeof (str2), _("%a %b %d %Y"), &week_end_tm);
		}
	}

	g_snprintf (text, sizeof (text), _("Current week (%s - %s)"), str1, str2);

	radio = gtk_radio_button_new_with_label (group, text);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
	gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

	/* Month */

	strftime (text, sizeof (text), _("Current month (%b %Y)"), &tm);
	radio = gtk_radio_button_new_with_label (group, text);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
	gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

	/* Year */

	strftime (text, sizeof (text), _("Current year (%Y)"), &tm);
	radio = gtk_radio_button_new_with_label (group, text);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
	gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

	/* Select default */

	e_dialog_widget_hook_value (dialog, radio, view, (gpointer) print_view_map);

	gtk_widget_show_all (box);
	return box;
}


static void
print_day_view (GnomePrintContext *pc, GnomeCalendar *gcal, time_t date,
		double left, double right, double top, double bottom)
{
	int i, days = 1;
	double todo, header, l;
	char buf[100];

	for (i = 0; i < days; i++) {
		todo = (right - left) * 0.75 + left;
		header = top - HEADER_HEIGHT;

		/* FIXME: What is the name supposed to be for? */
		gnome_print_beginpage (pc, "Calendar Day View");

		/* Print the main view with all the events in. */
		print_day_details (pc, gcal, date,
				   left, todo - 2.0, header, bottom);

		/* Print the TaskPad down the right. */
		print_todo_details (pc, gcal, 0, INT_MAX,
				    todo, right, header, bottom);

		/* Print the filled border around the header. */
		print_border (pc, left, right, top, header + 2.0, 1.0, 0.9);

		/* Print the 2 mini calendar-months. */
		l = right - SMALL_MONTH_PAD - SMALL_MONTH_WIDTH * 2
			- SMALL_MONTH_SPACING;
		print_month_small (pc, gcal, date,
				   l, l + SMALL_MONTH_WIDTH,
				   top - 4, header + 4,
				   DATE_MONTH | DATE_YEAR, date, date, FALSE);

		l += SMALL_MONTH_SPACING + SMALL_MONTH_WIDTH;
		print_month_small (pc, gcal, time_add_month (date, 1),
				   l, l + SMALL_MONTH_WIDTH,
				   top - 4, header + 4,
				   DATE_MONTH | DATE_YEAR, 0, 0, FALSE);

		/* Print the date, e.g. '8th May, 2001'. */
		format_date (date, DATE_DAY | DATE_MONTH | DATE_YEAR,
			     buf, 100);
		print_text_size (pc, 24, buf, ALIGN_LEFT,
				 left + 4, todo, top - 4, top - 4 - 24);

		/* Print the day, e.g. 'Tuesday'. */
		format_date (date, DATE_DAYNAME, buf, 100);
		print_text_size (pc, 18, buf, ALIGN_LEFT,
				 left + 4, todo, top - 32, top - 32 - 18);

		gnome_print_showpage (pc);
		date = time_add_day (date, 1);
	}
}


static void
print_week_view (GnomePrintContext *pc, GnomeCalendar *gcal, time_t date,
		 double left, double right, double top, double bottom)
{
	double header, l;
	char buf[100];
	time_t when;
	gint week_start_day;

	header = top - HEADER_HEIGHT;

	/* FIXME: What is the name supposed to be for? */
	gnome_print_beginpage (pc, "Calendar Week View");

	week_start_day = calendar_config_get_week_start_day ();
	when = time_week_begin (date, week_start_day);
	/* If the week starts on a Sunday, we have to show the Saturday first,
	   since the weekend is compressed. */
	if (week_start_day == 0)
		when = time_add_day (when, -1);

	/* Print the main week view. */
	print_week_summary (pc, gcal, when, FALSE, 1, 0,
			    WEEK_NORMAL_FONT_SIZE,
			    left, right, header, bottom);

	/* Print the border around the main view. */
	print_border (pc, left, right, header, bottom, 1.0, -1.0);

	/* Print the border around the header area. */
	print_border (pc, left, right, top, header + 2.0, 1.0, 0.9);

	/* Print the 2 mini calendar-months. */
	l = right - SMALL_MONTH_PAD - SMALL_MONTH_WIDTH * 2
		- SMALL_MONTH_SPACING;
	print_month_small (pc, gcal, when,
			   l, l + SMALL_MONTH_WIDTH,
			   top - 4, header + 4,
			   DATE_MONTH | DATE_YEAR, when,
			   time_add_week (when, 1), FALSE);

	l += SMALL_MONTH_SPACING + SMALL_MONTH_WIDTH;
	print_month_small (pc, gcal, time_add_month (when, 1),
			   l, l + SMALL_MONTH_WIDTH,
			   top - 4, header + 4,
			   DATE_MONTH | DATE_YEAR, when,
			   time_add_week (when, 1), FALSE);

	/* Print the start day of the week, e.g. '7th May 2001'. */
	format_date (when, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
	print_text_size (pc, 24, buf, ALIGN_LEFT,
			 left + 3, right, top - 4, top - 4 - 24);

	/* Print the end day of the week, e.g. '13th May 2001'. */
	when = time_add_day (when, 6);
	format_date (when, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
	print_text_size (pc, 24, buf, ALIGN_LEFT,
			 left + 3, right, top - 24 - 3, top - 24 - 3 - 24);

	gnome_print_showpage (pc);
}


static void
print_month_view (GnomePrintContext *pc, GnomeCalendar *gcal, time_t date,
		  double left, double right, double top, double bottom)
{
	double header;
	char buf[100];

	header = top - HEADER_HEIGHT;

	/* FIXME: What is the name supposed to be for? */
	gnome_print_beginpage (pc, "Calendar Month View");

	/* Print the main month view. */
	print_month_summary (pc, gcal, date, left, right, header, bottom);

	/* Print the border around the header. */
	print_border (pc, left, right, top, header, 1.0, 0.9);

	/* Print the 2 mini calendar-months. */
	print_month_small (pc, gcal, time_add_month (date, 1),
			   right - (right - left) / 7 + 2, right - 8,
			   top - 4, header,
			   DATE_MONTH | DATE_YEAR, 0, 0, FALSE);
	print_month_small (pc, gcal, time_add_month (date, -1),
			   left + 8, left + (right - left) / 7 - 2,
			   top - 4, header,
			   DATE_MONTH | DATE_YEAR, 0, 0, FALSE);

	/* Print the month, e.g. 'May 2001'. */
	format_date (date, DATE_MONTH | DATE_YEAR, buf, 100);
	print_text_size (pc, 24, buf, ALIGN_CENTER,
			 left + 3, right - 3, top - 3, top - 3 - 24);

	gnome_print_showpage (pc);
}


static void
print_year_view (GnomePrintContext *pc, GnomeCalendar *gcal, time_t date,
		 double left, double right, double top, double bottom)
{
	char buf[100];

	/* FIXME: What is the name supposed to be for? */
	gnome_print_beginpage (pc, "Calendar Year View");

	print_year_summary (pc, gcal, date, left, right, top - 50, bottom,
			    TRUE);

	/* centered title */
	format_date (date, DATE_YEAR, buf, 100);
	print_text_size (pc, 24, buf, ALIGN_CENTER,
			 left+3, right, top-3, top - 27);

	gnome_print_showpage (pc);
}

static void
write_label_piece (time_t t, char *buffer, int size, char *stext, char *etext)
{
	struct tm *tmp_tm;
	int len;
	
	tmp_tm = localtime (&t);
	if (stext != NULL)
		strcat (buffer, stext);

	len = strlen (buffer);
	e_time_format_date_and_time (tmp_tm,
				     calendar_config_get_24_hour_format (), 
				     FALSE, FALSE,
				     &buffer[len], size - len);
	if (etext != NULL)
		strcat (buffer, etext);
}

static void
print_date_label (GnomePrintContext *pc, CalComponent *comp,
		  double left, double right, double top, double bottom)
{
	CalComponentDateTime datetime;
	time_t start = 0, end = 0, complete = 0, due = 0;
	static char buffer[1024];

	cal_component_get_dtstart (comp, &datetime);
	if (datetime.value)
		start = icaltime_as_timet (*datetime.value);
	cal_component_get_dtend (comp, &datetime);
	if (datetime.value)
		end = icaltime_as_timet (*datetime.value);
	cal_component_get_due (comp, &datetime);
	if (datetime.value)
		due = icaltime_as_timet (*datetime.value);
	cal_component_get_completed (comp, &datetime.value);
	if (datetime.value)
		complete = icaltime_as_timet (*datetime.value);

	buffer[0] = '\0';

	if (start > 0)
		write_label_piece (start, buffer, 1024, NULL, NULL);

	if (end > 0 && start > 0)
		write_label_piece (end, buffer, 1024, _(" to "), NULL);

	if (complete > 0) {
		if (start > 0)
			write_label_piece (complete, buffer, 1024, _(" (Completed "), ")");
		else
			write_label_piece (complete, buffer, 1024, _("Completed "), NULL);
	}
	
	if (due > 0 && complete == 0) {
		if (start > 0)
			write_label_piece (due, buffer, 1024, _(" (Due "), ")");
		else
			write_label_piece (due, buffer, 1024, _("Due "), NULL);
	}

	print_text_size (pc, 12, buffer, ALIGN_LEFT,
			 left, right, top, top - 15);
}

static void
print_comp_item (GnomePrintContext *pc, CalComponent *comp,
		 double left, double right, double top, double bottom)
{
	GnomeFont *font;
	CalComponentVType vtype;
	CalComponentText text;
	GSList *desc, *l;
	
	vtype = cal_component_get_vtype (comp);
	
	switch (vtype) {
	case CAL_COMPONENT_EVENT:
	case CAL_COMPONENT_TODO:
		/* Summary */
		cal_component_get_summary (comp, &text);
		print_text_size (pc, 18, text.value, ALIGN_LEFT,
				 left+3, right, top-3, top - 21);
		top -= 21;
	
		/* Date information */
		print_date_label (pc, comp, left+3, right, top-3, top - 15);
		top -= 30;

		/* Description */
		font = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, FALSE, 12);
		cal_component_get_description_list (comp, &desc);
		for (l = desc; l != NULL; l = l->next) {
			CalComponentText *text = l->data;
		
			if (text->value != NULL)
				top = bound_text (pc, font, text->value, left, right, top-3, bottom, 3);
		}
		cal_component_free_text_list (desc);

		break;
	default:
	}

	gnome_print_showpage (pc);
}

void
print_calendar (GnomeCalendar *gcal, gboolean preview, time_t date,
		PrintView default_view)
{
	GnomePrinter *printer;
	GnomePrintMaster *gpm;
	GnomePrintContext *pc;
	int copies, collate;
	double l, r, t, b;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	printer = NULL;
	copies = 1;
	collate = FALSE;

	if (!preview) {
		GtkWidget *gpd;
		GtkWidget *range;
		int view;

		gpd = gnome_print_dialog_new (_("Print Calendar"),
					      GNOME_PRINT_DIALOG_RANGE
					      | GNOME_PRINT_DIALOG_COPIES);

		view = (int) default_view;
		range = range_selector_new (gpd, date, &view);
		gnome_print_dialog_construct_range_custom (GNOME_PRINT_DIALOG (gpd), range);

		gnome_dialog_set_default (GNOME_DIALOG (gpd),
					  GNOME_PRINT_PRINT);

		/* Run dialog */

		switch (gnome_dialog_run (GNOME_DIALOG (gpd))) {
		case GNOME_PRINT_PRINT:
			break;

		case GNOME_PRINT_PREVIEW:
			preview = TRUE;
			break;

		case -1:
			return;

		default:
			gnome_dialog_close (GNOME_DIALOG (gpd));
			return;
		}

		e_dialog_get_values (gpd);
		default_view = (PrintView) view;

		gnome_print_dialog_get_copies (GNOME_PRINT_DIALOG (gpd),
					       &copies, &collate);
		printer = gnome_print_dialog_get_printer (GNOME_PRINT_DIALOG (gpd));

		gnome_dialog_close (GNOME_DIALOG (gpd));
	}

	/* FIXME: allow configuration of paper size */

	gpm = gnome_print_master_new ();

	if (paper_info == NULL)
		paper_info = gnome_paper_with_name (gnome_paper_name_default ());
	gnome_print_master_set_paper (gpm, paper_info);

	if (printer)
		gnome_print_master_set_printer (gpm, printer);

	gnome_print_master_set_copies (gpm, copies, collate);

	pc = gnome_print_master_get_context (gpm);

	l = gnome_paper_lmargin (paper_info);
	r = gnome_paper_pswidth (paper_info)
		- gnome_paper_rmargin (paper_info);
	t = gnome_paper_psheight (paper_info)
		- gnome_paper_tmargin (paper_info);
	b = gnome_paper_bmargin (paper_info);

	/* depending on the view, do a different output */
	switch (default_view) {
	case PRINT_VIEW_DAY:
		print_day_view (pc, gcal, date, l, r, t, b);
		break;
	case PRINT_VIEW_WEEK:
		print_week_view (pc, gcal, date, l, r, t, b);
		break;
	case PRINT_VIEW_MONTH:
		gnome_print_rotate (pc, 90);
		gnome_print_translate (pc, 0,
				       -gnome_paper_pswidth (paper_info));
		print_month_view (pc, gcal, date, b, t, r, l);
		break;
	case PRINT_VIEW_YEAR:
		print_year_view (pc, gcal, date, l, r, t, b);
		break;
	default:
		g_assert_not_reached ();
	}

	gnome_print_master_close (gpm);

	if (preview) {
		GnomePrintMasterPreview *gpmp;

		gpmp = gnome_print_master_preview_new (gpm,
						       _("Print Preview"));
		gtk_widget_show (GTK_WIDGET (gpmp));
	} else {
		gnome_print_master_print (gpm);
	}

	gtk_object_unref (GTK_OBJECT (gpm));
}


void
print_comp (CalComponent *comp, gboolean preview)
{
	GnomePrinter *printer;
	GnomePrintMaster *gpm;
	GnomePrintContext *pc;
	int copies, collate;
	double l, r, t, b;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	printer = NULL;
	copies = 1;
	collate = FALSE;

	if (!preview) {
		GtkWidget *gpd;

		gpd = gnome_print_dialog_new (_("Print Item"), 
					      GNOME_PRINT_DIALOG_COPIES);

		gnome_dialog_set_default (GNOME_DIALOG (gpd),
					  GNOME_PRINT_PRINT);

		/* Run dialog */

		switch (gnome_dialog_run (GNOME_DIALOG (gpd))) {
		case GNOME_PRINT_PRINT:
			break;

		case GNOME_PRINT_PREVIEW:
			preview = TRUE;
			break;

		case -1:
			return;

		default:
			gnome_dialog_close (GNOME_DIALOG (gpd));
			return;
		}

		e_dialog_get_values (gpd);

		gnome_print_dialog_get_copies (GNOME_PRINT_DIALOG (gpd),
					       &copies, &collate);
		printer = gnome_print_dialog_get_printer (GNOME_PRINT_DIALOG (gpd));

		gnome_dialog_close (GNOME_DIALOG (gpd));
	}

	/* FIXME: allow configuration of paper size */

	gpm = gnome_print_master_new ();

	if (paper_info == NULL)
		paper_info = gnome_paper_with_name (gnome_paper_name_default ());
	gnome_print_master_set_paper (gpm, paper_info);

	if (printer)
		gnome_print_master_set_printer (gpm, printer);

	gnome_print_master_set_copies (gpm, copies, collate);

	pc = gnome_print_master_get_context (gpm);

	l = gnome_paper_lmargin (paper_info);
	r = gnome_paper_pswidth (paper_info)
		- gnome_paper_rmargin (paper_info);
	t = gnome_paper_psheight (paper_info)
		- gnome_paper_tmargin (paper_info);
	b = gnome_paper_bmargin (paper_info);

	print_comp_item (pc, comp, l, r, t, b);

	gnome_print_master_close (gpm);

	if (preview) {
		GnomePrintMasterPreview *gpmp;

		gpmp = gnome_print_master_preview_new (gpm,
						       _("Print Preview"));
		gtk_widget_show (GTK_WIDGET (gpmp));
	} else {
		gnome_print_master_print (gpm);
	}

	gtk_object_unref (GTK_OBJECT (gpm));
}

void
print_setup (void)
{
	GtkWidget *dlg, *ps;
	gint btn;
	
	ps = gnome_paper_selector_new ();
	gtk_widget_show (ps);
	
	dlg = gnome_dialog_new (_("Print Setup"), 
				GNOME_STOCK_BUTTON_OK, 
				GNOME_STOCK_BUTTON_CANCEL, 
				NULL);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox), ps, TRUE, TRUE, 2);

	btn = gnome_dialog_run (GNOME_DIALOG (dlg));
	if (btn == 0) {
		gchar *name;
		
		name  = gnome_paper_selector_get_name (GNOME_PAPER_SELECTOR (ps));
		paper_info = gnome_paper_with_name (name);
	}
	
	gnome_dialog_close (GNOME_DIALOG (dlg));
}
