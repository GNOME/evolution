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
#include <math.h>
#include <gnome.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-copies.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-preview.h>
#include <libgnomeprint/gnome-printer-profile.h>
#include <libgnomeprint/gnome-printer-dialog.h>
#include <e-util/e-dialog-widgets.h>
#include <cal-util/timeutil.h>
#include "calendar-commands.h"
#include "gnome-cal.h"
#include "layout.h"



static void show_print_dialogue(void);

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
build_month (int month, int year, int start_on_monday, int *days, int *start, int *end)
{
	int i;
	int d_month, d_week;

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
	d_week = day_in_week (1, month, year);

	if (start_on_monday)
		d_week = (d_week + 6) % 7;

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
	ALIGN_CENTRE,
	ALIGN_BORDER= 1<<8
};

/* width = width of border, -'ve is no border
   fillcolour = shade of fill, -'ve is no fill */
static void
print_border(GnomePrintContext *pc, double l, double r, double t, double b, double width, double fillcolour)
{
	int i;
	gnome_print_gsave (pc);
	if (fillcolour<0.0)
		i=1;
	else
		i=0;
	for (;i<2;i++) {
		gnome_print_moveto(pc, l, t);
		gnome_print_lineto(pc, l, b);
		gnome_print_lineto(pc, r, b);
		gnome_print_lineto(pc, r, t);
		gnome_print_lineto(pc, l, t);
		if (i==0) {
			gnome_print_setrgbcolor(pc, fillcolour, fillcolour, fillcolour);
			gnome_print_fill(pc);
			if (width<0.0)
				i=2;
		} else {
			gnome_print_setrgbcolor(pc, 0, 0, 0);
			gnome_print_setlinewidth(pc, width);
			gnome_print_stroke(pc);
		}
	}
	gnome_print_grestore (pc);
}

/* outputs 1 line of aligned text in a box */
static void
print_text(GnomePrintContext *pc, GnomeFont *font, const char *text, enum align_box align, double l, double r, double t, double b)
{
	double w, x;
	gnome_print_gsave (pc);
	w = gnome_font_get_width_string(font, text);
	switch (align&3) {
	default:
	case ALIGN_LEFT:
		x = l;
		break;
	case ALIGN_RIGHT:
		x = l+(r-l)-w-2;
		break;
	case ALIGN_CENTRE:
		x = l+((r-l)-w)/2;
		break;
	}
	gnome_print_moveto(pc, x, t-font->size);
	gnome_print_setfont(pc, font);
	gnome_print_setrgbcolor (pc, 0,0,0);
	gnome_print_show(pc, text);
	gnome_print_grestore (pc);
}

/* gets/frees the font for you, as a bold font */
static void
print_text_size(GnomePrintContext *pc, double size, const char *text, enum align_box align, double l, double r, double t, double b)
{
	GnomeFont *font;

	font = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 0, size);
	print_text(pc, font, text, align, l, r, t, b);
	gtk_object_unref (GTK_OBJECT (font));
}

