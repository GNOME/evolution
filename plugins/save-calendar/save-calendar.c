/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Rodrigo Moya <rodrigo@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

/* This is prototype code only, this may, or may not, use undocumented
 * unstable or private internal function calls. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#ifdef USE_GTKFILECHOOSER
#  include <gtk/gtkfilechooser.h>
#  include <gtk/gtkfilechooserdialog.h>
#else
#  include <gtk/gtkfilesel.h>
#endif
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtk.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <calendar/gui/e-cal-popup.h>
#include <libgnomevfs/gnome-vfs.h>
#include <string.h>


enum {  /* GtkComboBox enum */
	DEST_NAME_COLUMN,
	DEST_HANDLER,
	N_DEST_COLUMNS

};
enum { /* CSV helper enum */
	ECALCOMPONENTTEXT,
	ECALCOMPONENTATTENDEE,
	CONSTCHAR
};

typedef struct _CsvConfig CsvConfig;
struct _CsvConfig {
	gchar *newline;
	gchar *quote;
	gchar *delimiter;
	gboolean header;
};
void org_gnome_save_calendar (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_save_tasks (EPlugin *ep, ECalPopupTargetSource *target);
static gboolean string_needsquotes (const char *value, CsvConfig *config);

static void
display_error_message (GtkWidget *parent, GError *error)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
					 error->message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}


