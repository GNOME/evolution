/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* pine-importer.c
 * 
 * Authors: 
 *    Iain Holmes  <iain@ximian.com>
 *    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2001 Ximian, Inc. (www.ximian.com)
 * Copyright 2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcheckbutton.h>

#include <gconf/gconf-client.h>

#include <libebook/e-book.h>
#include <libebook/e-destination.h>

#include <camel/camel-operation.h>

#include "mail-importer.h"

#include "mail/mail-mt.h"
#include "e-util/e-import.h"
#include "e-util/e-error.h"

#define d(x) x

struct _pine_import_msg {
	struct _mail_msg msg;

	EImport *import;
	EImportTarget *target;

	GMutex *status_lock;
	char *status_what;
	int status_pc;
	int status_timeout_id;
	CamelOperation *status;
};

static gboolean
pine_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	EImportTargetHome *s;
	char *maildir, *addrfile;
	gboolean md_exists, addr_exists;

	if (target->type != E_IMPORT_TARGET_HOME)
		return FALSE;

	s = (EImportTargetHome *)target;

	maildir = g_build_filename(s->homedir, "mail", NULL);
	md_exists = g_file_test(maildir, G_FILE_TEST_IS_DIR);
	g_free(maildir);

	addrfile = g_build_filename(s->homedir, ".addressbook", NULL);
	addr_exists = g_file_test(addrfile, G_FILE_TEST_IS_REGULAR);
	g_free (addrfile);

	return md_exists || addr_exists;
}

/*
See: http://www.washington.edu/pine/tech-notes/low-level.html

addressbook line is:
     <nickname>TAB<fullname>TAB<address>TAB<fcc>TAB<comments>
lists, address is:
     "(" <address>, <address>, <address>, ... ")"

<address> is rfc822 address, or alias address.
if rfc822 address includes a phrase, then that overrides <fullname>

FIXME: we dont handle aliases in lists.
*/

static void
import_contact(EBook *book, char *line)
{
	char **strings, *addr, **addrs;
	int i;
	GList *list;
	/*EContactName *name;*/
	EContact *card;
	size_t len;

	card = e_contact_new();
	strings = g_strsplit(line, "\t", 5);
	if (strings[0] && strings[1] && strings[2]) {
		e_contact_set(card, E_CONTACT_NICKNAME, strings[0]);
		e_contact_set(card, E_CONTACT_FULL_NAME, strings[1]);

		addr = strings[2];
		len = strlen(addr);
		if (addr[0] == '(' && addr[len-1] == ')') {
			addr[0] = 0;
			addr[len-1] = 0;
			addrs = g_strsplit(addr+1, ",", 0);
			list = NULL;
			/* So ... this api is just insane ... we set plain strings as the contact email if it
			   is a normal contact, but need to do this xml crap for mailing lists */
			for (i=0;addrs[i];i++) {
				EDestination *d;
				EVCardAttribute *attr;

				d = e_destination_new();
				e_destination_set_email(d, addrs[i]);

				attr = e_vcard_attribute_new (NULL, EVC_EMAIL);
				e_destination_export_to_vcard_attribute (d, attr);
				list = g_list_append(list, attr);
				g_object_unref(d);
			}
			e_contact_set_attributes(card, E_CONTACT_EMAIL, list);
			g_list_foreach(list, (GFunc)e_vcard_attribute_free, NULL);
			g_list_free(list);
			g_strfreev(addrs);
			e_contact_set(card, E_CONTACT_IS_LIST, GINT_TO_POINTER(TRUE));
		} else {
			e_contact_set(card, E_CONTACT_EMAIL_1, strings[2]);
		}

		/*name = e_contact_name_from_string(strings[1]);*/

		if (strings[3] && strings[4])
			e_contact_set(card, E_CONTACT_NOTE, strings[4]);

		/* FIXME Error checking */
		e_book_add_contact(book, card, NULL);
		g_object_unref(card);
	}
	g_strfreev (strings);
}

