/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Author : 
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <config.h>
#include <errno.h>
#include <gnome.h>
#include "mail.h"
#include "mail-threads.h"
#include "folder-browser.h"
#include "e-util/e-setup.h"
#include "filter/filter-editor.h"
#include "filter/filter-driver.h"
#include "widgets/e-table/e-table.h"

/* FIXME: is there another way to do this? */
#include "Evolution.h"
#include "evolution-storage.h"

#ifndef HAVE_MKSTEMP
#include <fcntl.h>
#include <sys/stat.h>
#endif

struct post_send_data {
	CamelFolder *folder;
	const char *uid;
	guint32 flags;
};

typedef struct rfm_s { 
	FolderBrowser *fb; 
	char *source_url; 
} rfm_t;

typedef struct rsm_s {
	EMsgComposer *composer;
	CamelTransport *transport;
	CamelMimeMessage *message;
	const char *subject;
	char *from;
	struct post_send_data *psd;
	gboolean ok;
} rsm_t;

static void
real_fetch_mail( gpointer user_data );

static void
real_send_mail( gpointer user_data );

static void
cleanup_send_mail( gpointer userdata );

static void
mail_exception_dialog (char *head, CamelException *ex, gpointer widget)
{
	char *msg;
	GtkWindow *window =
		GTK_WINDOW (gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW));

	msg = g_strdup_printf ("%s:\n%s", head,
			       camel_exception_get_description (ex));
	gnome_error_dialog_parented (msg, window);
	g_free (msg);
}

#ifdef USE_BROKEN_THREADS
static void
async_mail_exception_dialog (char *head, CamelException *ex, gpointer unused )
{
	mail_op_error( "%s: %s", head, camel_exception_get_description( ex ) );
}
#else
#define async_mail_exception_dialog mail_exception_dialog
#endif

static gboolean
check_configured (void)
{
	char *path;
	gboolean configured;

	path = g_strdup_printf ("=%s/config=/mail/configured", evolution_dir);
	if (gnome_config_get_bool (path)) {
		g_free (path);
		return TRUE;
	}

	mail_config_druid ();

	configured = gnome_config_get_bool (path);
	g_free (path);
	return configured;
}