static void
titled_box(GnomePrintContext *pc, const char *text, GnomeFont *font, enum align_box align, double *l, double *r, double *t, double *b, double linewidth)
{
	if (align&ALIGN_BORDER) {
		gnome_print_gsave(pc);
		print_border(pc, *l, *r, *t, *t-font->size-font->size*0.4, linewidth, 0.9);
		print_border(pc, *l, *r, *t-font->size-font->size*0.4, *b, linewidth, -1.0);
		gnome_print_grestore(pc);
		*l+=2;
		*r-=2;
		*b+=2;
	}
	print_text(pc, font, text, align, *l, *r, *t, *b);
	*t-=font->size*1.4;
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
print_month_small (GnomePrintContext *pc, GnomeCalendar *gcal,
		   time_t month, double left, double right, double top, double bottom,
		   int titleflags, time_t greystart, time_t greyend, int bordertitle)
{
	GnomeFont *font, *font_bold, *font_normal;
	time_t now, next;
	int x, y;
	int days[42];
	int day;
	char buf[100];
	struct tm tm;
	double xpad, ypad, size;
	char *daynames[] = { _("Su"), _("Mo"), _("Tu"), _("We"), _("Th"), _("Fr"), _("Sa") };

	xpad = (right-left)/7;
	ypad = (top-bottom)/8.3;
	if (xpad>ypad)
		size=ypad;
	else
		size=xpad;

	size = (xpad+ypad)/3.0;

        tm = *localtime (&month);

	/* get month days */
	build_month(tm.tm_mon, tm.tm_year+1900, week_starts_on_monday, days, 0, 0);

	/* build day-busy bits */
	now = time_month_begin(month);

	/* get title */
	format_date(month, titleflags, buf, 100);
	font = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 1, size*1.2); /* title font */
	if (bordertitle)
		print_border(pc,
			     left, left+7*xpad, top, top-font->size*1.3,
			     1.0, 0.9);
	print_text(pc, font, buf, ALIGN_CENTRE,
		   left, left+7*xpad, top, top - font->size);
	gtk_object_unref (GTK_OBJECT (font));

	font_normal = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0, size);
	font_bold = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 0, size);

	gnome_print_setrgbcolor (pc, 0,0,0);
	for (x=0;x<7;x++) {
		print_text(pc, font_bold, daynames[(week_starts_on_monday?x+1:x)%7], ALIGN_CENTRE,
			   left+x*xpad, left+(x+1)*xpad, bottom+7*ypad, bottom+7*ypad-font_bold->size);
	}

	for (y=0;y<6;y++) {
		for (x=0;x<7;x++) {
			day = days[y*7+x];
			if (day!=0) {
				GList *events;

				sprintf(buf, "%d", day);

				/* this is a slow messy way to do this ... but easy ... */
				events = cal_client_get_events_in_range (gcal->client,
									 now,
									 time_day_end (now));
				font = events ? font_bold : font_normal;
				cal_obj_instance_list_free (events);

				next = time_add_day(now, 1);
				if ((now>=greystart && now<greyend)
				    || (greystart>=now && greystart<next)) {
					print_border(pc,
						     left+x*xpad+xpad*0.1,
						     left+(x+1)*xpad+xpad*0.1,
						     bottom+(5-y)*ypad+font->size-ypad*0.15,
						     bottom+(5-y)*ypad-ypad*0.15,
						     -1.0, 0.75);
				}
				print_text(pc, font, buf, ALIGN_RIGHT,
					   left+x*xpad, left+(x+1)*xpad, bottom+(5-y)*ypad+font->size, bottom+(5-y)*ypad);
				now = next;
			}
		}
	}
	gtk_object_unref (GTK_OBJECT (font_normal));
	gtk_object_unref (GTK_OBJECT (font_bold));
}



/* wraps text into the print context, not taking up more than its allowed space */
static double
bound_text(GnomePrintContext *pc, GnomeFont *font, char *text, double left, double right, double top, double bottom, double indent)
{
	double maxwidth = right-left;
	double width;
	char *p;
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

	top -= font->size;
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
			width+=gnome_font_get_width(font, c);
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
			top -= font->size;
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
		top -= font->size;
	}
	g_free(outbuffer);
	return top;
}

/* Used with layout_events(), takes in a list element and returns the start and
 * end times for the event corresponding to that element.
 */
static void
event_layout_query_func (GList *instance, time_t *start, time_t *end)
{
	CalObjInstance *coi = instance->data;

	*start = coi->start;
	*end = coi->end;
}

