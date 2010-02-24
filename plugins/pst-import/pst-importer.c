/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* pst-importer.c
*
* Author: Chris Halls <chris.halls@credativ.co.uk>
*	  Bharath Acharya <abharath@novell.com>
*
* Copyright (C) 2006  Chris Halls
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) version 3.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with the program; if not, see <http://www.gnu.org/licenses/>
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define G_LOG_DOMAIN "eplugin-readpst"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include <gtk/gtk.h>

#include <e-util/e-import.h>

#include <libebook/e-contact.h>
#include <libebook/e-book.h>

#include <libecal/e-cal.h>
#include <libecal/e-cal-component.h>

#include <libedataserver/e-data-server-util.h>
#include <libedataserverui/e-source-selector-dialog.h>

#include <camel/camel-folder.h>
#include <camel/camel-store.h>
#include <camel/camel-exception.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-multipart.h>
#include <camel/camel-data-wrapper.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-file-utils.h>

#include <mail/mail-component.h>
#include <mail/mail-mt.h>
#include <mail/mail-tools.h>
#include <mail/em-utils.h>

#include <libpst/libpst.h>
#include <libpst/timeconv.h>

typedef struct _PstImporter PstImporter;

gint pst_init (pst_file *pst, gchar *filename);
gchar *get_pst_rootname (pst_file *pst, gchar *filename);
static void pst_error_msg (const gchar *fmt, ...);
static void pst_import_folders (PstImporter *m, pst_desc_tree *topitem);
static void pst_process_item (PstImporter *m, pst_desc_tree *d_ptr);
static void pst_process_folder (PstImporter *m, pst_item *item);
static void pst_process_email (PstImporter *m, pst_item *item);
static void pst_process_contact (PstImporter *m, pst_item *item);
static void pst_process_appointment (PstImporter *m, pst_item *item);
static void pst_process_task (PstImporter *m, pst_item *item);
static void pst_process_journal (PstImporter *m, pst_item *item);

static void pst_import_file (PstImporter *m);
gchar *foldername_to_utf8 (const gchar *pstname);
gchar *string_to_utf8(const gchar *string);
void contact_set_date (EContact *contact, EContactField id, FILETIME *date);
struct icaltimetype get_ical_date (FILETIME *date, gboolean is_date);
gchar *rfc2445_datetime_format (FILETIME *ft);

gboolean org_credativ_evolution_readpst_supported (EPlugin *epl, EImportTarget *target);
GtkWidget *org_credativ_evolution_readpst_getwidget (EImport *ei, EImportTarget *target, EImportImporter *im);
void org_credativ_evolution_readpst_import (EImport *ei, EImportTarget *target, EImportImporter *im);
void org_credativ_evolution_readpst_cancel (EImport *ei, EImportTarget *target, EImportImporter *im);
gint e_plugin_lib_enable (EPluginLib *ep, gint enable);

/* em-folder-selection-button.h is private, even though other internal evo plugins use it!
   so declare the functions here
   TODO: sort out whether this should really be private
*/
typedef struct _EMFolderSelectionButton        EMFolderSelectionButton;
GtkWidget *em_folder_selection_button_new (const gchar *title, const gchar *caption);
void        em_folder_selection_button_set_selection (EMFolderSelectionButton *button, const gchar *uri);
const gchar *em_folder_selection_button_get_selection (EMFolderSelectionButton *button);

static guchar pst_signature [] = { '!', 'B', 'D', 'N' };

struct _PstImporter {
	MailMsg base;

	EImport *import;
	EImportTarget *target;

	GMutex *status_lock;
	gchar *status_what;
	gint status_pc;
	gint status_timeout_id;
	CamelOperation *status;
	CamelException ex;

	pst_file pst;

	CamelOperation *cancel;
	CamelFolder *folder;
	gchar *parent_uri;
	gchar *folder_name;
	gchar *folder_uri;
	gint folder_count;
	gint current_item;

	EBook *addressbook;
	ECal *calendar;
	ECal *tasks;
	ECal *journal;
};

gboolean
org_credativ_evolution_readpst_supported (EPlugin *epl, EImportTarget *target)
{
	gchar signature[sizeof (pst_signature)];
	gboolean ret = FALSE;
	gint fd, n;
	EImportTargetURI *s;
	gchar *filename;

	if (target->type != E_IMPORT_TARGET_URI) {
		return FALSE;
	}

	s = (EImportTargetURI *)target;

	if (s->uri_src == NULL) {
		return TRUE;
	}

	if (strncmp (s->uri_src, "file:///", strlen ("file:///")) != 0) {
		return FALSE;
	}

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	fd = g_open (filename, O_RDONLY, 0);
	g_free (filename);

	if (fd != -1) {
		n = read (fd, signature, sizeof (pst_signature));
		ret = n == sizeof (pst_signature) && memcmp (signature, pst_signature, sizeof (pst_signature)) == 0;
		close (fd);
	}

	return ret;
}