void
real_fetch_mail (gpointer user_data )
{
	rfm_t *info;
	FolderBrowser *fb = NULL;
	CamelException *ex;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL;
	char *path, *url = NULL;
	FilterDriver *filter = NULL;
	char *userrules, *systemrules;
	char *tmp_mbox = NULL, *source;

	info = (rfm_t *) user_data;
	fb = info->fb;
	url = info->source_url;

	path = CAMEL_SERVICE (fb->folder->parent_store)->url->path;
	ex = camel_exception_new ();

	tmp_mbox = g_strdup_printf ("%s/movemail", path);

	/* If fetching mail from an mbox store, safely copy it to a
	 * temporary store first.
	 */
	if (!strncmp (url, "mbox:", 5)) {
		int tmpfd;

		printf("moving from a local mbox\n");

		tmpfd = open (tmp_mbox, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);

		if (tmpfd == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Couldn't create temporary "
					      "mbox: %s", g_strerror (errno));
			async_mail_exception_dialog ("Unable to move mail", ex, fb );
			goto cleanup;
		}
		close (tmpfd);

		/* Skip over "mbox:" plus host part (if any) of url. */
		source = url + 5;
		if (!strncmp (source, "//", 2))
			source = strchr (source + 2, '/');

		switch (camel_movemail (source, tmp_mbox, ex)) {
		case -1:
			async_mail_exception_dialog ("Unable to move mail", ex, fb);
			/* FALL THROUGH */

		case 0:
			goto cleanup;
		}

		folder = camel_store_get_folder (fb->folder->parent_store,
						 strrchr (tmp_mbox, '/') + 1,
						 FALSE, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			async_mail_exception_dialog ("Unable to move mail", ex, fb);
			goto cleanup;
		}
	} else {
		CamelFolder *sourcefolder;

		store = camel_session_get_store (session, url, ex);
		if (!store) {
			async_mail_exception_dialog ("Unable to get new mail", ex, fb);
			goto cleanup;
		}
		camel_service_connect (CAMEL_SERVICE (store), ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_USER_CANCEL)
				async_mail_exception_dialog ("Unable to get new mail", ex, fb);
			goto cleanup;
		}

		sourcefolder = camel_store_get_folder (store, "inbox",
						       FALSE, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			async_mail_exception_dialog ("Unable to get new mail", ex, fb);
			goto cleanup;
		}

		/* can we perform filtering on this source? */
		if (!(sourcefolder->has_summary_capability
		      && sourcefolder->has_search_capability)) {
			GPtrArray *uids;
			int i;

			printf("folder isn't searchable, performing movemail ...\n");

			folder = camel_store_get_folder (fb->folder->parent_store,
							 strrchr (tmp_mbox, '/') + 1,
							 TRUE, ex);
			if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
				async_mail_exception_dialog ("Unable to move mail", ex, fb);
				goto cleanup;
			}
			
			uids = camel_folder_get_uids (sourcefolder, ex);
			printf("got %d messages in source\n", uids->len);
			for (i = 0; i < uids->len; i++) {
				CamelMimeMessage *msg;
				printf("copying message %d to dest\n", i + 1);
				msg = camel_folder_get_message_by_uid (sourcefolder, uids->pdata[i], ex);
				if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
					async_mail_exception_dialog ("Unable to read message", ex, fb);
					gtk_object_unref((GtkObject *)msg);
					gtk_object_unref((GtkObject *)sourcefolder);
					goto cleanup;
				}

				camel_folder_append_message (folder, msg, ex);
				if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
					async_mail_exception_dialog ("Unable to write message", ex, fb);
					gtk_object_unref((GtkObject *)msg);
					gtk_object_unref((GtkObject *)sourcefolder);
					goto cleanup;
				}

				camel_folder_delete_message_by_uid(sourcefolder, uids->pdata[i], ex);
				gtk_object_unref((GtkObject *)msg);
			}
			camel_folder_free_uids (sourcefolder, uids);
			camel_folder_sync (sourcefolder, TRUE, ex);
			if (camel_exception_is_set (ex))
				async_mail_exception_dialog ("", ex, fb);
			gtk_object_unref((GtkObject *)sourcefolder);
		} else {
			printf("we can search on this folder, performing search!\n");
			folder = sourcefolder;
		}
	}

	if (camel_folder_get_message_count (folder, ex) == 0) {
		gnome_ok_dialog ("No new messages.");
		goto cleanup;
	} else if (camel_exception_is_set (ex)) {
		async_mail_exception_dialog ("Unable to get new mail", ex, fb);
		goto cleanup;
	}

	folder_browser_clear_search (fb);

	/* apply filtering rules to this inbox */
	filter = filter_driver_new();
	userrules = g_strdup_printf ("%s/filters.xml", evolution_dir);
	systemrules = g_strdup_printf("%s/evolution/filtertypes.xml", EVOLUTION_DATADIR);
	filter_driver_set_rules(filter, systemrules, userrules);
	filter_driver_set_session(filter, session);
	g_free(userrules);
	g_free(systemrules);

	if (filter_driver_run(filter, folder, fb->folder) == -1) {
		async_mail_exception_dialog ("Unable to get new mail", ex, fb);
		goto cleanup;
	}

	/* Redisplay. Ick. FIXME */
	path = g_strdup_printf ("file://%s", path);
	folder_browser_set_uri (fb, path);
	g_free (path);

 cleanup:
	g_free(tmp_mbox);

	if (filter)
		gtk_object_unref((GtkObject *)filter);
	if (url)
		g_free (url);
	if (folder) {
		camel_folder_sync (folder, TRUE, ex);
		gtk_object_unref (GTK_OBJECT (folder));
	}
	if (store) {
		camel_service_disconnect (CAMEL_SERVICE (store), ex);
		gtk_object_unref (GTK_OBJECT (store));
	}
	camel_exception_free (ex);
}

/* FIXME: This is BROKEN! It fetches mail into whatever folder you're
 * currently viewing.
 */
void
fetch_mail (GtkWidget *button, gpointer user_data)
{
	char *path, *url = NULL;
	rfm_t *info;

	if (!check_configured ())
		return;

	path = g_strdup_printf ("=%s/config=/mail/source", evolution_dir);
	url = gnome_config_get_string (path);
	g_free (path);

	if (!url) {
		GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (user_data),
							  GTK_TYPE_WINDOW);

		gnome_error_dialog_parented ("You have no remote mail source "
					     "configured", GTK_WINDOW (win));
		return;
	}

	/* This must be dynamically allocated so as not to be clobbered
	 * when we return. Actually, making it static in the whole file
	 * would probably work.
	 */

	info = g_new( rfm_t, 1 );
	info->fb = FOLDER_BROWSER( user_data );
	info->source_url = url;