/* Some helpers for the csv stuff */
static GString *
add_list_to_csv (GString *line, GSList *list_in, CsvConfig *config, gint type)
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
		gint cnt=0;
		while (list) {
			const char *str = NULL;
			if (cnt == 0)
				tmp = g_string_new ("");
			if (cnt > 0)
				needquotes = TRUE;
			switch (type) {
			case ECALCOMPONENTATTENDEE:
				str = ((ECalComponentAttendee*)list->data)->value;
				break;
			case ECALCOMPONENTTEXT:
				str = ((ECalComponentText*)list->data)->value;
				break;
			case CONSTCHAR:
			default:
				str = list->data;
				break;
			}
			if (!needquotes)
				needquotes = string_needsquotes (str, config);
			if (str)
				tmp = g_string_append (tmp, (const gchar*)str);
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
add_nummeric_to_csv (GString *line, gint *nummeric, CsvConfig *config)
{

	/* 
	 * This one will write {-1}..{00}..{01}..{99}
	 * it prepends a 0 if it's < 10 and > -1
	 */

	if (nummeric)
		g_string_append_printf  (line, "%s%d", (*nummeric<10 && *nummeric>-1)?"0":"", *nummeric);

	line = g_string_append (line, config->delimiter);
	return line;
}

static GString *
add_time_to_csv (GString *line, icaltimetype *time, CsvConfig *config)
{
	/*
	 * Perhaps we should check for quotes, delimiter and newlines in the
	 * resulting string: The translators can put it there!
	 *
	 * Or perhaps we shouldn't make this translatable?
	 * Or perhaps there is a library-function to do this?
	 */

	if (time) {
		g_string_append_printf (line, _("%s%d/%s%d/%s%d %s%d:%s%d:%s%d"), 
					(time->month < 10)?"0":"", time->month, 
					(time->day < 10)?"0":"", time->day, 
					(time->year < 10)?"0":"", time->year, 
					(time->hour < 10)?"0":"", time->hour, 
					(time->minute < 10)?"0":"", time->minute, 
					(time->second < 10)?"0":"", time->second);
	}

	line = g_string_append (line, config->delimiter);
	return line;
}

static gboolean
string_needsquotes (const char *value, CsvConfig *config)
{

	/* This is the actual need for quotes-checker */

	/* 
	 * These are the simple substring-checks 
	 *
	 * Example: {Mom, can you please do that for me?}
	 * Will be written as {"Mom, can you please do that for me?"}
	 */

	gboolean needquotes = strstr (value, config->delimiter) ? TRUE:FALSE;

	if (!needquotes) {
		needquotes = strstr (value, config->newline) ? TRUE:FALSE;
		if (!needquotes) 
			needquotes = strstr (value, config->quote) ? TRUE:FALSE;
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
		if ((len == 2) && (config->delimiter[1] = ' ')) {
			needquotes = strchr (value, config->delimiter[0])?TRUE:FALSE;
			if (!needquotes) {
				gint len = strlen (config->newline);
				if ((len == 2) && (config->newline[1] = ' ')) {
					needquotes = strchr (value, config->newline[0])?TRUE:FALSE;
					if (!needquotes) {
						gint len = strlen (config->quote);
						if ((len == 2) && (config->quote[1] = ' ')) {
							needquotes = strchr 
								(value, config->quote[0])?TRUE:FALSE;
						}
					}
				}
			}
		}
	}

	return needquotes;
}

static GString *
add_string_to_csv (GString *line, const char *value, CsvConfig *config)
{
	/* Will add a string to the record and will check for the need for quotes */

	if ((value) && (strlen(value)>0)) {
		gboolean needquotes = string_needsquotes (value, config);

		if (needquotes)
			line = g_string_append (line, config->quote);
		line = g_string_append (line, (const gchar*)value);
		if (needquotes)
			line = g_string_append (line, config->quote);
	}
	line = g_string_append (line, config->delimiter);
	return line;
}

static void
do_save_calendar_csv (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, char *dest_uri, gpointer data)
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
	ECal *source_client;
	GError *error = NULL;
	GList *objects=NULL;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	GnomeVFSURI *uri;
	GString *line = NULL;
	CsvConfig *config = data;
 
	primary_source = e_source_selector_peek_primary_selection (target->selector);

	if (!dest_uri)
		return;

	/* open source client */
	source_client = e_cal_new (primary_source, type);
	if (!e_cal_open (source_client, TRUE, &error)) {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error);
		g_object_unref (source_client);
		g_error_free (error);
		return;
	}

	uri = gnome_vfs_uri_new (dest_uri);
	result = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_WRITE);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_create (&handle, dest_uri, GNOME_VFS_OPEN_WRITE, TRUE, GNOME_VFS_PERM_USER_ALL);
		result = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_WRITE);
	}

	if (result == GNOME_VFS_OK && e_cal_get_object_list_as_comp (source_client, "#t", &objects, NULL)) {

		if (config->header) {
			line = g_string_new ("");
			g_string_append_printf (line, _("Uid%sSummary%sDescription List%sCategories List%s"
							"Comment List%sCompleted%sCreated%sContact List%s"
							"Start%sEnd%sDue%sPercent Done%sPriority%sUrl%s"
							"Attendees List%sLocation%sModified%s"),
						config->delimiter, config->delimiter, config->delimiter, config->delimiter, 
						config->delimiter, config->delimiter, config->delimiter, config->delimiter, 
						config->delimiter, config->delimiter, config->delimiter, config->delimiter, 
						config->delimiter, config->delimiter, config->delimiter, config->delimiter, 
						config->newline);

			gnome_vfs_write (handle, line->str, line->len, NULL);
			g_string_free (line, TRUE);
		}

	
		while (objects != NULL) {
			ECalComponent *comp = objects->data;
			gchar *delimiter_temp = NULL;
			const char *temp_constchar;
			GSList *temp_list;
			ECalComponentDateTime temp_dt;
			struct icaltimetype *temp_time;
			int *temp_int;
			ECalComponentText temp_comptext; 

			line = g_string_new ("");

			/* Getting the stuff */
			e_cal_component_get_uid (comp, &temp_constchar);
			line = add_string_to_csv (line, temp_constchar, config);

			e_cal_component_get_summary (comp, &temp_comptext);
			line = add_string_to_csv (line, &temp_comptext?temp_comptext.value:NULL, config);

			e_cal_component_get_description_list (comp, &temp_list);
			line = add_list_to_csv (line, temp_list, config, ECALCOMPONENTTEXT);
			if (temp_list)
				e_cal_component_free_text_list (temp_list);

			e_cal_component_get_categories_list (comp, &temp_list);
			line = add_list_to_csv (line, temp_list, config, CONSTCHAR);
			if (temp_list)
				e_cal_component_free_categories_list (temp_list);

			e_cal_component_get_comment_list (comp, &temp_list);
			line = add_list_to_csv (line, temp_list, config, ECALCOMPONENTTEXT);
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
			line = add_list_to_csv (line, temp_list, config, ECALCOMPONENTTEXT);
			if (temp_list)
				e_cal_component_free_text_list (temp_list);

			e_cal_component_get_dtstart (comp, &temp_dt);
			line = add_time_to_csv (line, temp_dt.value ? temp_dt.value : NULL, config);
			if (temp_dt.value)
				e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_dtend (comp, &temp_dt);
			line = add_time_to_csv (line, temp_dt.value ? temp_dt.value : NULL, config);
			if (temp_dt.value)
				e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_due (comp, &temp_dt);
			line = add_time_to_csv (line, temp_dt.value ? temp_dt.value : NULL, config);
			if (temp_dt.value)
				e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_percent (comp, &temp_int);
			line = add_nummeric_to_csv (line, temp_int, config);

			e_cal_component_get_priority (comp, &temp_int);
			line = add_nummeric_to_csv (line, temp_int, config);

			e_cal_component_get_url (comp, &temp_constchar);
			line = add_string_to_csv (line, temp_constchar, config);

			if (e_cal_component_has_attendees (comp)) {
				e_cal_component_get_attendee_list (comp, &temp_list);
				line = add_list_to_csv (line, temp_list, config, ECALCOMPONENTATTENDEE);
				if (temp_list)
					e_cal_component_free_attendee_list (temp_list);
			} else {
				line = add_list_to_csv (line, NULL, config, ECALCOMPONENTATTENDEE);
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
			 * if (temp_time) e_cal_component_free_icaltimetype (temp_time);
			 *
			 * Please uncomment and fix documentation if untrue
			 * http://www.gnome.org/projects/evolution/developer-doc/libecal/ECalComponent.html
			 *	#e-cal-component-get-last-modified
			 */
			gnome_vfs_write (handle, line->str, line->len, NULL);

			/* It's written, so we can free it */
			g_string_free (line, TRUE);

			objects = g_list_next (objects);
		}

		gnome_vfs_close (handle);
	}
	g_object_unref (source_client);
}

