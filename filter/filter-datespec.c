/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <glib.h>
#include <gtk/gtkcalendar.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtktable.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-stock.h>

#include "filter-datespec.h"
#include "e-util/e-sexp.h"

#define d(x)

static gboolean validate (FilterElement *fe);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *fds);
static void format_sexp (FilterElement *, GString *);

static void filter_datespec_class_init (FilterDatespecClass *class);
static void filter_datespec_init (FilterDatespec *gspaper);
static void filter_datespec_finalise (GtkObject *obj);

static void make_span_editor (FilterDatespec *fds);
static void adj_value_changed (GtkAdjustment *adj, gpointer user_data);
static void omenu_item_activated (GtkMenuItem *item, gpointer user_data);
static gchar *describe_button (FilterDatespec *fds);
static gchar *stringify_agoness (FilterDatespec *fds);
static void set_adjustments (FilterDatespec *fds);

static void cal_day_selected (GtkCalendar *cal, gpointer user_data);
static void cal_day_selected_double_click (GtkCalendar *cal, gpointer user_data);

#define PRIV(x) (((FilterDatespec *)(x))->priv)

typedef struct _timespan {
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

#define DAY_INDEX 3
#define N_TIMESPANS (sizeof (timespans) / sizeof (timespans[0]))

struct _FilterDatespecPrivate {
	GnomeDialog *gd;
	GtkWidget *descriptive_label;
	GtkWidget *cur_extra_widget;
	FilterDatespec_type selected_type;

	GtkWidget *date_chooser;
	GtkWidget *span_chooser;
	GtkWidget *omenu, *spinbutton, *recent_item;
	gboolean double_click;
};

static FilterElementClass *parent_class;

guint
filter_datespec_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterDatespec",
			sizeof (FilterDatespec),
			sizeof (FilterDatespecClass),
			(GtkClassInitFunc) filter_datespec_class_init,
			(GtkObjectInitFunc) filter_datespec_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_datespec_class_init (FilterDatespecClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class (filter_element_get_type ());
	
	object_class->finalize = filter_datespec_finalise;
	
	/* override methods */
	filter_element->validate = validate;
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
	o->priv = g_malloc0 (sizeof (*o->priv));
	o->type = FDST_UNKNOWN;
	PRIV(o)->selected_type = FDST_UNKNOWN;
}

static void
filter_datespec_finalise(GtkObject *obj)
{
	FilterDatespec *o = (FilterDatespec *)obj;
	
	if (o->priv)
		g_free (o->priv);
	
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
filter_datespec_new (void)
{
	FilterDatespec *o = (FilterDatespec *)gtk_type_new (filter_datespec_get_type ());
	return o;
}

static gboolean
validate (FilterElement *fe)
{
	FilterDatespec *fds = (FilterDatespec *) fe;
	gboolean valid = TRUE;
	
	if (fds->value <= 0) {
		GtkWidget *gd;
		
		valid = FALSE;
		
		if (fds->type == FDST_UNKNOWN)
			gd = gnome_ok_dialog (_("You have forgotten to choose a date."));
		else
			gd = gnome_ok_dialog (_("You have chosen an invalid date."));
		
		gnome_dialog_run_and_close (GNOME_DIALOG (gd));
	}
	
	return valid;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value, work;
	FilterDatespec *fds = (FilterDatespec *)fe;
	gchar str[32];
	
	d(printf ("Encoding datespec as xml\n"));
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", "datespec");
	
	work = xmlNewChild (value, NULL, "datespec", NULL);
	sprintf (str, "%d", fds->type);
	xmlSetProp (work, "type", str);
	sprintf (str, "%d", (int)fds->value);
	xmlSetProp (work, "value", str);
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterDatespec *fds = (FilterDatespec *)fe;
	xmlNodePtr n;
	gchar *val;
	
	d(printf ("Decoding datespec from xml %p\n", fe));
	
	xmlFree (fe->name);
	fe->name = xmlGetProp (node, "name");
	
	n = node->childs;
	while (n) {
		if (!strcmp (n->name, "datespec")) {
			val = xmlGetProp (n, "type");
			fds->type = atoi (val);
			xmlFree (val);
			val = xmlGetProp (n, "value");
			fds->value = atoi (val);
			xmlFree (val);
			break;
		}
		n = n->next;
	}
	return 0;
}

static void
activate_now (GtkMenuItem *item, FilterDatespec *fds)
{
	if (PRIV (fds)->cur_extra_widget) {
		gtk_container_remove (GTK_CONTAINER (PRIV(fds)->gd->vbox),
				      PRIV (fds)->cur_extra_widget);
		PRIV (fds)->cur_extra_widget = NULL;
	}
	
	gtk_label_set_text (GTK_LABEL (PRIV (fds)->descriptive_label),
			    _("The message's date will be compared against\n"
			      "whatever the time is when the filter is run\n"
			      "or vfolder is opened."));
	
	PRIV (fds)->selected_type = FDST_NOW;
}

static void
activate_specified (GtkMenuItem *item, FilterDatespec *fds)
{
	struct tm *seltime;
	
	/* Remove other widget if it exists */
	
	if (PRIV (fds)->cur_extra_widget) {
		gtk_container_remove (GTK_CONTAINER (PRIV (fds)->gd->vbox),
				      PRIV (fds)->cur_extra_widget);
		PRIV (fds)->cur_extra_widget = NULL;
	}
	
	/* Set description */
	
	gtk_label_set_text (GTK_LABEL (PRIV (fds)->descriptive_label),
			    _("The message's date will be compared against\n"
			      "the time that you specify here."));
	
	/* Reset if going from one type to another */
	if (PRIV (fds)->selected_type != FDST_SPECIFIED)
		fds->value = 0;
	
	PRIV (fds)->selected_type = FDST_SPECIFIED;
	
	/* Set the calendar's time */
	
	if (fds->value > 0) {
		/* gmtime? */
		seltime = localtime (&(fds->value));
		
		gtk_calendar_select_month (GTK_CALENDAR (PRIV (fds)->date_chooser),
					   seltime->tm_mon,
					   seltime->tm_year + 1900);
		gtk_calendar_select_day (GTK_CALENDAR (PRIV (fds)->date_chooser),
					 seltime->tm_mday);
		/* free seltime?? */
	}
	
	gtk_box_pack_start (GTK_BOX (PRIV (fds)->gd->vbox),
			    PRIV (fds)->date_chooser,
			    TRUE, TRUE, 3);
	gtk_widget_show (PRIV (fds)->date_chooser);
	PRIV (fds)->cur_extra_widget = PRIV (fds)->date_chooser;
}

static void
activate_x_ago (GtkMenuItem *item, FilterDatespec *fds)
{
	if (PRIV (fds)->cur_extra_widget) {
		gtk_container_remove (GTK_CONTAINER (PRIV (fds)->gd->vbox),
				      PRIV (fds)->cur_extra_widget);
		PRIV (fds)->cur_extra_widget = NULL;
	}
	
	gtk_label_set_text (GTK_LABEL (PRIV (fds)->descriptive_label),
			    _("The message's date will be compared against\n"
			      "a time relative to when the filter is run;\n"
			      "\"a week ago\", for example."));
	
	/* Reset if going from one type to another */
	if (PRIV (fds)->selected_type != FDST_X_AGO)
		fds->value = 0;
	
	PRIV (fds)->selected_type = FDST_X_AGO;

	if (fds->value > 0)
		set_adjustments (fds);
	
	gtk_box_pack_start (GTK_BOX (PRIV (fds)->gd->vbox),
			    PRIV (fds)->span_chooser,
			    TRUE, TRUE, 3);
	gtk_widget_show (PRIV (fds)->span_chooser);
	PRIV (fds)->cur_extra_widget = PRIV (fds)->span_chooser;
}

typedef void (*my_menu_callback) (GtkMenuItem *, FilterDatespec *);

static void
button_clicked (GtkButton *button, FilterDatespec *fds)
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
	
	PRIV (fds)->descriptive_label = gtk_label_new("");
	PRIV (fds)->cur_extra_widget = NULL;
	PRIV (fds)->double_click = FALSE;
	
	/* The calendar */
	
	PRIV (fds)->date_chooser = gtk_calendar_new ();
	gtk_object_ref (GTK_OBJECT (PRIV (fds)->date_chooser));
	gtk_signal_connect (GTK_OBJECT (PRIV (fds)->date_chooser), "day_selected",
			    cal_day_selected, fds);
	gtk_signal_connect (GTK_OBJECT (PRIV (fds)->date_chooser), "day_selected_double_click",
			    cal_day_selected_double_click, fds);
	
	/* The span editor thingie */
	
	make_span_editor (fds);
	gtk_object_ref (GTK_OBJECT (PRIV (fds)->span_chooser));
	
	/* The dialog */
	
	gd = (GnomeDialog *) gnome_dialog_new (_("Select a time to compare against"), 
					       GNOME_STOCK_BUTTON_OK, 
					       GNOME_STOCK_BUTTON_CANCEL, 
					       NULL);
	PRIV (fds)->gd = gd;
	
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
	
	gtk_box_pack_start (GTK_BOX (gd->vbox), PRIV (fds)->descriptive_label, TRUE, TRUE, 3);
	gtk_misc_set_alignment (GTK_MISC (PRIV (fds)->descriptive_label), 0.5, 0.5);
	gtk_widget_show (PRIV (fds)->descriptive_label);
	
	/* Set up the current view */
	
	if (fds->type == FDST_UNKNOWN)
		fds->type = FDST_NOW;
	PRIV (fds)->selected_type = fds->type;

	(callbacks[fds->type]) (NULL, fds);
	
	/* go go gadget gnomedialog! */
	
	switch (gnome_dialog_run_and_close(gd)) {
	case -1: /*wm close*/
		if (PRIV (fds)->double_click == FALSE)
			break;
		/* else fall */
	case 0:
		fds->type = PRIV (fds)->selected_type;
		
		PRIV (fds)->descriptive_label = NULL;
		
		desc = describe_button (fds);
		gtk_label_set_text (GTK_LABEL (GTK_BIN (button)->child), desc);
		g_free (desc);
		/* falllllll */
	case 1:
		/* cancel */
		break;
	}
	
	gtk_widget_destroy (PRIV (fds)->date_chooser);
	gtk_widget_destroy (PRIV (fds)->span_chooser);
}

static GtkWidget *
get_widget (FilterElement *fe)
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
	gtk_signal_connect (GTK_OBJECT (button), "clicked", button_clicked, fds);
	
	gtk_widget_show (button);
	gtk_widget_show (label);
	return button;
}