static void
checkbox_mail_toggle_cb (GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-mail", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
checkbox_addr_toggle_cb (GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-addr", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
checkbox_appt_toggle_cb (GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-appt", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
checkbox_task_toggle_cb (GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-task", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
checkbox_journal_toggle_cb (GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-journal", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
folder_selected (EMFolderSelectionButton *button, EImportTargetURI *target)
{
	g_free (target->uri_dest);
	target->uri_dest = g_strdup (em_folder_selection_button_get_selection (button));
}

/**
 * Suggest a folder to import data into
 * @param target
 * @return
 */
static gchar *
get_suggested_foldername (EImportTargetURI *target)
{
	const gchar *inbox;
	gchar *delim, *filename;
	gchar *rootname = NULL;
	GString *foldername;
	pst_file pst;

	/* Suggest a folder that is in the same mail storage as the users' inbox,
	   with a name derived from the .PST file */
	inbox = mail_component_get_folder_uri (NULL, MAIL_COMPONENT_FOLDER_INBOX);

	delim = g_strrstr (inbox, "#");
	if (delim != NULL) {
		foldername = g_string_new_len (inbox, delim-inbox);
	} else {
		foldername = g_string_new (inbox);
	}

	g_string_append_c (foldername, '#');

	filename = g_filename_from_uri (target->uri_src, NULL, NULL);

	if (pst_init (&pst, filename) == 0) {
		rootname = get_pst_rootname (&pst, filename);
	}

	g_free (filename);

	if (rootname != NULL) {
		gchar *utf8name;
		utf8name = foldername_to_utf8 (rootname);
		g_string_append (foldername, utf8name);
		g_free (utf8name);
		g_free (rootname);
	} else {
		g_string_append (foldername, "outlook_data");
	}

	if (mail_tool_uri_to_folder (foldername->str, 0, NULL) != NULL) {
		/* Folder exists - add a number */
		gint i, len;
		len = foldername->len;
		CamelFolder *folder;

		for (i=1; i<10000; i++) {
			g_string_truncate (foldername, len);
			g_string_append_printf (foldername, "_%d", i);
			if ((folder=mail_tool_uri_to_folder (foldername->str, 0, NULL)) == NULL) {
				/* Folder does not exist */
				break;
			}
		}

		if (folder != NULL) {
			pst_error_msg ("Error searching for an unused folder name. uri=%s", foldername);
		}
	}

	return g_string_free (foldername, FALSE);

}

GtkWidget *
org_credativ_evolution_readpst_getwidget (EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *hbox, *framebox, *w;
	gchar *foldername;

	g_datalist_set_data (&target->data, "pst-do-mail", GINT_TO_POINTER (TRUE));
	g_datalist_set_data (&target->data, "pst-do-addr", GINT_TO_POINTER (TRUE));
	g_datalist_set_data (&target->data, "pst-do-appt", GINT_TO_POINTER (TRUE));
	g_datalist_set_data (&target->data, "pst-do-task", GINT_TO_POINTER (TRUE));
	g_datalist_set_data (&target->data, "pst-do-journal", GINT_TO_POINTER (TRUE));

	framebox = gtk_vbox_new (FALSE, 2);

	/* Mail */
	hbox = gtk_hbox_new (FALSE, 0);
	w = gtk_check_button_new_with_mnemonic (_("_Mail"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, TRUE);
	g_signal_connect (w, "toggled", G_CALLBACK (checkbox_mail_toggle_cb), target);
	gtk_box_pack_start ((GtkBox *) hbox, w, FALSE, FALSE, 0);

	w = em_folder_selection_button_new (_("Select folder"), _("Select folder to import into"));
	foldername = get_suggested_foldername ((EImportTargetURI *) target);
	((EImportTargetURI *) target)->uri_dest = g_strdup (foldername);
	em_folder_selection_button_set_selection ((EMFolderSelectionButton *) w, foldername);
	g_signal_connect (w, "selected", G_CALLBACK (folder_selected), target);
	gtk_box_pack_end ((GtkBox *) hbox, w, FALSE, FALSE, 0);

	w = gtk_label_new (_("Destination folder:"));
	gtk_box_pack_end ((GtkBox *) hbox, w, FALSE, TRUE, 6);

	gtk_box_pack_start ((GtkBox *) framebox, hbox, FALSE, FALSE, 0);

	/* Address book */
	w = gtk_check_button_new_with_mnemonic (_("_Address Book"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, FALSE);
	/*gtk_widget_set_sensitive ((GtkWidget *)w, FALSE);*/ /* Disable until implemented */
	g_signal_connect (w, "toggled", G_CALLBACK (checkbox_addr_toggle_cb), target);
	gtk_box_pack_start ((GtkBox *) framebox, w, FALSE, FALSE, 0);

	/* Appointments */
	w = gtk_check_button_new_with_mnemonic (_("A_ppointments"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, FALSE);
	g_signal_connect (w, "toggled", G_CALLBACK (checkbox_appt_toggle_cb), target);
	gtk_box_pack_start ((GtkBox *)framebox, w, FALSE, FALSE, 0);

	/* Tasks */
	w = gtk_check_button_new_with_mnemonic (_("_Tasks"));
	gtk_toggle_button_set_active ((GtkToggleButton *)w, FALSE);
	g_signal_connect (w, "toggled", G_CALLBACK (checkbox_task_toggle_cb), target);
	gtk_box_pack_start ((GtkBox *)framebox, w, FALSE, FALSE, 0);

	/* Journal */
	w = gtk_check_button_new_with_mnemonic (_("_Journal entries"));
	gtk_toggle_button_set_active ((GtkToggleButton *)w, FALSE);
	g_signal_connect (w, "toggled", G_CALLBACK (checkbox_journal_toggle_cb), target);
	gtk_box_pack_start ((GtkBox *)framebox, w, FALSE, FALSE, 0);

	gtk_widget_show_all (framebox);

	g_free (foldername);

	return framebox;
}

static gchar *
pst_import_describe (PstImporter *m, gint complete)
{
	return g_strdup (_("Importing Outlook data"));
}

static ECal*
open_ecal (ECalSourceType type, gchar *name)
{
	/* Hack - grab the first calendar we can find
		TODO - add a selection mechanism in get_widget */
	ESource *primary;
	ESourceList *source_list;
	ECal *cal;

	if ((e_cal_get_sources (&source_list, type, NULL)) == 0) {
		g_warning ("Could not get any sources of type %s.", name);
		return NULL;
	}

	primary = e_source_list_peek_source_any (source_list);

	if ((cal = e_cal_new (primary, type)) == NULL) {
		g_warning ("Could not create %s.", name);
		g_object_unref (source_list);
		return NULL;
	}

	e_cal_open (cal, TRUE, NULL);
	g_object_unref (primary);
	g_object_unref (source_list);

	return cal;
}

static void
pst_import_import (PstImporter *m)
{
	CamelOperation *oldcancel = NULL;

	oldcancel = camel_operation_register (m->status);

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-addr"))) {
		/* Hack - grab the first address book we can find
		   TODO - add a selection mechanism in get_widget */
		ESource *primary;
		ESourceList *source_list;

		if (e_book_get_addressbooks (&source_list, NULL)) {
			primary = e_source_list_peek_source_any (source_list);

			if ((m->addressbook = e_book_new (primary,NULL))) {
				e_book_open (m->addressbook, TRUE, NULL);
				g_object_unref (primary);
				g_object_unref (source_list);
			} else {
				g_warning ("Could not create EBook.");
			}
		} else {
			g_warning ("Could not get address books.");
		}
	}

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-appt"))) {
		m->calendar = open_ecal (E_CAL_SOURCE_TYPE_EVENT, "calendar");
	}

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-task"))) {
		m->tasks = open_ecal (E_CAL_SOURCE_TYPE_TODO, "task list");
	}

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-journal"))) {
		m->journal = open_ecal (E_CAL_SOURCE_TYPE_JOURNAL, "journal");
	}

	pst_import_file (m);

/* FIXME: Crashes often in here.
	if (m->addressbook) {
		g_object_unref (m->addressbook);
	}

	if (m->calendar) {
		g_object_unref (m->calendar);
	}

	if (m->tasks) {
		g_object_unref (m->tasks);
	}

	if (m->journal) {
		g_object_unref (m->journal);
	}
*/
	camel_operation_register (oldcancel);
}

static void
pst_import_file (PstImporter *m)
{
	gint ret;
	gchar *filename;
	pst_item *item = NULL;
	pst_desc_tree *d_ptr;

	filename = g_filename_from_uri (((EImportTargetURI *)m->target)->uri_src, NULL, NULL);
	m->parent_uri = g_strdup (((EImportTargetURI *)m->target)->uri_dest); /* Destination folder, was set in our widget */

	camel_operation_start (NULL, _("Importing `%s'"), filename);

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-mail"))) {
		mail_tool_uri_to_folder (m->parent_uri, CAMEL_STORE_FOLDER_CREATE, &m->base.ex);
	}

	ret = pst_init (&m->pst, filename);

	if (ret < 0) {
		g_free (filename);
		camel_operation_end (NULL);
		return;
	}

	g_free (filename);

	camel_operation_progress_count (NULL, 1);

	if ((item = pst_parse_item (&m->pst, m->pst.d_head, NULL)) == NULL) {
		pst_error_msg ("Could not get root record");
		return;
	}

	camel_operation_progress_count (NULL, 2);

	if ((d_ptr = pst_getTopOfFolders (&m->pst, item)) == NULL) {
		pst_error_msg ("Top of folders record not found. Cannot continue");
		return;
	}

	camel_operation_progress_count (NULL, 3);
	pst_import_folders (m, d_ptr);

	camel_operation_progress_count (NULL, 4);

	camel_operation_end (NULL);

	pst_freeItem (item);

}

static void
pst_import_folders (PstImporter *m, pst_desc_tree *topitem)
{
	pst_desc_tree *d_ptr;
	gchar *seperator;

	d_ptr = topitem->child;

	/* Walk through folder tree */
	while (d_ptr != NULL && (camel_operation_cancel_check (NULL) == FALSE)) {

		pst_process_item (m, d_ptr);

		if (d_ptr->child != NULL) {
			g_free (m->parent_uri);
			m->parent_uri = g_strdup (m->folder_uri);
			d_ptr = d_ptr->child;
		} else if (d_ptr->next != NULL) {
			d_ptr = d_ptr->next;
		} else {
			while (d_ptr != topitem && d_ptr->next == NULL) {
				if (m->folder_uri) {
					g_free(m->folder_uri);
				}

				m->folder_uri = g_strdup (m->parent_uri);
				seperator = g_strrstr (m->parent_uri, "/");

				if (seperator != NULL) {
					*seperator = '\0'; /* Truncate uri */
				}

				d_ptr = d_ptr->parent;

			}
			if (d_ptr == topitem) {
				return;
			}

			d_ptr = d_ptr->next;
		}
	}
}

static void
pst_process_item (PstImporter *m, pst_desc_tree *d_ptr)
{
	pst_item *item = NULL;

	if (d_ptr->desc == NULL)
		return;

	item = pst_parse_item (&m->pst, d_ptr, NULL);

	if (item == NULL)
		return;

	if (item->message_store != NULL) {
		pst_error_msg ("A second message_store has been found - ignored");
		pst_freeItem (item);
		return;
	}

	if (item->folder != NULL) {
		pst_process_folder (m, item);
		camel_operation_start (NULL, _("Importing `%s'"), item->file_as.str);
	} else {
		if (m->folder_count && (m->current_item < m->folder_count)) {
			camel_operation_progress (NULL, (m->current_item * 100) / m->folder_count);
		} else {
			camel_operation_progress (NULL, 100);
		}

		if (item->email != NULL &&
			(item->type == PST_TYPE_NOTE || item->type == PST_TYPE_REPORT)) {

			if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-mail")))
				pst_process_email (m, item);

		} else if (item->contact && item->type == PST_TYPE_CONTACT) {

			if (m->addressbook && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-addr"))) {
				pst_process_contact (m, item);
			}

		} else if (item->type == PST_TYPE_APPOINTMENT && item->appointment) {

			if (m->calendar && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-appt"))) {
				pst_process_appointment (m, item);
			}

		} else if (item->type == PST_TYPE_TASK && item->appointment) {

			if (m->tasks && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-task"))) {
				pst_process_task (m, item);
			}

		} else if (item->type == PST_TYPE_JOURNAL && item->appointment) {

			if (m->journal && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-journal"))) {
				pst_process_journal (m, item);
			}

		}

		m->current_item++;
	}

	pst_freeItem (item);

	if (d_ptr->next == NULL) {
		camel_operation_end (NULL);
	}
}

/**
 * Convert string to utf8. Currently we just use the locale, but maybe there is encoding
 * information hidden somewhere in the PST file?
 *
 * @param string String from PST file
 * @return utf8 representation (caller should free), or NULL for error.
 */
gchar *
string_to_utf8(const gchar *string)
{
	gchar *utf8;

	utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);

	return utf8;
}

/**
 * Convert foldername to utf8 and escape characters if needed
 * @param foldername from PST file
 * @return converted folder name, or NULL for error. Caller should free
 */
gchar *
foldername_to_utf8 (const gchar *pstname)
{
	gchar *utf8name, *folder_name;

	utf8name = string_to_utf8(pstname);

	if (utf8name == NULL) {
		folder_name = camel_url_encode (pstname, NULL);
		g_warning ("foldername_to_utf8: Cannot convert to utf8! foldername=%s", folder_name);
	} else {
		/* Encode using the current locale */
		folder_name = camel_url_encode (utf8name, NULL);
		g_free (utf8name);
	}

	g_strdelimit (folder_name, "/", '_');
	g_strescape (folder_name, NULL);

	return folder_name;
}

static void
pst_process_folder (PstImporter *m, pst_item *item)
{
	gchar *uri;
	g_free (m->folder_name);
	g_free (m->folder_uri);

	if (item->file_as.str != NULL) {
		m->folder_name = foldername_to_utf8 (item->file_as.str);
	} else {
		g_critical ("Folder: No name! item->file_as=%s", item->file_as.str);
		m->folder_name = g_strdup ("unknown_name");
	}

	uri = g_strjoin ("/", m->parent_uri, m->folder_name, NULL);
	m->folder_uri = uri;

	if (m->folder) {
		camel_object_unref (m->folder);
		m->folder = NULL;
	}

	m->folder_count = item->folder->item_count;
	m->current_item = 0;
}

/**
 * Create current folder in mail hierarchy. Parent folders will also be created.
 * @param m PstImporter set to current folder
 */
static void
pst_create_folder (PstImporter *m)
{
	const gchar *parent;
	gchar *dest, *dest_end, *pos;
	gint dest_len;

	parent = ((EImportTargetURI *)m->target)->uri_dest;
	dest = g_strdup (m->folder_uri);

	g_assert (g_str_has_prefix (dest, parent));

	dest_len = strlen (dest);
	dest_end = dest + dest_len;

	pos = dest + strlen(parent);

	while (pos != NULL && pos < dest_end) {
		pos = g_strstr_len (pos+1, dest_end-pos, "/");
		if (pos != NULL) {
			CamelFolder *folder;

			*pos = '\0';

			folder = mail_tool_uri_to_folder (dest, CAMEL_STORE_FOLDER_CREATE, &m->base.ex);
			camel_object_unref(folder);
			*pos = '/';
		}
	}

	g_free (dest);

	if (m->folder) {
		camel_object_unref (m->folder);
	}

	m->folder = mail_tool_uri_to_folder (m->folder_uri, CAMEL_STORE_FOLDER_CREATE, &m->base.ex);

}

/**
 * Create a camel mime part from given PST attachment
 * @param attach attachment to convert
 * @return CamelMimePart containing data and mime type
 */
static CamelMimePart *
attachment_to_part (PstImporter *m, pst_item_attach *attach)
{
	CamelMimePart *part;
	gchar *mimetype;

	part = camel_mime_part_new ();

	if (attach->filename2.str || attach->filename1.str) {
		camel_mime_part_set_filename (part, (attach->filename2.str ? attach->filename2.str : attach->filename1.str));
		camel_mime_part_set_disposition (part, "attachment");
		camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);
	} else {
		camel_mime_part_set_disposition (part, "inline");
	}

	if (attach->mimetype.str != NULL) {
		mimetype = attach->mimetype.str;
	} else {
		mimetype = "application/octet-stream";
	}

	if (attach->data.data != NULL) {
		camel_mime_part_set_content (part, attach->data.data, attach->data.size, mimetype);
	} else {
		pst_binary attach_rc;
		attach_rc = pst_attach_to_mem (&m->pst, attach);

		camel_mime_part_set_content (part, (gchar *) attach_rc.data, attach_rc.size, mimetype);
		free(attach_rc.data);
	}

	return part;
}

static void
pst_process_email (PstImporter *m, pst_item *item)
{
	CamelMimeMessage *msg;
	CamelInternetAddress *addr;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelMessageInfo *info;
	pst_item_attach *attach;

	if (m->folder == NULL) {
		pst_create_folder (m);
	}

	camel_folder_freeze (m->folder);

	msg = camel_mime_message_new ();

	if (item->subject.str != NULL) {
		gchar *subj;

		subj = string_to_utf8 (item->subject.str);
		if (subj == NULL) {
			g_warning ("Could not convert email subject to utf8: %s", item->subject.str);
			camel_mime_message_set_subject (msg, "(lost subject)");
		} else {
			camel_mime_message_set_subject (msg, subj);
			g_free(subj);
		}
	}

	addr = camel_internet_address_new ();

	if (item->email->outlook_sender_name.str != NULL && item->email->outlook_sender.str != NULL) {
		camel_internet_address_add (addr, item->email->outlook_sender_name.str, item->email->outlook_sender.str);
	} else if (item->email->outlook_sender_name.str != NULL) {
		camel_address_decode (CAMEL_ADDRESS (addr), item->email->outlook_sender_name.str);
	} else if (item->email->outlook_sender.str != NULL) {
		camel_address_decode (CAMEL_ADDRESS (addr), item->email->outlook_sender.str);
	} else {
		/* Evo prints a warning if no from is set, so supply an empty address */
		camel_internet_address_add (addr, "", "");
	}

	camel_mime_message_set_from (msg, addr);
	camel_object_unref (addr);

	if (item->email->sent_date != NULL) {
		camel_mime_message_set_date (msg, pst_fileTimeToUnixTime (item->email->sent_date), 0);
	}

	if (item->email->messageid.str != NULL) {
		camel_mime_message_set_message_id (msg, item->email->messageid.str);
	}

	if (item->email->header.str != NULL) {
		/* Use mime parser to read headers */
		CamelStream *stream;
		/*g_debug ("  Email headers length=%zd", strlen (item->email->header));*/
		/*g_message ("  Email headers... %s...", item->email->header);*/

		stream = camel_stream_mem_new_with_buffer (item->email->header.str, strlen (item->email->header.str));
		if (camel_data_wrapper_construct_from_stream ((CamelDataWrapper *)msg, stream) == -1)
			g_warning ("Error reading headers, skipped");

	} else {

		if (item->email->sentto_address.str != NULL) {
			addr = camel_internet_address_new ();

			if (camel_address_decode (CAMEL_ADDRESS (addr), item->email->sentto_address.str) > 0);
				camel_mime_message_set_recipients (msg, "To", addr);

			camel_object_unref (addr);
		}

		if (item->email->cc_address.str != NULL) {
			addr = camel_internet_address_new ();

			if (camel_address_decode (CAMEL_ADDRESS (addr), item->email->cc_address.str) > 0);
				camel_mime_message_set_recipients (msg, "CC", addr);

			camel_object_unref (addr);
		}
	}

	mp = camel_multipart_new ();

	if (item->attach != NULL) {

		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mp), "multipart/mixed");

	} else if (item->email->htmlbody.str && item->body.str) {

		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mp), "multipart/alternate");

	} else if (item->email->htmlbody.str) {

		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mp), "text/html");

	}

	camel_multipart_set_boundary (mp, NULL);

	if (item->body.str != NULL) {
		/* Read internet headers */

		/*g_debug ("  Email body length=%zd", strlen (item->email->body));
		g_message ("  Email body %100s...", item->email->body);*/

		part = camel_mime_part_new ();
		camel_mime_part_set_content (part, item->body.str, strlen (item->body.str), "text/plain");
		camel_multipart_add_part (mp, part);
		camel_object_unref (part);
	}

	if (item->email->htmlbody.str != NULL) {
		/*g_debug ("  HTML body length=%zd", strlen (item->email->htmlbody));*/
		part = camel_mime_part_new ();
		camel_mime_part_set_content (part, item->email->htmlbody.str, strlen (item->email->htmlbody.str), "text/html");
		camel_multipart_add_part (mp, part);
		camel_object_unref (part);
	}

	for (attach = item->attach; attach; attach = attach->next) {
		if (attach->data.data || attach->i_id) {
			part = attachment_to_part(m, attach);
			camel_multipart_add_part (mp, part);
			camel_object_unref (part);
		}
	}

	/*camel_mime_message_dump (msg, TRUE);*/

	if (item->email->htmlbody.str || item->attach) {
		camel_medium_set_content_object (CAMEL_MEDIUM (msg), CAMEL_DATA_WRAPPER (mp));
	} else if (item->body.str) {
		camel_mime_part_set_content (CAMEL_MIME_PART (msg), item->body.str, strlen (item->body.str), "text/plain");
	} else {
		g_warning ("Email without body. Subject:%s",
				(item->subject.str ? item->subject.str : "(empty)"));
		camel_mime_part_set_content (CAMEL_MIME_PART (msg), "\n", 1, "text/plain");
	}

	info = camel_message_info_new (NULL);

	/* Read message flags (see comments in libpst.c */
	if (item->flags & 0x01)
		camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, ~0);

	if (item->email->importance == 2)
		camel_message_info_set_flags (info, CAMEL_MESSAGE_FLAGGED, ~0);

	if (item->flags & 0x08)
		camel_message_info_set_flags (info, CAMEL_MESSAGE_DRAFT, ~0);

	camel_folder_append_message (m->folder, msg, info, NULL, &m->ex);
	camel_message_info_free (info);
	camel_object_unref (msg);

	camel_folder_sync (m->folder, FALSE, NULL);
	camel_folder_thaw (m->folder);

	if (camel_exception_is_set (&m->ex)) {
		g_critical ("Exception!");
		camel_exception_clear (&m->ex);
		return;
	}

}

