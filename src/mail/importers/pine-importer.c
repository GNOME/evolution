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
 *		Iain Holmes  <iain@ximian.com>
 *	Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libebook/libebook.h>

#include "mail-importer.h"

#include "mail/e-mail-backend.h"
#include "shell/e-shell.h"

#define d(x)

struct _pine_import_msg {
	MailMsg base;

	EImport *import;
	EImportTarget *target;

	GMutex status_lock;
	gchar *status_what;
	gint status_pc;
	gint status_timeout_id;
	GCancellable *cancellable;
};

static gboolean
pine_supported (EImport *ei,
                EImportTarget *target,
                EImportImporter *im)
{
	gchar *maildir, *addrfile;
	gboolean md_exists, addr_exists;

	if (target->type != E_IMPORT_TARGET_HOME)
		return FALSE;

	maildir = g_build_filename (g_get_home_dir (), "mail", NULL);
	md_exists = g_file_test (maildir, G_FILE_TEST_IS_DIR);
	g_free (maildir);

	addrfile = g_build_filename (g_get_home_dir (), ".addressbook", NULL);
	addr_exists = g_file_test (addrfile, G_FILE_TEST_IS_REGULAR);
	g_free (addrfile);

	return md_exists || addr_exists;
}

/*
 * See: http://www.washington.edu/pine/tech-notes/low-level.html
 * 
 * addressbook line is:
 *      <nickname>TAB<fullname>TAB<address>TAB<fcc>TAB<comments>
 * lists, address is:
 *      "(" <address>, <address>, <address>, ... ")"
 * 
 * <address> is rfc822 address, or alias address.
 * if rfc822 address includes a phrase, then that overrides <fullname>
 * 
 * FIXME: we don't handle aliases in lists.
 */

static void
import_contact (EBookClient *book_client,
                gchar *line)
{
	gchar **strings, *addr, **addrs;
	gint i;
	GList *list;
	/*EContactName *name;*/
	EContact *card;
	gsize len;
	GError *error = NULL;

	card = e_contact_new ();
	strings = g_strsplit (line, "\t", 5);
	if (strings[0] && strings[1] && strings[2]) {
		gchar *new_uid = NULL;

		e_contact_set (card, E_CONTACT_NICKNAME, strings[0]);
		e_contact_set (card, E_CONTACT_FULL_NAME, strings[1]);

		addr = strings[2];
		len = strlen (addr);
		if (addr[0] == '(' && addr[len - 1] == ')') {
			addr[0] = 0;
			addr[len - 1] = 0;
			addrs = g_strsplit (addr + 1, ",", 0);
			list = NULL;
			/* XXX So ... this api is just insane ... we set
			 *     plain strings as the contact email if it
			 *     is a normal contact, but need to do this
			 *     XML crap for mailing lists. */
			for (i = 0; addrs[i]; i++) {
				EDestination *d;
				EVCardAttribute *attr;

				d = e_destination_new ();
				e_destination_set_email (d, addrs[i]);

				attr = e_vcard_attribute_new (NULL, EVC_EMAIL);
				e_destination_export_to_vcard_attribute (d, attr);
				list = g_list_prepend (list, attr);
				g_object_unref (d);
			}
			e_vcard_append_attributes_take (E_VCARD (card), g_list_reverse (list));
			g_list_free (list);
			g_strfreev (addrs);
			e_contact_set (card, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));
		} else {
			e_contact_set (card, E_CONTACT_EMAIL_1, strings[2]);
		}

		/*name = e_contact_name_from_string(strings[1]);*/

		if (strings[3] && strings[4])
			e_contact_set (card, E_CONTACT_NOTE, strings[4]);

		e_book_client_add_contact_sync (
			book_client, card, E_BOOK_OPERATION_FLAG_NONE, &new_uid, NULL, &error);

		if (error != NULL) {
			g_warning (
				"%s: Failed to add contact: %s",
				G_STRFUNC, error->message);
			g_error_free (error);
		} else {
			g_free (new_uid);
		}

		g_object_unref (card);
	}
	g_strfreev (strings);
}