static void
print_day_details (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		   double left, double right, double top, double bottom)
{
	time_t start, end;
	GList *l, *events;
	int num_slots, *allocations, *slots;
	int i;
	GnomeFont *font_hour, *font_minute, *font_summary;
	double yinc, y, yend, x, xend;
	double width=40, slot_width;
	char buf[20];

	yinc = (top-bottom)/24;

	/* fill static detail */
	font_hour = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 0, yinc/2);
	font_minute = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 0, yinc/3);
	font_summary = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0, yinc/3);

	gnome_print_setrgbcolor (pc, 0, 0, 0);

	/* internal lines */
	gnome_print_setlinewidth(pc, 0.0);
	gnome_print_moveto(pc, left+width, bottom);
	gnome_print_lineto(pc, left+width, top);
	gnome_print_stroke (pc);

	for (i=0;i<24;i++) {
		y = top - yinc*(i+1);
		print_border(pc, left+1, left+width-1, y, y+yinc-1, -1.0, 0.9);
		gnome_print_setrgbcolor (pc, 0, 0, 0);

		/* the hour label/minute */
		sprintf(buf, "%d", i);
		print_text(pc, font_hour, buf, ALIGN_RIGHT, left, left+width/2, y+yinc, y);
		switch(i) {
		case 12: sprintf(buf, _("pm")); break;
		case 0: sprintf(buf, _("am")); break;
		default: sprintf(buf, "00"); break;
		}
		print_text(pc, font_minute, buf, ALIGN_LEFT, left+width/2, left+width/2, y+yinc, y);

		/* internal lines */
		gnome_print_moveto(pc, left+width, y);
		gnome_print_lineto(pc, right, y);
		gnome_print_stroke (pc);
		gnome_print_moveto(pc, left+width/2, y+yinc/2);
		gnome_print_lineto(pc, right, y+yinc/2);
		gnome_print_stroke (pc);

	}

	start = time_day_begin(whence);
	end = time_day_end(start);

	events = cal_client_get_events_in_range (gcal->client, start, end);

	layout_events (events, event_layout_query_func, &num_slots, &allocations, &slots);

	slot_width = (right-left-width)/num_slots;

	for (i = 0, l = events; l != NULL; l = l->next, i++) {
		CalObjInstance *coi;
		char *str_ico;
		iCalObject *ico;
		CalObjFindStatus status;

		coi = l->data;
		str_ico = cal_client_get_object (gcal->client, coi->uid);

		if (!str_ico) {
			/* The object could have disappeared from the server */
			continue;
		}

		status = ical_object_find_in_string (coi->uid, str_ico, &ico);
		g_free (str_ico);

		switch (status) {
		case CAL_OBJ_FIND_SUCCESS:
			/* Go on */
			break;

		case CAL_OBJ_FIND_SYNTAX_ERROR:
			g_message ("print_day_details(): syntax error in fetched object");
			continue;

		case CAL_OBJ_FIND_NOT_FOUND:
			g_message ("print_day_details(): could not find fetched object");
			continue;
		}

		y = top - (top - bottom) * (coi->start - start) / (end - start) - 1;
		yend = top - (top - bottom) * (coi->end - start) / (end - start) + 1;
		x = left + width + slot_width * allocations[i];

		if (num_slots > 0)
			x++;

		xend = x + slots[i] * slot_width - 2;

		print_border (pc, x, xend, y, yend, 0.0, 0.9);

		bound_text (pc, font_summary, ico->summary, x, xend, y, yend, 0);
		ical_object_unref (ico);
	}

	cal_obj_instance_list_free (events);
	g_free (allocations);
	g_free (slots);

	print_border (pc, left, right, top, bottom, 1.0, -1.0);

	gtk_object_unref (GTK_OBJECT (font_hour));
	gtk_object_unref (GTK_OBJECT (font_minute));
	gtk_object_unref (GTK_OBJECT (font_summary));
}

#if 0
#define TIME_FMT "%X"
#else
#define TIME_FMT "%l:%M%p"
#endif

