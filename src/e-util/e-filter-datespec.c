/*
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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-filter-datespec.h"
#include "e-filter-part.h"
#include "e-misc-utils.h"

#ifdef G_OS_WIN32
#ifdef localtime_r
#undef localtime_r
#endif
#define localtime_r(tp,tmp) memcpy(tmp,localtime(tp),sizeof(struct tm))
#endif

#define d(x)

typedef struct {
	guint32 seconds;
	const gchar *past_singular;
	const gchar *past_plural;
	const gchar *future_singular;
	const gchar *future_plural;
	gfloat max;
} timespan;

#if 0

/* Don't delete this code, since it is needed so that xgettext can extract the translations.
 * Please, keep these strings in sync with the strings in the timespans array */

	ngettext ("1 second ago", "%d seconds ago", 1);
	ngettext ("1 second in the future", "%d seconds in the future", 1);
	ngettext ("1 minute ago", "%d minutes ago", 1);
	ngettext ("1 minute in the future", "%d minutes in the future", 1);
	ngettext ("1 hour ago", "%d hours ago", 1);
	ngettext ("1 hour in the future", "%d hours in the future", 1);
	ngettext ("1 day ago", "%d days ago", 1);
	ngettext ("1 day in the future", "%d days in the future", 1);
	ngettext ("1 week ago", "%d weeks ago", 1);
	ngettext ("1 week in the future", "%d weeks in the future", 1)
	ngettext ("1 month ago", "%d months ago", 1);
	ngettext ("1 month in the future", "%d months in the future", 1);
	ngettext ("1 year ago", "%d years ago", 1);
	ngettext ("1 year in the future", "%d years in the future", 1);

#endif

static const timespan timespans[] = {
	{ 1, "1 second ago", "%d seconds ago", "1 second in the future", "%d seconds in the future", 59.0 },
	{ 60, "1 minute ago", "%d minutes ago", "1 minute in the future", "%d minutes in the future", 59.0 },
	{ 3600, "1 hour ago", "%d hours ago", "1 hour in the future", "%d hours in the future", 23.0 },
	{ 86400, "1 day ago", "%d days ago", "1 day in the future", "%d days in the future", 31.0 },
	{ 604800, "1 week ago", "%d weeks ago", "1 week in the future", "%d weeks in the future", 52.0 },
	{ 2419200, "1 month ago", "%d months ago", "1 month in the future", "%d months in the future", 12.0 },
	{ 31557600, "1 year ago", "%d years ago", "1 year in the future", "%d years in the future", 1000.0 },
};

#define DAY_INDEX 3

struct _EFilterDatespecPrivate {
	GtkWidget *label_button;
	GtkWidget *notebook_type, *combobox_type, *calendar_specify, *spin_relative, *combobox_relative, *combobox_past_future;
	EFilterDatespecType type;
	gint span;
};

G_DEFINE_TYPE_WITH_PRIVATE (EFilterDatespec, e_filter_datespec, E_TYPE_FILTER_ELEMENT)

static gint
get_best_span (time_t val)
{
	gint i;

	for (i = G_N_ELEMENTS (timespans) - 1; i >= 0; i--) {
		if (val % timespans[i].seconds == 0)
			return i;
	}

	return 0;
}

static void
describe_to_buffer (EFilterDatespec *fds,
		    gchar *buf,
		    gint buf_size,
		    gboolean with_fallback)
{
	switch (fds->type) {
	case FDST_UNKNOWN:
		if (with_fallback)
			g_snprintf (buf, buf_size, _("<click here to select a date>"));
		else
			g_snprintf (buf, buf_size, "%s", "");
		break;
	case FDST_NOW:
		g_snprintf (buf, buf_size, _("now"));
		break;
	case FDST_SPECIFIED: {
		struct tm tm;

		localtime_r (&fds->value, &tm);
		/* strftime for date filter display, only needs to show a day date (i.e. no time) */
		strftime (buf, buf_size, _("%d-%b-%Y"), &tm);
		break; }
	case FDST_X_AGO:
		if (fds->value == 0)
			g_snprintf (buf, buf_size, _("now"));
		else {
			gint span, count;

			span = get_best_span (fds->value);
			count = fds->value / timespans[span].seconds;
			g_snprintf (buf, buf_size, ngettext (timespans[span].past_singular, timespans[span].past_plural, count), count);
		}
		break;
	case FDST_X_FUTURE:
		if (fds->value == 0)
			g_snprintf (buf, buf_size, _("now"));
		else {
			gint span, count;

			span = get_best_span (fds->value);
			count = fds->value / timespans[span].seconds;
			g_snprintf (buf, buf_size, ngettext (timespans[span].future_singular, timespans[span].future_plural, count), count);
		}
		break;
	}
}

