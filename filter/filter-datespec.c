/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include <time.h>
#include <gtk/gtk.h>
#include <gnome.h>

#include "filter-datespec.h"
#include "e-util/e-sexp.h"

#define d(x) x

static void xml_create(FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(FilterElement *fe);
static int xml_decode(FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget(FilterElement *fe);
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *fds);
static void format_sexp(FilterElement *, GString *);
static void filter_datespec_class_init	(FilterDatespecClass *class);
static void filter_datespec_init	(FilterDatespec *gspaper);
static void filter_datespec_finalise	(GtkObject *obj);

static void make_span_editor (FilterDatespec *fds);
static void adj_value_changed (GtkAdjustment *adj, gpointer user_data);
static gchar *describe_button (FilterDatespec *fds);
static gchar *stringify_agoness (FilterDatespec *fds);
static void set_adjustments (FilterDatespec *fds);

static void cal_day_selected (GtkCalendar *cal, gpointer user_data);
static void cal_day_selected_double_click (GtkCalendar *cal, gpointer user_data);

#define PRIV(x) (((FilterDatespec *)(x))->priv)

typedef struct _timespan
{
	guint32 seconds;
	const gchar *singular;
	const gchar *plural;
	gfloat max;
} timespan;

static const timespan timespans[] = {
	{ 31557600, N_("year"), N_("years"), 1000.0 },
	{ 2419200, N_("month"), N_("months"), 12.0 },
	{ 604800, N_("week"), N_("weeks"), 52.0 },
	{ 86400, N_("day"), N_("days"), 31.0 },
	{ 3600, N_("hour"), N_("hours"), 23.0 },
	{ 60, N_("minute"), N_("minutes"), 59.0 },
	{ 1, N_("second"), N_("seconds"), 59.0 }
};

#define N_TIMECHUNKS 3

static const guint timechunks[N_TIMECHUNKS] = { 2, 2, 3 };
#define MAX_CHUNK 3

#define N_TIMESPANS (sizeof (timespans) / sizeof (timespans[0]))

struct _FilterDatespecPrivate {
	GnomeDialog *gd;
	GtkWidget *descriptive_label;
	GtkWidget *cur_extra_widget;
	FilterDatespec_type selected_type;

	GtkWidget *date_chooser;
	GtkWidget *span_chooser;
	gboolean double_click;
	GtkWidget **spinbuttons;
};

static FilterElementClass *parent_class;

guint
filter_datespec_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterDatespec",
			sizeof(FilterDatespec),
			sizeof(FilterDatespecClass),
			(GtkClassInitFunc)filter_datespec_class_init,
			(GtkObjectInitFunc)filter_datespec_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_datespec_class_init (FilterDatespecClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_element_get_type ());

	object_class->finalize = filter_datespec_finalise;

	/* override methods */
	filter_element->xml_create = xml_create;
	filter_element->xml_encode = xml_encode;
	filter_element->xml_decode = xml_decode;
	filter_element->get_widget = get_widget;
	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;
}

static void
filter_datespec_init (FilterDatespec *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
	o->type = FDST_UNKNOWN;
	PRIV(o)->selected_type = FDST_UNKNOWN;
	PRIV(o)->spinbuttons = g_new (GtkWidget *, N_TIMESPANS );
}

static void
filter_datespec_finalise(GtkObject *obj)
{
	FilterDatespec *o = (FilterDatespec *)obj;

	if (o->priv) {
		g_free (PRIV(o)->spinbuttons);
		g_free (o->priv);
	}

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_datespec_new:
 *
 * Create a new FilterDatespec object.
 * 
 * Return value: A new #FilterDatespec object.
 **/
FilterDatespec *
filter_datespec_new(void)
{
	FilterDatespec *o = (FilterDatespec *)gtk_type_new(filter_datespec_get_type ());
	return o;
}

static void xml_create(FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);
}

