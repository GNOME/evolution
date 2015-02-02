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
 *		Philip Van Hoof <pvanhoof@gnome.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

#include "format-handler.h"

typedef struct _CsvConfig CsvConfig;
struct _CsvConfig {
	gchar *newline;
	gchar *quote;
	gchar *delimiter;
	gboolean header;
};

static gboolean string_needsquotes (const gchar *value, CsvConfig *config);

typedef struct _CsvPluginData CsvPluginData;
struct _CsvPluginData
{
	GtkWidget *delimiter_entry, *newline_entry, *quote_entry, *header_check;
};

static void
display_error_message (GtkWidget *parent,
                       GError *error)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (parent), 0,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		"%s", error->message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

enum { /* CSV helper enum */
	ECALCOMPONENTTEXT,
	ECALCOMPONENTATTENDEE,
	CONSTCHAR
};

/* Some helpers for the csv stuff */
static GString *
add_list_to_csv (GString *line,
                 GSList *list_in,
                 CsvConfig *config,
                 gint type)
{

	/*
	 * This one will write 'ECalComponentText' and 'const char' GSLists. It will
	 * put quotes around the complete written value if there's was only one value
	 * but it required having quotes and if there was more than one value (in which
	 * case delimiters are used to separate them, hence the need for the quotes).
	 */

	if (list_in) {
		gboolean needquotes = FALSE;
		GSList *list = list_in;
		GString *tmp = NULL;
		gint cnt = 0;
		while (list) {
			const gchar *str = NULL;
			if (cnt == 0)
				tmp = g_string_new ("");
			if (cnt > 0)
				needquotes = TRUE;
			switch (type) {
			case ECALCOMPONENTATTENDEE:
				str = ((ECalComponentAttendee *) list->data)->value;
				break;
			case ECALCOMPONENTTEXT:
				str = ((ECalComponentText *) list->data)->value;
				break;
			case CONSTCHAR:
			default:
				str = list->data;
				break;
			}
			if (!needquotes)
				needquotes = string_needsquotes (str, config);
			if (str)
				tmp = g_string_append (tmp, (const gchar *) str);
			list = g_slist_next (list); cnt++;
			if (list)
				tmp = g_string_append (tmp, config->delimiter);
		}

		if (needquotes)
			line = g_string_append (line, config->quote);
		line = g_string_append_len (line, tmp->str, tmp->len);
		g_string_free (tmp, TRUE);
		if (needquotes)
			line = g_string_append (line, config->quote);
	}

	line = g_string_append (line, config->delimiter);
	return line;
}

static GString *
add_nummeric_to_csv (GString *line,
                     gint *nummeric,
                     CsvConfig *config)
{

	/*
	 * This one will write {-1}..{00}..{01}..{99}
	 * it prepends a 0 if it's < 10 and > -1
	 */

	if (nummeric)
		g_string_append_printf (
			line, "%s%d",
			(*nummeric < 10 && *nummeric > -1) ? "0" : "",
			*nummeric);

	return g_string_append (line, config->delimiter);
}

static GString *
add_time_to_csv (GString *line,
                 icaltimetype *time,
                 CsvConfig *config)
{

	if (time) {
		gboolean needquotes = FALSE;
		struct tm mytm = icaltimetype_to_tm (time);
		gchar *str = (gchar *) g_malloc (sizeof (gchar) * 200);

		/* Translators: the %F %T is the third argument for a
		 * strftime function.  It lets you define the formatting
		 * of the date in the csv-file. */
		e_utf8_strftime (str, 200, _("%F %T"), &mytm);

		needquotes = string_needsquotes (str, config);

		if (needquotes)
			line = g_string_append (line, config->quote);

		line = g_string_append (line, str);

		if (needquotes)
			line = g_string_append (line, config->quote);

		g_free (str);

	}

	line = g_string_append (line, config->delimiter);

	return line;
}