/* sets button label */
static void
set_button (EFilterDatespec *fds)
{
	gchar buf[128];

	describe_to_buffer (fds, buf, sizeof (buf), TRUE);

	gtk_label_set_text ((GtkLabel *) fds->priv->label_button, buf);
}

static void
get_values (EFilterDatespec *fds)
{
	EFilterDatespec *self = E_FILTER_DATESPEC (fds);

	switch (fds->priv->type) {
	case FDST_SPECIFIED: {
		guint year, month, day;
		struct tm tm;

		gtk_calendar_get_date ((GtkCalendar *) self->priv->calendar_specify, &year, &month, &day);
		memset (&tm, 0, sizeof (tm));
		tm.tm_mday = day;
		tm.tm_year = year - 1900;
		tm.tm_mon = month;
		fds->value = mktime (&tm);
		/* what about timezone? */
		break; }
	case FDST_X_FUTURE:
	case FDST_X_AGO: {
		gint val;

		val = gtk_spin_button_get_value_as_int ((GtkSpinButton *) self->priv->spin_relative);
		fds->value = timespans[self->priv->span].seconds * val;
		break; }
	case FDST_NOW:
	default:
		break;
	}

	fds->type = self->priv->type;
}

static void
set_values (EFilterDatespec *fds)
{
	gint note_type;
	EFilterDatespec *self = E_FILTER_DATESPEC (fds);

	self->priv->type = fds->type == FDST_UNKNOWN ? FDST_NOW : fds->type;

	note_type = self->priv->type == FDST_X_FUTURE ? FDST_X_AGO : self->priv->type; /* FUTURE and AGO use the same notebook pages/etc. */

	switch (self->priv->type) {
	case FDST_NOW:
	case FDST_UNKNOWN:
		/* noop */
		break;
	case FDST_SPECIFIED:
	{
		struct tm tm;

		localtime_r (&fds->value, &tm);
		gtk_calendar_select_month ((GtkCalendar *) self->priv->calendar_specify, tm.tm_mon, tm.tm_year + 1900);
		gtk_calendar_select_day ((GtkCalendar *) self->priv->calendar_specify, tm.tm_mday);
		break;
	}
	case FDST_X_AGO:
		self->priv->span = get_best_span (fds->value);
		gtk_spin_button_set_value ((GtkSpinButton *) self->priv->spin_relative, fds->value / timespans[self->priv->span].seconds);
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->combobox_relative), self->priv->span);
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->combobox_past_future), 0);
		break;
	case FDST_X_FUTURE:
		self->priv->span = get_best_span (fds->value);
		gtk_spin_button_set_value ((GtkSpinButton *) self->priv->spin_relative, fds->value / timespans[self->priv->span].seconds);
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->combobox_relative), self->priv->span);
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->combobox_past_future), 1);
		break;
	}

	gtk_notebook_set_current_page ((GtkNotebook *) self->priv->notebook_type, note_type);
	gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->combobox_type), note_type);
}

static void
set_combobox_type (GtkComboBox *combobox,
                   EFilterDatespec *fds)
{
	fds->priv->type = gtk_combo_box_get_active (combobox);
	gtk_notebook_set_current_page ((GtkNotebook *) fds->priv->notebook_type, fds->priv->type);
}

static void
set_combobox_relative (GtkComboBox *combobox,
                       EFilterDatespec *fds)
{
	fds->priv->span = gtk_combo_box_get_active (combobox);
}