static xmlNodePtr xml_encode(FilterElement *fe)
{
	xmlNodePtr value, work;
	FilterDatespec *fds = (FilterDatespec *)fe;
	gchar str[32];

	d(printf("Encoding datespec as xml\n"));

	value = xmlNewNode(NULL, "value");
	xmlSetProp(value, "name", fe->name);
	xmlSetProp(value, "type", "datespec");

	work = xmlNewChild(value, NULL, "datespec", NULL);
	sprintf (str, "%d", fds->type);
	xmlSetProp(work, "type", str);
	sprintf (str, "%d", (int)fds->value);
	xmlSetProp(work, "value", str);

	return value;
}

static int xml_decode(FilterElement *fe, xmlNodePtr node)
{
	FilterDatespec *fds = (FilterDatespec *)fe;
	xmlNodePtr n;
	gchar *val;

	d(printf("Decoding datespec from xml %p\n", fe));

	fe->name = xmlGetProp(node, "name");

	n = node->childs;
	while (n) {
		if (!strcmp(n->name, "datespec")) {
			val = xmlGetProp(n, "type");
			fds->type = atoi (val);
			g_free (val);
			val = xmlGetProp(n, "value");
			fds->value = atoi (val);
			g_free (val);
			break;
		}
		n = n->next;
	}
	return 0;
}

static void activate_now(GtkMenuItem *item, FilterDatespec *fds)
{
	if (PRIV(fds)->cur_extra_widget) {
		gtk_container_remove (GTK_CONTAINER (PRIV(fds)->gd->vbox),
				      PRIV(fds)->cur_extra_widget);
		PRIV (fds)->cur_extra_widget = NULL;
	}

						     
	gtk_label_set_text (GTK_LABEL (PRIV(fds)->descriptive_label),
			    _("The message's date will be compared against\n"
			      "whatever the time is when the filter is run\n"
			      "or vfolder is opened."));

	PRIV(fds)->selected_type = FDST_NOW;
}

static void activate_specified(GtkMenuItem *item, FilterDatespec *fds)
{
	struct tm *seltime;

	/* Remove other widget if it exists */

	if (PRIV(fds)->cur_extra_widget) {
		gtk_container_remove (GTK_CONTAINER (PRIV(fds)->gd->vbox),
				      PRIV(fds)->cur_extra_widget);
		PRIV (fds)->cur_extra_widget = NULL;
	}

	/* Set description */

	gtk_label_set_text (GTK_LABEL (PRIV(fds)->descriptive_label),
			    _("The message's date will be compared against\n"
			      "the time that you specify here."));

	/* Reset if going from one type to another */
	if (PRIV(fds)->selected_type != FDST_SPECIFIED)
		fds->value = 0;

	PRIV(fds)->selected_type = FDST_SPECIFIED;

	/* Set the calendar's time */

	if (fds->value > 0) {
		/* gmtime? */
		seltime = localtime (&(fds->value));

		gtk_calendar_select_month (GTK_CALENDAR (PRIV(fds)->date_chooser),
					   seltime->tm_mon,
					   seltime->tm_year + 1900);
		gtk_calendar_select_day (GTK_CALENDAR (PRIV(fds)->date_chooser),
					 seltime->tm_mday);
		/* free seltime?? */
	}

	gtk_box_pack_start (GTK_BOX (PRIV(fds)->gd->vbox),
			    PRIV(fds)->date_chooser,
			    TRUE, TRUE, 3);
	gtk_widget_show (PRIV(fds)->date_chooser);
	PRIV(fds)->cur_extra_widget = PRIV(fds)->date_chooser;
}