static void
print_day_summary (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		   double left, double right, double top, double bottom,
		   double size, int totime, int titleformat)
{
	time_t start, end;
	GList *l, *events;
	int i;
	GnomeFont *font_summary;
	double y, yend, x, xend, inc, incsmall;
	char buf[100];
	double margin;
	struct tm tm;

	/* fill static detail */
	font_summary = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0, size);

	gnome_print_setfont (pc, font_summary);

	start = time_day_begin(whence);
	end = time_day_end(start);

	tm = *localtime(&start);

	format_date(start, titleformat, buf, 100);
	titled_box (pc, buf, font_summary, ALIGN_RIGHT | ALIGN_BORDER,
		    &left, &right, &top, &bottom, 0.0);

	events = cal_client_get_events_in_range (gcal->client, start, end);

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

	for (i=0, l = events; l != NULL; l = l->next, i++) {
		CalObjInstance *coi;
		char *str_ico;
		iCalObject *ico;
		CalObjFindStatus status;

		coi = l->data;
		str_ico = cal_client_get_object (gcal->client, coi->uid);

		if (!str_ico) {
			/* The object could have disappeared from the server */
			continue;
		}

		status = ical_object_find_in_string (coi->uid, str_ico, &ico);
		g_free (str_ico);

		switch (status) {
		case CAL_OBJ_FIND_SUCCESS:
			/* Go on */
			break;

		case CAL_OBJ_FIND_SYNTAX_ERROR:
			g_message ("print_day_summary(): syntax error in fetched object");
			continue;

		case CAL_OBJ_FIND_NOT_FOUND:
			g_message ("print_day_summary(): could not find fetched object");
			continue;
		}

		x = left + incsmall;
		xend = right - inc;

		if (y - font_summary->size < bottom)
			break;

		tm = *localtime (&coi->start);
		strftime (buf, 100, TIME_FMT, &tm);
		gnome_print_moveto (pc, x + (margin
					     - gnome_font_get_width_string (font_summary, buf)),
				    y - font_summary->size);
		gnome_print_show (pc, buf);

		if (totime) {
			tm = *localtime (&coi->end);
			strftime (buf, 100, TIME_FMT, &tm);
			gnome_print_moveto (pc,
					    (x + margin + inc
					     + (margin
						- gnome_font_get_width_string (font_summary, buf))),
					    y - font_summary->size);
			gnome_print_show (pc, buf);

			y = bound_text (pc, font_summary, ico->summary,
					x + margin * 2 + inc * 2, xend,
					y, yend, 0);
		} else {
			/* we also indent back after each time is printed */
			y = bound_text (pc, font_summary, ico->summary,
					x + margin + inc, xend,
					y, yend, -margin + inc);
		}

		y += font_summary->size - inc;

		ical_object_unref (ico);
	}

	cal_obj_instance_list_free (events);

	gtk_object_unref (GTK_OBJECT (font_summary));
}

static void
print_week_summary (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		    double left, double right, double top, double bottom)
{
	double y, l, r, t, b;
	time_t now;
	int i;

	l = left;
	r = (right-left)/2+left;
	t = top;
	y = (top-bottom)/3;
	b = top-y;
	now = time_week_begin(whence); /* returns sunday, we need monday */
	now = time_add_day(now, 1);
	for (i = 0; i < 7; i++) {
		print_day_summary (pc, gcal, now, l, r, t, b,
				   10, TRUE, DATE_DAY | DATE_DAYNAME | DATE_MONTH);
		now = time_add_day (now, 1);
		switch (i) {
		case 5:
			y /= 2.0;
			b += y;
		case 0:
		case 1:
		case 3:
		case 4:
			t -= y;
			b -= y;
			break;
		case 2:
			l = r;
			r = right;
			t = top;
			b = t-y;
			break;
		case 6:
			break;
		}
	}
}

static void
print_year_summary (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		    double left, double right, double top, double bottom, int morerows)
{
	double y, x, l, r, t, b;
	time_t now;
	int xx, yy, rows, cols;

	l = left;
	t = top;
	if (morerows) {
		rows=4;
		cols=3;
	} else {
		rows=3;
		cols=4;
	}
	y = (top-bottom)/rows;
	x = (right-left)/cols;
	r = l+x;
	b = top-y;
	now = time_year_begin(whence);
	for (yy = 0; yy < rows; yy++) {
		t = top - y * yy;
		b = t - y;
		for (xx = 0; xx < cols; xx++) {
			l = left + x * xx;
			r = l + x;
			print_month_small (pc, gcal, now,
					   l + 8, r - 8, t - 8, b + 8, DATE_MONTH, 0, 0, TRUE);
			now = time_add_month (now, 1);
		}
	}
}