static void
set_combobox_past_future (GtkComboBox *combobox,
                          EFilterDatespec *fds)
{
	if (gtk_combo_box_get_active (combobox) == 0)
		fds->type = fds->priv->type = FDST_X_AGO;
	else
		fds->type = fds->priv->type = FDST_X_FUTURE;
}

static void
button_clicked (GtkButton *button,
                EFilterDatespec *fds)
{
	EFilterDatespec *self = E_FILTER_DATESPEC (fds);
	GtkWidget *content_area;
	GtkWidget *toplevel;
	GtkDialog *dialog;
	GtkBuilder *builder;

	/* XXX I think we're leaking the GtkBuilder. */
	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "filter.ui");

	toplevel = e_builder_get_widget (builder, "filter_datespec");

	dialog = (GtkDialog *) gtk_dialog_new ();
	gtk_window_set_title (
		GTK_WINDOW (dialog),
		_("Select a time to compare against"));
	gtk_dialog_add_buttons (
		dialog,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	self->priv->notebook_type = e_builder_get_widget (builder, "notebook_type");
	self->priv->combobox_type = e_builder_get_widget (builder, "combobox_type");
	self->priv->calendar_specify = e_builder_get_widget (builder, "calendar_specify");
	self->priv->spin_relative = e_builder_get_widget (builder, "spin_relative");
	self->priv->combobox_relative = e_builder_get_widget (builder, "combobox_relative");
	self->priv->combobox_past_future = e_builder_get_widget (builder, "combobox_past_future");

	set_values (fds);

	g_signal_connect (
		self->priv->combobox_type, "changed",
		G_CALLBACK (set_combobox_type), fds);
	g_signal_connect (
		self->priv->combobox_relative, "changed",
		G_CALLBACK (set_combobox_relative), fds);
	g_signal_connect (
		self->priv->combobox_past_future, "changed",
		G_CALLBACK (set_combobox_past_future), fds);

	content_area = gtk_dialog_get_content_area (dialog);
	gtk_box_pack_start (GTK_BOX (content_area), toplevel, TRUE, TRUE, 3);

	if (gtk_dialog_run (dialog) == GTK_RESPONSE_OK) {
		get_values (fds);
		set_button (fds);
	}

	gtk_widget_destroy ((GtkWidget *) dialog);
}

static gboolean
filter_datespec_validate (EFilterElement *element,
                          EAlert **alert)
{
	EFilterDatespec *fds = E_FILTER_DATESPEC (element);
	gboolean valid;

	g_warn_if_fail (alert == NULL || *alert == NULL);

	valid = fds->type != FDST_UNKNOWN;
	if (!valid) {
		if (alert)
			*alert = e_alert_new ("filter:no-date", NULL);
	}

	return valid;
}

static gint
filter_datespec_eq (EFilterElement *element_a,
                    EFilterElement *element_b)
{
	EFilterDatespec *datespec_a = E_FILTER_DATESPEC (element_a);
	EFilterDatespec *datespec_b = E_FILTER_DATESPEC (element_b);

	/* Chain up to parent's eq() method. */
	if (!E_FILTER_ELEMENT_CLASS (e_filter_datespec_parent_class)->
		eq (element_a, element_b))
		return FALSE;

	return (datespec_a->type == datespec_b->type) &&
		(datespec_a->value == datespec_b->value);
}

static xmlNodePtr
filter_datespec_xml_encode (EFilterElement *element)
{
	xmlNodePtr value, work;
	EFilterDatespec *fds = E_FILTER_DATESPEC (element);
	gchar str[32];

	d (printf ("Encoding datespec as xml\n"));

	value = xmlNewNode (NULL, (xmlChar *)"value");
	xmlSetProp (value, (xmlChar *)"name", (xmlChar *) element->name);
	xmlSetProp (value, (xmlChar *)"type", (xmlChar *)"datespec");

	work = xmlNewChild (value, NULL, (xmlChar *)"datespec", NULL);
	sprintf (str, "%d", fds->type);
	xmlSetProp (work, (xmlChar *)"type", (xmlChar *) str);
	sprintf (str, "%d", (gint) fds->value);
	xmlSetProp (work, (xmlChar *)"value", (xmlChar *) str);

	return value;
}