static void
do_save_calendar_ical (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, char *dest_uri, gpointer data)
{
	ESource *primary_source;
	ECal *source_client, *dest_client;
	GError *error = NULL;

	primary_source = e_source_selector_peek_primary_selection (target->selector);

	if (!dest_uri)
		return;

	/* open source client */
	source_client = e_cal_new (primary_source, type);
	if (!e_cal_open (source_client, TRUE, &error)) {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error);
		g_object_unref (source_client);
		g_error_free (error);
		return;
	}

	/* open destination client */
	error = NULL;
	dest_client = e_cal_new_from_uri (dest_uri, type);
	if (e_cal_open (dest_client, FALSE, &error)) {
		GList *objects;

		if (e_cal_get_object_list (source_client, "#t", &objects, NULL)) {
			while (objects != NULL) {
				icalcomponent *icalcomp = objects->data;

				/* FIXME: deal with additions/modifications */

				/* FIXME: This stores a directory with one file in it, the user expects only a file */

				/* FIXME: It would be nice if this ical-handler would use gnome-vfs rather than e_cal_* */

				error = NULL;
				if (!e_cal_create_object (dest_client, icalcomp, NULL, &error)) {
					display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error);
					g_error_free (error);
				}

				/* remove item from the list */
				objects = g_list_remove (objects, icalcomp);
				icalcomponent_free (icalcomp);
			}
		}
	} else {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error);
		g_error_free (error);
	}

	/* terminate */
	g_object_unref (source_client);
	g_object_unref (dest_client);
}


static void 
on_type_combobox_changed (GtkComboBox *combobox, gpointer data)
{
	void (*handler) (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, char *dest_uri, gpointer data);
	GtkTreeIter iter;
	GtkWidget *csv_options = data;
	GtkTreeModel *model = gtk_combo_box_get_model (combobox);

	gtk_combo_box_get_active_iter (combobox, &iter);

	gtk_tree_model_get (model, &iter, 
		DEST_HANDLER, &handler, -1);

	if (handler == do_save_calendar_csv) {
		gtk_widget_show (csv_options);
	} else {
		gtk_widget_hide (csv_options);
	}
}

