/*
 * Evolution calendar - Print support
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "print.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "comp-util.h"
#include "e-cal-model.h"
#include "e-day-view.h"
#include "e-day-view-layout.h"
#include "itip-utils.h"
#include "e-week-view.h"
#include "e-week-view-layout.h"
#include "e-task-table.h"

typedef struct PrintCompItem PrintCompItem;
typedef struct PrintCalItem PrintCalItem;

struct PrintCompItem {
	ECalClient *client;
	ECalComponent *comp;
	ICalTimezone *zone;
	gboolean use_24_hour_format;
};

static void
print_comp_item_free (gpointer ptr)
{
	PrintCompItem *pci = ptr;

	if (pci) {
		g_clear_object (&pci->client);
		g_clear_object (&pci->comp);
		g_clear_object (&pci->zone);
		g_slice_free (PrintCompItem, pci);
	}
}

struct PrintCalItem {
	ECalendarView *cal_view;
	ETable *tasks_table;
	EPrintView print_view_type;
	time_t start;
};

static void
print_cal_item_free (gpointer ptr,
		     GClosure *closure)
{
	PrintCalItem *pci = ptr;

	if (pci) {
		g_clear_object (&pci->cal_view);
		g_clear_object (&pci->tasks_table);
		g_slice_free (PrintCalItem, pci);
	}
}

static gdouble
evo_calendar_print_renderer_get_width (GtkPrintContext *context,
                                       PangoFontDescription *font,
                                       const gchar *text)
{
	PangoLayout *layout;
	gint layout_width, layout_height;

	layout = gtk_print_context_create_pango_layout (context);

	pango_layout_set_font_description (layout, font);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_indent (layout, 0);
	pango_layout_get_size (layout, &layout_width, &layout_height);

	g_object_unref (layout);

	return pango_units_to_double (layout_width);
}

static gdouble
evo_calendar_print_renderer_get_height (GtkPrintContext *context,
                                        PangoFontDescription *font,
                                        const gchar *text)
{
	PangoLayout *layout;
	gint layout_width, layout_height;

	layout = gtk_print_context_create_pango_layout (context);

	pango_layout_set_font_description (layout, font);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_indent (layout, 0);
	pango_layout_get_size (layout, &layout_width, &layout_height);

	g_object_unref (layout);

	return pango_units_to_double (layout_height);
}

static gdouble
get_font_size (PangoFontDescription *font)
{
	g_return_val_if_fail (font, 0.0);

	return pango_units_to_double (pango_font_description_get_size (font));
}

static gint
get_day_view_time_divisions (void)
{
	GSettings *settings;
	gint time_divisions;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	time_divisions = g_settings_get_int (settings, "time-divisions");
	if (time_divisions < 5 || time_divisions > 30)
		time_divisions = 30;

	g_object_unref (settings);

	return time_divisions;
}

/*
 * Note that most dimensions are in points (1/72 of an inch) since that is
 * what gnome-print uses.
 */

/* GtkHTML prints using a fixed margin. It has code to get the margins from
 * gnome-print keys, but it's commented out. The corresponding code here
 * doesn't seem to work either (getting zero margins), so we adopt
 * gtkhtml's cheat. */

#define TEMP_MARGIN .05

/* The fonts to use */
#define FONT_FAMILY "Sans"

/* The font size to use for normal text. */
#define DAY_NORMAL_FONT_SIZE	12
#define WEEK_NORMAL_FONT_SIZE	12
#define MONTH_NORMAL_FONT_SIZE	8
#define WEEK_EVENT_FONT_SIZE	9
#define WEEK_SMALL_FONT_SIZE	8

/* The height of the header bar across the top of the Day, Week & Month views,
 * which contains the dates shown and the 2 small calendar months. */
#define HEADER_HEIGHT		80

/* The width of the small calendar months, the space from the right edge of
 * the header rectangle, and the space between the months. */
#define MIN_SMALL_MONTH_WIDTH	120
#define SMALL_MONTH_PAD		5
#define SMALL_MONTH_SPACING	20

/* The minimum number of rows we leave space for for the long events in the
 * day view. */
#define DAY_VIEW_MIN_ROWS_IN_TOP_DISPLAY	2

/* The row height for long events in the day view. */
#define DAY_VIEW_ROW_HEIGHT		14

#define CALC_DAY_VIEW_ROWS(divis)		((60 / divis) * 24)

/* The width of the column with all the times in it. */
#define DAY_VIEW_TIME_COLUMN_WIDTH	36

/* The space on the right of each event. */
#define DAY_VIEW_EVENT_X_PAD		8

/* Allowance for small errors in floating point comparisons. */
#define EPSILON			0.01

/* border of the top title, the other-month's days and such */
#define DARKER_BORDER 0.94

/* The weird month of September 1752, where 3 Sep through 13 Sep were
 * eliminated due to the Gregorian reformation. */
static const gint sept_1752[42] = {
	 0,  0,  1,  2, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30,
	 0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0
};
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
	guint8 cols_per_row[CALC_DAY_VIEW_ROWS (1)];
	gboolean use_24_hour_format;
	ICalTimezone *zone;
};

struct psinfo {
	gint days_shown;
	time_t day_starts[E_WEEK_VIEW_MAX_WEEKS * 7 + 1];

	GArray *events;

	gint rows_per_cell;
	gint rows_per_compressed_cell;
	GDateWeekday display_start_weekday;
	gboolean multi_week_view;
	gint weeks_shown;
	gint month;
	gboolean compress_weekend;
	gboolean use_24_hour_format;
	gdouble row_height;
	gdouble header_row_height;
	ICalTimezone *zone;
};

/* Convenience function to help the transition to timezone functions.
 * It converts a time_t to a struct tm. */
static void
convert_timet_to_struct_tm (time_t time,
                            ICalTimezone *zone,
                            struct tm *tm)
{
	ICalTime *tt;

	/* Convert it to an ICalTime. */
	tt = i_cal_time_new_from_timet_with_zone (time, FALSE, zone);

	*tm = e_cal_util_icaltime_to_tm (tt);

	g_clear_object (&tt);
}

/* Fills the 42-element days array with the day numbers for the specified
 * month.  Slots outside the bounds of the month are filled with zeros.
 * The starting and ending indexes of the days are returned in the start
 * and end arguments. */
static void
build_month (ECalModel *model,
             gint month,
             gint year,
             gint *days,
             gint *start,
             gint *end)
{
	gint i;
	gint d_month;
	gint d_week;
	GDateWeekday weekday;
	GDateWeekday week_start_day;

	/* Note that months are zero-based, so September is month 8 */

	if ((year == 1752) && (month == 8)) {
		memcpy (days, sept_1752, 42 * sizeof (gint));

		if (start)
			*start = SEPT_1752_START;

		if (end)
			*end = SEPT_1752_END;

		return;
	}

	for (i = 0; i < 42; i++)
		days[i] = 0;

	d_month = time_days_in_month (year, month);
	/* Get the start weekday in the month, 0=Sun to 6=Sat. */
	d_week = time_day_of_week (1, month, year);

	/* Get the configuration setting specifying which weekday we put on
	 * the left column. */
	week_start_day = e_cal_model_get_week_start_day (model);

	weekday = e_weekday_from_tm_wday (d_week);

	/* Figure out which square we want to put the 1 in. */
	weekday = (weekday + 7 - week_start_day) % 7;

	for (i = 0; i < d_month; i++)
		days[weekday + i] = i + 1;

	if (start)
		*start = e_weekday_get_days_between (week_start_day, weekday);

	if (end)
		*end = d_week + d_month - 1;
}

static PangoFontDescription *
get_font_for_size (gdouble height,
                   PangoWeight weight)
{
	PangoFontDescription *desc;
	gint size;

	#define MAGIC_SCALE_FACTOR (0.86)
	size = pango_units_from_double (height * MAGIC_SCALE_FACTOR);

	desc = pango_font_description_new ();
	pango_font_description_set_size (desc, size);
	pango_font_description_set_weight (desc, weight);
	pango_font_description_set_family_static (desc, FONT_FAMILY);

	return desc;
}

/* Prints a rectangle, with or without a border, filled or outline, and
 * possibly with triangular arrows at one or both horizontal edges.
 * width      = width of border, -ve means no border.
 * red,green,blue = bgcolor to fill,   -ve means no fill.
 * left_triangle_width, right_triangle_width = width from edge of rectangle to
 *        point of triangle, or -ve for no triangle. */