static gint
filter_datespec_xml_decode (EFilterElement *element,
                            xmlNodePtr node)
{
	EFilterDatespec *fds = E_FILTER_DATESPEC (element);
	xmlNodePtr n;
	xmlChar *val;

	d (printf ("Decoding datespec from xml %p\n", element));

	xmlFree (element->name);
	element->name = (gchar *) xmlGetProp (node, (xmlChar *)"name");

	n = node->children;
	while (n) {
		if (!strcmp ((gchar *) n->name, "datespec")) {
			val = xmlGetProp (n, (xmlChar *)"type");
			fds->type = atoi ((gchar *) val);
			xmlFree (val);
			val = xmlGetProp (n, (xmlChar *)"value");
			fds->value = atoi ((gchar *) val);
			xmlFree (val);
			break;
		}
		n = n->next;
	}

	return 0;
}

static GtkWidget *
filter_datespec_get_widget (EFilterElement *element)
{
	EFilterDatespec *fds = E_FILTER_DATESPEC (element);
	GtkWidget *button;

	fds->priv->label_button = gtk_label_new ("");
	set_button (fds);

	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), fds->priv->label_button);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (button_clicked), fds);

	gtk_widget_show (button);
	gtk_widget_show (fds->priv->label_button);

	return button;
}

static void
filter_datespec_format_sexp (EFilterElement *element,
                             GString *out)
{
	EFilterDatespec *fds = E_FILTER_DATESPEC (element);

	switch (fds->type) {
	case FDST_UNKNOWN:
		g_warning ("user hasn't selected a datespec yet!");
		/* fall through */
	case FDST_NOW:
		g_string_append (out, "(get-current-date)");
		break;
	case FDST_SPECIFIED:
		g_string_append_printf (out, "%d", (gint) fds->value);
		break;
	case FDST_X_AGO:
		switch (get_best_span (fds->value)) {
		case 5: /* months */
			g_string_append_printf (out, "(get-relative-months (- 0 %d))", (gint) (fds->value / timespans[5].seconds));
			break;
		case 6: /* years */
			g_string_append_printf (out, "(get-relative-months (- 0 %d))", (gint) (12 * fds->value / timespans[6].seconds));
			break;
		default:
			g_string_append_printf (out, "(- (get-current-date) %d)", (gint) fds->value);
			break;
		}
		break;
	case FDST_X_FUTURE:
		switch (get_best_span (fds->value)) {
		case 5: /* months */
			g_string_append_printf (out, "(get-relative-months %d)", (gint) (fds->value / timespans[5].seconds));
			break;
		case 6: /* years */
			g_string_append_printf (out, "(get-relative-months %d)", (gint) (12 * fds->value / timespans[6].seconds));
			break;
		default:
			g_string_append_printf (out, "(+ (get-current-date) %d)", (gint) fds->value);
			break;
		}
		break;
	}
}

static void
filter_datespec_describe (EFilterElement *element,
			  GString *out)
{
	EFilterDatespec *fds = E_FILTER_DATESPEC (element);
	gchar buf[128];

	describe_to_buffer (fds, buf, sizeof (buf), FALSE);

	g_string_append (out, buf);
}

static void
e_filter_datespec_class_init (EFilterDatespecClass *class)
{
	EFilterElementClass *filter_element_class;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->validate = filter_datespec_validate;
	filter_element_class->eq = filter_datespec_eq;
	filter_element_class->xml_encode = filter_datespec_xml_encode;
	filter_element_class->xml_decode = filter_datespec_xml_decode;
	filter_element_class->get_widget = filter_datespec_get_widget;
	filter_element_class->format_sexp = filter_datespec_format_sexp;
	filter_element_class->describe = filter_datespec_describe;
}

static void
e_filter_datespec_init (EFilterDatespec *datespec)
{
	datespec->priv = e_filter_datespec_get_instance_private (datespec);
	datespec->type = FDST_UNKNOWN;
}

/**
 * filter_datespec_new:
 *
 * Create a new EFilterDatespec object.
 *
 * Return value: A new #EFilterDatespec object.
 **/
EFilterDatespec *
e_filter_datespec_new (void)
{
	return g_object_new (E_TYPE_FILTER_DATESPEC, NULL);
}