static void
print_month_summary (GnomePrintContext *pc, GnomeCalendar *gcal, time_t whence,
		     double left, double right, double top, double bottom)
{
	time_t now, today;
	int days[42];
	int day;
	struct tm tm;
	int x, y;
	char buf[100];
	GnomeFont *font_days;

	now = time_month_begin(whence);
        tm = *localtime (&now);

	/* get month days */
	build_month(tm.tm_mon, tm.tm_year+1900, week_starts_on_monday, days, 0, 0);

	/* a little margin */
	top -= 4;

	/* do day names ... */
	font_days = gnome_font_new_closest ("Times", GNOME_FONT_BOLD, 0, 10);
	gnome_print_setfont(pc, font_days);
	for (x=0;x<7;x++) {
		today = time_add_day(now, days[6+x]);
		format_date(today, DATE_DAYNAME, buf, 100);
		print_text(pc, font_days, buf, ALIGN_CENTRE,
			   (right-left)*x/7+left, (right-left)*(x+1)/7+left,
			   top, top-font_days->size);
	}
	top -= font_days->size*1.5;
	gtk_object_unref (GTK_OBJECT (font_days));

	for (y=0;y<6;y++) {
		for (x=0;x<7;x++) {
			day = days[y*7+x];
			if (day!=0) {
				print_day_summary (pc, gcal, now,
						   (right-left)*x/7+left,
						   (right-left)*(x+1)/7+left,
						   top - (top-bottom)*y/6,
						   top - (top-bottom)*(y+1)/6, 6, FALSE,
						   day==1?(DATE_DAY|DATE_MONTH):DATE_DAY);
				now = time_add_day(now, 1);
			}
		}
	}
}

static void
print_todo_details (GnomePrintContext *pc, GnomeCalendar *gcal, time_t start, time_t end,
		    double left, double right, double top, double bottom)
{
	GList *l, *todos;
	int i;
	GnomeFont *font_summary;
	double y, yend, x, xend;

	todos = cal_client_get_uids (gcal->client, CALOBJ_TYPE_TODO);

	font_summary = gnome_font_new_closest ("Times", GNOME_FONT_BOOK, 0, 10);

	gnome_print_setrgbcolor (pc, 0, 0, 0);
	gnome_print_setlinewidth (pc, 0.0);

	titled_box (pc, _("TODO Items"), font_summary,
		    ALIGN_CENTRE | ALIGN_BORDER, &left, &right, &top, &bottom, 1.0);

	y = top - 3;
	yend = bottom - 2;

	for (i = 0, l = todos; l != NULL; l = l->next, i++) {
		char *str_ico;
		iCalObject *ico;
		CalObjFindStatus status;

		str_ico = cal_client_get_object (gcal->client, l->data);

		if (!str_ico) {
			/* The object could have disappeared from the server */
			continue;
		}

		status = ical_object_find_in_string (l->data, str_ico, &ico);
		g_free (str_ico);

		switch (status) {
		case CAL_OBJ_FIND_SUCCESS:
			/* Go on */
			break;

		case CAL_OBJ_FIND_SYNTAX_ERROR:
			g_message ("print_todo_details(): syntax error in fetched object");
			continue;

		case CAL_OBJ_FIND_NOT_FOUND:
			g_message ("print_todo_details(): could not find fetched object");
			continue;
		}

		x = left;
		xend = right-2;

		if (y < bottom)
			break;

		y = bound_text (pc, font_summary, ico->summary, x + 2, xend, y, yend, 0);
		y += font_summary->size;
		gnome_print_moveto (pc, x, y - 3);
		gnome_print_lineto (pc, xend, y - 3);
		gnome_print_stroke (pc);
		y -= 3;

		ical_object_unref (ico);
	}

	cal_obj_uid_list_free (todos);

	gtk_object_unref (GTK_OBJECT (font_summary));
}

#if 0

static GnomePrintContext *
print_context (int preview, char *paper)
{
	GtkWidget *toplevel, *canvas, *sw;
	GnomePrinter *printer;
	GnomePrintContext *pc;

	if (preview) {
		gtk_widget_push_colormap (gdk_rgb_get_cmap ());
		gtk_widget_push_visual (gdk_rgb_get_visual ());

		toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_widget_set_usize (toplevel, 700, 700);
		sw = gtk_scrolled_window_new (NULL, NULL);
		canvas = gnome_canvas_new_aa ();
		gtk_container_add (GTK_CONTAINER (toplevel), sw);
		gtk_container_add (GTK_CONTAINER (sw), canvas);

		gnome_canvas_set_pixels_per_unit((GnomeCanvas *)canvas, 1);

		pc = gnome_print_preview_new ((GnomeCanvas *)canvas, paper);

		gtk_widget_show_all (toplevel);

		gtk_widget_pop_visual ();
		gtk_widget_pop_colormap ();
	} else {
		printer = gnome_printer_dialog_new_modal ();

		if (!printer)
			return NULL;

		pc = gnome_print_context_new_with_paper_size (printer, paper);
	}

	return pc;
}

