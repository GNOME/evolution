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
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtknotebook.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-stock.h>
#include <glade/glade.h>

#include "filter-datespec.h"
#include "e-util/e-sexp.h"

#define d(x)

static gboolean validate (FilterElement *fe);
static int date_eq(FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *fds);
static void format_sexp (FilterElement *, GString *);

static void filter_datespec_class_init (FilterDatespecClass *class);
static void filter_datespec_init (FilterDatespec *gspaper);
static void filter_datespec_finalise (GtkObject *obj);

#define PRIV(x) (((FilterDatespec *)(x))->priv)

typedef struct _timespan {
	guint32 seconds;
	const gchar *singular;
	const gchar *plural;
	gfloat max;
} timespan;

static const timespan timespans[] = {
	{ 1, N_("second"), N_("seconds"), 59.0 },
	{ 60, N_("minute"), N_("minutes"), 59.0 },
	{ 3600, N_("hour"), N_("hours"), 23.0 },
	{ 86400, N_("day"), N_("days"), 31.0 },
	{ 604800, N_("week"), N_("weeks"), 52.0 },
	{ 2419200, N_("month"), N_("months"), 12.0 },
	{ 31557600, N_("year"), N_("years"), 1000.0 },
};

#define DAY_INDEX 3
#define N_TIMESPANS (sizeof (timespans) / sizeof (timespans[0]))

struct _FilterDatespecPrivate {
	GtkWidget *label_button;
	GtkWidget *notebook_type, *option_type, *calendar_specify, *spin_relative, *option_relative;
	FilterDatespec_type type;
	int span;
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
	filter_element->eq = date_eq;
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
}

static void
filter_datespec_finalise(GtkObject *obj)
{
	FilterDatespec *o = (FilterDatespec *)obj;
	
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
	gboolean valid;
	
	valid = fds->type != FDST_UNKNOWN;
	if (!valid) {
		GtkWidget *gd = gnome_ok_dialog (_("You have forgotten to choose a date."));
		gnome_dialog_run_and_close (GNOME_DIALOG (gd));
	}
	
	return valid;
}