static void
contact_set_string (EContact *contact, EContactField id, gchar *string)
{
	if (string != NULL) {
		e_contact_set (contact, id, string);
	}
}

static void
unknown_field (EContact *contact, GString *notes, gchar *name, gchar *string)
{
	/* Field could not be mapped directly so add to notes field */
	if (string != NULL) {
		g_string_append_printf (notes, "%s: %s\n", name, string);
	}
}

static void
contact_set_address (EContact *contact, EContactField id, gchar *address, gchar *city, gchar *country, gchar *po_box, gchar *postal_code, gchar *state, gchar *street)
{
	EContactAddress *eaddress;

	if (address || city || country || po_box || postal_code || state || street) {
		eaddress = g_new0 (EContactAddress, 1);
		if (po_box) {
			eaddress->po = g_strdup (po_box);
		}
		//eaddress->ext =

		if (street) {
			eaddress->street = g_strdup (street);
		}

		if (city) {
			eaddress->locality = g_strdup (city);
		}

		if (state) {
			eaddress->region = g_strdup (state);
		}

		if (postal_code) {
			eaddress->code = g_strdup (postal_code);
		}

		if (country) {
			eaddress->country = g_strdup (country);
		}

		e_contact_set (contact, id, eaddress);
	}
}

void
contact_set_date (EContact *contact, EContactField id, FILETIME *date)
{
	if (date && (date->dwLowDateTime || date->dwHighDateTime) ) {
		time_t t1;
		struct tm tm;
		EContactDate *bday;
		bday = e_contact_date_new ();

		t1 = pst_fileTimeToUnixTime (date);
		gmtime_r (&t1, &tm);

		bday->year = tm.tm_year + 1900;
		bday->month = tm.tm_mon + 1;
		bday->day = tm.tm_mday;

		e_contact_set (contact, id, bday);
	}
}