static void
import_contacts(void)
{
	ESource *primary;
	ESourceList *source_list;
	EBook *book;
	char *name;
	GString *line;
	FILE *fp;
	size_t offset;

	printf("importing pine addressbook\n");

	if (!e_book_get_addressbooks(&source_list, NULL))
		return;		

	name = g_build_filename(g_get_home_dir(), ".addressbook", NULL);
	fp = fopen(name, "r");
	g_free(name);
	if (fp == NULL)
		return;

	primary = e_source_list_peek_source_any(source_list);
	/* FIXME Better error handling */
	if ((book = e_book_new(primary,NULL)) == NULL) {
		fclose(fp);
		g_warning ("Could not create EBook.");
		return;
	}
	
	e_book_open(book, TRUE, NULL);
	g_object_unref(primary);
	g_object_unref(source_list);

	line = g_string_new("");
	g_string_set_size(line, 256);
	offset = 0;
	while (fgets(line->str+offset, 256, fp)) {
		size_t len;

		len = strlen(line->str+offset)+offset;
		if (line->str[len-1] == '\n')
			g_string_truncate(line, len-1);
		else if (!feof(fp)) {
			offset = len;
			g_string_set_size(line, len+256);
			continue;
		} else {
			g_string_truncate(line, len);
		}

		import_contact(book, line->str);
		offset = 0;
	}

	g_string_free(line, TRUE);
	fclose(fp);
	g_object_unref(book);
}

static char *
pine_import_describe (struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Importing Pine data"));
}

static MailImporterSpecial pine_special_folders[] = {
	{ "sent-mail", "Sent" }, /* pine */
	{ "saved-messages", "Drafts" },	/* pine */
	{ 0 },
};

static void
pine_import_import(struct _mail_msg *mm)
{
	struct _pine_import_msg *m = (struct _pine_import_msg *) mm;

	if (GPOINTER_TO_INT(g_datalist_get_data(&m->target->data, "pine-do-addr")))
		import_contacts();

	if (GPOINTER_TO_INT(g_datalist_get_data(&m->target->data, "pine-do-mail"))) {
		char *path;

		path = g_build_filename(g_get_home_dir(), "mail", NULL);
		mail_importer_import_folders_sync(path, pine_special_folders, 0, m->status);
		g_free(path);
	}
}

static void
pine_import_imported(struct _mail_msg *mm)
{
	struct _pine_import_msg *m = (struct _pine_import_msg *)mm;

	printf("importing complete\n");

	if (!camel_exception_is_set(&mm->ex)) {
		GConfClient *gconf;

		gconf = gconf_client_get_default();
		if (GPOINTER_TO_INT(g_datalist_get_data(&m->target->data, "pine-do-addr")))
			gconf_client_set_bool(gconf, "/apps/evolution/importer/pine/addr", TRUE, NULL);
		if (GPOINTER_TO_INT(g_datalist_get_data(&m->target->data, "pine-do-mail")))
			gconf_client_set_bool(gconf, "/apps/evolution/importer/pine/mail", TRUE, NULL);
		g_object_unref(gconf);
	}

	e_import_complete(m->import, (EImportTarget *)m->target);
}

static void
pine_import_free(struct _mail_msg *mm)
{
	struct _pine_import_msg *m = (struct _pine_import_msg *)mm;

	camel_operation_unref(m->status);

	g_free(m->status_what);
	g_mutex_free(m->status_lock);

	g_source_remove(m->status_timeout_id);
	m->status_timeout_id = 0;

	g_object_unref(m->import);
}

static void
pine_status(CamelOperation *op, const char *what, int pc, void *data)
{
	struct _pine_import_msg *importer = data;

	if (pc == CAMEL_OPERATION_START)
		pc = 0;
	else if (pc == CAMEL_OPERATION_END)
		pc = 100;

	g_mutex_lock(importer->status_lock);
	g_free(importer->status_what);
	importer->status_what = g_strdup(what);
	importer->status_pc = pc;
	g_mutex_unlock(importer->status_lock);
}