static void
print_border_with_triangles (GtkPrintContext *pc,
                             gdouble x1,
                             gdouble x2,
                             gdouble y1,
                             gdouble y2,
                             gdouble line_width,
                             GdkRGBA bg_rgba,
                             gdouble left_triangle_width,
                             gdouble right_triangle_width)
{
	cairo_t *cr = gtk_print_context_get_cairo_context (pc);

	cairo_save (cr);

	/* Fill in the interior of the rectangle, if desired. */
	if (bg_rgba.red >= -EPSILON && bg_rgba.green >= -EPSILON && bg_rgba.blue >= -EPSILON) {

		cairo_move_to (cr, x1, y1);

		if (left_triangle_width > 0.0)
			cairo_line_to (
				cr, x1 - left_triangle_width,
				(y1 + y2) / 2);

		cairo_line_to (cr, x1, y2);
		cairo_line_to (cr, x2, y2);

		if (right_triangle_width > 0.0)
			cairo_line_to (cr, x2 + right_triangle_width, (y1 + y2) / 2);

		cairo_line_to (cr, x2, y1);
		cairo_close_path (cr);
		gdk_cairo_set_source_rgba (cr, &bg_rgba);
		cairo_fill (cr);
		cairo_restore (cr);
		cairo_save (cr);
	}

	/* Draw the outline, if desired. */
	if (line_width >= EPSILON) {

		cr = gtk_print_context_get_cairo_context (pc);
		cairo_move_to (cr, x1, y1);

		if (left_triangle_width > 0.0)
			cairo_line_to (
				cr, x1 - left_triangle_width,
				(y1 + y2) / 2);

		cairo_line_to (cr, x1, y2);
		cairo_line_to (cr, x2, y2);

		if (right_triangle_width > 0.0)
			cairo_line_to (
				cr, x2 + right_triangle_width,
				(y1 + y2) / 2);

		cairo_line_to (cr, x2, y1);
		cairo_close_path (cr);
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_set_line_width (cr, line_width);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

/* Prints a rectangle, with or without a border, and filled or outline.
 * width      = width of border, -ve means no border.
 * fillcolor = shade of fill,   -ve means no fill. */
static void
print_border_rgb (GtkPrintContext *pc,
                  gdouble x1,
                  gdouble x2,
                  gdouble y1,
                  gdouble y2,
                  gdouble line_width,
                  GdkRGBA bg_rgba)
{
	print_border_with_triangles (
		pc, x1, x2, y1, y2, line_width,
		bg_rgba, -1.0, -1.0);
}

static void
print_border (GtkPrintContext *pc,
              gdouble x1,
              gdouble x2,
              gdouble y1,
              gdouble y2,
              gdouble line_width,
              gdouble fillcolor)
{
	GdkRGBA bg_rgba;

	bg_rgba.red = fillcolor;
	bg_rgba.green = fillcolor;
	bg_rgba.blue = fillcolor;
	bg_rgba.alpha = 1.0;

	print_border_rgb (pc, x1, x2, y1, y2, line_width, bg_rgba);
}

static void
print_rectangle (GtkPrintContext *context,
                 gdouble x,
                 gdouble y,
                 gdouble width,
                 gdouble height,
                 GdkRGBA bg_rgba)
{
	cairo_t *cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);

	cairo_rectangle (cr, x, y, width, height);
	gdk_cairo_set_source_rgba (cr, &bg_rgba);
	cairo_fill (cr);

	cairo_restore (cr);
}

/* Recreate the layout by shrinking the text string if we have a line that's
 * too high.
 */
static PangoLayout *
shrink_text_to_line (PangoLayout *layout,
                     gint layout_width,
                     gint layout_height,
                     GtkPrintContext *context,
                     PangoFontDescription *desc,
                     const gchar *text,
                     PangoAlignment alignment,
                     gdouble x1,
                     gdouble x2,
                     gdouble y1,
                     gdouble y2)
{
	gint new_length, len;

	if (layout_width == 0 || x2 - x1 < EPSILON)
		return layout; /* Do nothing */

	len = strlen (text);
	new_length = (gint) floor (pango_units_from_double (x2 - x1) /
			(gdouble) layout_width * (gdouble) len);

	/* in case the cut would be in the middle of a UTF-8 character */
	while (new_length < len && !g_utf8_validate (text, new_length, NULL)) {
		new_length++;
	}

	if (new_length < len) {
		g_object_unref (layout); /* Destroy old layout */
		layout = gtk_print_context_create_pango_layout (context);

		pango_layout_set_font_description (layout, desc);
		pango_layout_set_alignment (layout, alignment);
		pango_layout_set_text (layout, text, new_length);
	}

	return layout;
}

static void
print_set_fg_for_bg (cairo_t *cr,
		     const GdkRGBA *bg_rgba)
{
	GdkRGBA fg_rgba;

	if (!bg_rgba) {
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		return;
	}

	fg_rgba = e_utils_get_text_color_for_background (bg_rgba);
	gdk_cairo_set_source_rgba (cr, &fg_rgba);
}

/* Prints 1 line of aligned text in a box. It is centered vertically, and
 * the horizontal alignment can be either PANGO_ALIGN_LEFT, PANGO_ALIGN_RIGHT,
 * or PANGO_ALIGN_CENTER. Text is truncated if too long for cell. */
static gdouble
print_text_line (GtkPrintContext *context,
                 PangoFontDescription *desc,
                 const gchar *text,
                 PangoAlignment alignment,
                 gdouble x1,
                 gdouble x2,
                 gdouble y1,
                 gdouble y2,
                 gboolean shrink,
		 const GdkRGBA *bg_rgba)
{
	PangoLayout *layout;
	gint layout_width, layout_height;
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (context);
	layout = gtk_print_context_create_pango_layout (context);

	pango_layout_set_font_description (layout, desc);
	pango_layout_set_alignment (layout, alignment);
	pango_layout_set_text (layout, text, -1);

	/* Grab the width before expanding the layout. */
	pango_layout_get_size (layout, &layout_width, &layout_height);

	if (shrink && layout_width > pango_units_from_double (x2 - x1)) /* Too wide */
		layout = shrink_text_to_line (
			layout, layout_width, layout_height,
			context, desc, text, alignment,
			x1, x2, y1, y2);

	pango_layout_set_width (layout, pango_units_from_double (x2 - x1));

	cairo_save (cr);

	/* Set a clipping rectangle. */
	cairo_move_to (cr, x1, y1);
	cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
	cairo_clip (cr);

	cairo_new_path (cr);
	print_set_fg_for_bg (cr, bg_rgba);

	cairo_move_to (cr, x1, y1);
	pango_cairo_show_layout (cr, layout);

	cairo_stroke (cr);

	cairo_restore (cr);

	g_object_unref (layout);

	return pango_units_to_double (layout_width);
}

/* Prints 1 or more lines of aligned text in a box. It is centered vertically, and
 * the horizontal alignment can be either PANGO_ALIGN_LEFT, PANGO_ALIGN_RIGHT,
 * or PANGO_ALIGN_CENTER. */
static double
print_text (GtkPrintContext *context,
            PangoFontDescription *desc,
            const gchar *text,
            PangoAlignment alignment,
            gdouble x1,
            gdouble x2,
            gdouble y1,
            gdouble y2)
{
	return print_text_line (
		context, desc,
		text, alignment,
		x1, x2, y1, y2, FALSE, NULL);
}

/* gets/frees the font for you, as a bold font */
static gdouble
print_text_size_bold (GtkPrintContext *context,
                      const gchar *text,
                      PangoAlignment alignment,
                      gdouble x1,
                      gdouble x2,
                      gdouble y1,
                      gdouble y2)
{
	PangoFontDescription *font;
	gdouble w;

	font = get_font_for_size (ABS (y2 - y1) * 0.5, PANGO_WEIGHT_BOLD);
	w = print_text (context, font, text, alignment, x1, x2, y1, y2);
	pango_font_description_free (font);

	return w;
}

/* gets/frees the font for you, as a bold font - absolute size parameter */
static double
print_text_abs_bold (GtkPrintContext *context,
                     const gchar *text,
                     gdouble font_size,
                     PangoAlignment alignment,
                     gdouble x1,
                     gdouble x2,
                     gdouble y1,
                     gdouble y2)
{
	PangoFontDescription *font;
	gdouble w;

	font = get_font_for_size (font_size, PANGO_WEIGHT_BOLD);
	w = print_text_line (context, font, text, alignment, x1, x2, y1, y2, TRUE, NULL);
	pango_font_description_free (font);

	return w;
}

static void
titled_box (GtkPrintContext *context,
            const gchar *text,
            PangoFontDescription *font,
            PangoAlignment alignment,
            gdouble *x1,
            gdouble *y1,
            gdouble *x2,
            gdouble *y2,
            gdouble linewidth)
{
	gdouble size;

	size = evo_calendar_print_renderer_get_height (context, font, text);
	print_border (context, *x1, *x2, *y1, *y1 + size + 2, linewidth, DARKER_BORDER);
	print_border (context, *x1, *x2, *y1 + size + 2, *y2, linewidth, -1.0);
	*x1 += 2;
	*x2 -= 2;
	*y2 += 2;
	print_text (context, font, text, alignment, *x1, *x2, *y1 + 1.0, *y1 + size);
	*y1 += size + 2;
}

static gboolean
get_show_week_numbers (void)
{
	GSettings *settings;
	gboolean show_week_numbers;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	show_week_numbers =
		g_settings_get_boolean (settings, "show-week-numbers");

	g_object_unref (settings);

	return show_week_numbers;
}

enum datefmt {
	DATE_MONTH = 1 << 0,
	DATE_DAY = 1 << 1,
	DATE_DAYNAME = 1 << 2,
	DATE_YEAR = 1 << 3
};

/*
  format the date 'nicely' and consistently for various headers
*/
static gchar *
format_date (struct tm *tm,
             gint flags,
             gchar *buffer,
             gint bufflen)
{
	GString *fmt = g_string_new ("");

	if (flags & DATE_DAYNAME) {
		g_string_append (fmt, "%A");
	}
	if (flags & DATE_DAY) {
		if (flags & DATE_DAYNAME)
			g_string_append_c (fmt, ' ');
		g_string_append (fmt, e_cal_recur_get_localized_nth (tm->tm_mday - 1));
	}
	if (flags & DATE_MONTH) {
		if (flags & (DATE_DAY | DATE_DAYNAME))
			g_string_append_c (fmt, ' ');
		g_string_append (fmt, "%B");
		if ((flags & (DATE_DAY | DATE_YEAR)) == (DATE_DAY | DATE_YEAR))
			g_string_append_c (fmt, ',');
	}
	if (flags & DATE_YEAR) {
		if (flags & (DATE_DAY | DATE_DAYNAME | DATE_MONTH))
			g_string_append_c (fmt, ' ');
		g_string_append (fmt, "%Y");
	}
	e_utf8_strftime (buffer, bufflen, fmt->str, tm);
	buffer[bufflen - 1] = '\0';

	g_string_free (fmt, TRUE);

	return buffer;
}

static gboolean
instance_cb (ICalComponent *comp,
	     ICalTime *instance_start,
	     ICalTime *instance_end,
	     gpointer user_data,
	     GCancellable *cancellable,
	     GError **error)
{

	gboolean *found = ((ECalModelGenerateInstancesData *) user_data)->cb_data;

	*found = TRUE;

	return FALSE;
}

const gchar *daynames[] = {
	/* G_DATE_BAD_WEEKDAY */ "",
	/* Translators: These are workday abbreviations,
	 * e.g. Su=Sunday and Th=thursday */
	/* G_DATE_MONDAY      */ N_("Mo"),
	/* G_DATE_TUESDAY     */ N_("Tu"),
	/* G_DATE_WEDNESDAY   */ N_("We"),
	/* G_DATE_THURSDAY    */ N_("Th"),
	/* G_DATE_FRIDAY      */ N_("Fr"),
	/* G_DATE_SATURDAY    */ N_("Sa"),
	/* G_DATE_SUNDAY      */ N_("Su")
};

static gdouble
calc_small_month_width (GtkPrintContext *context,
                        gdouble for_height)
{

	PangoFontDescription *font_bold;
	gdouble res = 0.0;
	gint ii;

	font_bold = get_font_for_size (for_height / 7.4, PANGO_WEIGHT_BOLD);
	res = MAX (evo_calendar_print_renderer_get_width (
		context, font_bold, "23"), res);
	for (ii = G_DATE_MONDAY; ii < G_N_ELEMENTS (daynames); ii++) {
		res = MAX (evo_calendar_print_renderer_get_width (
			context, font_bold, _(daynames[ii])), res);
	}

	pango_font_description_free (font_bold);

	/* res is max cell width, so multiply it with column
	 * count plus some space between columns. */
	res = (res + 1.0) * (7 + (get_show_week_numbers () ? 1 : 0)) - 1.0;

	if (res < MIN_SMALL_MONTH_WIDTH)
		res = MIN_SMALL_MONTH_WIDTH;

	return res;
}

/*
  print out the month small, embolden any days with events.
*/
static void
print_month_small (GtkPrintContext *context,
                   ECalModel *model,
                   time_t month,
                   gdouble x1,
                   gdouble y1,
                   gdouble x2,
                   gdouble y2,
                   gint titleflags,
                   time_t greystart,
                   time_t greyend,
                   gint bordertitle)
{
	ICalTimezone *zone;
	PangoFontDescription *font, *font_bold, *font_normal;
	time_t now, next;
	gint x, y;
	gint day;
	gint days[42];
	GDateWeekday weekday;
	GDateWeekday week_start_day;
	gchar buf[100];
	struct tm tm;
	gdouble font_size;
	gdouble header_size, col_width, row_height, text_xpad, w;
	gdouble cell_top, cell_bottom, cell_left, cell_right, text_right;
	gboolean week_numbers;
	cairo_t *cr;

	zone = e_cal_model_get_timezone (model);

	week_numbers = get_show_week_numbers ();

	/* Print the title, e.g. 'June 2001', in the top 16% of the area. */
	convert_timet_to_struct_tm (month, zone, &tm);
	format_date (&tm, titleflags, buf, 100);

	header_size = ABS (y2 - y1) * 0.16;

	font = get_font_for_size (header_size, PANGO_WEIGHT_BOLD);
	if (bordertitle)
		print_border (context, x1, x2, y1, y1 + header_size, 1.0, DARKER_BORDER);
	print_text (
		context, font, buf, PANGO_ALIGN_CENTER, x1, x2,
		y1, y1 + header_size);
	pango_font_description_free (font);

	y1 += header_size;
	col_width = (x2 - x1) / (7 + (week_numbers ? 1 : 0));

	/* The top row with the day abbreviations gets an extra bit of
	 * vertical space around it. */
	row_height = ABS (y2 - y1) / 7.4;

	/* First we need to calculate a reasonable font size. We start with a
	 * rough guess of just under the height of each row. */
	font_size = row_height;

	/* get month days */
	convert_timet_to_struct_tm (month, zone, &tm);
	build_month (model, tm.tm_mon, tm.tm_year + 1900, days, NULL, NULL);

	font_normal = get_font_for_size (font_size, PANGO_WEIGHT_NORMAL);
	font_bold = get_font_for_size (font_size, PANGO_WEIGHT_BOLD);

	/* Get a reasonable estimate of the largest number we will need,
	 * and use it to calculate the offset from the right edge of the
	 * cell that we should put the numbers. */
	w = evo_calendar_print_renderer_get_width (context, font_bold, "23");
	text_xpad = (col_width - w) / 2;

	cr = gtk_print_context_get_cairo_context (context);
	cairo_set_source_rgb (cr, 0, 0, 0);

	/* Print the abbreviated day names across the top in bold. */
	week_start_day = e_cal_model_get_week_start_day (model);
	weekday = week_start_day;
	for (x = 0; x < 7; x++) {
		print_text (
			context, font_bold,
			_(daynames[weekday]), PANGO_ALIGN_RIGHT,
			x1 + (x + (week_numbers ? 1 : 0)) * col_width,
			x1 + (x + 1 + (week_numbers ? 1 : 0)) * col_width,
			y1, y1 + row_height * 1.4);
		weekday = e_weekday_get_next (weekday);
	}

	y1 += row_height * 1.4;

	now = time_month_begin_with_zone (month, zone);
	for (y = 0; y < 6; y++) {

		cell_top = y1 + y * row_height;
		cell_bottom = cell_top + row_height;

		if (week_numbers) {
			cell_left = x1;
			/* We add a 0.05 to make sure the cells meet up with
			 * each other. Otherwise you sometimes get lines
			 * between them which looks bad. Maybe I'm not using
			 * coords in the way gnome-print expects. */
			cell_right = cell_left + col_width + 0.05;
			text_right = cell_right - text_xpad;

			/* last week can be empty */
			for (x = 0; x < 7; x++) {
				day = days[y * 7 + x];
				if (day != 0)
					break;
			}

			if (day != 0) {
				time_t week_begin;
				gint wday;

				wday = e_weekday_to_tm_wday (week_start_day);
				week_begin = time_week_begin_with_zone (
					now, wday, zone);

				convert_timet_to_struct_tm (
					week_begin, zone, &tm);

				/* Month in e_calendar_item_get_week_number
				 * is also zero-based. */
				sprintf (
					buf, "%d",
					e_calendar_item_get_week_number (
					NULL, tm.tm_mday, tm.tm_mon,
					tm.tm_year + 1900));

				print_text (
					context, font_normal,
					buf, PANGO_ALIGN_RIGHT,
					cell_left, text_right,
					cell_top, cell_bottom);
			}
		}

		for (x = 0; x < 7; x++) {

			cell_left = x1 + (x + (week_numbers ? 1 : 0)) * col_width;
			/* We add a 0.05 to make sure the cells meet up with
			 * each other. Otherwise you sometimes get lines
			 * between them which looks bad. Maybe I'm not using
			 * coords in the way gnome-print expects. */
			cell_right = cell_left + col_width + 0.05;
			text_right = cell_right - text_xpad;

			day = days[y * 7 + x];
			if (day != 0) {
				gboolean found = FALSE;
				sprintf (buf, "%d", day);

				/* this is a slow messy way to do this ... but easy ... */
				e_cal_model_generate_instances_sync (
					model, now,
					time_day_end_with_zone (now, zone),
					NULL, instance_cb, &found);

				font = found ? font_bold : font_normal;

				next = time_add_day_with_zone (now, 1, zone);
				if ((now >= greystart && now < greyend)
				    || (greystart >= now && greystart < next)) {
					print_border (
						context,
						cell_left, cell_right,
						cell_top, cell_bottom,
						-1.0, 0.75);
				}
				print_text (
					context, font, buf, PANGO_ALIGN_RIGHT,
					cell_left, text_right,
					cell_top, cell_bottom);

				now = next;
			}
		}
	}
	pango_font_description_free (font_normal);
	pango_font_description_free (font_bold);
}

/* wraps text into the print context, not taking up more than its allowed space */
static gdouble
bound_text (GtkPrintContext *context,
            PangoFontDescription *font,
            const gchar *text,
            gint len,
            gdouble x1,
            gdouble y1,
            gdouble x2,
            gdouble y2,
            gboolean can_wrap,
	    const GdkRGBA *bg_rgba,
            gdouble *last_page_start,
            gint *pages)
{
	PangoLayout *layout;
	gint layout_width, layout_height;
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (context);
	layout = gtk_print_context_create_pango_layout (context);

	pango_layout_set_font_description (layout, font);
	pango_layout_set_text (layout, text, len);
	pango_layout_set_width (layout, pango_units_from_double (x2 - x1));

	if (can_wrap)
		pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);

	pango_layout_get_size (layout, &layout_width, &layout_height);

	if (last_page_start &&
		y1 + pango_units_to_double (layout_height) >
		y2 + (*last_page_start)) {
		/* draw this on new page */
		if (pages)
			*pages = *pages + 1;

		*last_page_start = *last_page_start + y2;
		y1 = *last_page_start + 10.0;
	}

	if (!last_page_start || (y1 >= 0.0 && y1 < y2)) {
		cairo_save (cr);

		/* Set a clipping rectangle. */
		cairo_move_to (cr, x1, y1);
		cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
		cairo_clip (cr);
		cairo_new_path (cr);

		print_set_fg_for_bg (cr, bg_rgba);

		cairo_move_to (cr, x1, y1);
		pango_cairo_show_layout (cr, layout);
		cairo_stroke (cr);

		cairo_restore (cr);
	}

	g_object_unref (layout);

	return y1 + pango_units_to_double (layout_height);
}

/* Draw the borders, lines, and times down the left of the day view. */
static void
print_day_background (GtkPrintContext *context,
                      ECalModel *model,
                      time_t whence,
                      struct pdinfo *pdi,
                      gdouble left,
                      gdouble right,
                      gdouble top,
                      gdouble bottom)
{
	PangoFontDescription *font_hour, *font_minute;
	gdouble yinc, y;
	gdouble width = DAY_VIEW_TIME_COLUMN_WIDTH;
	gdouble font_size, max_font_size, hour_font_size, minute_font_size;
	gchar buf[20];
	const gchar *minute;
	gboolean use_24_hour;
	gint i, hour, row;
	gdouble hour_minute_x, hour_minute_width;
	cairo_t *cr;

	use_24_hour = e_cal_model_get_use_24_hour_format (model);

	/* Fill the time column in light-gray. */
	print_border (context, left, left + width, top, bottom, -1.0, DARKER_BORDER);

	/* Draw the border around the entire view. */
	cr = gtk_print_context_get_cairo_context (context);

	cairo_set_source_rgb (cr, 0, 0, 0);
	print_border (context, left, right, top, bottom, 1.0, -1.0);

	/* Draw the vertical line on the right of the time column. */
	cr = gtk_print_context_get_cairo_context (context);
	cairo_set_line_width (cr, 0.0);
	cairo_move_to (cr, left + width, bottom);
	cairo_line_to (cr, left + width, top);
	cairo_stroke (cr);

	/* Calculate the row height. */
	if (top > bottom)
		yinc = (top - bottom) / (pdi->end_hour - pdi->start_hour);
	else
		yinc = (bottom - top) / (pdi->end_hour - pdi->start_hour);

        /* Get the 2 fonts we need. */
	font_size = yinc * 0.6;
	max_font_size = width * 0.45;
	hour_font_size = MIN (font_size, max_font_size);
	font_hour = get_font_for_size (hour_font_size, PANGO_WEIGHT_BOLD);

	font_size = yinc * 0.33;
	max_font_size = width * 0.2;
	minute_font_size = MIN (font_size, max_font_size);
	font_minute = get_font_for_size (minute_font_size, PANGO_WEIGHT_BOLD);
	hour_minute_width = evo_calendar_print_renderer_get_width (
		context, font_minute, use_24_hour ? "00" : _("am"));
	if (!use_24_hour)
		hour_minute_width = MAX (
			hour_minute_width,
			evo_calendar_print_renderer_get_width (
				context, font_minute, _("pm")));

	row = 0;
	hour_minute_x = left + width - hour_minute_width - 3;
	for (i = pdi->start_hour; i < pdi->end_hour; i++) {
		y = top + yinc * (row + 1);
		cr = gtk_print_context_get_cairo_context (context);
		cairo_set_source_rgb (cr, 0, 0, 0);

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
		print_text (
			context, font_hour, buf, PANGO_ALIGN_RIGHT,
			left, hour_minute_x,
			y - yinc, y - yinc + hour_font_size);
		print_text (
			context, font_minute, minute, PANGO_ALIGN_LEFT,
			hour_minute_x, left + width - 3,
			y - yinc, y - yinc + minute_font_size);

                /* Draw the horizontal line between hours, across the entire
		   width of the day view. */
		cr = gtk_print_context_get_cairo_context (context);
		cairo_move_to (cr, left, y);
		cairo_line_to (cr, right, y);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);

		/* Draw the horizontal line for the 1/2-hours, across the
		 * entire width except for part of the time column. */
		cairo_move_to (cr, left + width * 0.6, y - yinc / 2);
		cairo_line_to (cr, right, y - yinc / 2);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
		row++;
	}

	pango_font_description_free (font_hour);
	pango_font_description_free (font_minute);
}