static void activate_x_ago(GtkMenuItem *item, FilterDatespec *fds)
{
	if (PRIV(fds)->cur_extra_widget) {
		gtk_container_remove (GTK_CONTAINER (PRIV(fds)->gd->vbox),
				      PRIV(fds)->cur_extra_widget);
		PRIV (fds)->cur_extra_widget = NULL;
	}

	gtk_label_set_text (GTK_LABEL (PRIV(fds)->descriptive_label),
			    _("The message's date will be compared against\n"
			      "a time relative to when the filter is run;\n"
			      "\"a week ago\", for example."));

	/* Reset if going from one type to another */
	if (PRIV(fds)->selected_type != FDST_X_AGO)
		fds->value = 0;

	PRIV(fds)->selected_type = FDST_X_AGO;

	if (fds->value > 0)
		set_adjustments (fds);

	gtk_box_pack_start (GTK_BOX (PRIV(fds)->gd->vbox),
			    PRIV(fds)->span_chooser,
			    TRUE, TRUE, 3);
	gtk_widget_show (PRIV(fds)->span_chooser);
	PRIV(fds)->cur_extra_widget = PRIV(fds)->span_chooser;

}

typedef void (*my_menu_callback) (GtkMenuItem *, FilterDatespec *);

static void button_clicked(GtkButton *button, FilterDatespec *fds)
{
	GnomeDialog *gd;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *menu;
	GtkWidget *selectomatic;
	GtkWidget *sep;
	int i;
	gchar *desc;

	/* keep in sync with FilterDatespec_type! */
	const char *items[] = { N_("the current time"), N_("a time you specify"), 
			       N_("a time relative to the current time"), NULL };
	const my_menu_callback callbacks[]
		= { activate_now, activate_specified, activate_x_ago };

	PRIV(fds)->descriptive_label = gtk_label_new("");
	PRIV(fds)->cur_extra_widget = NULL;
	PRIV(fds)->double_click = FALSE;

	/* The calendar */

	PRIV(fds)->date_chooser = gtk_calendar_new ();
	gtk_object_ref (GTK_OBJECT (PRIV(fds)->date_chooser));
	gtk_signal_connect (GTK_OBJECT (PRIV(fds)->date_chooser), "day_selected",
			    cal_day_selected, fds);
	gtk_signal_connect (GTK_OBJECT (PRIV(fds)->date_chooser), "day_selected_double_click",
			    cal_day_selected_double_click, fds);

	/* The span editor thingie */

	make_span_editor (fds);
	gtk_object_ref (GTK_OBJECT (PRIV(fds)->span_chooser));

	/* The dialog */

	gd = (GnomeDialog *) gnome_dialog_new ("Select a time to compare against", 
					       GNOME_STOCK_BUTTON_OK, 
					       GNOME_STOCK_BUTTON_CANCEL, 
					       NULL);
	PRIV(fds)->gd = gd;

	/* The menu */

	menu = gtk_menu_new ();
	
	for (i = 0; items[i]; i++) {
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (gettext (items[i]));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    callbacks[i], fds);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
	}

	gtk_widget_show (menu);

	/* The selector */

	selectomatic = gtk_option_menu_new();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (selectomatic), GTK_WIDGET (menu));
	if (fds->type != FDST_UNKNOWN)
		/* Keep in sync with FilterDatespec_type! */
		gtk_option_menu_set_history (GTK_OPTION_MENU (selectomatic), fds->type);

	gtk_widget_show ((GtkWidget *)selectomatic);

	/* The label */

	label = gtk_label_new (_("Compare against"));
	gtk_widget_show (label);

	/* The hbox */

	box = gtk_hbox_new (FALSE, 3);
	gtk_box_pack_start (GTK_BOX (box), label,
			    TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (box), selectomatic,
			    TRUE, TRUE, 2);
	gtk_widget_show (box);
	gtk_box_pack_start ((GtkBox *)gd->vbox, (GtkWidget *)box, TRUE, TRUE, 3);

	/* The separator */

	sep = gtk_hseparator_new ();
	gtk_widget_show (sep);
	gtk_box_pack_start (GTK_BOX (gd->vbox), sep, TRUE, TRUE, 3);

	/* The descriptive label */

	gtk_box_pack_start (GTK_BOX (gd->vbox), PRIV(fds)->descriptive_label, TRUE, TRUE, 3);
	gtk_misc_set_alignment (GTK_MISC (PRIV(fds)->descriptive_label), 0.5, 0.5);
	gtk_widget_show (PRIV(fds)->descriptive_label);

	/* Set up the current view */

	if (fds->type == FDST_UNKNOWN)
		fds->type = FDST_NOW;

	(callbacks[fds->type]) (NULL, fds);

	/* go go gadget gnomedialog! */

	switch (gnome_dialog_run_and_close(gd)) {
	case -1: /*wm close*/
		if (PRIV(fds)->double_click == FALSE)
			break;
		/* else fall */
	case 0:
		fds->type = PRIV(fds)->selected_type;

		PRIV(fds)->descriptive_label = NULL;

		desc = describe_button (fds);
		gtk_label_set_text (GTK_LABEL (GTK_BIN (button)->child), desc);
		g_free (desc);
		/* falllllll */
	case 1:
		/* cancel */
		break;
	}

	gtk_widget_destroy (PRIV(fds)->date_chooser);
	gtk_widget_destroy (PRIV(fds)->span_chooser);
}