static void 
build_code (FilterElement *fe, GString *out, struct _FilterPart *fp)
{
	return;
}

static void 
format_sexp (FilterElement *fe, GString *out)
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
		
		g_string_append (str, _(" ago"));
	}
	
	ret = str->str;
	g_string_free (str, FALSE);
	return ret;
}

static void
make_span_editor (FilterDatespec *fds)
{
	int i;
	GtkObject *adj;
	GtkWidget *hbox, *menu, *om, *sb, *label;

	/*PRIV (fds)->span_chooser = gtk_vbox_new (TRUE, 3);*/

	hbox = gtk_hbox_new (TRUE, 3);

	adj = gtk_adjustment_new (0.0, 0.0,
				  /*timespans[i].max*/100000.0,
				  1.0, 10.0, 0.0);
	sb = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 0, 0);
	gtk_widget_show (GTK_WIDGET (sb));
	gtk_box_pack_start (GTK_BOX (hbox), sb, TRUE, TRUE, 0);

	menu = gtk_menu_new ();
	for (i = 0; i < N_TIMESPANS; i++) {
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (gettext (timespans[i].plural));
		gtk_object_set_data (GTK_OBJECT (item), "timespan", (gpointer) &(timespans[i]));
		gtk_signal_connect (GTK_OBJECT (item), "activate", omenu_item_activated, fds);
		gtk_widget_show (item);
		gtk_menu_prepend (GTK_MENU (menu), item);

		if (i == DAY_INDEX)
			PRIV (fds)->recent_item = item;
	}

	om = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (om), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (om), DAY_INDEX);
	gtk_widget_show (om);
	gtk_box_pack_start (GTK_BOX (hbox), om, FALSE, TRUE, 0);

	label = gtk_label_new (_("ago"));
	gtk_widget_show (label);
	gtk_misc_set_padding (GTK_MISC (label), 3, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

	gtk_widget_show (hbox);
	
	PRIV (fds)->span_chooser = hbox;
	PRIV (fds)->omenu = om;
	PRIV (fds)->spinbutton = sb;

	/* if we do this earlier, we get the signal before the private
	 * members have been set up. */
	gtk_signal_connect (adj, "value_changed",
			    adj_value_changed, fds);
}