/* This adds one event to the view, adding it to the appropriate array. */
static gint
print_day_add_event (ECalModelComponent *comp_data,
                     time_t start,
                     time_t end,
                     ICalTimezone *zone,
                     gint days_shown,
                     time_t *day_starts,
                     GArray *long_events,
                     GArray **events)

{
	EDayViewEvent event;
	gint day, offset;
	ICalTime *start_tt, *end_tt;

#if 0
	g_print ("Day view lower: %s", ctime (&day_starts[0]));
	g_print ("Day view upper: %s", ctime (&day_starts[days_shown]));
	g_print ("Event start: %s", ctime (&start));
	g_print ("Event end  : %s", ctime (&end));
	g_print ("Event summary: %s\n", i_cal_component_get_summary (comp_data->icalcomp));
#endif

	/* Check that the event times are valid. */
	g_return_val_if_fail (start <= end, -1);
	g_return_val_if_fail (start < day_starts[days_shown], -1);
	g_return_val_if_fail (end > day_starts[0], -1);

	start_tt = i_cal_time_new_from_timet_with_zone (start, FALSE, zone);
	end_tt = i_cal_time_new_from_timet_with_zone (end, FALSE, zone);

	event.comp_data = comp_data;
	event.start = start;
	event.end = end;
	event.canvas_item = NULL;

	/* Calculate the start & end minute, relative to the top of the
	 * display. */
	/*offset = day_view->first_hour_shown * 60
	  + day_view->first_minute_shown;*/
	offset = 0;
	event.start_minute = i_cal_time_get_hour (start_tt) * 60 + i_cal_time_get_minute (start_tt) - offset;
	event.end_minute = i_cal_time_get_hour (end_tt) * 60 + i_cal_time_get_minute (end_tt) - offset;

	event.start_row_or_col = 0;
	event.num_columns = 0;

	/* Find out which array to add the event to. */
	for (day = 0; day < days_shown; day++) {
		if (start >= day_starts[day] && end <= day_starts[day + 1]) {

			/* Special case for when the appointment ends at
			 * midnight, i.e. the start of the next day. */
			if (end == day_starts[day + 1]) {

				/* If the event last the entire day, then we
				 * skip it here so it gets added to the top
				 * canvas. */
				if (start == day_starts[day])
				    break;

				event.end_minute = 24 * 60;
			}
			g_array_append_val (events[day], event);
			return day;
		}
	}

	/* The event wasn't within one day so it must be a long event,
	 * i.e. shown in the top canvas. */
	g_array_append_val (long_events, event);
	return E_DAY_VIEW_LONG_EVENT;
}

static gboolean
print_day_details_cb (ICalComponent *comp,
		      ICalTime *istart,
		      ICalTime *iend,
		      gpointer user_data,
		      GCancellable *cancellable,
		      GError **error)
{
	ECalModelGenerateInstancesData *mdata = (ECalModelGenerateInstancesData *) user_data;
	struct pdinfo *pdi = (struct pdinfo *) mdata->cb_data;
	ICalTime *startt, *endtt;

	startt = i_cal_time_convert_to_zone (istart, pdi->zone);
	endtt = i_cal_time_convert_to_zone (iend, pdi->zone);

	print_day_add_event (
		mdata->comp_data,
		i_cal_time_as_timet_with_zone (startt, pdi->zone),
		i_cal_time_as_timet_with_zone (endtt, pdi->zone),
		pdi->zone, pdi->days_shown, pdi->day_starts,
		pdi->long_events, pdi->events);

	g_clear_object (&startt);
	g_clear_object (&endtt);

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
			g_object_run_dispose (G_OBJECT (event->canvas_item));
	}

	g_array_set_size (array, 0);
}

static const gchar *
get_type_as_string (ICalParameterCutype cutype)
{
	const gchar *res;

	switch (cutype) {
		case I_CAL_CUTYPE_NONE:       res = NULL;            break;
		case I_CAL_CUTYPE_INDIVIDUAL: res = _("Individual"); break;
		case I_CAL_CUTYPE_GROUP:      res = _("Group");      break;
		case I_CAL_CUTYPE_RESOURCE:   res = _("Resource");   break;
		case I_CAL_CUTYPE_ROOM:       res = _("Room");       break;
		default:                      res = _("Unknown");    break;
	}

	return res;
}

static const gchar *
get_role_as_string (ICalParameterRole role)
{
	const gchar *res;

	switch (role) {
		case I_CAL_ROLE_NONE:           res = NULL;                      break;
		case I_CAL_ROLE_CHAIR:          res = _("Chair");                break;
		case I_CAL_ROLE_REQPARTICIPANT: res = _("Required Participant"); break;
		case I_CAL_ROLE_OPTPARTICIPANT: res = _("Optional Participant"); break;
		case I_CAL_ROLE_NONPARTICIPANT: res = _("Non-Participant");      break;
		default:                        res = _("Unknown");              break;
	}

	return res;
}

static gdouble
print_attendees (GtkPrintContext *context,
                 PangoFontDescription *font,
                 cairo_t *cr,
                 gdouble left,
                 gdouble right,
                 gdouble top,
                 gdouble bottom,
                 ECalComponent *comp,
                 gint page_nr,
                 gint *pages)
{
	GSList *attendees = NULL, *l;

	g_return_val_if_fail (context != NULL, top);
	g_return_val_if_fail (font != NULL, top);
	g_return_val_if_fail (cr != NULL, top);

	attendees = e_cal_component_get_attendees (comp);

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;
		const gchar *value;

		if (!attendee)
			continue;

		value = e_cal_util_get_attendee_email (attendee);
		if (value && *value) {
			GString *text;
			const gchar *tmp;

			tmp = get_type_as_string (e_cal_component_attendee_get_cutype (attendee));
			text = g_string_new (tmp ? tmp : "");

			if (tmp)
				g_string_append_c (text, ' ');

			if (e_cal_component_attendee_get_cn (attendee) && e_cal_component_attendee_get_cn (attendee)[0])
				g_string_append (text, e_cal_component_attendee_get_cn (attendee));
			else {
				g_string_append (text, e_cal_util_strip_mailto (value));
			}

			tmp = get_role_as_string (e_cal_component_attendee_get_role (attendee));
			if (tmp) {
				g_string_append (text, " (");
				g_string_append (text, tmp);
				g_string_append_c (text, ')');
			}

			if (top > bottom) {
				top = 10.0;
				cairo_show_page (cr);
			}

			top = bound_text (
				context, font, text->str, -1, left + 40.0,
				top, right, bottom, FALSE, NULL, NULL, pages);

			g_string_free (text, TRUE);
		}
	}

	g_slist_free_full (attendees, e_cal_component_attendee_free);

	return top;
}

static gchar *
get_summary_with_location (ICalComponent *icomp)
{
	const gchar *location;
	gchar *text, *summary;

	g_return_val_if_fail (icomp != NULL, NULL);

	summary = e_calendar_view_dup_component_summary (icomp);

	location = i_cal_component_get_location (icomp);
	if (location && *location) {
		text = g_strdup_printf ("%s (%s)", summary ? summary : "", location);
		g_free (summary);
	} else {
		text = summary ? summary : g_strdup ("");
	}

	return text;
}

static void
print_day_long_event (GtkPrintContext *context,
                      PangoFontDescription *font,
                      gdouble left,
                      gdouble right,
                      gdouble top,
                      gdouble bottom,
                      gdouble row_height,
                      EDayViewEvent *event,
                      struct pdinfo *pdi,
                      ECalModel *model)
{
	gdouble x1, x2, y1, y2;
	gdouble left_triangle_width = -1.0, right_triangle_width = -1.0;
	gchar *text;
	gchar buffer[32];
	struct tm date_tm;
	GdkRGBA bg_rgba;

	if (!is_comp_data_valid (event))
		return;

	/* If the event starts before the first day being printed, draw a
	 * triangle. (Note that I am assuming we are just showing 1 day at
	 * the moment.) */
	if (event->start < pdi->day_starts[0])
		left_triangle_width = 4;

	/* If the event ends after the last day being printed, draw a
	 * triangle. */
	if (event->end > pdi->day_starts[1])
		right_triangle_width = 4;

	x1 = left + 10;
	x2 = right - 10;
	y1 = top + event->start_row_or_col * row_height + 1;
	y2 = y1 + row_height - 1;

	if (!e_cal_model_get_rgba_for_component (model, event->comp_data, &bg_rgba)) {
		bg_rgba.red = 0.95;
		bg_rgba.green = 0.95;
		bg_rgba.blue = 0.95;
		bg_rgba.alpha = 1.0;
	}

	print_border_with_triangles (
		context, x1, x2, y1, y2, 0.5, bg_rgba,
		left_triangle_width,
		right_triangle_width);

	/* If the event starts after the first day being printed, we need to
	 * print the start time. */
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
		x1 += print_text_line (context, font, buffer, PANGO_ALIGN_LEFT, x1, x2, y1, y2, FALSE, &bg_rgba);
	}

	/* If the event ends before the end of the last day being printed,
	 * we need to print the end time. */
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
		x2 -= print_text_line (context, font, buffer, PANGO_ALIGN_RIGHT, x1, x2, y1, y2, FALSE, &bg_rgba);
	}

	/* Print the text. */
	text = get_summary_with_location (event->comp_data->icalcomp);

	x1 += 4;
	x2 -= 4;
	print_text_line (context, font, text, PANGO_ALIGN_CENTER, x1, x2, y1, y2, TRUE, &bg_rgba);

	g_free (text);
}

static void
print_day_event (GtkPrintContext *context,
                 PangoFontDescription *font,
                 gdouble left,
                 gdouble right,
                 gdouble top,
                 gdouble bottom,
                 EDayViewEvent *event,
                 struct pdinfo *pdi,
                 ECalModel *model)
{
	gdouble x1, x2, y1, y2, col_width, row_height;
	gint start_offset, end_offset, start_row, end_row;
	gchar *text, start_buffer[32], end_buffer[32];
	gboolean display_times = FALSE;
	struct tm date_tm;
	GdkRGBA bg_rgba;

	if (!is_comp_data_valid (event))
		return;

	if ((event->start_minute >= pdi->end_minute_offset)
	    || (event->end_minute <= pdi->start_minute_offset))
		return;

	start_offset = event->start_minute - pdi->start_minute_offset;
	end_offset = event->end_minute - pdi->start_minute_offset;

	start_row = start_offset / pdi->mins_per_row;
	start_row = MAX (0, start_row);
	end_row = (end_offset - 1) / pdi->mins_per_row;
	end_row = MIN (pdi->rows - 1, end_row);
	col_width = (right - left) /
		pdi->cols_per_row[event->start_minute / pdi->mins_per_row];

	if (start_offset != start_row * pdi->mins_per_row
	    || end_offset != (end_row + 1) * pdi->mins_per_row)
		display_times = TRUE;

	x1 = left + event->start_row_or_col * col_width;
	x2 = x1 + event->num_columns * col_width - DAY_VIEW_EVENT_X_PAD;

	row_height = (bottom - top) / pdi->rows;
	y1 = top + start_row * row_height;
	y2 = top + (end_row + 1) * row_height;
#if 0
	g_print (
		"Event: %g,%g %g,%g\n  row_height: %g start_row: %i top: %g rows: %i\n",
		x1, y1, x2, y2, row_height, start_row, top, pdi->rows);
#endif

	if (!e_cal_model_get_rgba_for_component (model, event->comp_data, &bg_rgba)) {
		bg_rgba.red = 0.95;
		bg_rgba.green = 0.95;
		bg_rgba.blue = 0.95;
		bg_rgba.alpha = 1.0;
	}

	print_border_rgb (context, x1, x2, y1, y2, 1.0, bg_rgba);

	text = get_summary_with_location (event->comp_data->icalcomp);

	if (display_times) {
		gchar *t = NULL;

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

		t = text;
		text = g_strdup_printf (
			"%s - %s %s ",
			start_buffer, end_buffer, text);

		g_free (t);
	}

	bound_text (context, font, text, -1, x1 + 2, y1, x2 - 2, y2, FALSE, &bg_rgba, NULL, NULL);

	g_free (text);
}