static void
pst_process_contact (PstImporter *m, pst_item *item)
{
	pst_item_contact *c;
	EContact *ec;
	c = item->contact;
	GString *notes;

	notes = g_string_sized_new (2048);

	ec = e_contact_new ();
	/* pst's fullname field only contains first, middle, surname */
	if (c->display_name_prefix.str || c->suffix.str) {
		GString *name = g_string_sized_new (128);

		if (c->display_name_prefix.str) {
			g_string_assign (name, c->display_name_prefix.str);
		}

		if (c->first_name.str) {
			g_string_append_printf (name, "%s%s", (name->len ? " " : ""), c->first_name.str);
		}

		if (c->middle_name.str) {
			g_string_append_printf (name, "%s%s", (name->len ? " " : ""), c->middle_name.str);
		}

		if (c->surname.str) {
			g_string_append_printf (name, "%s%s", (name->len ? " " : ""), c->surname.str);
		}

		if (c->surname.str) {
			g_string_append_printf (name, "%s%s", (name->len ? " " : ""), c->surname.str);
		}

		contact_set_string (ec, E_CONTACT_FULL_NAME, name->str);
		g_string_free (name, TRUE);

	} else {
		contact_set_string (ec, E_CONTACT_FULL_NAME, c->fullname.str);
	}

	/* unknown_field (ec, notes, "initials", c->initials); */

	contact_set_string (ec, E_CONTACT_NICKNAME, c->nickname.str);

	contact_set_string (ec, E_CONTACT_ORG, c->company_name.str);
	contact_set_string (ec, E_CONTACT_ORG_UNIT, c->department.str);
	contact_set_string (ec, E_CONTACT_TITLE, c->job_title.str);

	contact_set_address (ec,E_CONTACT_ADDRESS_WORK,
			    c->business_address.str, c->business_city.str, c->business_country.str,
			    c->business_po_box.str, c->business_postal_code.str, c->business_state.str, c->business_street.str);

	contact_set_address (ec,E_CONTACT_ADDRESS_HOME,
			    c->home_address.str, c->home_city.str, c->home_country.str,
			    c->home_po_box.str, c->home_postal_code.str, c->home_state.str, c->home_street.str);

	contact_set_address (ec,E_CONTACT_ADDRESS_OTHER,
			    c->other_address.str, c->other_city.str, c->other_country.str,
			    c->other_po_box.str, c->other_postal_code.str, c->other_state.str, c->other_street.str);

	contact_set_string (ec, E_CONTACT_PHONE_ASSISTANT, c->assistant_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_BUSINESS_FAX, c->business_fax.str);
	contact_set_string (ec, E_CONTACT_PHONE_BUSINESS, c->business_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_BUSINESS_2, c->business_phone2.str);
	contact_set_string (ec, E_CONTACT_PHONE_CALLBACK, c->callback_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_CAR, c->car_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_COMPANY, c->company_main_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_HOME_FAX, c->home_fax.str);
	contact_set_string (ec, E_CONTACT_PHONE_HOME, c->home_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_HOME_2, c->home_phone2.str);
	contact_set_string (ec, E_CONTACT_PHONE_ISDN, c->isdn_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_MOBILE, c->mobile_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_OTHER_FAX, c->primary_fax.str);  /* ? */
	contact_set_string (ec, E_CONTACT_PHONE_PAGER, c->pager_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_PRIMARY, c->primary_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_RADIO, c->radio_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_TTYTDD, c->ttytdd_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_TELEX, c->telex.str);
	unknown_field (ec, notes, "account_name", c->account_name.str);
	contact_set_date (ec, E_CONTACT_ANNIVERSARY, c->wedding_anniversary);
	contact_set_string (ec, E_CONTACT_ASSISTANT, c->assistant_name.str);
	unknown_field (ec, notes, "billing_information", c->billing_information.str);
	contact_set_date (ec, E_CONTACT_BIRTH_DATE, c->birthday);
	/* contact_set_string (ec, E_CONTACT_CATEGORIES, c->??); */

	contact_set_string (ec, E_CONTACT_EMAIL_1 , c->address1.str);
	contact_set_string (ec, E_CONTACT_EMAIL_2 , c->address2.str);
	contact_set_string (ec, E_CONTACT_EMAIL_3 , c->address3.str);

	/*unknown_field (ec, notes, "address1_desc" , c->address1_desc);
	unknown_field (ec, notes, "address1_transport" , c->address1_transport);
	unknown_field (ec, notes, "address2_desc" , c->address2_desc);
	unknown_field (ec, notes, "address2_transport" , c->address2_transport);
	unknown_field (ec, notes, "address3_desc" , c->address3_desc);
	unknown_field (ec, notes, "address3_transport" , c->address3_transport);*/

	/*unknown_field (ec, notes, "def_postal_address", c->def_postal_address);*/

	/* unknown_field (ec, ??, c->gender); */
	unknown_field (ec, notes, "gov_id", c->gov_id.str);
	unknown_field (ec, notes, "customer_id", c->customer_id.str);
	unknown_field (ec, notes, "hobbies", c->hobbies.str);
	unknown_field (ec, notes, "followup", c->followup.str);

	contact_set_string (ec, E_CONTACT_FREEBUSY_URL , c->free_busy_address.str);

	unknown_field (ec, notes, "keyword", c->keyword.str);
	unknown_field (ec, notes, "language", c->language.str);
	unknown_field (ec, notes, "location", c->location.str);
	contact_set_string (ec, E_CONTACT_OFFICE, c->office_loc.str);
	unknown_field (ec, notes, "computer_name", c->computer_name.str);
	unknown_field (ec, notes, "ftp_site", c->ftp_site.str);

	contact_set_string (ec, E_CONTACT_MANAGER , c->manager_name.str);
	unknown_field (ec, notes, "mileage", c->mileage.str);
	unknown_field (ec, notes, "org_id", c->org_id.str);
	contact_set_string (ec, E_CONTACT_ROLE, c->profession.str);

	contact_set_string (ec, E_CONTACT_SPOUSE , c->spouse_name.str);

	if (c->personal_homepage.str) {
		contact_set_string (ec, E_CONTACT_HOMEPAGE_URL , c->personal_homepage.str);
		if (c->business_homepage.str) {
			unknown_field (ec, notes, "business_homepage", c->business_homepage.str);
		}
	} else if (c->business_homepage.str) {
		contact_set_string (ec, E_CONTACT_HOMEPAGE_URL , c->business_homepage.str);
	}

	if (item->comment.str) {
		g_string_append_printf (notes, "%s\n", item->comment.str);
	}

	if (item->email && item->body.str) {
		g_string_append_printf (notes, "%s\n", item->body.str);
	}

	contact_set_string (ec, E_CONTACT_NOTE, notes->str);
	g_string_free (notes, TRUE);

	e_book_add_contact (m->addressbook, ec, NULL);
	g_object_unref (ec);

}