static gboolean
string_needsquotes (const gchar *value,
                    CsvConfig *config)
{

	/* This is the actual need for quotes-checker */

	/*
	 * These are the simple substring-checks
	 *
	 * Example: {Mom, can you please do that for me?}
	 * Will be written as {"Mom, can you please do that for me?"}
	 */

	gboolean needquotes = strstr (value, config->delimiter) ? TRUE : FALSE;

	if (!needquotes) {
		needquotes = strstr (value, config->newline) ? TRUE : FALSE;
		if (!needquotes)
			needquotes = strstr (value, config->quote) ? TRUE : FALSE;
	}

	/*
	 * If the special-char is char+onespace (so like {, } {" }, {\n }) and it occurs
	 * the value that is going to be written
	 *
	 * In this case we don't trust the user . . . and are going to quote the string
	 * just to play save -- Quoting is always allowed in the CSV format. If you can
	 * avoid it, it's better to do so since a lot applications don't support CSV
	 * correctly! --.
	 *
	 * Example: {Mom,can you please do that for me?}
	 * This example will be written as {"Mom,can you please do that for me?"} because
	 * there's a {,} behind {Mom} and the delimiter is {, } (so we searched only the
	 * first character of {, } and didn't trust the user).
	 */

	if (!needquotes) {
		gint len = strlen (config->delimiter);
		if ((len == 2) && (config->delimiter[1] == ' ')) {
			needquotes = strchr (value, config->delimiter[0]) ? TRUE : FALSE;
			if (!needquotes) {
				len = strlen (config->newline);
				if ((len == 2) && (config->newline[1] == ' ')) {
					needquotes = strchr (value, config->newline[0]) ? TRUE : FALSE;
					if (!needquotes) {
						len = strlen (config->quote);
						if ((len == 2) && (config->quote[1] == ' ')) {
							needquotes = strchr
								(value, config->quote[0]) ? TRUE : FALSE;
						}
					}
				}
			}
		}
	}

	return needquotes;
}

static GString *
add_string_to_csv (GString *line,
                   const gchar *value,
                   CsvConfig *config)
{
	/* Will add a string to the record and will check for the need for quotes */

	if ((value) && (strlen (value) > 0)) {
		gboolean needquotes = string_needsquotes (value, config);

		if (needquotes)
			line = g_string_append (line, config->quote);
		line = g_string_append (line, (const gchar *) value);
		if (needquotes)
			line = g_string_append (line, config->quote);
	}
	line = g_string_append (line, config->delimiter);
	return line;
}

/* Convert what the user types to what he probably means */
static gchar *
userstring_to_systemstring (const gchar *userstring)
{
	const gchar *text = userstring;
	gint i = 0, len = strlen (text);
	GString *str = g_string_new ("");
	gchar *retval = NULL;

	while (i < len) {
		if (text[i] == '\\') {
			switch (text[i + 1]) {
			case 'n':
				str = g_string_append_c (str, '\n');
				i++;
				break;
			case '\\':
				str = g_string_append_c (str, '\\');
				i++;
				break;
			case 'r':
				str = g_string_append_c (str, '\r');
				i++;
				break;
			case 't':
				str = g_string_append_c (str, '\t');
				i++;
				break;
			}
		} else {
			str = g_string_append_c (str, text[i]);
		}

		i++;
	}

	retval = str->str;
	g_string_free (str, FALSE);

	return retval;
}