static void
print_day_details (GtkPrintContext *context,
                   ECalModel *model,
                   time_t whence,
                   gdouble left,
                   gdouble right,
                   gdouble top,
                   gdouble bottom)
{
	ICalTimezone *zone;
	EDayViewEvent *event;
	PangoFontDescription *font;
	time_t start, end;
	struct pdinfo pdi = { 0 };
	gint rows_in_top_display, i, rows_with_30_mins;
	gdouble font_size, max_font_size;
	cairo_t *cr;
	GdkPixbuf *pixbuf = NULL;
#define LONG_DAY_EVENTS_TOP_SPACING 4
#define LONG_DAY_EVENTS_BOTTOM_SPACING 2

	zone = e_cal_model_get_timezone (model);

	start = time_day_begin_with_zone (whence, zone);
	end = time_day_end_with_zone (start, zone);

	pdi.days_shown = 1;
	pdi.day_starts[0] = start;
	pdi.day_starts[1] = end;
	pdi.long_events = g_array_new (FALSE, FALSE, sizeof (EDayViewEvent));
	pdi.events[0] = g_array_new (FALSE, FALSE, sizeof (EDayViewEvent));
	pdi.start_hour = e_cal_model_get_work_day_start_hour (model);
	pdi.end_hour = e_cal_model_get_work_day_end_hour (model);
	if (e_cal_model_get_work_day_end_minute (model) != 0)
		pdi.end_hour++;
	pdi.mins_per_row = get_day_view_time_divisions ();
	pdi.rows = (pdi.end_hour - pdi.start_hour) * (60 / pdi.mins_per_row);
	pdi.start_minute_offset = pdi.start_hour * 60;
	pdi.end_minute_offset = pdi.end_hour * 60;
	pdi.use_24_hour_format = e_cal_model_get_use_24_hour_format (model);
	pdi.zone = e_cal_model_get_timezone (model);

	/* Get the events from the server. */
	e_cal_model_generate_instances_sync (model, start, end, NULL, print_day_details_cb, &pdi);
	qsort (
		pdi.long_events->data, pdi.long_events->len,
		sizeof (EDayViewEvent), e_day_view_event_sort_func);
	qsort (
		pdi.events[0]->data, pdi.events[0]->len,
		sizeof (EDayViewEvent), e_day_view_event_sort_func);

	/* Also print events outside of work hours */
	if (pdi.events[0]->len > 0) {
		ICalTime *tt;

		event = &g_array_index (pdi.events[0], EDayViewEvent, 0);
		tt = i_cal_time_new_from_timet_with_zone (event->start, FALSE, zone);
		if (i_cal_time_get_hour (tt) < pdi.start_hour)
			pdi.start_hour = i_cal_time_get_hour (tt);
		pdi.start_minute_offset = pdi.start_hour * 60;
		g_clear_object (&tt);

		event = &g_array_index (pdi.events[0], EDayViewEvent, pdi.events[0]->len - 1);
		tt = i_cal_time_new_from_timet_with_zone (event->end, FALSE, zone);
		if (i_cal_time_get_hour (tt) > pdi.end_hour || i_cal_time_get_hour (tt) == 0) {
			pdi.end_hour = i_cal_time_get_hour (tt) ? i_cal_time_get_hour (tt) : 24;
			if (i_cal_time_get_minute (tt) > 0)
				pdi.end_hour++;
		}
		pdi.end_minute_offset = pdi.end_hour * 60;
		g_clear_object (&tt);

		pdi.rows = (pdi.end_hour - pdi.start_hour) * (60 / pdi.mins_per_row);
	}

	/* Lay them out the long events, across the top of the page. */
	e_day_view_layout_long_events (
		pdi.long_events, pdi.days_shown,
		pdi.day_starts, &rows_in_top_display);

	 /*Print the long events. */
	font = get_font_for_size (12, PANGO_WEIGHT_NORMAL);

	/* We always leave space for DAY_VIEW_MIN_ROWS_IN_TOP_DISPLAY in the
	 * top display, but we may have more rows than that, in which case
	 * the main display area will be compressed. */
	/* Limit long day event to half the height of the panel */
	rows_in_top_display = MIN (
		MAX (rows_in_top_display,
		DAY_VIEW_MIN_ROWS_IN_TOP_DISPLAY),
		(bottom - top) * 0.5 / DAY_VIEW_ROW_HEIGHT);

	if (rows_in_top_display > pdi.long_events->len)
		rows_in_top_display = pdi.long_events->len;

	for (i = 0; i < rows_in_top_display && i < pdi.long_events->len; i++) {
		event = &g_array_index (pdi.long_events, EDayViewEvent, i);
		print_day_long_event (
			context, font, left, right,
			top + LONG_DAY_EVENTS_TOP_SPACING, bottom,
			DAY_VIEW_ROW_HEIGHT, event, &pdi, model);
	}

	cr = gtk_print_context_get_cairo_context (context);

	if (rows_in_top_display < pdi.long_events->len) {
		/* too many events */
		gint x, y;

		if (!pixbuf) {
			GError *error = NULL;

			pixbuf = gdk_pixbuf_new_from_resource ("/org.gnome.Evolution/jump.svg", &error);
			if (!pixbuf)
				g_warning ("%s: Failed to load 'jump.svg': %s", G_STRFUNC, error ? error->message : "Unknown error");
			g_clear_error (&error);
		}

		/* Right align - 10 comes from print_day_long_event  too */
		x = right - gdk_pixbuf_get_width (pixbuf) * 0.5 - 10;
		/* Placing '...' over the last all day event entry printed. '-1 -1' comes
			from print_long_day_event (top / bottom spacing in each cell) */
		y = top + LONG_DAY_EVENTS_TOP_SPACING
			+ DAY_VIEW_ROW_HEIGHT * (i - 1)
			+ (DAY_VIEW_ROW_HEIGHT - 1 - 1) * 0.5;

		cairo_save (cr);
		cairo_scale (cr, 0.5, 0.5);
		gdk_cairo_set_source_pixbuf (cr, pixbuf, x * 2.0, y * 2.0);
		cairo_paint (cr);
		cairo_restore (cr);
	}

	if (!rows_in_top_display)
		rows_in_top_display++;

	/* Draw the border around the long events. */

	cairo_set_source_rgb (cr, 0, 0, 0);
	print_border (
		context, left, right,
		top, top + rows_in_top_display * DAY_VIEW_ROW_HEIGHT +
		LONG_DAY_EVENTS_TOP_SPACING + LONG_DAY_EVENTS_BOTTOM_SPACING,
		1.0, -1.0);

	/* Adjust the area containing the main display. */
	top += rows_in_top_display * DAY_VIEW_ROW_HEIGHT
		+ LONG_DAY_EVENTS_TOP_SPACING
		+ LONG_DAY_EVENTS_BOTTOM_SPACING;

	/* Draw the borders, lines, and times down the left. */
	print_day_background (
		context, model, whence, &pdi,
		left, right, top, bottom);
	/* Now adjust to get rid of the time column. */
	left += DAY_VIEW_TIME_COLUMN_WIDTH;

	/* lay out the short events, within the day. */
	e_day_view_layout_day_events (
		pdi.events[0], CALC_DAY_VIEW_ROWS (pdi.mins_per_row),
		pdi.mins_per_row, pdi.cols_per_row, -1);

	/* use font like with 30 minutes time division */
	rows_with_30_mins = (pdi.end_hour - pdi.start_hour) * (60 / 30);
	if (rows_with_30_mins <= 0)
		rows_with_30_mins = 2;

	pango_font_description_free (font);

	/* print the short events. */
	if (top > bottom)
		max_font_size = ((top - bottom) / rows_with_30_mins) - 4;
	else
		max_font_size = ((bottom - top) / rows_with_30_mins) - 4;
	font_size = MIN (DAY_NORMAL_FONT_SIZE, max_font_size);
	font = get_font_for_size (font_size, PANGO_WEIGHT_NORMAL);

	for (i = 0; i < pdi.events[0]->len; i++) {
		event = &g_array_index (pdi.events[0], EDayViewEvent, i);
		print_day_event (
			context, font, left, right, top, bottom,
			event, &pdi, model);
	}

	/* Free everything. */
	if (pixbuf)
		g_object_unref (pixbuf);
	free_event_array (pdi.long_events);
	pango_font_description_free (font);
	g_array_free (pdi.long_events, TRUE);
	free_event_array (pdi.events[0]);
	g_array_free (pdi.events[0], TRUE);
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
print_week_long_event (GtkPrintContext *context,
                       PangoFontDescription *font,
                       struct psinfo *psi,
                       gdouble x1,
                       gdouble x2,
                       gdouble y1,
                       gdouble row_height,
                       EWeekViewEvent *event,
                       EWeekViewEventSpan *span,
                       gchar *text,
                       GdkRGBA bg_rgba)
{
	gdouble left_triangle_width = -1.0, right_triangle_width = -1.0;
	struct tm date_tm;
	gchar buffer[32];

	/* If the event starts before the first day of the span, draw a
	 * triangle to indicate it continues. */
	if (event->start < psi->day_starts[span->start_day])
		left_triangle_width = 4;

	/* If the event ends after the last day of the span, draw a
	 * triangle. */
	if (event->end > psi->day_starts[span->start_day + span->num_days])
		right_triangle_width = 4;

	print_border_with_triangles (
		context, x1 + 6, x2 - 6, y1, y1 + row_height, 0.0, bg_rgba,
		left_triangle_width, right_triangle_width);

	x1 += 6;
	x2 -= 6;

	/* If the event starts after the first day being printed, we need to
	 * print the start time. */
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

		x1 += 2;
		x1 += print_text_line (
			context, font, buffer, PANGO_ALIGN_LEFT,
			x1, x2 - 2, y1, y1 + row_height, TRUE, &bg_rgba);
	}

	/* If the event ends before the end of the last day being printed,
	 * we need to print the end time. */
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

		x2 -= 2;
		x2 -= print_text_line (
			context, font, buffer, PANGO_ALIGN_RIGHT,
			x1 + 2, x2, y1, y1 + row_height, TRUE, &bg_rgba);
	}

	x1 += 2;
	x2 -= 2;
	print_text_line (context, font, text, PANGO_ALIGN_CENTER, x1, x2, y1, y1 + row_height, TRUE, &bg_rgba);
}

static void
print_week_day_event (GtkPrintContext *context,
                      PangoFontDescription *font,
                      struct psinfo *psi,
                      gdouble x1,
                      gdouble x2,
                      gdouble y1,
                      gdouble row_height,
                      EWeekViewEvent *event,
                      EWeekViewEventSpan *span,
                      gchar *text,
                      GdkRGBA bg_rgba)
{
	struct tm date_tm;
	gchar buffer[32];

	date_tm.tm_year = 2001;
	date_tm.tm_mon = 0;
	date_tm.tm_mday = 1;
	date_tm.tm_hour = event->start_minute / 60;
	date_tm.tm_min = event->start_minute % 60;
	date_tm.tm_sec = 0;
	date_tm.tm_isdst = -1;

	e_time_format_time (&date_tm, psi->use_24_hour_format, FALSE,
			    buffer, sizeof (buffer));
	print_rectangle (context, x1 + 1, y1, x2 - x1 - 2, row_height, bg_rgba);
	x1 += print_text_line (
		context, font, buffer, PANGO_ALIGN_LEFT,
		x1 + 2, x2 - 3, y1, y1 + row_height, TRUE, &bg_rgba) + 4;

	if (psi->weeks_shown <= 2) {
		date_tm.tm_hour = event->end_minute / 60;
		date_tm.tm_min = event->end_minute % 60;

		e_time_format_time (&date_tm, psi->use_24_hour_format, FALSE,
				    buffer, sizeof (buffer));

		x1 += print_text_line (
			context, font, buffer, PANGO_ALIGN_LEFT,
			x1, x2 - 3, y1, y1 + row_height, TRUE, &bg_rgba) + 4;
	}

	print_text_line (
		context, font, text, PANGO_ALIGN_LEFT,
		x1, x2 - 3, y1, y1 + row_height, TRUE, &bg_rgba);
}