/**
 * Convert pst time to icaltimetype
 * @param date time value from libpst
 * @param is_date treat as date only (all day event)?
 * @return converted date
 */
struct icaltimetype
get_ical_date (FILETIME *date, gboolean is_date)
{
	if (date && (date->dwLowDateTime || date->dwHighDateTime) ) {
		time_t t;

		t = pst_fileTimeToUnixTime (date);
		return icaltime_from_timet_with_zone (t, is_date, NULL);
	} else {
		return icaltime_null_date ();
	}
}

static void
set_cal_attachments (ECal *cal, ECalComponent *ec, PstImporter *m, pst_item_attach *attach)
{
	GSList *list = NULL;
	const gchar *uid;
	gchar *store_dir;

	if (attach == NULL) {
		return;
	}

	e_cal_component_get_uid (ec, &uid);
	store_dir = g_filename_from_uri (e_cal_get_local_attachment_store (cal), NULL, NULL);

	while (attach != NULL) {
		const gchar * orig_filename;
		gchar *filename, *tmp, *path, *dirname, *uri;
		CamelMimePart *part;
		CamelDataWrapper *content;
		CamelStream *stream;
		struct stat st;

		part = attachment_to_part(m, attach);

		orig_filename = camel_mime_part_get_filename(part);

		if (orig_filename == NULL) {
			g_warning("Ignoring unnamed attachment");
			attach = attach->next;
			continue;  /* Ignore unnamed attachments */
		}

		tmp = camel_file_util_safe_filename (orig_filename);
		filename = g_strdup_printf ("%s-%s", uid, tmp);
		path = g_build_filename (store_dir, filename, NULL);

		g_free (tmp);
		g_free (filename);

		dirname = g_path_get_dirname(path);
		if (g_mkdir_with_parents(dirname, 0777) == -1) {
			g_warning("Could not create directory %s: %s", dirname, g_strerror(errno));
			g_free(dirname);
			attach = attach->next;
			continue;
		}
		g_free(dirname);

		if (g_access(path, F_OK) == 0) {
			if (g_access(path, W_OK) != 0) {
				g_warning("Could not write file %s - file exists", path);
				attach = attach->next;
				continue;
			}
		}

		if (g_stat(path, &st) != -1 && !S_ISREG(st.st_mode)) {
			g_warning("Could not write file %s - not a file", path);
			attach = attach->next;
			continue;
		}

		if (!(stream = camel_stream_fs_new_with_name (path, O_WRONLY | O_CREAT | O_TRUNC, 0666))) {
			g_warning ("Could not create stream for file %s - %s", path, g_strerror (errno));
			attach = attach->next;
			continue;
		}

		content = camel_medium_get_content_object (CAMEL_MEDIUM (part));

		if (camel_data_wrapper_decode_to_stream (content, stream) == -1
			|| camel_stream_flush (stream) == -1)
		{
			g_warning ("Could not write attachment to %s: %s", path, g_strerror (errno));
			camel_object_unref (stream);
			attach = attach->next;
			continue;
		}

		camel_object_unref (stream);

		uri = g_filename_to_uri (path, NULL, NULL);
		list = g_slist_append (list, g_strdup (uri));
		g_free (uri);

		camel_object_unref (part);
		g_free (path);

		attach = attach->next;

	}

	g_free (store_dir);

	e_cal_component_set_attachment_list (ec, list);
}