/* Convert what the user types to what he probably means */
static gchar *
userstring_to_systemstring (const gchar *userstring)
{
	const gchar *text = userstring;
	gint i=0, len = strlen(text);
	GString *str = g_string_new ("");
	gchar *retval = NULL;

	while (i < len) {
		if (text[i] == '\\') {
			switch (text[i+1]) {
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
ask_destination_and_save (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type)
{
	void (*handler) (EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, char *dest_uri, gpointer data);
	gpointer data = NULL;
	CsvConfig *config = NULL;

	GtkWidget *extra_widget = gtk_vbox_new (FALSE, 0);
	GtkComboBox *combo = GTK_COMBO_BOX(gtk_combo_box_new ());
	GtkTreeModel *model = GTK_TREE_MODEL (gtk_list_store_new (N_DEST_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER));
	GtkCellRenderer *renderer=NULL;
	GtkListStore *store = GTK_LIST_STORE (model);
	GtkTreeIter iter;
	GtkWidget *dialog = NULL;
	char *dest_uri = NULL;
	GtkWidget *header_check = gtk_check_button_new_with_label (_("Prepend a header")),
		*delimiter_entry = gtk_entry_new (),
		*newline_entry = gtk_entry_new (),
		*quote_entry = gtk_entry_new (),
		*table = gtk_table_new (4, 2, FALSE), *label = NULL,
		*csv_options = gtk_expander_new (_("Advanced options for the CSV format")),
		*vbox = gtk_vbox_new (FALSE, 0);

	/* The Type GtkComboBox */
	gtk_combo_box_set_model (combo, model);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), 
					renderer, "text", DEST_NAME_COLUMN, NULL);

	gtk_box_pack_start (GTK_BOX (extra_widget), GTK_WIDGET (combo), TRUE, TRUE, 0);
	gtk_list_store_clear (store);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, DEST_NAME_COLUMN, _("iCalendar format (.ics)"), -1);
	gtk_list_store_set (store, &iter, DEST_HANDLER, do_save_calendar_ical, -1);
	gtk_combo_box_set_active_iter (combo, &iter);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, DEST_NAME_COLUMN, _("Comma separated value format (.csv)"), -1);
	gtk_list_store_set (store, &iter, DEST_HANDLER, do_save_calendar_csv, -1);

	/* Advanced CSV options */
	gtk_entry_set_text (GTK_ENTRY(delimiter_entry), ", ");
	gtk_entry_set_text (GTK_ENTRY(quote_entry), "\"");
	gtk_entry_set_text (GTK_ENTRY(newline_entry), "\\n");

	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);
	label = gtk_label_new (_("Value delimiter:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.0);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, 
			  (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0); 
	gtk_table_attach (GTK_TABLE (table), delimiter_entry, 1, 2, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	label = gtk_label_new (_("Record delimiter:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.0);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, 
			  (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0); 
	gtk_table_attach (GTK_TABLE (table), newline_entry, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	label = gtk_label_new (_("Encapsulate values with:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.0);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, 
			  (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0); 
	gtk_table_attach (GTK_TABLE (table), quote_entry, 1, 2, 2, 3,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_box_pack_start (GTK_BOX (extra_widget), GTK_WIDGET (csv_options), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), header_check, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (csv_options), vbox);
	gtk_widget_show_all (vbox);

	g_signal_connect (G_OBJECT(combo), "changed", G_CALLBACK (on_type_combobox_changed), csv_options);

#ifdef USE_GTKFILECHOOSER
	dialog = gtk_file_chooser_dialog_new (_("Select destination file"),
					      NULL,
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE_AS, GTK_RESPONSE_OK,
		 			      NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), extra_widget);
#else
	/* I haven't tested this yet */
	dialog = gtk_file_selection_new (_("Select destination file"));
	gtk_box_pack_start (GTK_BOX (GTK_FILE_SELECTION (dialog)->main_vbox), extra_widget);
#endif
	gtk_widget_show_all (extra_widget);

	/* The default is ical, so hide the new options for now */
	gtk_widget_hide (csv_options);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	gtk_combo_box_get_active_iter (combo, &iter);
	gtk_tree_model_get (model, &iter, DEST_HANDLER, &handler, -1);

#ifdef USE_GTKFILECHOOSER
       dest_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
#else
       dest_uri = g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (dialog)));
#endif

	if (handler == do_save_calendar_csv) {
		config = g_new (CsvConfig, 1);

		config->delimiter = userstring_to_systemstring (gtk_entry_get_text (GTK_ENTRY(delimiter_entry)));
		config->newline = userstring_to_systemstring (gtk_entry_get_text (GTK_ENTRY(newline_entry)));
		config->quote = userstring_to_systemstring (gtk_entry_get_text (GTK_ENTRY(quote_entry)));
		config->header = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (header_check));

		data = config;
	}

	gtk_widget_destroy (dialog);

	handler (ep, target, type, dest_uri, data);

	if (handler == do_save_calendar_csv) {
		g_free (config->delimiter);
		g_free (config->quote);
		g_free (config->newline);
		g_free (config);
	}

	g_free (dest_uri);
}

void
org_gnome_save_calendar (EPlugin *ep, ECalPopupTargetSource *target)
{
	ask_destination_and_save (ep, target, E_CAL_SOURCE_TYPE_EVENT);
}

void
org_gnome_save_tasks (EPlugin *ep, ECalPopupTargetSource *target)
{
	ask_destination_and_save (ep, target, E_CAL_SOURCE_TYPE_TODO);
}