static void
import_contacts (void)
{
	EShell *shell;
	ESourceRegistry *registry;
	EClient *client = NULL;
	GList *list;
	gchar *name;
	GString *line;
	FILE *fp;
	gsize offset;
	const gchar *extension_name;
	GError *error = NULL;

	printf ("importing pine addressbook\n");

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;

	name = g_build_filename (g_get_home_dir (), ".addressbook", NULL);
	fp = fopen (name, "r");
	g_free (name);
	if (fp == NULL)
		return;

	list = e_source_registry_list_sources (registry, extension_name);

	if (list != NULL) {
		ESource *source;

		source = E_SOURCE (list->data);
		client = e_book_client_connect_sync (source, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, NULL, &error);
	} else {
		/* No address books exist. */
		g_warning ("%s: No address books exist.", G_STRFUNC);
		fclose (fp);
		return;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (error != NULL) {
		g_warning (
			"%s: Failed to open book client: %s",
			G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
		fclose (fp);
		return;
	}

	line = g_string_new ("");
	g_string_set_size (line, 256);
	offset = 0;
	while (fgets (line->str + offset, 256, fp)) {
		gsize len;

		len = strlen (line->str + offset) + offset;
		if (line->str[len - 1] == '\n')
			g_string_truncate (line, len - 1);
		else if (!feof (fp)) {
			offset = len;
			g_string_set_size (line, len + 256);
			continue;
		} else {
			g_string_truncate (line, len);
		}

		import_contact (E_BOOK_CLIENT (client), line->str);
		offset = 0;
	}

	g_string_free (line, TRUE);
	fclose (fp);
	g_object_unref (client);
}

static gchar *
pine_import_desc (struct _pine_import_msg *m)
{
	return g_strdup (_("Importing Pine data"));
}

static MailImporterSpecial pine_special_folders[] = {
	{ "sent-mail", "Sent" }, /* pine */
	{ "saved-messages", "Drafts" },	/* pine */
	{ NULL },
};

static void
pine_import_exec (struct _pine_import_msg *m,
                  GCancellable *cancellable,
                  GError **error)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *session;

	/* XXX Dig up the EMailSession from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pine-do-addr")))
		import_contacts ();

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pine-do-mail"))) {
		gchar *path;

		path = g_build_filename (g_get_home_dir (), "mail", NULL);
		mail_importer_import_folders_sync (
			session, path, pine_special_folders, 0, m->cancellable);
		g_free (path);
	}
}

static void
pine_import_done (struct _pine_import_msg *m)
{
	e_import_complete (m->import, (EImportTarget *) m->target, m->base.error);
}

static void
pine_import_free (struct _pine_import_msg *m)
{
	g_object_unref (m->cancellable);

	g_free (m->status_what);
	g_mutex_clear (&m->status_lock);

	g_source_remove (m->status_timeout_id);
	m->status_timeout_id = 0;

	g_object_unref (m->import);
}

static void
pine_status (CamelOperation *op,
             const gchar *what,
             gint pc,
             gpointer data)
{
	struct _pine_import_msg *importer = data;

	g_mutex_lock (&importer->status_lock);
	g_free (importer->status_what);
	importer->status_what = g_strdup (what);
	importer->status_pc = pc;
	g_mutex_unlock (&importer->status_lock);
}

static gboolean
pine_status_timeout (gpointer user_data)
{
	struct _pine_import_msg *importer = user_data;
	gint pc;
	gchar *what;

	if (importer->status_what) {
		g_mutex_lock (&importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock (&importer->status_lock);

		e_import_status (
			importer->import, (EImportTarget *)
			importer->target, what, pc);
	}

	return TRUE;
}

static MailMsgInfo pine_import_info = {
	sizeof (struct _pine_import_msg),
	(MailMsgDescFunc) pine_import_desc,
	(MailMsgExecFunc) pine_import_exec,
	(MailMsgDoneFunc) pine_import_done,
	(MailMsgFreeFunc) pine_import_free
};

static gint
mail_importer_pine_import (EImport *ei,
                           EImportTarget *target)
{
	GCancellable *cancellable;
	struct _pine_import_msg *m;
	gint id;

	cancellable = camel_operation_new ();
	m = mail_msg_new_with_cancellable (&pine_import_info, cancellable);
	g_datalist_set_data (&target->data, "pine-msg", m);
	m->import = ei;
	g_object_ref (m->import);
	m->target = target;
	m->status_timeout_id = e_named_timeout_add (
		100, pine_status_timeout, m);
	g_mutex_init (&m->status_lock);
	m->cancellable = cancellable;

	g_signal_connect (
		m->cancellable, "status",
		G_CALLBACK (pine_status), m);

	id = m->base.seq;

	mail_msg_fast_ordered_push (m);

	return id;
}

static void
checkbox_mail_toggle_cb (GtkToggleButton *tb,
                         EImportTarget *target)
{
	gboolean active;

	active = gtk_toggle_button_get_active (tb);

	g_datalist_set_data (
		&target->data, "pine-do-mail",
		GINT_TO_POINTER (active));
}

static void
checkbox_addr_toggle_cb (GtkToggleButton *tb,
                         EImportTarget *target)
{
	gboolean active;

	active = gtk_toggle_button_get_active (tb);

	g_datalist_set_data (
		&target->data, "pine-do-addr",
		GINT_TO_POINTER (active));
}

static GtkWidget *
pine_getwidget (EImport *ei,
                EImportTarget *target,
                EImportImporter *im)
{
	GtkWidget *box, *w;

	g_datalist_set_data (
		&target->data, "pine-do-mail",
		GINT_TO_POINTER (TRUE));
	g_datalist_set_data (
		&target->data, "pine-do-addr",
		GINT_TO_POINTER (TRUE));

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

	w = gtk_check_button_new_with_label (_("Mail"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, TRUE);
	g_signal_connect (
		w, "toggled",
		G_CALLBACK (checkbox_mail_toggle_cb), target);
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 0);

	w = gtk_check_button_new_with_label (_("Address Book"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, TRUE);
	g_signal_connect (
		w, "toggled",
		G_CALLBACK (checkbox_addr_toggle_cb), target);
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 0);

	gtk_widget_show_all (box);

	return box;
}

static void
pine_import (EImport *ei,
             EImportTarget *target,
             EImportImporter *im)
{
	if (GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pine-do-mail"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pine-do-addr")))
		mail_importer_pine_import (ei, target);
	else
		e_import_complete (ei, target, NULL);
}

static void
pine_cancel (EImport *ei,
             EImportTarget *target,
             EImportImporter *im)
{
	struct _pine_import_msg *m = g_datalist_get_data (&target->data, "pine-msg");

	if (m)
		g_cancellable_cancel (m->cancellable);
}

static EImportImporter pine_importer = {
	E_IMPORT_TARGET_HOME,
	0,
	pine_supported,
	pine_getwidget,
	pine_import,
	pine_cancel,
	NULL, /* get_preview */
};

EImportImporter *
pine_importer_peek (void)
{
	pine_importer.name = _("Evolution Pine importer");
	pine_importer.description = _("Import mail from Pine.");

	return &pine_importer;
}