static void
do_save_calendar_csv (FormatHandler *handler,
                      ESourceSelector *selector,
		      EClientCache *client_cache,
                      gchar *dest_uri)
{

	/*
	 * According to some documentation about CSV, newlines 'are' allowed
	 * in CSV-files. But you 'do' have to put the value between quotes.
	 * The helper 'string_needsquotes' will check for that
	 *
	 * http://www.creativyst.com/Doc/Articles/CSV/CSV01.htm
	 * http://www.creativyst.com/cgi-bin/Prod/15/eg/csv2xml.pl
	 */

	ESource *primary_source;
	EClient *source_client;
	GError *error = NULL;
	GSList *objects = NULL;
	GOutputStream *stream;
	GString *line = NULL;
	CsvConfig *config = NULL;
	CsvPluginData *d = handler->data;
	const gchar *tmp = NULL;

	if (!dest_uri)
		return;

	/* open source client */
	primary_source = e_source_selector_ref_primary_selection (selector);
	source_client = e_client_cache_get_client_sync (client_cache,
		primary_source, e_source_selector_get_extension_name (selector), 30, NULL, &error);
	g_object_unref (primary_source);

	/* Sanity check. */
	g_return_if_fail (
		((source_client != NULL) && (error == NULL)) ||
		((source_client == NULL) && (error != NULL)));

	if (source_client == NULL) {
		display_error_message (
			gtk_widget_get_toplevel (GTK_WIDGET (selector)),
			error);
		g_error_free (error);
		return;
	}

	config = g_new (CsvConfig, 1);

	tmp = gtk_entry_get_text (GTK_ENTRY (d->delimiter_entry));
	config->delimiter = userstring_to_systemstring (tmp ? tmp:", ");
	tmp = gtk_entry_get_text (GTK_ENTRY (d->newline_entry));
	config->newline = userstring_to_systemstring (tmp ? tmp:"\\n");
	tmp = gtk_entry_get_text (GTK_ENTRY (d->quote_entry));
	config->quote = userstring_to_systemstring (tmp ? tmp:"\"");
	config->header = gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (d->header_check));

	stream = open_for_writing (
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (selector))),
		dest_uri, &error);

	if (stream && e_cal_client_get_object_list_as_comps_sync (E_CAL_CLIENT (source_client), "#t", &objects, NULL, NULL)) {
		GSList *iter;

		if (config->header) {

			gint i = 0;

			static const gchar *labels[] = {
				 N_("UID"),
				 N_("Summary"),
				 N_("Description List"),
				 N_("Categories List"),
				 N_("Comment List"),
				 N_("Completed"),
				 N_("Created"),
				 N_("Contact List"),
				 N_("Start"),
				 N_("End"),
				 N_("Due"),
				 N_("percent Done"),
				 N_("Priority"),
				 N_("URL"),
				 N_("Attendees List"),
				 N_("Location"),
				 N_("Modified"),
			};

			line = g_string_new ("");
			for (i = 0; i < G_N_ELEMENTS (labels); i++) {
				if (i > 0)
					g_string_append (line, config->delimiter);
				g_string_append (line, _(labels[i]));
			}

			g_string_append (line, config->newline);

			g_output_stream_write_all (
				stream, line->str, line->len,
				NULL, NULL, NULL);
			g_string_free (line, TRUE);
		}

		for (iter = objects; iter; iter = iter->next) {
			ECalComponent *comp = iter->data;
			gchar *delimiter_temp = NULL;
			const gchar *temp_constchar;
			GSList *temp_list;
			ECalComponentDateTime temp_dt;
			struct icaltimetype *temp_time;
			gint *temp_int;
			ECalComponentText temp_comptext;

			line = g_string_new ("");

			/* Getting the stuff */
			e_cal_component_get_uid (comp, &temp_constchar);
			line = add_string_to_csv (line, temp_constchar, config);

			e_cal_component_get_summary (comp, &temp_comptext);
			line = add_string_to_csv (
				line, temp_comptext.value, config);

			e_cal_component_get_description_list (comp, &temp_list);
			line = add_list_to_csv (
				line, temp_list, config, ECALCOMPONENTTEXT);
			if (temp_list)
				e_cal_component_free_text_list (temp_list);

			e_cal_component_get_categories_list (comp, &temp_list);
			line = add_list_to_csv (
				line, temp_list, config, CONSTCHAR);
			if (temp_list)
				e_cal_component_free_categories_list (temp_list);

			e_cal_component_get_comment_list (comp, &temp_list);
			line = add_list_to_csv (
				line, temp_list, config, ECALCOMPONENTTEXT);
			if (temp_list)
				e_cal_component_free_text_list (temp_list);

			e_cal_component_get_completed (comp, &temp_time);
			line = add_time_to_csv (line, temp_time, config);
			if (temp_time)
				e_cal_component_free_icaltimetype (temp_time);

			e_cal_component_get_created (comp, &temp_time);
			line = add_time_to_csv (line, temp_time, config);
			if (temp_time)
				e_cal_component_free_icaltimetype (temp_time);

			e_cal_component_get_contact_list (comp, &temp_list);
			line = add_list_to_csv (
				line, temp_list, config, ECALCOMPONENTTEXT);
			if (temp_list)
				e_cal_component_free_text_list (temp_list);

			e_cal_component_get_dtstart (comp, &temp_dt);
			line = add_time_to_csv (
				line, temp_dt.value ?
				temp_dt.value : NULL, config);
			e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_dtend (comp, &temp_dt);
			line = add_time_to_csv (
				line, temp_dt.value ?
				temp_dt.value : NULL, config);
			e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_due (comp, &temp_dt);
			line = add_time_to_csv (
				line, temp_dt.value ?
				temp_dt.value : NULL, config);
			e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_percent (comp, &temp_int);
			line = add_nummeric_to_csv (line, temp_int, config);

			e_cal_component_get_priority (comp, &temp_int);
			line = add_nummeric_to_csv (line, temp_int, config);

			e_cal_component_get_url (comp, &temp_constchar);
			line = add_string_to_csv (line, temp_constchar, config);

			if (e_cal_component_has_attendees (comp)) {
				e_cal_component_get_attendee_list (comp, &temp_list);
				line = add_list_to_csv (
					line, temp_list, config,
					ECALCOMPONENTATTENDEE);
				if (temp_list)
					e_cal_component_free_attendee_list (temp_list);
			} else {
				line = add_list_to_csv (
					line, NULL, config,
					ECALCOMPONENTATTENDEE);
			}

			e_cal_component_get_location (comp, &temp_constchar);
			line = add_string_to_csv (line, temp_constchar, config);

			e_cal_component_get_last_modified (comp, &temp_time);

			/* Append a newline (record delimiter) */
			delimiter_temp = config->delimiter;
			config->delimiter = config->newline;

			line = add_time_to_csv (line, temp_time, config);

			/* And restore for the next record */
			config->delimiter = delimiter_temp;

			/* Important note!
			 * The documentation is not requiring this!
			 *
			 * if (temp_time)
			 *     e_cal_component_free_icaltimetype (temp_time);
			 *
			 * Please uncomment and fix documentation if untrue
			 * http://www.gnome.org/projects/evolution/
			 *	developer-doc/libecal/ECalComponent.html
			 *	#e-cal-component-get-last-modified
			 */
			g_output_stream_write_all (
				stream, line->str, line->len,
				NULL, NULL, &error);

			/* It's written, so we can free it */
			g_string_free (line, TRUE);
		}

		g_output_stream_close (stream, NULL, NULL);

		e_cal_client_free_ecalcomp_slist (objects);
	}

	if (stream)
		g_object_unref (stream);

	g_object_unref (source_client);

	g_free (config->delimiter);
	g_free (config->quote);
	g_free (config->newline);
	g_free (config);

	if (error != NULL) {
		display_error_message (
			gtk_widget_get_toplevel (GTK_WIDGET (selector)),
			error);
		g_error_free (error);
	}
}

