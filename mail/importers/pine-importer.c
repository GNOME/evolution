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
#include <ctype.h>
#include <string.h>

#include <glib.h>

#include <libgnomeui/gnome-messagebox.h>
#include <gtk/gtk.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/evolution-importer-client.h>
#include <importer/GNOME_Evolution_Importer.h>

#include "mail-importer.h"

#include "mail/mail-mt.h"
#include "mail/mail-component.h"

#include <libebook/e-book.h>
#include <addressbook/util/e-destination.h>

#define KEY "pine-mail-imported"

/*#define SUPER_IMPORTER_DEBUG*/
#ifdef SUPER_IMPORTER_DEBUG
#define d(x) x
#else
#define d(x)
#endif

typedef struct {
	EvolutionIntelligentImporter *ii;

	GMutex *status_lock;
	char *status_what;
	int status_pc;
	int status_timeout_id;
	CamelOperation *cancel;	/* cancel/status port */

	GtkWidget *mail;
	GtkWidget *address;

	gboolean do_mail;
	gboolean done_mail;
	gboolean do_address;
	gboolean done_address;

	/* GUI */
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *progressbar;
} PineImporter;

static void
pine_importer_response(GtkWidget *w, guint button, void *data)
{
	PineImporter *importer = data;

	if (button == GTK_RESPONSE_CANCEL
	    && importer->cancel)
		camel_operation_cancel(importer->cancel);
}

static GtkWidget *
create_importer_gui (PineImporter *importer)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(NULL, 0/*GTK_DIALOG_NO_SEPARATOR*/,
					GTK_MESSAGE_INFO, GTK_BUTTONS_CANCEL,
					_("Evolution is importing your old Pine data"));
	gtk_window_set_title (GTK_WINDOW (dialog), _("Importing..."));

	importer->label = gtk_label_new (_("Please wait"));
	importer->progressbar = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), importer->label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), importer->progressbar, FALSE, FALSE, 0);
	g_signal_connect(dialog, "response", G_CALLBACK(pine_importer_response), importer);

	return dialog;
}

static void
pine_store_settings (PineImporter *importer)
{
	GConfClient *gconf = gconf_client_get_default ();

	gconf_client_set_bool (gconf, "/apps/evolution/importer/pine/mail", importer->done_mail, NULL);
	gconf_client_set_bool (gconf, "/apps/evolution/importer/pine/address", importer->done_address, NULL);
	g_object_unref(gconf);
}

static void
pine_restore_settings (PineImporter *importer)
{
	GConfClient *gconf = gconf_client_get_default ();

	importer->done_mail = gconf_client_get_bool (gconf, "/apps/evolution/importer/pine/mail", NULL);
	importer->done_address = gconf_client_get_bool (gconf, "/apps/evolution/importer/pine/address", NULL);
	g_object_unref(gconf);
}

static gboolean
pine_can_import (EvolutionIntelligentImporter *ii, void *closure)
{
	PineImporter *importer = closure;
	char *maildir, *addrfile;
	gboolean md_exists = FALSE, addr_exists = FALSE;
	struct stat st;
	
	maildir = g_build_filename(g_get_home_dir(), "mail", NULL);
	md_exists = lstat(maildir, &st) == 0 && S_ISDIR(st.st_mode);
	g_free (maildir);

	importer->do_mail = md_exists && !importer->done_mail;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail), importer->do_mail);
	gtk_widget_set_sensitive (importer->mail, md_exists);

	addrfile = g_build_filename(g_get_home_dir(), ".addressbook", NULL);
	addr_exists = lstat(addrfile, &st) == 0 && S_ISREG(st.st_mode);
	g_free (addrfile);

	gtk_widget_set_sensitive (importer->address, addr_exists);

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
import_contacts(PineImporter *importer)
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

struct _pine_import_msg {
	struct _mail_msg msg;

	PineImporter *importer;
};

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

	if (m->importer->do_address)
		import_contacts(m->importer);

	if (m->importer->do_mail) {
		char *path;

		path = g_build_filename(g_get_home_dir(), "mail", NULL);
		mail_importer_import_folders_sync(path, pine_special_folders, 0, m->importer->cancel);
		g_free(path);
	}
}

static void
pine_import_imported(struct _mail_msg *mm)
{
}