static void
print_week_event (GtkPrintContext *context,
                  PangoFontDescription *font,
                  struct psinfo *psi,
                  gdouble left,
                  gdouble top,
                  gdouble cell_width,
                  gdouble cell_height,
                  ECalModel *model,
                  EWeekViewEvent *event,
                  GArray *spans)
{
	EWeekViewEventSpan *span;
	gint span_num;
	gchar *text;
	gint num_days, start_x, start_y, start_h, end_x, end_y, end_h;
	gdouble x1, x2, y1;
	GdkRGBA bg_rgba;
	GdkPixbuf *pixbuf = NULL;

	if (!is_comp_data_valid (event))
		return;

	text = get_summary_with_location (event->comp_data->icalcomp);

	for (span_num = 0; span_num < event->num_spans; span_num++) {
		span = &g_array_index (spans, EWeekViewEventSpan,
				       event->spans_index + span_num);

		if (e_week_view_layout_get_span_position (
		     event, span,
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

			x1 = left + start_x * cell_width;
			x2 = left + (end_x + 1) * cell_width;
			y1 = top + start_y * cell_height
				 + psi->header_row_height
				 + span->row * (psi->row_height + 2);

			if (!e_cal_model_get_rgba_for_component (model, event->comp_data, &bg_rgba)) {
				bg_rgba.red = 0.9;
				bg_rgba.green = 0.9;
				bg_rgba.blue = 0.9;
				bg_rgba.alpha = 1.0;
			}

			if (print_is_one_day_week_event (event, span,
							 psi->day_starts)) {
				print_week_day_event (
					context, font, psi,
					x1, x2, y1, psi->row_height,
					event, span, text, bg_rgba);
			} else {
				print_week_long_event (
					context, font, psi,
					x1, x2, y1, psi->row_height,
					event, span, text, bg_rgba);
			}
		} else {
			cairo_t *cr = gtk_print_context_get_cairo_context (context);

			e_week_view_layout_get_day_position (
				span->start_day,
				psi->multi_week_view,
				psi->weeks_shown,
				psi->display_start_weekday,
				psi->compress_weekend,
				&start_x, &start_y, &start_h);

			y1 = top + start_y * cell_height
				 + psi->header_row_height
				 + psi->rows_per_cell * (psi->row_height + 2);

			if (span->row >= psi->rows_per_compressed_cell && psi->compress_weekend) {
				GDateWeekday end_weekday;
				gboolean end_on_weekend;

				end_weekday = e_weekday_add_days (
					psi->display_start_weekday,
					span->start_day);

				end_on_weekend =
					(end_weekday == G_DATE_SATURDAY) ||
					(end_weekday == G_DATE_SUNDAY);

				if (end_on_weekend) {
					y1 = top + start_y * cell_height
						 + psi->header_row_height
						 + psi->rows_per_compressed_cell * (psi->row_height + 2);

				}
			}

			if (!pixbuf) {
				GError *error = NULL;

				pixbuf = gdk_pixbuf_new_from_resource ("/org.gnome.Evolution/jump.svg", &error);
				if (!pixbuf)
					g_warning ("%s: Failed to load 'jump.svg': %s", G_STRFUNC, error ? error->message : "Unknown error");
				g_clear_error (&error);
			}

			x1 = left + (start_x + 1) * cell_width - 6 -
				gdk_pixbuf_get_width (pixbuf) * 0.5;

			cairo_save (cr);
			cairo_scale (cr, 0.5, 0.5);
			gdk_cairo_set_source_pixbuf (cr, pixbuf, x1 * 2.0, y1 * 2.0);
			cairo_paint (cr);
			cairo_restore (cr);
		}
	}

	if (pixbuf)
		g_object_unref (pixbuf);

	g_free (text);
}

static void
print_week_view_background (GtkPrintContext *context,
                            PangoFontDescription *font,
                            struct psinfo *psi,
                            gdouble left,
                            gdouble top,
                            gdouble cell_width,
                            gdouble cell_height)
{
	struct tm tm;
	gint day, day_x, day_y, day_h;
	gdouble x1, x2, y1, y2, font_size, fillcolor;
	const gchar *format_string;
	gchar buffer[128];
	cairo_t *cr;

	font_size = get_font_size (font);

	for (day = 0; day < psi->days_shown; day++) {
		e_week_view_layout_get_day_position
			(day, psi->multi_week_view, psi->weeks_shown,
			 psi->display_start_weekday, psi->compress_weekend,
			 &day_x, &day_y, &day_h);

		x1 = left + day_x * cell_width;
		x2 = left + (day_x + 1) * cell_width;
		y1 = top + day_y * cell_height;
		y2 = y1 + day_h * cell_height;

		convert_timet_to_struct_tm (psi->day_starts[day], psi->zone, &tm);

		/* In the month view we draw a grey background for the end
		 * of the previous month and the start of the following. */
		fillcolor = -1.0;
		if (psi->multi_week_view && (tm.tm_mon != psi->month))
			fillcolor = DARKER_BORDER;

		print_border (context, x1, x2, y1, y2, 1.0, fillcolor);

		if (psi->multi_week_view) {
			if (!day || tm.tm_mday == 1)
				format_string = _("%d %B");
			else
				format_string = "%d";
		} else {
			cr = gtk_print_context_get_cairo_context (context);

			cairo_move_to (
				cr, x1 + 0.1 * cell_width,
				y1 + psi->header_row_height - 4);
			cairo_line_to (
				cr, x2,
				y1 + psi->header_row_height - 4);

			cairo_set_source_rgb (cr, 0, 0, 0);
			cairo_set_line_width (cr, 0.5);
			cairo_stroke (cr);

			/* strftime format %A = full weekday name, %d = day of
			 * month, %B = full month name. You can change the
			 * order but don't change the specifiers or add
			 * anything. */
			format_string = _("%A %d %B");

		}

		e_utf8_strftime (buffer, sizeof (buffer), format_string, &tm);

		print_text_line (
			context, font, buffer, PANGO_ALIGN_RIGHT,
			x1, x2 - 4, y1 + 2, y1 + 2 + font_size, TRUE, NULL);
	}
}

/* This adds one event to the view, adding it to the appropriate array. */
static gboolean
print_week_summary_cb (ICalComponent *comp,
		       ICalTime *istart,
		       ICalTime *iend,
		       gpointer user_data,
		       GCancellable *cancellable,
		       GError **error)
{
	EWeekViewEvent event;
	time_t start, end;
	ECalModelGenerateInstancesData *mdata = (ECalModelGenerateInstancesData *) user_data;
	struct psinfo *psi = (struct psinfo *) mdata->cb_data;
	ICalTime *startt, *endtt;

	startt = i_cal_time_convert_to_zone (istart, psi->zone);
	endtt = i_cal_time_convert_to_zone (iend, psi->zone);

	/* Check that the event times are valid. */

	start = i_cal_time_as_timet_with_zone (startt, psi->zone);
	end = i_cal_time_as_timet_with_zone (endtt, psi->zone);

	event.start_minute = i_cal_time_get_hour (startt) * 60 + i_cal_time_get_minute (startt);
	event.end_minute = i_cal_time_get_hour (endtt) * 60 + i_cal_time_get_minute (endtt);

	if (event.end_minute == 0 && start != end)
		event.end_minute = 24 * 60;

	g_clear_object (&startt);
	g_clear_object (&endtt);

#if 0
	g_print (
		"View start:%li end:%li  Event start:%li end:%li\n",
		psi->day_starts[0], psi->day_starts[psi->days_shown],
		start, end);
#endif

	g_return_val_if_fail (start <= end, TRUE);
	g_return_val_if_fail (start < psi->day_starts[psi->days_shown], TRUE);
	g_return_val_if_fail (end > psi->day_starts[0], TRUE);

	event.comp_data = g_object_ref (mdata->comp_data);

	event.start = start;
	event.end = end;
	event.spans_index = 0;
	event.num_spans = 0;

	g_array_append_val (psi->events, event);

	return TRUE;
}

static void
print_week_summary (GtkPrintContext *context,
                    ECalModel *model,
                    time_t whence,
                    gboolean multi_week_view,
                    gint weeks_shown,
                    gint month,
                    gdouble font_size,
                    gdouble font_size_background,
                    gdouble left,
                    gdouble right,
                    gdouble top,
                    gdouble bottom)
{
	ICalTimezone *zone;
	EWeekViewEvent *event;
	struct psinfo psi = { 0 };
	time_t day_start;
	gint rows_per_day[E_WEEK_VIEW_MAX_WEEKS * 7], day, event_num;
	GArray *spans;
	PangoFontDescription *font, *font_background;
	gdouble cell_width, cell_height;

	zone = e_cal_model_get_timezone (model);

	psi.days_shown = weeks_shown * 7;
	psi.events = g_array_new (FALSE, FALSE, sizeof (EWeekViewEvent));
	psi.multi_week_view = multi_week_view;
	psi.weeks_shown = weeks_shown;
	psi.month = month;
	psi.zone = zone;

	/* Get a few config settings. */
	if (multi_week_view)
		psi.compress_weekend = e_cal_model_get_compress_weekend (model);
	else
		psi.compress_weekend = TRUE;
	psi.use_24_hour_format = e_cal_model_get_use_24_hour_format (model);

	psi.display_start_weekday = e_cal_model_get_week_start_day (model);

	/* If weekends are compressed then we can't start on a Sunday. */
	if (psi.compress_weekend && psi.display_start_weekday == G_DATE_SUNDAY)
		psi.display_start_weekday = G_DATE_SATURDAY;

	day_start = time_day_begin_with_zone (whence, zone);
	for (day = 0; day <= psi.days_shown; day++) {
		psi.day_starts[day] = day_start;
		day_start = time_add_day_with_zone (day_start, 1, zone);
	}

	/* Get the events from the server. */
	e_cal_model_generate_instances_sync (
		model,
		psi.day_starts[0], psi.day_starts[psi.days_shown],
		NULL, print_week_summary_cb, &psi);
	qsort (
		psi.events->data, psi.events->len,
		sizeof (EWeekViewEvent), e_week_view_event_sort_func);

	/* Layout the events. */
	spans = e_week_view_layout_events (
		psi.events, NULL,
		psi.multi_week_view,
		psi.weeks_shown,
		psi.compress_weekend,
		psi.display_start_weekday,
		psi.day_starts, rows_per_day);

	/* Calculate the size of the cells. */
	if (multi_week_view) {
		cell_width = (right - left) / (psi.compress_weekend ? 6 : 7);
		cell_height = (bottom - top) / (weeks_shown * 2);
	} else {
		cell_width = (right - left) / 2;
		cell_height = (bottom - top) / 6;
	}

	/* Calculate the row height, using the normal font and with room for
	 * space or a rectangle around it. */
	psi.row_height = font_size * 1.2;
	psi.header_row_height = font_size * 1.5;

	/* Calculate how many rows we can fit into each type of cell. */
	psi.rows_per_cell = ((cell_height * 2) - psi.header_row_height)
		/ (psi.row_height + 2);
	psi.rows_per_compressed_cell = (cell_height - psi.header_row_height)
		/ (psi.row_height + 2);

	/* Draw the grid and the day names/numbers. */
	font_background = get_font_for_size (font_size_background, PANGO_WEIGHT_NORMAL);
	print_week_view_background (
		context, font_background, &psi, left, top,
		cell_width, cell_height);
	pango_font_description_free (font_background);

	/* Print the events. */
	font = get_font_for_size (font_size, PANGO_WEIGHT_NORMAL);
	for (event_num = 0; event_num < psi.events->len; event_num++) {
		event = &g_array_index (psi.events, EWeekViewEvent, event_num);
		print_week_event (
			context, font, &psi, left, top,
			cell_width, cell_height, model, event, spans);
	}

	pango_font_description_free (font);

	/* Free everything. */
	for (event_num = 0; event_num < psi.events->len; event_num++) {
		event = &g_array_index (psi.events, EWeekViewEvent, event_num);
		g_object_unref (event->comp_data);
	}
	g_array_free (psi.events, TRUE);
	g_array_free (spans, TRUE);
}

static void
print_month_summary (GtkPrintContext *context,
                     ECalModel *model,
		     ECalendarView *calendar_view,
		     EPrintView print_view_type,
                     time_t whence,
                     gdouble left,
                     gdouble right,
                     gdouble top,
                     gdouble bottom)
{
	ICalTimezone *zone;
	time_t date;
	struct tm tm;
	ICalTime *tt;
	gchar buffer[100];
	PangoFontDescription *font;
	gboolean compress_weekend;
	gint columns, col, month, weeks;
	GDateWeekday weekday;
	gint wday;
	gdouble font_size, cell_width, x1, x2, y1, y2;

	zone = e_cal_model_get_timezone (model);
	weekday = e_cal_model_get_week_start_day (model);
	compress_weekend = e_cal_model_get_compress_weekend (model);

	/* Remember which month we want. */
	date = time_day_begin_with_zone (whence, zone);
	if (date != time_month_begin_with_zone (date, zone)) {
		date = time_month_begin_with_zone (date, zone);
		date = time_add_month_with_zone (date, 1, zone);
	}
	tt = i_cal_time_new_from_timet_with_zone (date, FALSE, zone);
	month = i_cal_time_get_month (tt) - 1;
	g_clear_object (&tt);

	date = 0;
	weeks = 6;
	if (print_view_type == E_PRINT_VIEW_MONTH) {
		EWeekView *week_view;
		GDate first_day_shown;
		gboolean multi_week_view;
		gint weeks_shown;

		week_view = E_WEEK_VIEW (calendar_view);
		weeks_shown = e_week_view_get_weeks_shown (week_view);
		multi_week_view = e_week_view_get_multi_week_view (week_view);
		e_week_view_get_first_day_shown (week_view, &first_day_shown);

		if (multi_week_view) {
			weeks = weeks_shown;
			date = whence;
		}
	}

	/* Find the start of the month, and then the start of the week on
	 * or before that day. */
	if (!date)
		date = time_month_begin_with_zone (whence, zone);

	wday = e_weekday_to_tm_wday (weekday);
	date = time_week_begin_with_zone (date, wday, zone);

	/* If weekends are compressed then we can't start on a Sunday. */
	if (compress_weekend && weekday == G_DATE_SUNDAY)
		date = time_add_day_with_zone (date, -1, zone);

	/* do day names ... */

	/* We are only interested in outputting the weekday here, but we want
	 * to be able to step through the week without worrying about
	 * overflows making strftime choke, so we move near to the start of
	 * the month. */
	convert_timet_to_struct_tm (date, zone, &tm);
	tm.tm_mday = (tm.tm_mday % 7) + 7;

	font = get_font_for_size (MONTH_NORMAL_FONT_SIZE, PANGO_WEIGHT_BOLD);
	font_size = get_font_size (font);

	columns = compress_weekend ? 6 : 7;
	cell_width = (right - left) / columns;
	y1 = top + font_size * 1.5;
	y2 = y1 + font_size * 1.5;

	for (col = 0; col < columns; col++) {
		if (tm.tm_wday == 6 && compress_weekend)
			g_snprintf (
				buffer, sizeof (buffer), "%s/%s",
				e_get_weekday_name (G_DATE_SATURDAY, TRUE),
				e_get_weekday_name (G_DATE_SUNDAY, TRUE));
		else
			g_snprintf (
				buffer, sizeof (buffer), "%s",
				e_get_weekday_name (
				tm.tm_wday ? tm.tm_wday : 7, FALSE));

		x1 = left + cell_width * col;
		x2 = x1 + cell_width;

		print_border (context, x1, x2, y1, y2, 1.0, -1.0);
		print_text_line (context, font, buffer, PANGO_ALIGN_CENTER, x1, x2, y1, y2, TRUE, NULL);

		tm.tm_mday++;
		tm.tm_wday = (tm.tm_wday + 1) % 7;
	}
	pango_font_description_free (font);

	top = y2;
	print_week_summary (
		context, model, date, TRUE, weeks, month,
		MONTH_NORMAL_FONT_SIZE, MONTH_NORMAL_FONT_SIZE,
		left, right, top, bottom);
}

static void
print_todo_details (GtkPrintContext *context,
		    ETable *tasks_table,
                    time_t start,
                    time_t end,
                    gdouble left,
                    gdouble right,
                    gdouble top,
                    gdouble bottom)
{
	PangoFontDescription *font_summary;
	gdouble y, yend, x, xend;
	ICalTime *tt;
	ECalModel *model;
	gint rows, row;
	cairo_t *cr;

	/* We get the tasks directly from the TaskPad ETable. This means we
	 * get them filtered & sorted for free. */
	g_return_if_fail (tasks_table != NULL);
	model = e_task_table_get_model (E_TASK_TABLE (tasks_table));

	font_summary = get_font_for_size (12, PANGO_WEIGHT_NORMAL);

	cr = gtk_print_context_get_cairo_context (context);

	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 0.0);
	top +=2;

	titled_box (
		context, _("Tasks"), font_summary, PANGO_ALIGN_CENTER,
		&left, &top, &right, &bottom, 1.0);

	y = top;
	yend = bottom - 2;

	rows = e_table_model_row_count (E_TABLE_MODEL (model));
	for (row = 0; row < rows; row++) {
		ECalModelComponent *comp_data;
		ECalComponent *comp;
		ECalComponentText *summary;
		gint model_row;

		model_row = e_table_view_to_model_row (tasks_table, row);
		comp_data = e_cal_model_get_component_at (model, model_row);
		if (!comp_data)
			continue;

		comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (comp_data->icalcomp));
		if (!comp)
			continue;

		summary = e_cal_component_dup_summary_for_locale (comp, NULL);
		if (!summary || !e_cal_component_text_get_value (summary)) {
			e_cal_component_text_free (summary);
			g_object_unref (comp);
			continue;
		}

		x = left;
		xend = right - 2;
		if (y > bottom) {
			e_cal_component_text_free (summary);
			g_object_unref (comp);
			break;
		}

		/* Print the box to put the tick in. */
		print_border (context, x + 2, x + 8, y + 6, y + 15, 0.1, -1.0);

		/* If the task is complete, print a tick in the box. */
		tt = e_cal_component_get_completed (comp);
		if (tt) {
			g_clear_object (&tt);

			cr = gtk_print_context_get_cairo_context (context);
			cairo_set_source_rgb (cr, 0, 0, 0);
			cairo_move_to (cr, x + 3, y + 11);
			cairo_line_to (cr, x + 5, y + 14);
			cairo_line_to (cr, x + 7, y + 5.5);
			cairo_set_line_width (cr, 1);
			cairo_stroke (cr);
		}

		y = bound_text (
			context, font_summary, e_cal_component_text_get_value (summary), -1,
			x + 14, y + 4, xend, yend, FALSE, NULL, NULL, NULL);

		y += get_font_size (font_summary) - 5;
		cr = gtk_print_context_get_cairo_context (context);
		cairo_move_to (cr, x, y);
		cairo_line_to (cr, xend, y);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);

		e_cal_component_text_free (summary);
		g_object_unref (comp);
	}

	pango_font_description_free (font_summary);
}

static void
print_day_view (GtkPrintContext *context,
		ECalendarView *cal_view,
                ETable *tasks_table,
                time_t date)
{
	ECalModel *model;
	GtkPageSetup *setup;
	ICalTimezone *zone;
	gint i, days = 1;
	gdouble todo, l, week_numbers_inc, small_month_width;
	gchar buf[100];
	gdouble width, height;
	struct tm tm;

	model = e_calendar_view_get_model (cal_view);
	zone = e_cal_model_get_timezone (model);

	setup = gtk_print_context_get_page_setup (context);

	width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);
	height = gtk_page_setup_get_page_height (setup, GTK_UNIT_POINTS);
	small_month_width = calc_small_month_width (context, HEADER_HEIGHT);
	week_numbers_inc = get_show_week_numbers () ? small_month_width / 7.0 : 0;

	for (i = 0; i < days; i++) {
		todo = width * 0.75;

		/* Print the main view with all the events in. */
		print_day_details (
			context, model, date,
			0.0, todo - 2.0, HEADER_HEIGHT + 4,
			height);

		 /* Print the TaskPad down the right. */
		print_todo_details (
			context, tasks_table, 0, INT_MAX,
			todo, width, HEADER_HEIGHT + 4,
			height);

		/* Print the filled border around the header. */
		print_border (
			context, 0.0, width,
			0.0, HEADER_HEIGHT + 4, 1.0, DARKER_BORDER);

		/* Print the 2 mini calendar-months. */
		l = width - SMALL_MONTH_PAD -
			(small_month_width + week_numbers_inc) * 2 -
			SMALL_MONTH_SPACING;

		print_month_small (
			context, model, date,
			l, 2, l + small_month_width + week_numbers_inc, HEADER_HEIGHT + 2,
			DATE_MONTH | DATE_YEAR, date, date, FALSE);

		l += SMALL_MONTH_SPACING + small_month_width + week_numbers_inc;
		print_month_small (
			context, model,
			time_add_month_with_zone (date, 1, zone),
			l, 2, l + small_month_width + week_numbers_inc, HEADER_HEIGHT + 2,
			DATE_MONTH | DATE_YEAR, 0, 0, FALSE);

		/* Print the date, e.g. '8th May, 2001'. */
		convert_timet_to_struct_tm (date, zone, &tm);
		format_date (&tm, DATE_DAY | DATE_MONTH | DATE_YEAR,
			     buf, 100);

		print_text_size_bold (
			context, buf, PANGO_ALIGN_LEFT,
			4, todo, 4,
			4 + 24);

		/* Print the day, e.g. 'Tuesday'. */
		format_date (&tm, DATE_DAYNAME, buf, 100);

		print_text_size_bold (
			context, buf, PANGO_ALIGN_LEFT,
			4, todo,
			HEADER_HEIGHT + 9,
			HEADER_HEIGHT + 9 + 18);

		date = time_add_day_with_zone (date, 1, zone);
	 }
}