#ifdef USE_BROKEN_THREADS
	mail_operation_try( _("Fetching mail"), real_fetch_mail, NULL, info );
#else
	real_fetch_mail( info );
#endif
}

static gboolean
ask_confirm_for_empty_subject (EMsgComposer *composer)
{
	GtkWidget *message_box;
	int button;

	message_box = gnome_message_box_new (_("This message has no subject.\nReally send?"),
					     GNOME_MESSAGE_BOX_QUESTION,
					     GNOME_STOCK_BUTTON_YES, GNOME_STOCK_BUTTON_NO,
					     NULL);

	button = gnome_dialog_run_and_close (GNOME_DIALOG (message_box));

	if (button == 0)
		return TRUE;
	else
		return FALSE;
}

static void
real_send_mail( gpointer user_data )
{
	rsm_t *info = (rsm_t *) user_data;
	EMsgComposer *composer = NULL;
	CamelTransport *transport = NULL;
	CamelException *ex = NULL;
	CamelMimeMessage *message = NULL;
	const char *subject = NULL;
	char *from = NULL;
	struct post_send_data *psd = NULL;

#ifdef USE_BROKEN_THREADS
	mail_op_hide_progressbar();
	mail_op_set_message( "Connecting to transport..." );
#endif

	ex = camel_exception_new ();
	composer = info->composer;
	transport = info->transport;
	message = info->message;
	subject = info->subject;
	from = info->from;
	psd = info->psd;

	camel_mime_message_set_from (message, from);
	camel_medium_add_header (CAMEL_MEDIUM (message), "X-Mailer",
				 "Evolution (Developer Preview)");
	camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);

	camel_service_connect (CAMEL_SERVICE (transport), ex);

#ifdef USE_BROKEN_THREADS
	mail_op_set_message( "Connected. Sending..." );
#endif

	if (!camel_exception_is_set (ex))
		camel_transport_send (transport, CAMEL_MEDIUM (message), ex);

	if (!camel_exception_is_set (ex)) {
#ifdef USE_BROKEN_THREADS
		mail_op_set_message( "Sent. Disconnecting..." );
#endif 		
		camel_service_disconnect (CAMEL_SERVICE (transport), ex);
	}

	if (camel_exception_is_set (ex)) {
		async_mail_exception_dialog ("Could not send message", ex, composer);
		info->ok = FALSE;
	} else {
		if (psd) {
			guint32 set;

			set = camel_folder_get_message_flags (psd->folder,
							      psd->uid, ex);
			camel_folder_set_message_flags (psd->folder, psd->uid,
							psd->flags, ~set, ex);
		}
		info->ok = TRUE;

	}

	camel_exception_free (ex);
}

static void
cleanup_send_mail( gpointer userdata )
{
	rsm_t *info = (rsm_t *) userdata;
	
	if( info->ok ) {
		gtk_object_destroy (GTK_OBJECT (info->composer));
	}

	gtk_object_unref (GTK_OBJECT (info->message));
	g_free( info );
}

static void
composer_send_cb (EMsgComposer *composer, gpointer data)
{
	static CamelTransport *transport = NULL;
	struct post_send_data *psd = data;
	rsm_t *info;
	static char *from = NULL;
	const char *subject;
	CamelException *ex;
	CamelMimeMessage *message;
	char *name, *addr, *path;

	ex = camel_exception_new ();

	if (!from) {
		CamelInternetAddress *ciaddr;

		path = g_strdup_printf ("=%s/config=/mail/id_name",
					evolution_dir);
		name = gnome_config_get_string (path);
		g_assert (name);
		g_free (path);
		path = g_strdup_printf ("=%s/config=/mail/id_addr",
					evolution_dir);
		addr = gnome_config_get_string (path);
		g_assert (addr);
		g_free (path);

		ciaddr = camel_internet_address_new ();
		camel_internet_address_add (ciaddr, name, addr);

		from = camel_address_encode (CAMEL_ADDRESS (ciaddr));
	}

	if (!transport) {
		char *url;

		path = g_strdup_printf ("=%s/config=/mail/transport",
					evolution_dir);
		url = gnome_config_get_string (path);
		g_assert (url);
		g_free (path);

		transport = camel_session_get_transport (session, url, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Could not load mail transport",
					       ex, composer);
			camel_exception_free (ex);
			return;
		}
	}

	message = e_msg_composer_get_message (composer);

	subject = camel_mime_message_get_subject (message);
	if (subject == NULL || subject[0] == '\0') {
		if (! ask_confirm_for_empty_subject (composer)) {
			gtk_object_unref (GTK_OBJECT (message));
			return;
		}
	}

	info = g_new0( rsm_t, 1 );
	info->composer = composer;
	info->transport = transport;
	info->message = message;
	info->subject = subject;
	info->from = from;
	info->psd = psd;