static GtkWidget *
create_options_widget (FormatHandler *handler)
{
	GtkWidget *table = gtk_table_new (4, 2, FALSE), *label = NULL,
		  *csv_options = gtk_expander_new_with_mnemonic (
			_("A_dvanced options for the CSV format")),
		  *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	CsvPluginData *d = handler->data;

	d->delimiter_entry = gtk_entry_new ();
	d->newline_entry = gtk_entry_new ();
	d->quote_entry = gtk_entry_new ();
	d->header_check = gtk_check_button_new_with_mnemonic (
		_("Prepend a _header"));

	/* Advanced CSV options */
	gtk_entry_set_text (GTK_ENTRY (d->delimiter_entry), ", ");
	gtk_entry_set_text (GTK_ENTRY (d->quote_entry), "\"");
	gtk_entry_set_text (GTK_ENTRY (d->newline_entry), "\\n");

	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);
	label = gtk_label_new_with_mnemonic (_("_Value delimiter:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), d->delimiter_entry);
	gtk_table_attach (
		GTK_TABLE (table), label, 0, 1, 0, 1,
		(GtkAttachOptions) (GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (
		GTK_TABLE (table), d->delimiter_entry, 1, 2, 0, 1,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);
	label = gtk_label_new_with_mnemonic (_("_Record delimiter:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), d->newline_entry);
	gtk_table_attach (
		GTK_TABLE (table), label, 0, 1, 1, 2,
		(GtkAttachOptions) (GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (
		GTK_TABLE (table), d->newline_entry, 1, 2, 1, 2,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);
	label = gtk_label_new_with_mnemonic (_("_Encapsulate values with:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), d->quote_entry);
	gtk_table_attach (
		GTK_TABLE (table), label, 0, 1, 2, 3,
		(GtkAttachOptions) (GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (
		GTK_TABLE (table), d->quote_entry, 1, 2, 2, 3,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_box_pack_start (GTK_BOX (vbox), d->header_check, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_widget_show_all (vbox);

	gtk_container_add (GTK_CONTAINER (csv_options), vbox);

	return csv_options;
}

FormatHandler *csv_format_handler_new (void)
{
	FormatHandler *handler = g_new0 (FormatHandler, 1);

	handler->isdefault = FALSE;
	handler->combo_label = _("Comma separated values (.csv)");
	handler->filename_ext = ".csv";
	handler->data = g_new (CsvPluginData, 1);
	handler->options_widget = create_options_widget (handler);
	handler->save = do_save_calendar_csv;

	return handler;
}