static GtkWidget *
get_widget(FilterElement *fe)
{
	FilterDatespec *fds = (FilterDatespec *)fe;
	GtkWidget *button;
	GtkWidget *label;
	gchar *desc;

	desc = describe_button (fds);
	label = gtk_label_new (desc);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
	g_free (desc);

	button = gtk_button_new();
	gtk_container_add (GTK_CONTAINER (button), label);
	gtk_signal_connect(GTK_OBJECT (button), "clicked", button_clicked, fds);

	gtk_widget_show(button);
	gtk_widget_show(label);
	return button;
}

static void 
build_code(FilterElement *fe, GString *out, struct _FilterPart *fp)
{
	return;
}

static void 
format_sexp(FilterElement *fe, GString *out)
{
	FilterDatespec *fds = (FilterDatespec *)fe;

	switch (fds->type) {
	case FDST_UNKNOWN:
		g_warning ("user hasn't selected a datespec yet!");
		/* fall through */
	case FDST_NOW:
		g_string_append (out, "(get-current-date)");
		break;
	case FDST_SPECIFIED:
		g_string_sprintfa (out, "%d", (int) fds->value);
		break;
	case FDST_X_AGO:
		g_string_sprintfa (out, "(- (get-current-date) %d)", (int) fds->value);
		break;
	}
}

static gchar *
stringify_agoness (FilterDatespec *fds)
{
	time_t val;
	GString *str;
	gchar *ret;

	str = g_string_new("");
	val = fds->value;

	if (val == 0) {
		g_string_append (str, _("now"));
	} else {
		int where;

		where = 0;

		while (val) {
			int count;
			
			count = 0;
			
			while (timespans[where].seconds <= val) {
				count++;
				val -= timespans[where].seconds;
			}
			
			if (count != 0 ) {
				if (count > 1)
					g_string_sprintfa (str, "%d %s", (int) count, gettext (timespans[where].plural));
				else
					g_string_sprintfa (str, "%d %s", (int) count, gettext (timespans[where].singular));
				
				if (val)
					g_string_append (str, ", ");
			}
			
			where++;
		}

		g_string_append (str, " ago");
	}

	ret = str->str;
	g_string_free (str, FALSE);
	return ret;
}