#ifdef USE_BROKEN_THREADS
	mail_operation_try( "Send Message", real_send_mail, cleanup_send_mail, info );
#else
	real_send_mail( info );
	cleanup_send_mail( info );
#endif
}

static void
free_psd (GtkWidget *composer, gpointer user_data)
{
	struct post_send_data *psd = user_data;

	gtk_object_unref (GTK_OBJECT (psd->folder));
	g_free (psd);
}

void
compose_msg (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *composer;

	if (!check_configured ())
		return;

	composer = e_msg_composer_new ();

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_widget_show (composer);
}

/* Send according to a mailto (RFC 2368) URL. */
void
send_to_url (const char *url)
{
	GtkWidget *composer;

	if (!check_configured ())
		return;

	composer = e_msg_composer_new_from_url (url);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_widget_show (composer);
}	

static void
reply (FolderBrowser *fb, gboolean to_all)
{
	EMsgComposer *composer;
	struct post_send_data *psd;

	if (!check_configured ())
		return;

	psd = g_new (struct post_send_data, 1);
	psd->folder = fb->folder;
	gtk_object_ref (GTK_OBJECT (psd->folder));
	psd->uid = fb->message_list->selected_uid;
	psd->flags = CAMEL_MESSAGE_ANSWERED;

	composer = mail_generate_reply (fb->mail_display->current_message,
					to_all);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), psd); 
	gtk_signal_connect (GTK_OBJECT (composer), "destroy",
			    GTK_SIGNAL_FUNC (free_psd), psd); 

	gtk_widget_show (GTK_WIDGET (composer));	
}

void
reply_to_sender (GtkWidget *button, gpointer user_data)
{
	reply (FOLDER_BROWSER (user_data), FALSE);
}

void
reply_to_all (GtkWidget *button, gpointer user_data)
{
	reply (FOLDER_BROWSER (user_data), TRUE);
}


void
forward_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb;
	EMsgComposer *composer;

	if (!check_configured ())
		return;

	fb = FOLDER_BROWSER (user_data);
	composer = mail_generate_forward (fb->mail_display->current_message,
					  TRUE, TRUE);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);

	gtk_widget_show (GTK_WIDGET (composer));	
}

void
delete_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = user_data;
	MessageList *ml = fb->message_list;
	CamelException ex;
	guint32 flags;

	if (!fb->mail_display->current_message)
		return;

	camel_exception_init (&ex);

	flags = camel_folder_get_message_flags (fb->folder, ml->selected_uid,
						&ex);
	if (!camel_exception_is_set (&ex)) {
		/* Toggle the deleted flag without touching other flags. */
		camel_folder_set_message_flags (fb->folder, ml->selected_uid,
						CAMEL_MESSAGE_DELETED,
						~flags, &ex);
	}

	if (camel_exception_is_set (&ex)) {
		mail_exception_dialog ("Could not toggle deleted flag",
				       &ex, fb);
		camel_exception_clear (&ex);
		return;
	}

	/* Move the cursor down a row... FIXME: should skip other
	 * deleted messages.
	 */
	e_table_set_cursor_row (E_TABLE (ml->etable), ml->selected_row + 1);
}

static void real_expunge_folder( gpointer user_data )
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);
	CamelException ex;

#ifdef USE_BROKEN_THREADS
	mail_op_hide_progressbar();
	mail_op_set_message( "Expunging %s...", fb->message_list->folder->full_name );
#endif

	camel_exception_init(&ex);

	camel_folder_expunge(fb->message_list->folder, &ex);

	/* FIXME: is there a better way to force an update? */
	/* FIXME: Folder should raise a signal to say its contents has changed ... */
	e_table_model_changed (fb->message_list->table_model);

	if (camel_exception_get_id (&ex) != CAMEL_EXCEPTION_NONE) {
		async_mail_exception_dialog ("Unable to expunge deleted messages", &ex, fb);
	}
}