static void
omenu_item_activated (GtkMenuItem *item, gpointer user_data)
{
	FilterDatespec *fds = (FilterDatespec *) user_data;
	GtkOptionMenu *om;
	timespan *old_ts, *new_ts;
	int cur_val;
	gfloat new_val;

	if (!PRIV (fds)->recent_item) {
		PRIV (fds)->recent_item = GTK_WIDGET (item);
		return;
	}

	om = GTK_OPTION_MENU (PRIV (fds)->omenu);
	old_ts = gtk_object_get_data (GTK_OBJECT (PRIV (fds)->recent_item), "timespan");
	new_ts = gtk_object_get_data (GTK_OBJECT (item), "timespan");

	cur_val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (PRIV (fds)->spinbutton));

	/*if (old_ts->seconds > new_ts->seconds)*/
	new_val = ceil (cur_val * old_ts->seconds / new_ts->seconds);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (PRIV (fds)->spinbutton), new_val);
	PRIV (fds)->recent_item = GTK_WIDGET (item);
}

static void
adj_value_changed (GtkAdjustment *adj, gpointer user_data)
{
	FilterDatespec *fds = (FilterDatespec *) user_data;
	GtkOptionMenu *om;
	timespan *ts;

	om = GTK_OPTION_MENU (PRIV (fds)->omenu);

	if (om->menu_item == NULL) /* this has happened to me... dunno what it means */
		return;

	ts = gtk_object_get_data (GTK_OBJECT (om->menu_item), "timespan");
	fds->value = ts->seconds * 
		(gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (PRIV (fds)->spinbutton)));
}