static void
fill_calcomponent (PstImporter *m, pst_item *item, ECalComponent *ec, const gchar *type)
{
	pst_item_appointment *a;
	pst_item_email *e;

	a = item->appointment;
	e = item->email;
	ECalComponentText text;
	struct icaltimetype tt_start, tt_end;
	ECalComponentDateTime dt_start, dt_end;

	g_return_if_fail (item->appointment != NULL);

	if (item->create_date) {
		struct icaltimetype tt;
		tt = get_ical_date (item->create_date, FALSE);
		e_cal_component_set_created (ec, &tt);
	}
	if (item->modify_date) {
		struct icaltimetype tt;
		tt = get_ical_date (item->modify_date, FALSE);
		e_cal_component_set_last_modified (ec, &tt);
	}

	if (e) {
		if (item->subject.str || e->processed_subject.str) {
			if (item->subject.str) {
				text.value = item->subject.str;
			} else if (e->processed_subject.str) {
				text.value = e->processed_subject.str;
			}

			text.altrep = NULL; /* email->proc_subject? */
			e_cal_component_set_summary (ec, &text);
		}
		if (item->body.str) {
			GSList l;
			text.value = item->body.str;
			text.altrep = NULL;
			l.data = &text;
			l.next = NULL;
			e_cal_component_set_description_list (ec, &l);
		}
	} else {
		g_warning ("%s without subject / body!", type);
	}

	if (a->location.str) {
		e_cal_component_set_location (ec, a->location.str);
	}

	if (a->start) {
		tt_start = get_ical_date (a->start, a->all_day);
		dt_start.value = &tt_start;
		dt_start.tzid = NULL;
		e_cal_component_set_dtstart (ec, &dt_start);
	}

	if (a->end) {
		tt_end = get_ical_date (a->end, a->all_day);
		dt_end.value = &tt_end;
		dt_end.tzid = NULL;
		e_cal_component_set_dtend (ec, &dt_end);
	}

	switch (a->showas) {
		case PST_FREEBUSY_TENTATIVE:
			e_cal_component_set_status (ec, ICAL_STATUS_TENTATIVE);
			break;
		case PST_FREEBUSY_FREE:
			// mark as transparent and as confirmed
			e_cal_component_set_transparency (ec, E_CAL_COMPONENT_TRANSP_TRANSPARENT);
		case PST_FREEBUSY_BUSY:
		case PST_FREEBUSY_OUT_OF_OFFICE:
			e_cal_component_set_status (ec, ICAL_STATUS_CONFIRMED);
			break;
	}
	switch (a->label) {
		case PST_APP_LABEL_NONE:
			break;
		case PST_APP_LABEL_IMPORTANT:
			e_cal_component_set_categories (ec, "Important"); break;
		case PST_APP_LABEL_BUSINESS:
			e_cal_component_set_categories (ec, "Business"); break;
		case PST_APP_LABEL_PERSONAL:
			e_cal_component_set_categories (ec, "Personal"); break;
		case PST_APP_LABEL_VACATION:
			e_cal_component_set_categories (ec, "Vacation"); break;
		case PST_APP_LABEL_MUST_ATTEND:
			e_cal_component_set_categories (ec, "Must-attend"); break;
		case PST_APP_LABEL_TRAVEL_REQ:
			e_cal_component_set_categories (ec, "Travel-required"); break;
		case PST_APP_LABEL_NEEDS_PREP:
			e_cal_component_set_categories (ec, "Needs-preparation"); break;
		case PST_APP_LABEL_BIRTHDAY:
			e_cal_component_set_categories (ec, "Birthday"); break;
		case PST_APP_LABEL_ANNIVERSARY:
			e_cal_component_set_categories (ec, "Anniversary"); break;
		case PST_APP_LABEL_PHONE_CALL:
			e_cal_component_set_categories (ec, "Phone-call"); break;
	}

	if (a->alarm || a->alarm_minutes) {
		ECalComponentAlarm *alarm;
		ECalComponentAlarmTrigger trigger;

		alarm = e_cal_component_alarm_new ();

		if (a->alarm_minutes) {
			trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
			trigger.u.rel_duration = icaldurationtype_from_int (- (a->alarm_minutes)*60);
			e_cal_component_alarm_set_trigger (alarm, trigger);
		}

		if (a->alarm) {
			if (a->alarm_filename.str) {
				e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_AUDIO);
			} else {
				e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);
			}
		}

		e_cal_component_add_alarm (ec, alarm);
		e_cal_component_alarm_free(alarm);

	}

	if (a->recurrence_description.str != PST_APP_RECUR_NONE) {
		struct icalrecurrencetype  r;
		GSList recur_list;

		icalrecurrencetype_clear (&r);
		r.interval = 1; /* Interval not implemented in libpst */
		if (a->recurrence_end) {
			r.until = get_ical_date (a->recurrence_end, FALSE);
		}

		switch (a->recurrence_type) {
			case PST_APP_RECUR_DAILY:
				r.freq = ICAL_DAILY_RECURRENCE; break;
			case PST_APP_RECUR_WEEKLY:
				r.freq = ICAL_WEEKLY_RECURRENCE; break;
			case PST_APP_RECUR_MONTHLY:
				r.freq = ICAL_MONTHLY_RECURRENCE; break;
			case PST_APP_RECUR_YEARLY:
				r.freq = ICAL_YEARLY_RECURRENCE; break;
			default:
				r.freq = ICAL_NO_RECURRENCE;
		}

		recur_list.data = &r;
		recur_list.next = NULL;
		e_cal_component_set_rrule_list (ec, &recur_list);
	}

}