static void
pine_import_free(struct _mail_msg *mm)
{
	/*struct _pine_import_msg *m = (struct _pine_import_msg *)mm;*/
}

static struct _mail_msg_op pine_import_op = {
	pine_import_describe,
	pine_import_import,
	pine_import_imported,
	pine_import_free,
};

static int
mail_importer_pine_import(PineImporter *importer)
{
	struct _pine_import_msg *m;
	int id;

	m = mail_msg_new(&pine_import_op, NULL, sizeof (*m));
	m->importer = importer;

	id = m->msg.seq;
	
	e_thread_put(mail_thread_queued, (EMsg *) m);

	return id;
}

static void
pine_status(CamelOperation *op, const char *what, int pc, void *data)
{
	PineImporter *importer = data;

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
	PineImporter *importer = data;
	int pc;
	char *what;

	if (!importer->status_what)
		return TRUE;

	g_mutex_lock(importer->status_lock);
	what = importer->status_what;
	importer->status_what = NULL;
	pc = importer->status_pc;
	g_mutex_unlock(importer->status_lock);

	gtk_progress_bar_set_fraction((GtkProgressBar *)importer->progressbar, (gfloat)(pc/100.0));
	gtk_progress_bar_set_text((GtkProgressBar *)importer->progressbar, what);
	
	return TRUE;
}

static void
pine_create_structure (EvolutionIntelligentImporter *ii, void *closure)
{
	PineImporter *importer = closure;

	if (importer->do_address || importer->do_mail) {
		importer->dialog = create_importer_gui (importer);
		gtk_widget_show_all (importer->dialog);
		importer->status_timeout_id = g_timeout_add(100, pine_status_timeout, importer);
		importer->cancel = camel_operation_new(pine_status, importer);

		mail_msg_wait(mail_importer_pine_import(importer));

		camel_operation_unref(importer->cancel);
		g_source_remove(importer->status_timeout_id);
		importer->status_timeout_id = 0;

		if (importer->do_address)
			importer->done_address = TRUE;
		if (importer->do_mail)
			importer->done_mail = TRUE;
	}

	pine_store_settings (importer);

	bonobo_object_unref (BONOBO_OBJECT (ii));
}

static void
pine_destroy_cb (PineImporter *importer, GtkObject *object)
{
	pine_store_settings (importer);

	if (importer->status_timeout_id)
		g_source_remove(importer->status_timeout_id);
	g_free(importer->status_what);
	g_mutex_free(importer->status_lock);

	if (importer->dialog)
		gtk_widget_destroy(importer->dialog);

	g_free(importer);
}

/* Fun inity stuff */

/* Fun control stuff */
static void
checkbox_toggle_cb(GtkToggleButton *tb, gboolean *do_item)
{
	*do_item = gtk_toggle_button_get_active(tb);
}

static BonoboControl *
create_checkboxes_control (PineImporter *importer)
{
	GtkWidget *hbox;
	BonoboControl *control;

	hbox = gtk_hbox_new (FALSE, 2);

	importer->mail = gtk_check_button_new_with_label (_("Mail"));
	gtk_signal_connect (GTK_OBJECT (importer->mail), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_mail);

	importer->address = gtk_check_button_new_with_label (_("Addressbook"));
	gtk_signal_connect (GTK_OBJECT (importer->address), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_address);

	gtk_box_pack_start (GTK_BOX (hbox), importer->mail, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), importer->address, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);
	control = bonobo_control_new (hbox);
	return control;
}

BonoboObject *
pine_intelligent_importer_new(void)
{
	EvolutionIntelligentImporter *importer;
	BonoboControl *control;
	PineImporter *pine;
	char *message = N_("Evolution has found Pine mail files.\n"
			   "Would you like to import them into Evolution?");

	pine = g_new0 (PineImporter, 1);
	pine->status_lock = g_mutex_new();
	pine_restore_settings(pine);
	importer = evolution_intelligent_importer_new (pine_can_import,
						       pine_create_structure,
						       _("Pine"),
						       _(message), pine);
	g_object_weak_ref((GObject *)importer, (GWeakNotify)pine_destroy_cb, pine);
	pine->ii = importer;

	control = create_checkboxes_control(pine);
	bonobo_object_add_interface(BONOBO_OBJECT(importer), BONOBO_OBJECT(control));

	return BONOBO_OBJECT(importer);
}