static int
date_eq(FilterElement *fe, FilterElement *cm)
{
	FilterDatespec *fd = (FilterDatespec *)fe, *cd = (FilterDatespec *)cm;

        return ((FilterElementClass *)(parent_class))->eq(fe, cm)
		&& (fd->type == cd->type)
		&& (fd->value == cd->value);
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

static int get_best_span(time_t val)
{
	int i;

	for (i=N_TIMESPANS-1;i>=0;i--) {
		if (val % timespans[i].seconds == 0)
			return i;
	}

	return 0;
}

/* sets button label */
static void
set_button(FilterDatespec *fds)
{
	char buf[128];
	char *label = buf;

	switch (fds->type) {
	case FDST_UNKNOWN:
		label = _("<click here to select a date>");
		break;
	case FDST_NOW:
		label = _("now");
		break;
	case FDST_SPECIFIED: {
		struct tm tm;

		localtime_r(&fds->value, &tm);
		/* strftime for date filter display, only needs to show a day date (i.e. no time) */
		strftime(buf, sizeof(buf), _("%d-%b-%Y"), &tm);
		break; }
	case FDST_X_AGO:
		if (fds->value == 0)
			label = _("now");
		else {
			int span, count;

			span = get_best_span(fds->value);
			count = fds->value / timespans[span].seconds;

			if (count == 1)
				/* 1 (minute|day|...) ago (singular time ago) */
				sprintf(buf, _("%d %s ago"), count, timespans[span].singular);
			else
				/* N (minutes|days|...) ago (plural time ago) */
				sprintf(buf, _("%d %s ago"), count, timespans[span].plural);
		}
		break;
	}

	gtk_label_set_text((GtkLabel *)fds->priv->label_button, label);
}

static void
get_values(FilterDatespec *fds)
{
	struct _FilterDatespecPrivate *p = PRIV(fds);

	switch(fds->priv->type) {
	case FDST_SPECIFIED: {
		guint year, month, day;
		struct tm tm;

		gtk_calendar_get_date((GtkCalendar *)p->calendar_specify, &year, &month, &day);
		memset(&tm, 0, sizeof(tm));
		tm.tm_mday = day;
		tm.tm_year = year - 1900;
		tm.tm_mon = month;
		fds->value = mktime(&tm);
		/* what about timezone? */
		break; }
	case FDST_X_AGO: {
		int val;

		val = gtk_spin_button_get_value_as_int((GtkSpinButton *)p->spin_relative);
		fds->value = timespans[p->span].seconds * val;
		break; }
	case FDST_NOW:
	default:
		break;
	}

	fds->type = p->type;
}

static void
set_values(FilterDatespec *fds)
{
	struct _FilterDatespecPrivate *p = PRIV(fds);

	p->type = fds->type==FDST_UNKNOWN ? FDST_NOW : fds->type;

	switch (p->type) {
	case FDST_NOW:
	case FDST_UNKNOWN:
		/* noop */
		break;
	case FDST_SPECIFIED: {
		struct tm tm;

		localtime_r(&fds->value, &tm);
		gtk_calendar_select_month((GtkCalendar*)p->calendar_specify, tm.tm_mon, tm.tm_year + 1900);
		gtk_calendar_select_day((GtkCalendar*)p->calendar_specify, tm.tm_mday);
		break; }
	case FDST_X_AGO:
		p->span = get_best_span(fds->value);
		gtk_spin_button_set_value((GtkSpinButton*)p->spin_relative, fds->value/timespans[p->span].seconds);
		gtk_option_menu_set_history((GtkOptionMenu*)p->option_relative, p->span);
		break;
	}

	gtk_notebook_set_page((GtkNotebook*)p->notebook_type, p->type);
	gtk_option_menu_set_history((GtkOptionMenu*)p->option_type, p->type);
}


static void
set_option_type(GtkMenu *menu, FilterDatespec *fds)
{
	GtkWidget *w;

	/* ugh, no other way to 'get_history' */
	w = gtk_menu_get_active(menu);
	fds->priv->type = g_list_index(GTK_MENU_SHELL(menu)->children, w);
	gtk_notebook_set_page((GtkNotebook*)fds->priv->notebook_type, fds->priv->type);
}

static void
set_option_relative(GtkMenu *menu, FilterDatespec *fds)
{
	GtkWidget *w;

	w = gtk_menu_get_active(menu);
	fds->priv->span = g_list_index(GTK_MENU_SHELL(menu)->children, w);
}

static void
dialogue_clicked(GnomeDialog *gd, int button, FilterDatespec *fds)
{
	if (button != 0)
		return;

	get_values(fds);
	set_button(fds);
}

static void
button_clicked (GtkButton *button, FilterDatespec *fds)
{
	GnomeDialog *gd;
	struct _FilterDatespecPrivate *p = PRIV(fds);
	GtkWidget *w, *x;
	GladeXML *gui;

	gui = glade_xml_new(FILTER_GLADEDIR "/filter.glade", "filter_datespec");
	w = glade_xml_get_widget(gui, "filter_datespec");

	gd = (GnomeDialog *) gnome_dialog_new (_("Select a time to compare against"), 
					       GNOME_STOCK_BUTTON_OK, 
					       GNOME_STOCK_BUTTON_CANCEL, 
					       NULL);

	p->notebook_type = glade_xml_get_widget(gui, "notebook_type");
	p->option_type = glade_xml_get_widget(gui, "option_type");
	p->calendar_specify = glade_xml_get_widget(gui, "calendar_specify");
	p->spin_relative = glade_xml_get_widget(gui, "spin_relative");
	p->option_relative = glade_xml_get_widget(gui, "option_relative");

	set_values(fds);

	gtk_signal_connect((GtkObject *)GTK_OPTION_MENU(p->option_type)->menu, "deactivate", set_option_type, fds);
	gtk_signal_connect((GtkObject *)GTK_OPTION_MENU(p->option_relative)->menu, "deactivate", set_option_relative, fds);

	gtk_box_pack_start ((GtkBox *)gd->vbox, w, TRUE, TRUE, 3);

	gtk_signal_connect((GtkObject *)gd, "clicked", dialogue_clicked, fds);

	gnome_dialog_run_and_close(gd);

	return;
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterDatespec *fds = (FilterDatespec *)fe;
	GtkWidget *button;
	
	fds->priv->label_button = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (fds->priv->label_button), 0.5, 0.5);
	set_button(fds);
	
	button = gtk_button_new();
	gtk_container_add (GTK_CONTAINER (button), fds->priv->label_button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", button_clicked, fds);
	
	gtk_widget_show (button);
	gtk_widget_show (fds->priv->label_button);
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