#endif

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
	GtkWidge *radio;
	GSList *group;
	char text[1024];
	struct tm tm;
	time_t week_begin, week_end;
	struct tm week_begin_tm, week_end_tm;

	box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);

	tm = *localtime (&at);

	/* Day */

	strftime (text, sizeof (text), _("Current day (%a %b %d %Y)"), &tm);
	radio = gtk_radio_button_new_with_label (NULL, text);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
	gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

	/* Week */

	week_begin = time_week_begin (at);
	week_end = time_add_day (time_time_week_end (at), -1);

	week_begin_tm = *localtime (&week_begin);
	week_end_tm = *localtime (&week_end);

	/* FIXME: how to make this localization-friendly? */

	if (week_begin_tm.tm_mon == week_end_tm.tm_mon) {
		char month[128];
		char day1[128];
		char day2[128];

		strftime (month, sizeof (month), _("%a"), &week_begin_tm);
		strftime (day1, sizeof (day1), _("%b"), &week_begin_tm);
		strftime (day2, sizeof (day2), _("%b"), &week_end_tm);

		g_snprintf (text, sizeof (text), _("Current week (%s %s %d - %s %d %d)"),
			    day1, month, week_begin_tm.tm_mday,
			    day2, week_end_tm.tm_mday,
			    week_begin_tm.tm_year + 1900);
	} else {
		char month1[128];
		char month2[128];
		char day1[128];
		char day2[128];

		strftime (month1, sizeof (month1), _("%a"), &week_begin_tm);
		strftime (month2, sizeof (month2), _("%a"), &week_end_tm);
		strftime (day1, sizeof (day1), _("%b"), &week_begin_tm);
		strftime (day2, sizeof (day2), _("%b"), &week_end_tm);

		if (week_begin_tm.tm_year == week_end_tm.tm_year)
			g_snprintf (text, sizeof (text),
				    _("Current week (%s %s %d - %s %s %d %d)"),
				    day1, month1, week_begin_tm.tm_mday,
				    day2, month2, week_end_tm.tm_mday,
				    week_begin_tm.tm_year + 1900);
		else
			g_snprintf (text, sizeof (text),
				    _("Current week (%s %s %d %d - %s %s %d %d)"),
				    day1, month1, week_begin_tm.tm_mday,
				    week_begin_tm.tm_year + 1900,
				    day2, month2, week_end_tm.tm_mday,
				    week_end_tm.tm_year + 1900);
	}

	radio = gtk_radio_button_new_with_label (group, text);
	gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

	/* Month */

	strftime (text, sizeof (text), _("Current month (%a %Y)"), &tm);
	radio = gtk_radio_button_new_with_label (group, text);
	gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

	/* Year */

	strftime (text, sizeof (text), _("Current year (%Y)"), &tm);
	radio = gtk_radio_button_new_with_label (group, text);
	gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

	/* Select default */

	e_dialog_widget_hook_value (dialog, radio, view, print_view_map);
}

/* Shows the print dialog.  The new values are returned in the at and view
 * variables.
 */
static enum GnomePrintButtons
print_dialog (GnomeCalendar *gcal, time_t *at, PrintView *view, GnomePrinter **printer)
{
	GnomePrintDialog *pd;
	GtkWidget *range;
	int int_view;
	enum GnomePrintButtons retval;

	pd = gnome_print_dialog_new (_("Print Calendar"),
				     GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);

	int_view = *view;

	range = range_selector_new (GTK_WIDGET (pd), *at, &int_view);
	gnome_print_dialog_construct_range_custom (pd, range);

	gnome_dialog_set_default (GNOME_DIALOG (pd), GNOME_PRINT_PRINT);

	/* Run! */

	retval = gnome_dialog_run (GNOME_DIALOG (pd));

	switch (retval) {
	case GNOME_PRINT_PRINT:
	case GNOME_PRINT_PREVIEW:
		e_dialog_get_values (GTK_WIDGET (pd));
		*view = int_view;

		*printer = gnome_print_dialog_get_printer (pd);
		break;

	default:
		retval = GNOME_PRINT_CANCEL;
		break;
	}

	gnome_dialog_close (GNOME_DIALOG (pd));
	return retval;
}