static void
set_adjustments (FilterDatespec *fds)
{
	time_t val;
	int i;
	
	val = fds->value;

	for (i = 0; i < N_TIMESPANS; i++) {
		if (val % timespans[i].seconds == 0) {
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (PRIV (fds)->spinbutton),
						   (gfloat) val / timespans[i].seconds);
			break;
		}
	}

	gtk_option_menu_set_history (GTK_OPTION_MENU (PRIV (fds)->omenu),
				     N_TIMESPANS - (i + 1));
}

static gchar *
format_time (time_t time)
{
	struct tm *as_tm;
	char buf[128];

	/* no idea if this format is the 'correct' one */

	as_tm = localtime (&time);
	strftime (buf, 128, _("%b %d %l:%M %p"), as_tm);
	return g_strdup (buf);
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
		desc = format_time (fds->value);
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
	struct tm seltime;
	
	seltime.tm_sec = 0;
	seltime.tm_min = 0;
	seltime.tm_hour = 0;
	seltime.tm_mday = cal->selected_day;
	seltime.tm_mon = cal->month;
	seltime.tm_year = cal->year - 1900;
	seltime.tm_isdst = -1;
	
	fds->value = mktime (&seltime);
}

static void 
cal_day_selected_double_click (GtkCalendar *cal, gpointer user_data)
{
	FilterDatespec *fds = (FilterDatespec *)user_data;
	
	cal_day_selected (cal, user_data);
	PRIV (fds)->double_click = TRUE;
	gnome_dialog_close (PRIV (fds)->gd);
}