void
expunge_folder (BonoboUIHandler *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);

	if (fb->message_list->folder) {
#ifdef USE_BROKEN_THREADS
		mail_operation_try( "Expunge Folder", real_expunge_folder, NULL, fb );
#else
		real_expunge_folder( fb );
#endif
	}
}

static void
filter_druid_clicked(FilterEditor *fe, int button, FolderBrowser *fb)
{
	printf("closing dialog\n");
	if (button == 0) {
		char *user;

		user = g_strdup_printf ("%s/filters.xml", evolution_dir);
		filter_editor_save_rules(fe, user);
		printf("saving filter options to '%s'\n", user);
		g_free(user);
	}
	if (button != -1) {
		gnome_dialog_close((GnomeDialog *)fe);
	}
}

void
filter_edit (BonoboUIHandler *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);
	FilterEditor *fe;
	char *user, *system;

	printf("Editing filters ...\n");
	fe = filter_editor_new();

	user = g_strdup_printf ("%s/filters.xml", evolution_dir);
	system = g_strdup_printf("%s/evolution/filtertypes.xml", EVOLUTION_DATADIR);
	filter_editor_set_rule_files(fe, system, user);
	g_free(user);
	g_free(system);
	gnome_dialog_append_buttons((GnomeDialog *)fe, GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, 0);
	gtk_signal_connect((GtkObject *)fe, "clicked", filter_druid_clicked, fb);
	gtk_widget_show((GtkWidget *)fe);
}

static void
vfolder_editor_clicked(FilterEditor *fe, int button, FolderBrowser *fb)
{
	printf("closing dialog\n");
	if (button == 0) {
		char *user;

		user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
		filter_editor_save_rules(fe, user);
		printf("saving vfolders to '%s'\n", user);
		g_free(user);

		/* FIXME: this is also not the way to do this, see also
		   component-factory.c */
		{
			EvolutionStorage *storage;
			FilterDriver *fe;
			int i, count;
			char *user, *system;
			extern char *evolution_dir;

			storage = gtk_object_get_data((GtkObject *)fb, "e-storage");
	
			fe = filter_driver_new();
			user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
			system = g_strdup_printf("%s/evolution/vfoldertypes.xml", EVOLUTION_DATADIR);
			filter_driver_set_rules(fe, system, user);
			g_free(user);
			g_free(system);
			count = filter_driver_rule_count(fe);
			for (i=0;i<count;i++) {
				struct filter_option *fo;
				GString *query;
				struct filter_desc *desc = NULL;
				char *desctext, descunknown[64];
				char *name;
				
				fo = filter_driver_rule_get(fe, i);
				if (fo == NULL)
					continue;
				query = g_string_new("");
				if (fo->description)
					desc = fo->description->data;
				if (desc)
					desctext = desc->data;
				else {
					sprintf(descunknown, "volder-%p", fo);
					desctext = descunknown;
				}
				g_string_sprintf(query, "vfolder:/%s/vfolder/%s?", evolution_dir, desctext);
				filter_driver_expand_option(fe, query, NULL, fo);
				name = g_strdup_printf("/%s", desctext);
				printf("Adding new vfolder: %s\n", query->str);
				evolution_storage_new_folder (storage, name,
							      "mail",
							      query->str,
							      name+1);
				g_string_free(query, TRUE);
				g_free(name);
			}
			gtk_object_unref((GtkObject *)fe);
		}

	}
	if (button != -1) {
		gnome_dialog_close((GnomeDialog *)fe);
	}
}

void
vfolder_edit (BonoboUIHandler *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);
	FilterEditor *fe;
	char *user, *system;

	printf("Editing vfolders ...\n");
	fe = filter_editor_new();

	user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
	system = g_strdup_printf("%s/evolution/vfoldertypes.xml", EVOLUTION_DATADIR);
	filter_editor_set_rule_files(fe, system, user);
	g_free(user);
	g_free(system);
	gnome_dialog_append_buttons((GnomeDialog *)fe, GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, 0);
	gtk_signal_connect((GtkObject *)fe, "clicked", vfolder_editor_clicked, fb);
	gtk_widget_show((GtkWidget *)fe);
}

void
providers_config (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GtkWidget *pc;

	printf("Configuring Providers ...\n");
	pc = providers_config_new();

	gtk_widget_show(pc);
}