static void
print_work_week_background (GtkPrintContext *context,
                            ECalModel *model,
                            time_t whence,
                            struct pdinfo *pdi,
                            gdouble left,
                            gdouble right,
                            gdouble top,
                            gdouble bottom)
{
	PangoFontDescription *font_hour, *font_minute;
	gdouble yinc, y;
	gdouble width = DAY_VIEW_TIME_COLUMN_WIDTH;
	gdouble day_width;
	gdouble font_size, max_font_size, hour_font_size, minute_font_size;
	gchar buf[20];
	const gchar *minute;
	const gint LONG_EVENT_OFFSET = 6;
	gboolean use_24_hour;
	gint i, hour, row;
	gdouble hour_minute_xl, hour_minute_xr;
	cairo_t *cr;

	use_24_hour = e_cal_model_get_use_24_hour_format (model);

	/* Fill the left time column in light-gray. */
	print_border (context, left, left + width, top, bottom, -1.0, DARKER_BORDER);
	/* Fill the right time column in light-gray */
	print_border (context, right - width, right, top, bottom, -1.0, DARKER_BORDER);

	/* Draw the border around the entire view. */
	cr = gtk_print_context_get_cairo_context (context);

	cairo_set_source_rgb (cr, 0, 0, 0);
	print_border (context, left, right, top, bottom, 1.0, -1.0);

	/* Draw the vertical line on the right of the time column. */
	cr = gtk_print_context_get_cairo_context (context);
	cairo_set_line_width (cr, 0.0);
	cairo_move_to (cr, left + width, bottom);
	cairo_line_to (cr, left + width, top);
	cairo_stroke (cr);

	cairo_move_to (cr, right - width, bottom);
	cairo_line_to (cr, right - width, top);
	cairo_stroke (cr);

	/* Calculate the row height. */
	if (top > bottom)
		yinc = (top - bottom) / (pdi->end_hour - pdi->start_hour);
	else
		yinc = (bottom - top) / (pdi->end_hour - pdi->start_hour);

        /* Get the 2 fonts we need. */
	font_size = yinc * 0.6;
	max_font_size = width * 0.45;
	hour_font_size = MIN (font_size, max_font_size);
	font_hour = get_font_for_size (hour_font_size, PANGO_WEIGHT_BOLD);

	font_size = yinc * 0.33;
	max_font_size = width * 0.2;
	minute_font_size = MIN (font_size, max_font_size);
	font_minute = get_font_for_size (minute_font_size, PANGO_WEIGHT_BOLD);
	hour_minute_xr = evo_calendar_print_renderer_get_width (
		context, font_minute, use_24_hour ? "00" : _("am"));
	if (!use_24_hour)
		hour_minute_xr = MAX (
			hour_minute_xr,
			evo_calendar_print_renderer_get_width (
				context, font_minute, _("pm")));

	row = 0;
	hour_minute_xl = left + width - hour_minute_xr - 3;
	hour_minute_xr = right - hour_minute_xr - 3;
	for (i = pdi->start_hour; i < pdi->end_hour; i++) {
		y = top + yinc * (row + 1);
		cr = gtk_print_context_get_cairo_context (context);
		cairo_set_source_rgb (cr, 0, 0, 0);

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
		print_text (
			context, font_hour, buf, PANGO_ALIGN_RIGHT,
			left, hour_minute_xl,
			y - yinc, y - yinc + hour_font_size);
		print_text (
			context, font_minute, minute, PANGO_ALIGN_LEFT,
			hour_minute_xl, left + width - 3,
			y - yinc, y - yinc + minute_font_size);

		/* To the right */
		print_text (
			context, font_hour, buf, PANGO_ALIGN_RIGHT,
			right - width, hour_minute_xr,
			y - yinc, y - yinc + hour_font_size);
		print_text (
			context, font_minute, minute, PANGO_ALIGN_LEFT,
			hour_minute_xr, right - 3,
			y - yinc, y - yinc + minute_font_size);

                /* Draw the horizontal line between hours, across the entire
		   width of the day view. */
		cr = gtk_print_context_get_cairo_context (context);
		cairo_move_to (cr, left, y);
		cairo_line_to (cr, right, y);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);

		/* Draw the horizontal line for the 1/2-hours, across the
		 * entire width except for part of the time column. */
		cairo_move_to (cr, left + width * 0.6, y - yinc / 2);
		cairo_line_to (cr, right, y - yinc / 2);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
		row++;
	}

	/* Draw the vertical lines for the days */
	day_width = (right - left - 2 *width) / pdi->days_shown;
	for (i = 0; i < pdi->days_shown - 1; ++i) {
	  cr = gtk_print_context_get_cairo_context (context);
	  cairo_move_to (cr, left + width + day_width * (i + 1), top);
	  cairo_line_to (cr, left + width + day_width * (i + 1), bottom);
	  cairo_set_line_width (cr, 1);
	  cairo_stroke (cr);
	}

	/* And now the ones from the border to the hours, looks weird otherwise */
	cr = gtk_print_context_get_cairo_context (context);
	cairo_move_to (cr, left, HEADER_HEIGHT);
	cairo_line_to (cr, left, HEADER_HEIGHT + DAY_VIEW_ROW_HEIGHT + LONG_EVENT_OFFSET);

	cairo_move_to (cr, right, HEADER_HEIGHT);
	cairo_line_to (cr, right, HEADER_HEIGHT + DAY_VIEW_ROW_HEIGHT + LONG_EVENT_OFFSET);
	cairo_stroke (cr);

	pango_font_description_free (font_hour);
	pango_font_description_free (font_minute);
}

static void
print_work_week_day_details (GtkPrintContext *context,
                             ECalModel *model,
                             time_t whence,
                             gdouble left,
                             gdouble right,
                             gdouble top,
                             gdouble bottom,
                             struct pdinfo *_pdi)
{
	ICalTimezone *zone;
	EDayViewEvent *event;
	PangoFontDescription *font;
	time_t start, end;
	struct pdinfo pdi = { 0 };
	gint rows_in_top_display, i, rows_with_30_mins;
	gdouble font_size, max_font_size;
	cairo_t *cr;
	GdkPixbuf *pixbuf = NULL;
#define LONG_DAY_EVENTS_TOP_SPACING 4
#define LONG_DAY_EVENTS_BOTTOM_SPACING 2

	zone = e_cal_model_get_timezone (model);

	start = time_day_begin_with_zone (whence, zone);
	end = time_day_end_with_zone (start, zone);

	pdi.days_shown = 1;
	pdi.day_starts[0] = start;
	pdi.day_starts[1] = end;
	pdi.long_events = g_array_new (FALSE, FALSE, sizeof (EDayViewEvent));
	pdi.events[0] = g_array_new (FALSE, FALSE, sizeof (EDayViewEvent));
	pdi.start_hour = _pdi->start_hour;
	pdi.end_hour = _pdi->end_hour;
	pdi.mins_per_row = get_day_view_time_divisions ();
	pdi.rows = (pdi.end_hour - pdi.start_hour) * (60 / pdi.mins_per_row);
	pdi.start_minute_offset = pdi.start_hour * 60;
	pdi.end_minute_offset = pdi.end_hour * 60;
	pdi.use_24_hour_format = e_cal_model_get_use_24_hour_format (model);
	pdi.zone = e_cal_model_get_timezone (model);

	/* Get the events from the server. */
	e_cal_model_generate_instances_sync (model, start, end, NULL, print_day_details_cb, &pdi);
	qsort (
		pdi.long_events->data, pdi.long_events->len,
		sizeof (EDayViewEvent), e_day_view_event_sort_func);
	qsort (
		pdi.events[0]->data, pdi.events[0]->len,
		sizeof (EDayViewEvent), e_day_view_event_sort_func);

	/* TODO: This should be redundant */
	/* Also print events outside of work hours */
	if (pdi.events[0]->len > 0) {
		ICalTime *tt;

		event = &g_array_index (pdi.events[0], EDayViewEvent, 0);
		tt = i_cal_time_new_from_timet_with_zone (event->start, FALSE, zone);
		if (i_cal_time_get_hour (tt) < pdi.start_hour)
			pdi.start_hour = i_cal_time_get_hour (tt);
		pdi.start_minute_offset = pdi.start_hour * 60;
		g_clear_object (&tt);

		event = &g_array_index (pdi.events[0], EDayViewEvent, pdi.events[0]->len - 1);
		tt = i_cal_time_new_from_timet_with_zone (event->end, FALSE, zone);
		if (i_cal_time_get_hour (tt) > pdi.end_hour || i_cal_time_get_hour (tt) == 0) {
			pdi.end_hour = i_cal_time_get_hour (tt) ? i_cal_time_get_hour (tt) : 24;
			if (i_cal_time_get_minute (tt) > 0)
				pdi.end_hour++;
		}
		pdi.end_minute_offset = pdi.end_hour * 60;
		g_clear_object (&tt);

		pdi.rows = (pdi.end_hour - pdi.start_hour) * (60 / pdi.mins_per_row);
	}

	/* Lay them out the long events, across the top of the page. */
	e_day_view_layout_long_events (
		pdi.long_events, pdi.days_shown,
		pdi.day_starts, &rows_in_top_display);

	 /*Print the long events. */
	font = get_font_for_size (12, PANGO_WEIGHT_NORMAL);

	/* We always leave space for DAY_VIEW_MIN_ROWS_IN_TOP_DISPLAY in the
	 * top display, but we may have more rows than that, in which case
	 * the main display area will be compressed. */
	/* Limit long day event to half the height of the panel */
	rows_in_top_display = MIN (
		MAX (rows_in_top_display,
		DAY_VIEW_MIN_ROWS_IN_TOP_DISPLAY),
		(bottom - top) * 0.5 / DAY_VIEW_ROW_HEIGHT);

	if (rows_in_top_display > pdi.long_events->len)
		rows_in_top_display = pdi.long_events->len;

	for (i = 0; i < rows_in_top_display && i < pdi.long_events->len; i++) {
		event = &g_array_index (pdi.long_events, EDayViewEvent, i);
		print_day_long_event (
			context, font, left, right,
			top + LONG_DAY_EVENTS_TOP_SPACING, bottom,
			DAY_VIEW_ROW_HEIGHT, event, &pdi, model);
	}

	cr = gtk_print_context_get_cairo_context (context);

	if (rows_in_top_display < pdi.long_events->len) {
		/* too many events */
		gint x, y;

		if (!pixbuf) {
			GError *error = NULL;

			pixbuf = gdk_pixbuf_new_from_resource ("/org.gnome.Evolution/jump.svg", &error);
			if (!pixbuf)
				g_warning ("%s: Failed to load 'jump.svg': %s", G_STRFUNC, error ? error->message : "Unknown error");
			g_clear_error (&error);
		}

		/* Right align - 10 comes from print_day_long_event  too */
		x = right - gdk_pixbuf_get_width (pixbuf) * 0.5 - 10;
		/* Placing '...' over the last all day event entry printed. '-1 -1' comes
			from print_long_day_event (top / bottom spacing in each cell) */
		y = top + LONG_DAY_EVENTS_TOP_SPACING
			+ DAY_VIEW_ROW_HEIGHT * (i - 1)
			+ (DAY_VIEW_ROW_HEIGHT - 1 - 1) * 0.5;

		cairo_save (cr);
		cairo_scale (cr, 0.5, 0.5);
		gdk_cairo_set_source_pixbuf (cr, pixbuf, x * 2.0, y * 2.0);
		cairo_paint (cr);
		cairo_restore (cr);
	}

	if (!rows_in_top_display)
		rows_in_top_display++;

	/* Draw the border around the long events. */

	cairo_set_source_rgb (cr, 0, 0, 0);
	print_border (
		context, left, right,
		top, top + rows_in_top_display * DAY_VIEW_ROW_HEIGHT +
		LONG_DAY_EVENTS_TOP_SPACING + LONG_DAY_EVENTS_BOTTOM_SPACING,
		1.0, -1.0);

	/* Adjust the area containing the main display. */
	top += rows_in_top_display * DAY_VIEW_ROW_HEIGHT
		+ LONG_DAY_EVENTS_TOP_SPACING
		+ LONG_DAY_EVENTS_BOTTOM_SPACING;

	/* lay out the short events, within the day. */
	e_day_view_layout_day_events (
		pdi.events[0], CALC_DAY_VIEW_ROWS (pdi.mins_per_row),
		pdi.mins_per_row, pdi.cols_per_row, -1);

	/* use font like with 30 minutes time division */
	rows_with_30_mins = (pdi.end_hour - pdi.start_hour) * (60 / 30);
	if (rows_with_30_mins <= 0)
		rows_with_30_mins = 2;

	pango_font_description_free (font);

	/* print the short events. */
	if (top > bottom)
		max_font_size = ((top - bottom) / rows_with_30_mins) - 4;
	else
		max_font_size = ((bottom - top) / rows_with_30_mins) - 4;
	font_size = MIN (DAY_NORMAL_FONT_SIZE, max_font_size);
	font = get_font_for_size (font_size, PANGO_WEIGHT_NORMAL);

	for (i = 0; i < pdi.events[0]->len; i++) {
		event = &g_array_index (pdi.events[0], EDayViewEvent, i);
		print_day_event (
			context, font, left,
			right, top, bottom, event, &pdi, model);
	}

	/* Free everything. */
	if (pixbuf)
		g_object_unref (pixbuf);
	free_event_array (pdi.long_events);
	pango_font_description_free (font);
	g_array_free (pdi.long_events, TRUE);
	free_event_array (pdi.events[0]);
	g_array_free (pdi.events[0], TRUE);
}

/* Figure out what the overal hour limits are */
static gboolean
print_work_week_view_cb (ICalComponent *comp,
			 ICalTime *istart,
			 ICalTime *iend,
			 gpointer user_data,
			 GCancellable *cancellable,
			 GError **error)
{
	ECalModelGenerateInstancesData *mdata = (ECalModelGenerateInstancesData *) user_data;
	struct pdinfo *pdi = (struct pdinfo *) mdata->cb_data;
	ICalTime *startt, *endtt;

	startt = i_cal_time_convert_to_zone (istart, pdi->zone);
	endtt = i_cal_time_convert_to_zone (iend, pdi->zone);

	pdi->start_hour = MIN (pdi->start_hour, i_cal_time_get_hour (startt));

	/* If we're past the hour, use the next one */
	pdi->end_hour = MAX (pdi->end_hour, i_cal_time_get_minute (endtt) ? i_cal_time_get_hour (endtt) + 1 : i_cal_time_get_hour (endtt));

	g_clear_object (&startt);
	g_clear_object (&endtt);

	return TRUE;
}