void
print_calendar (GnomeCalendar *gcal, time_t at, PrintView default_view)
{
	GnomePrinter *printer;
	GnomePrintMaster *gpm;
	GnomePrintContext *pc;

	switch (print_dialog (gcal, &at, &default_view, &printer)) {
	case GNOME_PRINT_PRINT:
		break;

	case GNOME_PRINT_PREVIEW:
		/* FIXME */
		break;

	default:
		return;
	}

	g_assert (printer != NULL);

	gpm = gnome_print_master_new ();
	gnome_print_master_set_printer (gpm, printer);

	const GnomePaper *paper_info;
	double l, r, t, b, todo, header;
	char buf[100];
	time_t when;
	char *paper = "A4";
	GnomePrintMaster *gpm = gnome_print_master_new ();

	pc = gnome_print_master_get_context (gpm);
	paper_info = gnome_paper_with_name (paper);
	gnome_print_master_set_paper (gpm, paper_info);

	l = gnome_paper_lmargin (paper_info);
	r = gnome_paper_pswidth (paper_info) - gnome_paper_rmargin (paper_info);
	t = gnome_paper_psheight (paper_info) - gnome_paper_tmargin (paper_info);
	b = gnome_paper_bmargin (paper_info);

	/* depending on the view, do a different output */
	switch (default_view) {
	case VIEW_DAY: {
		int i, days = 1;

		for (i = 0; i < days; i++) {
			todo = ((r - l) / 5) * 4 + l;
			header = t - 70;
			print_todo_details (pc, gcal, 0, INT_MAX, todo, r, header, b);
			print_day_details (pc, gcal, at, l, todo - 2.0, header, b);

			print_border (pc, l, r, t, header + 2.0, 1.0, 0.9);

			print_month_small (pc, gcal, at, r - 190, r - 104, t - 4,
					   header + 8, DATE_MONTH | DATE_YEAR, at, at, FALSE);
			print_month_small (pc, gcal, time_add_month (at, 1), r - 90, r - 4, t - 4,
					   header + 8, DATE_MONTH | DATE_YEAR, 0, 0, FALSE);

			format_date (at, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
			print_text_size (pc, 24, buf, ALIGN_LEFT, l + 3, todo, t - 3, header);

			format_date (at, DATE_DAYNAME, buf, 100);
			print_text_size (pc, 18, buf, ALIGN_LEFT, l + 3, todo, t - 27 - 4, header);
			gnome_print_showpage (pc);
			at = time_add_day (at, 1);
		}
		break;
	}
	case VIEW_WEEK:
		header = t - 70;
		print_week_summary (pc, gcal, at, l, r, header, b);

		/* more solid total outline */
		print_border (pc, l, r, header, b, 1.0, -1.0);

		/* header border */
		print_border (pc, l, r, t, header + 2.0, 1.0, 0.9);

		when = time_week_begin (at);
		when = time_add_day (when, 1);

		print_month_small (pc, gcal, at, r - 90, r - 4, t - 4,
				   header + 8, DATE_MONTH | DATE_YEAR, when, time_add_week (when, 1),
				   FALSE);
		print_month_small (pc, gcal, time_add_month (at, -1), r - 190, r - 104, t - 4,
				   header + 8, DATE_MONTH | DATE_YEAR, when, time_add_week (when, 1),
				   FALSE);

		format_date (when, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
		print_text_size (pc, 24, buf, ALIGN_LEFT, l + 3, r, t - 4, header);

		when = time_add_day (when, 6);
		format_date (when, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
		print_text_size (pc, 24, buf, ALIGN_LEFT, l + 3, r, t - 24 - 3, header);
		gnome_print_showpage (pc);
		break;

	case VIEW_MONTH:
		header = t - 70;
		gnome_print_rotate (pc, 90);
		gnome_print_translate (pc, 0, -gnome_paper_pswidth (paper_info));
		/*print_month_summary(pc, cal, at, l, r, header, b);*/
		print_month_summary (pc, gcal, at, b, t, r - 70, l);

		print_border (pc, b, t, r, r - 72.0, 1.0, 0.9);

		print_month_small (pc, gcal, time_add_month (at, 1),
				   t - (t - b) / 7 + 2, t - 8, r - 4, r - 68,
				   DATE_MONTH | DATE_YEAR, 0, 0, FALSE);
		print_month_small (pc, gcal, time_add_month (at, -1),
				   b + 8, b + (t - b) / 7 - 2, r - 4, r - 68,
				   DATE_MONTH | DATE_YEAR, 0, 0, FALSE);

		/* centered title */
		format_date (at, DATE_MONTH | DATE_YEAR, buf, 100);
		print_text_size (pc, 24, buf, ALIGN_CENTRE, b + 3, t, r - 3, l);
		gnome_print_showpage (pc);
		break;

	case VIEW_YEAR:
#if 0
		/* landscape */
		gnome_print_rotate(pc, 90);
		gnome_print_translate(pc, 0, -gnome_paper_pswidth(paper_info));
		print_year_summary(pc, gcal, at, b, t, r-50, l, FALSE);

		/* centered title */
		format_date(at, DATE_YEAR, buf, 100);
		print_text_size(pc, 24, buf, ALIGN_CENTRE, b+3, t, r-3, l);
#else
		/* portrait */
		print_year_summary(pc, gcal, at, l, r, t-50, b, TRUE);

		/* centered title */
		format_date(at, DATE_YEAR, buf, 100);
		print_text_size(pc, 24, buf, ALIGN_CENTRE, l+3, r, t-3, b);
#endif
		gnome_print_showpage(pc);
		break;

	default:
		g_assert_not_reached ();
	}

	gnome_print_master_close (gpm);
	gnome_print_master_print (gpm);
}

#if 0

/*
  Load the printing dialogue box
*/
static void
show_print_dialogue(void)
{
	GtkWidget *print_dialogue;
	GtkWidget *printer_widget;
	GtkWidget *paper_widget;
	GtkWidget *print_copies;
	GtkWidget *table;

	print_dialogue =
		gnome_dialog_new(_("Print Calendar"),
				 GNOME_STOCK_BUTTON_OK,
				 _("Preview"),
				 GNOME_STOCK_BUTTON_CANCEL,
				 NULL);

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(GNOME_DIALOG (print_dialogue)->vbox), GTK_WIDGET (table));
	gtk_widget_show(table);

	printer_widget = gnome_printer_widget_new ();
	gtk_table_attach((GtkTable *)table, printer_widget, 1, 2, 0, 2, 0, 0, 4, 4);
        gtk_widget_show(printer_widget);

#if 0
	frame = gtk_frame_new("Select Paper");
	paper_widget = gnome_paper_selector_new ();
	gtk_table_attach(table, frame, 0, 1, 0, 1, 0, 0, 4, 4);
	gtk_container_add(frame, GTK_WIDGET (paper_widget));
        gtk_widget_show(paper_widget);
        gtk_widget_show(frame);
#else
	paper_widget = gnome_paper_selector_new ();
	gtk_table_attach((GtkTable *)table, paper_widget, 0, 1, 0, 1, GTK_SHRINK, GTK_SHRINK, 4, 4);
        gtk_widget_show(paper_widget);
#endif
	print_copies = gnome_print_copies_new ();
	gtk_table_attach((GtkTable *)table, print_copies, 0, 1, 1, 2, 0, 0, 4, 4);
        gtk_widget_show(print_copies);

#if 0
	frame = gtk_frame_new("Print Range");
	gtk_table_attach_defaults(table, frame, 0, 1, 1, 2);
        gtk_widget_show(frame);
#endif

	gnome_dialog_run_and_close (GNOME_DIALOG (print_dialogue));
}

#endif
