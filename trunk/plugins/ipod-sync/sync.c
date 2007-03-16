/*
 * sync.c
 *
 * (C)2004 Justin Wake <jwake@iinet.net.au>
 *
 * Licensed under the GNU GPL v2. See COPYING.
 *
 */

#include "config.h"
#include "evolution-ipod-sync.h"
#include <gnome.h>

#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include <libecal/e-cal.h>
#include <libical/ical.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define EBOOK_SOURCE_LIST "/apps/evolution/addressbook/sources"
#define ECAL_SOURCE_LIST "/apps/evolution/calendar/sources"
#define ETASK_SOURCE_LIST "/apps/evolution/tasks/sources"

extern GtkWidget *progress_bar;
extern IPod ipod_info;

static void pulse (void)
{
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progress_bar));
	g_main_context_iteration (NULL, FALSE);
}

/**
 * Something bad happened.
 */
static void error_dialog (char *title, char *error)
{
	GtkWidget *error_dlg = 
			gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
											"<span weight=\"bold\" size=\"larger\">"
											"%s</span>\n\n%s.", title, error);
	
	gtk_dialog_set_has_separator (GTK_DIALOG (error_dlg), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (error_dlg), 5);
	gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (error_dlg)->label),
									  TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dlg),
												GTK_RESPONSE_OK);
	
	gtk_dialog_run (GTK_DIALOG (error_dlg));
	gtk_widget_destroy (error_dlg);
}

/**
 * Something really bad happened.
 */
static void critical_error (char *title, char *error)
{
	error_dialog (title, error);
	gtk_main_quit ();
	exit (EXIT_FAILURE);
}

static GSList *
get_source_uris_for_type (char *key)
{
	ESourceList *sources;
	GSList		*groups;
	GSList		*uris = NULL;
	GSList		*item, *source;
	sources = e_source_list_new_for_gconf_default (key);
	groups = e_source_list_peek_groups (sources);

	for (item = groups; item != NULL; item = item->next)
	{
		ESourceGroup *group;

		g_assert (item->data != NULL);

		group = E_SOURCE_GROUP (item->data);
		for (source = e_source_group_peek_sources(group);
			  source != NULL;
			  source = source->next)
		{
			gchar *uri;
			g_assert (source->data != NULL);
			uri = e_source_get_uri (E_SOURCE (source->data));
			uris = g_slist_append (uris, uri);
		}
	}

	g_object_unref (sources);

	return uris;
}

static void
free_uri_list (GSList *uris)
{
	g_slist_foreach (uris, (GFunc)g_free, NULL);
	g_slist_free (uris);
}

/**
 * Force the data into little-endian output.
 *
 * Note: data must be of even length.
 */
static void
force_little_endian (gunichar2 *data, int length)
{
	int i;
	
	/* We're big-endian? 
	   (A little tidier than before) */
	if (G_BYTE_ORDER == G_BIG_ENDIAN)
	{
		for (i = 0; i < length; i++)
		{
			gunichar2 c = data[i];

			c = ((((guint16)(c) & 0xFF00) >> 8) |
				  (((guint16)(c) & 0x00FF) << 8));

			data[i] = c;
		}
	}	
}

/**
 * Write a string of data to a file on the iPod.
 *
 * Will return if the write worked, otherwise will
 * display an error dialog and end the program.
 */
static void
write_to_ipod (GString *str, char *path, char *filename)
{
	char *output_path;
	char *output_file;
	FILE *f;
	guchar		*utf8;
	gunichar2	*utf16;
	guchar		bom[2] = {0xFF, 0xFE};
	int			i, count;
	
	output_path = g_build_path (G_DIR_SEPARATOR_S,
										ipod_info.mount_point,
										path, NULL);

	if (!g_file_test (output_path, G_FILE_TEST_IS_DIR))
	{
		if (mkdir (output_path, 0777) != 0)
			critical_error (_("No output directory!"),
								 _("The output directory was not found on "
								 	"iPod! Please ensure that iPod has been correctly "
									"set up and try again."));
	}

	output_file = g_build_filename (output_path, filename, NULL);

	g_free (output_path);
	
	f = fopen (output_file, "w");

	g_free (output_file);
	
	if (f == NULL)
	{
		critical_error (_("Could not export data!"), strerror (errno));
	}

	/* Convert the input string into UTF16 */
	utf8 = str->str;
	if (g_utf8_validate (utf8, -1, NULL))
	{
		utf16 = g_utf8_to_utf16 (utf8, -1, NULL, NULL, NULL);
		
		/* Swap the bytes if we're big-endian so that the output
		 * remains little-endian according to the BOM. */
		force_little_endian (utf16, g_utf8_strlen (utf8, -1));
	}
	
	count = 2 * g_utf8_strlen (utf8, -1);
	
	/* Write the BOM 
	 * 0xFF 0xFE
	 * UTF-16 Little Endian
	 */
	for (i = 0; i < 2; i++)
		fwrite (&bom[i], 1, 1, f);
	
	if ((fwrite(utf16, count, 1, f) != 1) &&
		 (count > 0))
	{	
		g_free (utf16);
		fclose (f);
		critical_error (_("Could not export data!"),
							 _("Exporting data failed."));
	}

	g_free (utf16);
	fclose (f);
}