static void
pst_process_appointment (PstImporter *m, pst_item *item)
{
	ECalComponent *ec;

	g_return_if_fail (item->appointment != NULL);

	ec = e_cal_component_new ();
	e_cal_component_set_new_vtype (ec, E_CAL_COMPONENT_EVENT);

	fill_calcomponent (m, item, ec, "appointment");
	set_cal_attachments (m->calendar, ec, m, item->attach);

	if (!e_cal_create_object (m->calendar, e_cal_component_get_icalcomponent (ec), NULL, NULL)) {
		g_warning("Creation of appointment failed");
		g_free(ec);
	}

	g_object_unref (ec);

}

static void
pst_process_task (PstImporter *m, pst_item *item)
{
	ECalComponent *ec;

	g_return_if_fail (item->appointment != NULL);

	ec = e_cal_component_new ();
	e_cal_component_set_new_vtype (ec, E_CAL_COMPONENT_TODO);

	fill_calcomponent (m, item, ec, "task");
	set_cal_attachments (m->tasks, ec, m, item->attach);

	/* Note - libpst is missing many fields. E.g. task status, start/completion date, % complete */

	if (!e_cal_create_object (m->tasks, e_cal_component_get_icalcomponent (ec), NULL, NULL)) {
		g_warning("Creation of task failed");
		g_free(ec);
	}

	g_object_unref (ec);

}