static void
make_span_editor (FilterDatespec *fds)
{
	int i;
	int chunk;
	int delta;
	GtkWidget *table;

	/*PRIV(fds)->span_chooser = gtk_vbox_new (TRUE, 3);*/
	table = gtk_table_new (N_TIMECHUNKS, MAX_CHUNK * 2, FALSE);

	i = 0;

	for (chunk = 0; chunk < N_TIMECHUNKS; chunk++ ) {
		/*GtkWidget *hbox;*/

		/*hbox = gtk_hbox_new (FALSE, 1);*/
		/*gtk_box_pack_start (GTK_BOX (PRIV(fds)->span_chooser),
		 *		    hbox, TRUE, TRUE, 1);
		 */
		/*gtk_table_attach (GTK_TABLE (PRIV(fds)->span_chooser),
		 *		  hbox,
		 *		  0, 1, chunk, chunk + 1,
		 *		  0, GTK_EXPAND | GTK_FILL,
		 *		  3, 3);
		 *gtk_widget_show (hbox);
		 */

		for (delta = 0; delta < timechunks[chunk]; delta++, i++ ) {
			gchar *text;
			GtkObject *adj;
			GtkWidget *sb;
			GtkWidget *label;

			adj = gtk_adjustment_new (0.0, 0.0,
						  timespans[i].max,
						  1.0, 10.0, 0.0);
			
			sb = gtk_spin_button_new (GTK_ADJUSTMENT (adj),
						  0, 0);

			/*gtk_box_pack_start (GTK_BOX (hbox), sb, FALSE, FALSE, 1);*/
			gtk_table_attach (GTK_TABLE (table), sb,
					  delta * 2, delta * 2 + 1,
					  chunk, chunk + 1,
					  0, GTK_EXPAND | GTK_FILL,
					  2, 4);
			PRIV(fds)->spinbuttons[i] = sb;

			gtk_widget_show (GTK_WIDGET (sb));
			
			if (delta + 1 < timechunks[chunk])
				text = g_strdup_printf ("%s, ", gettext (timespans[i].plural));
			else
				text = g_strdup_printf ("%s ago", gettext (timespans[i].plural));

			label = gtk_label_new (text);
			g_free (text);

			/*gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 3);*/
			gtk_table_attach (GTK_TABLE (table), label,
					  delta * 2 + 1, (delta + 1) * 2,
					  chunk, chunk + 1,
					  0, GTK_EXPAND | GTK_FILL,
					  2, 4);
			gtk_widget_show (label);

			gtk_signal_connect (adj, "value_changed",
					    adj_value_changed, fds);
		}
	}

	PRIV(fds)->span_chooser = table;
}

static void
adj_value_changed (GtkAdjustment *adj, gpointer user_data)
{
	FilterDatespec *fds = (FilterDatespec *) user_data;
	int i;

	fds->value = 0;

	for (i = 0; i < N_TIMESPANS; i++)
		fds->value += timespans[i].seconds * 
			(gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (PRIV(fds)->spinbuttons[i])));
}

static void
set_adjustments (FilterDatespec *fds)
{
	time_t val;
	int where;

	val = fds->value;
	where = 0;

	while (val) {
		int count;
		
		count = 0;
		
		while (timespans[where].seconds <= val) {
			count++;
			val -= timespans[where].seconds;
		}
			
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (PRIV(fds)->spinbuttons[where]),
					   (gfloat) count);
		where++;
	}
}

static gchar *
describe_button (FilterDatespec *fds)
{
	gchar *desc = NULL;

	switch (fds->type) {
	case FDST_UNKNOWN:
		desc = g_strdup (_("<click here to select a date>"));
		break;
	case FDST_NOW:
		desc = g_strdup (_("now"));
		break;
	case FDST_SPECIFIED:
		desc = g_strdup (ctime (&(fds->value)));
		break;
	case FDST_X_AGO:
		desc = stringify_agoness (fds);
		break;
	}

	return desc;
}

static void
cal_day_selected (GtkCalendar *cal, gpointer user_data)
{
	FilterDatespec *fds = (FilterDatespec *)user_data;
	extern int daylight;
	struct tm seltime;

	seltime.tm_sec = 0;
	seltime.tm_min = 0;
	seltime.tm_hour = 0;
	seltime.tm_mday = cal->selected_day;
	seltime.tm_mon = cal->month;
	seltime.tm_year = cal->year - 1900;
	seltime.tm_isdst = daylight;

	fds->value = mktime (&seltime);
}

static void 
cal_day_selected_double_click (GtkCalendar *cal, gpointer user_data)
{
	FilterDatespec *fds = (FilterDatespec *)user_data;

	cal_day_selected (cal, user_data);
	PRIV(fds)->double_click = TRUE;
	gnome_dialog_close (PRIV(fds)->gd);
}