static gboolean
pine_status_timeout(void *data)
{
	struct _pine_import_msg *importer = data;
	int pc;
	char *what;

	if (importer->status_what) {
		g_mutex_lock(importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock(importer->status_lock);

		e_import_status(importer->import, (EImportTarget *)importer->target, what, pc);
	}

	return TRUE;
}

static struct _mail_msg_op pine_import_op = {
	pine_import_describe,
	pine_import_import,
	pine_import_imported,
	pine_import_free,
};

static int
mail_importer_pine_import(EImport *ei, EImportTarget *target)
{
	struct _pine_import_msg *m;
	int id;

	m = mail_msg_new(&pine_import_op, NULL, sizeof (*m));
	g_datalist_set_data(&target->data, "pine-msg", m);
	m->import = ei;
	g_object_ref(m->import);
	m->target = target;
	m->status_timeout_id = g_timeout_add(100, pine_status_timeout, m);
	m->status_lock = g_mutex_new();
	m->status = camel_operation_new(pine_status, m);

	id = m->msg.seq;
	
	e_thread_put(mail_thread_queued, (EMsg *)m);

	return id;
}

static void
checkbox_mail_toggle_cb(GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data(&target->data, "pine-do-mail", GINT_TO_POINTER(gtk_toggle_button_get_active(tb)));
}

static void
checkbox_addr_toggle_cb(GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data(&target->data, "pine-do-addr", GINT_TO_POINTER(gtk_toggle_button_get_active(tb)));
}

static GtkWidget *
pine_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *box, *w;
	GConfClient *gconf;
	gboolean done_mail, done_addr;

	gconf = gconf_client_get_default ();
	done_mail = gconf_client_get_bool (gconf, "/apps/evolution/importer/pine/mail", NULL);
	done_addr = gconf_client_get_bool (gconf, "/apps/evolution/importer/pine/address", NULL);
	g_object_unref(gconf);

	g_datalist_set_data(&target->data, "pine-do-mail", GINT_TO_POINTER(!done_mail));
	g_datalist_set_data(&target->data, "pine-do-addr", GINT_TO_POINTER(!done_addr));

	box = gtk_vbox_new(FALSE, 2);

	w = gtk_check_button_new_with_label(_("Mail"));
	gtk_toggle_button_set_active((GtkToggleButton *)w, !done_mail);
	g_signal_connect(w, "toggled", G_CALLBACK(checkbox_mail_toggle_cb), target);
	gtk_box_pack_start((GtkBox *)box, w, FALSE, FALSE, 0);

	w = gtk_check_button_new_with_label(_("Addressbook"));
	gtk_toggle_button_set_active((GtkToggleButton *)w, !done_addr);
	g_signal_connect(w, "toggled", G_CALLBACK(checkbox_addr_toggle_cb), target);
	gtk_box_pack_start((GtkBox *)box, w, FALSE, FALSE, 0);

	gtk_widget_show_all(box);

	return box;
}

static void
pine_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	if (GPOINTER_TO_INT(g_datalist_get_data(&target->data, "pine-do-mail"))
	    || GPOINTER_TO_INT(g_datalist_get_data(&target->data, "pine-do-addr")))
		mail_importer_pine_import(ei, target);
	else
		e_import_complete(ei, target);
}

static void
pine_cancel(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	struct _pine_import_msg *m = g_datalist_get_data(&target->data, "pine-msg");

	if (m)
		camel_operation_cancel(m->status);
}

static EImportImporter pine_importer = {
	E_IMPORT_TARGET_HOME,
	0,
	pine_supported,
	pine_getwidget,
	pine_import,
	pine_cancel,
};

EImportImporter *
pine_importer_peek(void)
{
	pine_importer.name = _("Evolution Pine importer");
	pine_importer.description = _("Import mail from Pine.");

	return &pine_importer;
}