static void
print_work_week_view (GtkPrintContext *context,
                      ECalendarView *cal_view,
                      time_t date)
{
	GtkPageSetup *setup;
	ICalTimezone *zone;
	time_t when, start, end;
	gdouble width, height, l;
	gdouble small_month_width;
	gdouble weeknum_inc;
	gint i, days, end_days;
	gchar buf[100];
	const gint LONG_EVENT_OFFSET = 6;
	struct pdinfo pdi = { 0 };
	struct tm tm;
	gdouble day_width, day_x;
	gboolean had_setup_days;
	ECalModel *model;
	GDateWeekday start_weekday, first_used_weekday, weekday;

	model = e_calendar_view_get_model (cal_view);
	zone = e_cal_model_get_timezone (model);

	setup = gtk_print_context_get_page_setup (context);

	width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);
	height = gtk_page_setup_get_page_height (setup, GTK_UNIT_POINTS);

	small_month_width = calc_small_month_width (context, HEADER_HEIGHT);
	weeknum_inc = get_show_week_numbers () ? small_month_width / 7.0 : 0;

	start_weekday = e_cal_model_get_week_start_day (model);

	if (start_weekday == G_DATE_BAD_WEEKDAY)
		start_weekday = G_DATE_MONDAY;

	first_used_weekday = start_weekday;
	pdi.start_hour = e_cal_model_get_work_day_start_hour (model);
	pdi.end_hour = e_cal_model_get_work_day_end_hour (model);

	days = 0;
	for (i = 0, weekday = start_weekday; i < 7; i++, weekday = e_weekday_get_next (weekday)) {
		if (e_cal_model_get_work_day (model, weekday)) {
			gint start_hour, start_minute, end_hour, end_minute;

			e_cal_model_get_work_day_range_for (model, weekday, &start_hour, &start_minute, &end_hour, &end_minute);

			if (end_minute)
				end_hour++;

			if (!days) {
				pdi.start_hour = start_hour;
				pdi.end_hour = end_hour;
				first_used_weekday = weekday;
			} else {
				pdi.start_hour = MIN (pdi.start_hour, start_hour);
				pdi.end_hour = MAX (pdi.end_hour, end_hour);
			}

			days++;
			end_days = i + 1;
		}
	}

	had_setup_days = days != 0;

	if (!had_setup_days) {
		days = 7;
		end_days = 7;
	}

	start = time_week_begin_with_zone (date, first_used_weekday == G_DATE_SUNDAY ? 0 : first_used_weekday, zone);
	end = time_add_day_with_zone (start, end_days, zone);

	pdi.days_shown = days;
	pdi.zone = zone;

	e_cal_model_generate_instances_sync (model, start, end, NULL, print_work_week_view_cb, &pdi);

	print_work_week_background (
		context, model, date, &pdi, 0.0, width,
		HEADER_HEIGHT + DAY_VIEW_ROW_HEIGHT + LONG_EVENT_OFFSET,
		height);

	print_border (context, 0.0, width, 0.0, HEADER_HEIGHT, 1.0, DARKER_BORDER);

	/* Print the 2 mini calendar-months. */
	l = width - SMALL_MONTH_PAD - (small_month_width + weeknum_inc) * 2 -
		SMALL_MONTH_SPACING;

	print_month_small (
		context, model, start,
		l, 4, l + small_month_width + weeknum_inc, HEADER_HEIGHT + 4,
		DATE_MONTH | DATE_YEAR, start, end, FALSE);

	l += SMALL_MONTH_SPACING + small_month_width + weeknum_inc;
	print_month_small (
		context, model,
		time_add_month_with_zone (start, 1, zone),
		l, 4, l + small_month_width + weeknum_inc, HEADER_HEIGHT + 4,
		DATE_MONTH | DATE_YEAR, start, end, FALSE);

	/* Print the start day of the week, e.g. '7th May 2001'. */
	convert_timet_to_struct_tm (start, zone, &tm);
	format_date (&tm, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
	print_text_size_bold (
		context, buf, PANGO_ALIGN_LEFT,
		3, width,
		4, 4 + 24);

	/* Print the end day of the week, e.g. '13th May 2001'. */
	/* We need to substract one or the wrong day will be printed */
	convert_timet_to_struct_tm (
		time_add_day_with_zone (end, -1, zone), zone, &tm);
	format_date (&tm, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
	print_text_size_bold (
		context, buf, PANGO_ALIGN_LEFT,
		3, width,
		24 + 3, 24 + 3 + 24);

	/* Now print each days' events */
	day_width = (width - 2 * DAY_VIEW_TIME_COLUMN_WIDTH) / days;
	when = start;
	for (i = 0, weekday = first_used_weekday; i < days; weekday = e_weekday_get_next (weekday)) {
		if (!had_setup_days || e_cal_model_get_work_day (model, weekday)) {
			day_x = DAY_VIEW_TIME_COLUMN_WIDTH + day_width * i;

			/* Print the day, e.g. 'Tuesday'. */
			convert_timet_to_struct_tm (when, zone, &tm);
			format_date (&tm, DATE_DAYNAME, buf, 100);

			print_text_size_bold (
				context, buf, PANGO_ALIGN_LEFT,
				day_x + 4, day_x + day_width,
				HEADER_HEIGHT + 4, HEADER_HEIGHT + 4 + 18);

			print_work_week_day_details (
				context, model, when,
				day_x, day_x + day_width,
				HEADER_HEIGHT, height, &pdi);

			i++;
		}

		when = time_add_day_with_zone (when, 1, zone);
	}
}

static void
print_week_view (GtkPrintContext *context,
                 ECalendarView *cal_view,
                 time_t date)
{
	GtkPageSetup *setup;
	ECalModel *model;
	ICalTimezone *zone;
	gdouble l, week_numbers_inc, small_month_width;
	gchar buf[100];
	time_t when;
	GDateWeekday week_start_day;
	gint wday;
	struct tm tm;
	gdouble width, height;

	setup = gtk_print_context_get_page_setup (context);

	width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);
	height = gtk_page_setup_get_page_height (setup, GTK_UNIT_POINTS);
	small_month_width = calc_small_month_width (context, HEADER_HEIGHT);
	week_numbers_inc = get_show_week_numbers () ? small_month_width / 7.0 : 0;

	model = e_calendar_view_get_model (cal_view);
	zone = e_cal_model_get_timezone (model);

	convert_timet_to_struct_tm (date, zone, &tm);
	week_start_day = e_cal_model_get_week_start_day (model);

	wday = e_weekday_to_tm_wday (week_start_day);
	when = time_week_begin_with_zone (date, wday, zone);

	/* If the week starts on a Sunday, we have to show the Saturday first,
	 * since the weekend is compressed. */
	if (week_start_day == G_DATE_SUNDAY) {
		if (tm.tm_wday == 6)
			when = time_add_day_with_zone (when, 6, zone);
		else
			when = time_add_day_with_zone (when, -1, zone);
	}

	/* Print the main week view. */
	print_week_summary (
		context, model, when, FALSE, 1, 0,
		WEEK_EVENT_FONT_SIZE, WEEK_SMALL_FONT_SIZE,
		0.0, width,
		HEADER_HEIGHT + 20, height);

	/* Print the border around the main view. */
	print_border (
		context, 0.0, width, HEADER_HEIGHT ,
		height, 1.0, -1.0);

	/* Print the border around the header area. */
	print_border (
		context, 0.0, width,
		0.0, HEADER_HEIGHT + 2.0 + 20, 1.0, DARKER_BORDER);

	/* Print the 2 mini calendar-months. */
	l = width - SMALL_MONTH_PAD - (small_month_width + week_numbers_inc) * 2
		- SMALL_MONTH_SPACING;
	print_month_small (
		context, model, when,
		l, 4, l + small_month_width + week_numbers_inc, HEADER_HEIGHT + 10,
		DATE_MONTH | DATE_YEAR, when,
		time_add_week_with_zone (when, 1, zone), FALSE);

	l += SMALL_MONTH_SPACING + small_month_width + week_numbers_inc;
	print_month_small (
		context, model,
		time_add_month_with_zone (when, 1, zone),
		l, 4, l + small_month_width + week_numbers_inc, HEADER_HEIGHT + 10,
		DATE_MONTH | DATE_YEAR, when,
		time_add_week_with_zone (when, 1, zone), FALSE);

	/* Print the start day of the week, e.g. '7th May 2001'. */
	convert_timet_to_struct_tm (when, zone, &tm);
	format_date (&tm, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
	print_text_abs_bold (
		context, buf, WEEK_NORMAL_FONT_SIZE, PANGO_ALIGN_LEFT,
		3, width, 4, 4 + 24);

	/* Print the end day of the week, e.g. '13th May 2001'. */
	when = time_add_day_with_zone (when, 6, zone);
	convert_timet_to_struct_tm (when, zone, &tm);
	format_date (&tm, DATE_DAY | DATE_MONTH | DATE_YEAR, buf, 100);
	print_text_abs_bold (
		context, buf, WEEK_NORMAL_FONT_SIZE, PANGO_ALIGN_LEFT,
		3, width, 24 + 3, 24 + 3 + 24);
}

static void
print_month_view (GtkPrintContext *context,
                  ECalendarView *cal_view,
		  EPrintView print_view_type,
                  time_t date)
{
	ECalModel *model;
	GtkPageSetup *setup;
	ICalTimezone *zone;
	gchar buf[100];
	gdouble width, height;
	gdouble l, week_numbers_inc, small_month_width;
	struct tm tm;

	model = e_calendar_view_get_model (cal_view);
	zone = e_cal_model_get_timezone (model);

	setup = gtk_print_context_get_page_setup (context);

	width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);
	height = gtk_page_setup_get_page_height (setup, GTK_UNIT_POINTS);
	small_month_width = calc_small_month_width (context, HEADER_HEIGHT);
	week_numbers_inc = get_show_week_numbers () ? small_month_width / 7.0 : 0;

	/* Print the main month view. */
	print_month_summary (context, model, cal_view, print_view_type, date, 0.0, width, HEADER_HEIGHT, height);

	/* round the date to match the expected month */
	date = time_day_begin_with_zone (date, zone);
	if (date != time_month_begin_with_zone (date, zone)) {
		date = time_month_begin_with_zone (date, zone);
		date = time_add_month_with_zone (date, 1, zone);
	}

	/* Print the border around the header. */
	print_border (context, 0.0, width, 0.0, HEADER_HEIGHT + 10, 1.0, DARKER_BORDER);

	l = width - SMALL_MONTH_PAD - small_month_width - week_numbers_inc;

	/* Print the 2 mini calendar-months. */
	print_month_small (
		context, model,
		time_add_month_with_zone (date, 1, zone),
		l, 4, l + small_month_width + week_numbers_inc, HEADER_HEIGHT + 4,
		DATE_MONTH | DATE_YEAR, 0, 0, FALSE);

	print_month_small (
		context, model,
		time_add_month_with_zone (date, -1, zone),
		SMALL_MONTH_PAD, 4, SMALL_MONTH_PAD + small_month_width + week_numbers_inc, HEADER_HEIGHT + 4,
		DATE_MONTH | DATE_YEAR, 0, 0, FALSE);

	/* Print the month, e.g. 'May 2001'. */
	convert_timet_to_struct_tm (date, zone, &tm);
	format_date (&tm, DATE_MONTH | DATE_YEAR, buf, 100);
	print_text_size_bold (
		context, buf, PANGO_ALIGN_CENTER,
		3, width - 3,
		3, 3 + 24);

}

static gboolean
same_date (struct tm tm1,
	   time_t t2,
	   ICalTimezone *zone)
{
	struct tm tm2;

	convert_timet_to_struct_tm (t2, zone, &tm2);

	return
	    tm1.tm_mday == tm2.tm_mday &&
	    tm1.tm_mon == tm2.tm_mon &&
	    tm1.tm_year == tm2.tm_year;
}

static void
write_label_piece (time_t t,
                   time_t *start_cmp,
                   ICalTimezone *zone,
                   gboolean use_24_hour_format,
                   gchar *buffer,
                   gint size,
                   gchar *stext,
                   const gchar *etext)
{
	struct tm tmp_tm;
	gint len;

	convert_timet_to_struct_tm (t, zone, &tmp_tm);

	if (stext != NULL) {
		len = strlen (buffer);
		g_snprintf (buffer + len, size - len, "%s", stext);
	}

	len = strlen (buffer);
	if (start_cmp && same_date (tmp_tm, *start_cmp, zone))
		e_time_format_time (
			&tmp_tm, use_24_hour_format,
			FALSE, &buffer[len], size - len);
	else
		e_time_format_date_and_time (
			&tmp_tm, use_24_hour_format, FALSE,
			FALSE, &buffer[len], size - len);

	if (etext != NULL) {
		len = strlen (buffer);
		g_snprintf (buffer + len, size - len, "%s", etext);
	}
}

static ICalTimezone *
get_zone_from_tzid (ECalClient *client,
                    const gchar *tzid)
{
	ICalTimezone *zone;

	if (!tzid)
		return NULL;

	/* Note that the timezones may not be on the server, so we try to get
	 * the builtin timezone with the TZID first. */
	zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
	if (!zone && tzid) {
		GError *error = NULL;

		if (!e_cal_client_get_timezone_sync (client, tzid, &zone, NULL, &error))
			zone = NULL;

		if (error != NULL) {
			g_warning (
				"Couldn't get timezone '%s' from server: %s",
				tzid ? tzid : "", error->message);
			g_error_free (error);
		}
	}

	return zone;
}

static void
print_date_label (GtkPrintContext *context,
                  ECalComponent *comp,
                  ECalClient *client,
                  ICalTimezone *zone,
                  gboolean use_24_hour_format,
                  gdouble left,
                  gdouble right,
                  gdouble top,
                  gdouble bottom)
{
	ICalTimezone *start_zone, *end_zone, *due_zone, *completed_zone;
	ICalTime *completed_tt;
	ECalComponentDateTime *datetime;
	time_t start = 0, end = 0, complete = 0, due = 0;
	static gchar buffer[1024];

	datetime = e_cal_component_get_dtstart (comp);
	if (datetime && e_cal_component_datetime_get_value (datetime)) {
		ICalTime *value = e_cal_component_datetime_get_value (datetime);

		start_zone = get_zone_from_tzid (client, e_cal_component_datetime_get_tzid (datetime));
		if (!start_zone || i_cal_time_is_date (value))
			start_zone = zone;
		start = i_cal_time_as_timet_with_zone (value, start_zone);
	}
	e_cal_component_datetime_free (datetime);

	datetime = e_cal_component_get_dtend (comp);
	if (datetime && e_cal_component_datetime_get_value (datetime)) {
		ICalTime *value = e_cal_component_datetime_get_value (datetime);

		end_zone = get_zone_from_tzid (client, e_cal_component_datetime_get_tzid (datetime));
		if (!end_zone || i_cal_time_is_date (value))
			end_zone = zone;
		end = i_cal_time_as_timet_with_zone (value, end_zone);
	}
	e_cal_component_datetime_free (datetime);

	datetime = e_cal_component_get_due (comp);
	if (datetime && e_cal_component_datetime_get_value (datetime)) {
		ICalTime *value = e_cal_component_datetime_get_value (datetime);

		due_zone = get_zone_from_tzid (client, e_cal_component_datetime_get_tzid (datetime));
		if (!due_zone || i_cal_time_is_date (value))
			due_zone = zone;
		due = i_cal_time_as_timet_with_zone (value, due_zone);
	}
	e_cal_component_datetime_free (datetime);

	completed_tt = e_cal_component_get_completed (comp);
	if (completed_tt) {
		completed_zone = i_cal_timezone_get_utc_timezone ();
		complete = i_cal_time_as_timet_with_zone (completed_tt, completed_zone);
		g_clear_object (&completed_tt);
	}

	buffer[0] = '\0';

	if (start > 0)
		write_label_piece (
			start, NULL, zone, use_24_hour_format,
			buffer, 1024, NULL, NULL);

	if (end > 0 && start > 0) {
		write_label_piece (
			end, &start, zone, use_24_hour_format,
			/* Translators: This is part of "START to END" text,
			 * where START and END are date/times. */
			buffer, 1024, _(" to "), NULL);
	}

	if (complete > 0) {
		if (start > 0) {
			write_label_piece (
				complete, NULL, zone, use_24_hour_format,
				/* Translators: This is part of "START to END
				 * (Completed COMPLETED)", where COMPLETED is a
				 * completed date/time. */
				buffer, 1024, _(" (Completed "), ")");
		} else {
			write_label_piece (
				complete, &start, zone, use_24_hour_format,
				/* Translators: This is part of "Completed COMPLETED",
				 * where COMPLETED is a completed date/time. */
				buffer, 1024, _("Completed "), NULL);
		}
	}

	if (due > 0 && complete == 0) {
		if (start > 0) {
			write_label_piece (
				due, NULL, zone, use_24_hour_format,
				/* Translators: This is part of "START (Due DUE)",
				 * where START and DUE are dates/times. */
				buffer, 1024, _(" (Due "), ")");
		} else {
			write_label_piece (
				due, &start, zone, use_24_hour_format,
				/* Translators: This is part of "Due DUE",
				 * where DUE is a date/time due the event
				 * should be finished. */
				buffer, 1024, _("Due "), NULL);
		}
	}

	print_text_size_bold (
		context, buffer, PANGO_ALIGN_LEFT,
		left, right, top, top + 24);
}