static GString *
uri_list_to_vcard_string (GSList *uris)
{
	GString 		*str = NULL;
	EBook 		*book = NULL;
	EBookQuery 	*qry = NULL;
	GList			*contacts = NULL, *c = NULL;
	GSList		*uri;

	qry = e_book_query_field_exists (E_CONTACT_FILE_AS);
	
	str = g_string_new (NULL);
	
	for (uri = uris; uri != NULL; uri = uri->next)
	{
		g_assert (uri->data != NULL);

		book = e_book_new_from_uri (uri->data, NULL);
	
		if (e_book_open (book, TRUE, NULL) == FALSE)
		{
			error_dialog (_("Could not open addressbook!"),
							  _("Could not open the Evolution addressbook to export data."));

			/* Maybe the next one will work. */
			continue;
		}

		if (e_book_get_contacts (book, qry, &contacts, NULL) == FALSE)
		{
			/* Looks like this one is empty. */
			g_object_unref (book);
			continue;
		}

		/* Loop through the contacts, adding them to the string. */
		for (c = contacts; c != NULL; c = c->next)
		{
			gchar *tmp;
			EContact *contact = E_CONTACT (c->data);
			
			tmp = e_vcard_to_string (E_VCARD (contact),
											 EVC_FORMAT_VCARD_30);

			g_string_append (str, tmp);
			g_string_append (str, "\r\n");
			g_free (tmp);
			g_object_unref (contact);
		}

		if (contacts != NULL)
			g_list_free (contacts);
		
		g_object_unref (book);
	}

	/* Okay, all done. */
	e_book_query_unref (qry);

	return (str);
}

static GString *
uri_list_to_vcal_string (GSList *uris, ECalSourceType type)
{
	GString 		*str = NULL;
	ECal 			*cal = NULL;
	icalcomponent *obj = NULL;
	GList			*objects = NULL, *o = NULL;
	GSList		*uri;
	
	str = g_string_new (NULL);

	for (uri = uris; uri != NULL; uri = uri->next)
	{
		g_assert (uri->data != NULL);

		cal = e_cal_new_from_uri (uri->data, type);
		
		if (e_cal_open (cal, TRUE, NULL) == FALSE)
		{
			error_dialog (_("Could not open calendar/todo!"),
							  _("Could not open the Evolution calendar/todo list to export data."));

			/* Maybe the next one will work. */
			continue;
		}

		
		e_cal_get_object_list (cal, "#t", &objects, NULL);

		for (o = objects; o != NULL; o = o->next)
		{
			gchar *tmp;
			icalcomponent *comp;
		
			g_assert (o->data != NULL);

			comp = o->data;
			tmp = e_cal_get_component_as_string (cal, comp);
			g_string_append (str, tmp);
			g_free (tmp);
		}
		
		g_object_unref (cal);
	}

	/* Okay, all done. */

	return (str);
}

/* Attempt to export the addressbook. */
static void
export_addressbook (void)
{
	GSList *uris;
	GString *data;
	pulse ();
	
	uris = get_source_uris_for_type (EBOOK_SOURCE_LIST);

	pulse ();
	
	data = uri_list_to_vcard_string (uris);

	write_to_ipod (data, "/Contacts/", "evolution.vcf");
	
	g_string_free (data, TRUE);

	pulse ();

	free_uri_list (uris);

	pulse ();
}

/* Attempt to export the calendar(s). */
static void
export_calendar (void)
{
	GSList *uris;
	GString *data;

	pulse ();

	uris = get_source_uris_for_type (ECAL_SOURCE_LIST);

	pulse ();

	data = uri_list_to_vcal_string (uris, E_CAL_SOURCE_TYPE_EVENT);

	write_to_ipod (data, "/Calendars/", "evolution-cal.ics");
	
	g_string_free (data, TRUE);

	free_uri_list (uris);
	
	pulse ();
}

/* Attempt to export the task list(s). */
static void
export_tasks (void)
{
	GSList *uris;
	GString *data;

	pulse ();

	uris = get_source_uris_for_type (ETASK_SOURCE_LIST);

	pulse ();

	data = uri_list_to_vcal_string (uris, E_CAL_SOURCE_TYPE_TODO);

	write_to_ipod (data, "/Calendars/", "evolution-todo.ics");

	g_string_free (data, TRUE);

	free_uri_list (uris);
	
	pulse ();
}

void
export_to_ipod (void)
{
	pulse ();
	
	if (ipod_info.addressbook == TRUE)
		export_addressbook ();

	if (ipod_info.calendar == TRUE)
		export_calendar ();
	
	if (ipod_info.tasks == TRUE)
		export_tasks ();

	pulse ();
	sync ();
	pulse ();
	return;
}