static void
pst_process_journal (PstImporter *m, pst_item *item)
{
	struct pst_item_journal *j;
	ECalComponent *ec;

	g_return_if_fail (item->appointment != NULL);

	j = item->journal;
	ec = e_cal_component_new ();
	e_cal_component_set_new_vtype (ec, E_CAL_COMPONENT_JOURNAL);

	fill_calcomponent (m, item, ec, "journal");
	set_cal_attachments (m->journal, ec, m, item->attach);

	/* Note - an Evo memo entry does not have date started/finished or type fields :( */
	/*if (j) {
		ECalComponentText text;
		struct icaltimetype tt_start, tt_end;
		ECalComponentDateTime dt_start, dt_end;

		if (j->start) {
			tt_start = get_ical_date (j->start, FALSE);
			dt_start.value = &tt_start;
			dt_start.tzid = NULL;
			e_cal_component_set_dtstart (ec, &dt_start);
			g_message ("journal start:%s", rfc2445_datetime_format (j->start));
		}

		if (j->end) {
			tt_end = get_ical_date (j->end, FALSE);
			dt_end.value = &tt_end;
			dt_end.tzid = NULL;
			e_cal_component_set_dtend (ec, &dt_end);
			g_message ("end:%s", rfc2445_datetime_format (j->end));
		}
		g_message ("type: %s", j->type);
	}*/

	if (!e_cal_create_object (m->journal, e_cal_component_get_icalcomponent (ec), NULL, NULL)) {
		g_warning("Creation of journal entry failed");
		g_free(ec);
	}

	g_object_unref (ec);

}

/* Print an error message - maybe later bring up an error dialog? */
static void
pst_error_msg (const gchar *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	g_critical (fmt, ap);
	va_end (ap);
}

static void
pst_import_imported (PstImporter *m)
{
	e_import_complete (m->target->import, (EImportTarget *)m->target);
}

static void
pst_import_free (PstImporter *m)
{
//	pst_close (&m->pst);
	camel_operation_unref (m->status);

	g_free (m->status_what);
	g_mutex_free (m->status_lock);

	g_source_remove (m->status_timeout_id);
	m->status_timeout_id = 0;

	g_free (m->folder_name);
	g_free (m->folder_uri);
	g_free (m->parent_uri);

	g_object_unref (m->import);
}

static MailMsgInfo pst_import_info = {
	sizeof (PstImporter),
	(MailMsgDescFunc) pst_import_describe,
	(MailMsgExecFunc) pst_import_import,
	(MailMsgDoneFunc) pst_import_imported,
	(MailMsgFreeFunc) pst_import_free,
};

static gboolean
pst_status_timeout (gpointer data)
{
	PstImporter *importer = data;
	gint pc;
	gchar *what;

	if (importer->status_what) {
		g_mutex_lock (importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock (importer->status_lock);

		e_import_status (importer->target->import, (EImportTarget *)importer->target, what, pc);
	}

	return TRUE;
}

static void
pst_status (CamelOperation *op, const gchar *what, gint pc, gpointer data)
{
	PstImporter *importer = data;

	if (pc == CAMEL_OPERATION_START) {
		pc = 0;
	} else if (pc == CAMEL_OPERATION_END) {
		pc = 100;
	}

	g_mutex_lock (importer->status_lock);
	g_free (importer->status_what);
	importer->status_what = g_strdup (what);
	importer->status_pc = pc;
	g_mutex_unlock (importer->status_lock);
}

static gint
pst_import (EImport *ei, EImportTarget *target)
{
	PstImporter *m;
	gint id;

	m = mail_msg_new (&pst_import_info);
	g_datalist_set_data (&target->data, "pst-msg", m);
	m->import = ei;
	g_object_ref (m->import);
	m->target = target;

	m->parent_uri = NULL;
	m->folder_name = NULL;
	m->folder_uri = NULL;

	m->addressbook = NULL;
	m->calendar = NULL;
	m->tasks = NULL;
	m->journal = NULL;

	m->status_timeout_id = g_timeout_add (100, pst_status_timeout, m);
	/*m->status_timeout_id = NULL;*/
	m->status_lock = g_mutex_new ();
	m->status = camel_operation_new (pst_status, m);

	id = m->base.seq;

	mail_msg_unordered_push (m);

	return id;
}

/* Start the main import operation */
void
org_credativ_evolution_readpst_import (EImport *ei, EImportTarget *target, EImportImporter *im)
{
	if (GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-mail"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-addr"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-appt"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-task"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-journal"))) {
				pst_import (ei, target);
	} else {
		e_import_complete (target->import, target);
	}
}

void
org_credativ_evolution_readpst_cancel (EImport *ei, EImportTarget *target, EImportImporter *im)
{
	PstImporter *m = g_datalist_get_data (&target->data, "pst-msg");

	if (m) {
		camel_operation_cancel (m->status);
	}
}

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
{
	if (enable) {
		bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
		g_message ("pst Plugin enabled");
	} else {
		g_message ("pst Plugin disabled");
	}

	return 0;
}

/**
 * Open PST file and determine root folder name
 * @param pst: pst_file structure to be used by libpst
 * @param filename : path to file
 * @return 0 for sucess, -1 for failure
 */
gint
pst_init (pst_file *pst, gchar *filename)
{

#if 0
	gchar *d_log = "readpst.log";
	/* initialize log file */
	DEBUG_INIT (d_log);
	DEBUG_REGISTER_CLOSE ();
#endif

	if (pst_open (pst, filename) < 0) {
		pst_error_msg ("Error opening PST file %s", filename);
		return -1;
	}

	if (pst_load_index (pst) < 0) {
		pst_error_msg ("Error loading indexes");
		return -1;
	}

	if (pst_load_extended_attributes (pst) < 0) {
		pst_error_msg ("Error loading file items");
		return -1;
	}

	return 0;
}

/**
 * Open determine root folder name of PST file
 * @param pst: pst_file structure to be used by libpst
 * @param filename : if non NULL, fallback to this name if folder name is not available
 * @return pointer to name of root folder (should be freed by caller), or NULL if error
 */
gchar *
get_pst_rootname (pst_file *pst, gchar *filename)
{
	pst_item *item = NULL;
	gchar *rootname = NULL;

	if ((item = pst_parse_item (pst, pst->d_head, NULL)) == NULL) {
		pst_error_msg ("Could not get root record");
		return NULL;
	}

	if (item->message_store == NULL) {
		pst_error_msg ("Could not get root message store");
		pst_freeItem (item);
		return NULL;
	}

	/* default the file_as to the same as the main filename if it doesn't exist */
	if (item->file_as.str == NULL) {
		if (filename == NULL) {
			pst_freeItem (item);
			return NULL;
		}
		rootname = g_path_get_basename (filename);
	} else {
		rootname = g_strdup (item->file_as.str);
	}

	pst_freeItem (item);

	return rootname;
}