static void
print_calendar_draw_page (GtkPrintOperation *operation,
                          GtkPrintContext *context,
                          gint page_nr,
                          PrintCalItem *pcali)
{
	switch (pcali->print_view_type) {
		case E_PRINT_VIEW_DAY:
			print_day_view (context, pcali->cal_view, pcali->tasks_table, pcali->start);
			break;
		case E_PRINT_VIEW_WORKWEEK:
			print_work_week_view (context, pcali->cal_view, pcali->start);
			break;
		case E_PRINT_VIEW_WEEK:
			print_week_view (context, pcali->cal_view, pcali->start);
			break;
		case E_PRINT_VIEW_MONTH:
			print_month_view (context, pcali->cal_view, pcali->print_view_type, pcali->start);
			break;
		default:
			g_return_if_reached ();
	}
}

void
print_calendar (ECalendarView *cal_view,
		ETable *tasks_table,
		EPrintView print_view_type,
                GtkPrintOperationAction action,
                time_t start)
{
	GtkPrintOperation *operation;
	PrintCalItem *pci;

	g_return_if_fail (cal_view != NULL);
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (print_view_type == E_PRINT_VIEW_MONTH) {
		EWeekView *week_view = E_WEEK_VIEW (cal_view);

		if (e_week_view_get_multi_week_view (week_view))
			start = week_view->day_starts[0];
	}

	pci = g_slice_new0 (PrintCalItem);
	pci->cal_view = g_object_ref (cal_view);
	pci->tasks_table = g_object_ref (tasks_table);
	pci->print_view_type = print_view_type;
	pci->start = start;

	operation = e_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1);

	g_signal_connect_data (
		operation, "draw_page",
		G_CALLBACK (print_calendar_draw_page), pci,
		print_cal_item_free, 0);

	gtk_print_operation_run (operation, action, NULL, NULL);

	g_object_unref (operation);
}

/* returns number of required pages, when page_nr is -1 */
static gint
print_comp_draw_real (GtkPrintOperation *operation,
                      GtkPrintContext *context,
                      gint page_nr,
                      PrintCompItem *pci)
{
	GtkPageSetup *setup;
	PangoFontDescription *font;
	ECalClient *client;
	ECalComponent *comp;
	ECalComponentVType vtype;
	ECalComponentText *text;
	GSList *contact_list, *elem;
	const gchar *title;
	gchar *categories, *location;
	gchar *categories_string, *location_string, *summary_string;
	gdouble header_size;
	cairo_t *cr;
	gdouble width, height, page_start;
	gdouble top;
	gint pages = 1;

	setup = gtk_print_context_get_page_setup (context);

	width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);
	height = gtk_page_setup_get_page_height (setup, GTK_UNIT_POINTS);

	top = 0.0;

	/* Either draw only the right page or do not draw
	 * anything when calculating number of pages. */
	if (page_nr != -1)
		top = top - ((page_nr) * height);
	else
		top = height;

	page_start = top;

        /* PrintCompItem structure contains elements to be used
         * with the Print Context , obtained in comp_draw_page
         */
	client = pci->client;
	comp = pci->comp;

	vtype = e_cal_component_get_vtype (comp);

	/* We should only be asked to print VEVENTs, VTODOs, or VJOURNALs. */
	if (vtype == E_CAL_COMPONENT_EVENT)
		title = _("Appointment");
	else if (vtype == E_CAL_COMPONENT_TODO)
		title = _("Task");
	else if (vtype == E_CAL_COMPONENT_JOURNAL)
		title = _("Memo");
	else
		return pages;

	cr = gtk_print_context_get_cairo_context (context);

	/* Print the title in a box at the top of the page. */
	font = get_font_for_size (18, PANGO_WEIGHT_BOLD);
	header_size = 40;

	if (page_nr == 0) {
		print_border (
			context, 0.0, width, 0.0, header_size,
			1.0, DARKER_BORDER);
		print_text (
			context, font, title, PANGO_ALIGN_CENTER, 0.0, width,
			0.1, header_size - 0.1);
	}

	pango_font_description_free (font);

	top += header_size + 30;

	/* Summary */
	font = get_font_for_size (18, PANGO_WEIGHT_BOLD);
	text = e_cal_component_dup_summary_for_locale (comp, NULL);
	summary_string = g_strdup_printf (_("Summary: %s"), (text && e_cal_component_text_get_value (text)) ? e_cal_component_text_get_value (text) : "");
	top = bound_text (
		context, font, summary_string, -1, 0.0, top, width,
		height, FALSE, NULL, &page_start, &pages);
	e_cal_component_text_free (text);
	g_free (summary_string);

	/* Location */
	location = e_cal_component_get_location (comp);
	if (location && location[0]) {
		location_string = g_strdup_printf (
			_("Location: %s"),
			location);
		top = bound_text (
			context, font, location_string, -1, 0.0,
			top + 3, width, height, FALSE, NULL, &page_start, &pages);
		g_free (location_string);
	}
	g_free (location);

	/* Date information */
	if (page_nr == 0)
		print_date_label (
			context, comp, client,
			pci->zone, pci->use_24_hour_format,
			0.0, width, top + 3, top + 15);
	top += 20;

	/* Attendees */
	if ((page_nr == 0) && e_cal_component_has_attendees (comp)) {
		top = bound_text (
			context, font, _("Attendees: "), -1, 0.0,
			top, width, height, FALSE, NULL, &page_start, &pages);
		pango_font_description_free (font);
		font = get_font_for_size (12, PANGO_WEIGHT_NORMAL);
		top = print_attendees (
			context, font, cr, 0.0, width,
			top, height, comp, page_nr, &pages);
		top += get_font_size (font) - 6;
	}

	pango_font_description_free (font);

	font = get_font_for_size (12, PANGO_WEIGHT_NORMAL);

	/* For a VTODO we print the Status, Priority, % Complete and URL. */
	if (vtype == E_CAL_COMPONENT_TODO) {
		ICalComponent *icomp;
		ICalProperty *prop;
		ICalPropertyStatus status;
		const gchar *status_string = NULL;
		gint percent;
		gint priority;
		gchar *url;

		icomp = e_cal_component_get_icalcomponent (comp);

		/* Estimated duration */
		prop = i_cal_component_get_first_property (icomp, I_CAL_ESTIMATEDDURATION_PROPERTY);
		if (prop) {
			ICalDuration *duration;

			duration = i_cal_property_get_estimatedduration (prop);

			if (duration) {
				gint seconds;

				seconds = i_cal_duration_as_int (duration);
				if (seconds > 0) {
					gchar *tmp = e_cal_util_seconds_to_string (seconds);
					gchar *estimated_duration = g_strdup_printf (_("Estimated duration: %s"), tmp);
					top = bound_text (
						context, font, estimated_duration, -1,
						0.0, top, width, height, FALSE, NULL, &page_start, &pages);
					top += get_font_size (font) - 6;
					g_free (estimated_duration);
					g_free (tmp);
				}
			}

			g_clear_object (&duration);
			g_object_unref (prop);
		}

		/* Status */
		status = e_cal_component_get_status (comp);
		if (status != I_CAL_STATUS_NONE) {
			status_string = cal_comp_util_status_to_localized_string (I_CAL_VTODO_COMPONENT, status);

			if (status_string) {
				gchar *status_text = g_strdup_printf (
					_("Status: %s"),
					status_string);
				top = bound_text (
					context, font, status_text, -1,
					0.0, top, width, height, FALSE, NULL, &page_start, &pages);
				top += get_font_size (font) - 6;
				g_free (status_text);
			}
		}

		/* Priority */
		priority = e_cal_component_get_priority (comp);
		if (priority >= 0) {
			gchar *pri_text;

			pri_text = g_strdup_printf (
				_("Priority: %s"),
				e_cal_util_priority_to_string (priority));
			top = bound_text (
				context, font, pri_text, -1,
				0.0, top, width, height, FALSE, NULL,
				&page_start, &pages);
			top += get_font_size (font) - 6;
			g_free (pri_text);
		}

		/* Percent Complete */
		percent = e_cal_component_get_percent_complete (comp);
		if (percent >= 0) {
			gchar *percent_string;

			percent_string = g_strdup_printf (_("Percent Complete: %i"), percent);

			top = bound_text (
				context, font, percent_string, -1,
				0.0, top, width, height, FALSE, NULL, &page_start, &pages);
			top += get_font_size (font) - 6;

			g_free (percent_string);
		}

		/* URL */
		url = e_cal_component_get_url (comp);
		if (url && url[0]) {
			gchar *url_string;

			url_string = g_strdup_printf (_("URL: %s"), url);

			top = bound_text (
				context, font, url_string, -1,
				0.0, top, width, height, TRUE, NULL, &page_start, &pages);
			top += get_font_size (font) - 6;
			g_free (url_string);
		}

		g_free (url);
	}

	/* Categories */
	categories = e_cal_component_get_categories (comp);
	if (categories && categories[0]) {
		categories_string = g_strdup_printf (
			_("Categories: %s"), categories);
		top = bound_text (
			context, font, categories_string, -1,
			0.0, top, width, height, TRUE, NULL, &page_start, &pages);
		top += get_font_size (font) - 6;
		g_free (categories_string);
	}
	g_free (categories);

	/* Contacts */
	contact_list = e_cal_component_get_contacts (comp);
	if (contact_list) {
		GString *contacts = g_string_new (_("Contacts: "));
		for (elem = contact_list; elem; elem = elem->next) {
			ECalComponentText *t = elem->data;
			/* Put a comma between contacts. */
			if (elem != contact_list)
				g_string_append (contacts, ", ");
			g_string_append (contacts, e_cal_component_text_get_value (t));
		}
		g_slist_free_full (contact_list, e_cal_component_text_free);

		top = bound_text (
			context, font, contacts->str, -1,
			0.0, top, width, height, TRUE, NULL, &page_start, &pages);
		top += get_font_size (font) - 6;
		g_string_free (contacts, TRUE);
	}
	top += 16;

	/* Description */
	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_JOURNAL) {
		GSList *desc;

		desc = e_cal_component_get_descriptions (comp);
		for (elem = desc; elem; elem = g_slist_next (elem)) {
			ECalComponentText *ptext = elem->data;
			const gchar *line, *next_line;

			for (line = e_cal_component_text_get_value (ptext); line != NULL; line = next_line) {
				next_line = strchr (line, '\n');

				top = bound_text (
					context, font, line,
					next_line ? next_line - line : -1,
					0.0, top + 3, width, height, TRUE, NULL,
					&page_start, &pages);

				if (next_line) {
					next_line++;
					if (!*next_line)
						next_line = NULL;
				}
			}
		}

		g_slist_free_full (desc, e_cal_component_text_free);
	} else {
		text = e_cal_component_dup_description_for_locale (comp, NULL);

		if (text) {
			const gchar *line, *next_line;

			for (line = e_cal_component_text_get_value (text); line != NULL; line = next_line) {
				next_line = strchr (line, '\n');

				top = bound_text (
					context, font, line,
					next_line ? next_line - line : -1,
					0.0, top + 3, width, height, TRUE, NULL,
					&page_start, &pages);

				if (next_line) {
					next_line++;
					if (!*next_line)
						next_line = NULL;
				}
			}
		}
		e_cal_component_text_free (text);
	}

	pango_font_description_free (font);

	return pages;
}

static void
print_comp_draw_page (GtkPrintOperation *operation,
                      GtkPrintContext *context,
                      gint page_nr,
                      PrintCompItem *pci)
{
	print_comp_draw_real (operation, context, page_nr, pci);
}

static void
print_comp_begin_print (GtkPrintOperation *operation,
                        GtkPrintContext *context,
                        PrintCompItem *pci)
{
	gint pages;

	pages = print_comp_draw_real (operation, context, -1, pci);

	gtk_print_operation_set_n_pages (operation, pages);
}

void
print_comp (ECalComponent *comp,
            ECalClient *cal_client,
            ICalTimezone *zone,
            gboolean use_24_hour_format,
            GtkPrintOperationAction action)
{
	GtkPrintOperation *operation;
	PrintCompItem *pci;

	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	pci = g_slice_new0 (PrintCompItem);
	pci->comp = g_object_ref (comp);
	pci->client = cal_client ? g_object_ref (cal_client) : NULL;
	pci->zone = zone ? g_object_ref (zone) : NULL;
	pci->use_24_hour_format = use_24_hour_format;

	operation = e_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1);

	g_object_set_data_full (G_OBJECT (operation), "e-print-context-data", pci, print_comp_item_free);

	g_signal_connect (
		operation, "begin-print",
		G_CALLBACK (print_comp_begin_print), pci);

	g_signal_connect (
		operation, "draw-page",
		G_CALLBACK (print_comp_draw_page), pci);

	gtk_print_operation_run (operation, action, NULL, NULL);

	g_object_unref (operation);
}

static void
print_title (GtkPrintContext *context,
             const gchar *text,
             gdouble page_width)
{
	PangoFontDescription *desc;
	PangoLayout *layout;
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (context);

	desc = pango_font_description_from_string (FONT_FAMILY " Bold 18");

	layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_font_description (layout, desc);
	pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	pango_layout_set_width (layout, pango_units_from_double (page_width));

	cairo_save (cr);

	cairo_move_to (cr, 0.0, 0.0);
	pango_cairo_show_layout (cr, layout);
	cairo_restore (cr);

	cairo_translate (cr, 0.0, 18);

	g_object_unref (layout);

	pango_font_description_free (desc);
}

struct print_opts {
	EPrintable *printable;
	gchar *print_header;
};

static void
print_opts_free (gpointer ptr,
		 GClosure *closure)
{
	struct print_opts *opts = ptr;

	if (opts) {
		g_clear_object (&opts->printable);
		g_free (opts->print_header);
		g_slice_free (struct print_opts, opts);
	}
}

static void
print_table_draw_page (GtkPrintOperation *operation,
                       GtkPrintContext *context,
                       gint page_nr,
                       struct print_opts *opts)
{
	GtkPageSetup *setup;
	gdouble width;
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (context);
	setup = gtk_print_context_get_page_setup (context);

	width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);

	do {
		cairo_save (cr);

		/* TODO Allow the user to customize the title. */
		print_title (context, opts->print_header, width);

		if (e_printable_data_left (opts->printable))
			e_printable_print_page (
				opts->printable, context, width, 24, TRUE);

		cairo_restore (cr);
	} while (e_printable_data_left (opts->printable));
}

void
print_table (ETable *table,
             const gchar *dialog_title,
             const gchar *print_header,
             GtkPrintOperationAction action)
{
	GtkPrintOperation *operation;
	EPrintable *printable;
	struct print_opts *opts;

	printable = e_table_get_printable (table);
	g_object_ref_sink (printable);
	e_printable_reset (printable);

	operation = e_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1);

	opts = g_slice_new0 (struct print_opts);
	opts->printable = g_object_ref (printable);
	opts->print_header = g_strdup (print_header);

	g_signal_connect_data (
		operation, "draw_page",
		G_CALLBACK (print_table_draw_page), opts,
		print_opts_free, 0);

	gtk_print_operation_run (operation, action, NULL, NULL);

	g_object_unref (operation);
}
