/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 2009 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib/gi18n.h>
#include "mail-account-view.h"
#include <libedataserverui/e-passwords.h>
#include <libedataserver/e-source-group.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-account-list.h>
#include "mail-view.h"
#include "e-util/e-config.h"
#include "mail/mail-config.h"
#include "mail/mail-session.h"
#include "mail-guess-servers.h"

struct _MailAccountViewPrivate {
	GtkWidget *tab_str;

	GtkWidget *calendar;
	GtkWidget *gcontacts;
	GtkWidget *gmail_info_label;

	gboolean is_gmail;
	gboolean is_yahoo;
	gboolean do_gcontacts;
	gboolean do_calendar;

	gchar *username;

	GtkWidget *yahoo_cal_entry;
};

G_DEFINE_TYPE (MailAccountView, mail_account_view, GTK_TYPE_VBOX)

enum {
	VIEW_CLOSE,
	LAST_SIGNAL
};

enum {
	ERROR_NO_FULLNAME = 1,
	ERROR_NO_EMAIL = 2,
	ERROR_INVALID_EMAIL = 3,
	ERROR_NO_PASSWORD = 4,
};

struct _dialog_errors {
	gint error;
	const gchar *detail;
} dialog_errors[] = {
	{ ERROR_NO_FULLNAME, N_("Please enter your full name.") },
	{ ERROR_NO_EMAIL, N_("Please enter your email address.") },
	{ ERROR_INVALID_EMAIL, N_("The email address you have entered is invalid.") },
	{ ERROR_NO_PASSWORD, N_("Please enter your password.") }
};
static guint signals[LAST_SIGNAL] = { 0 };

static void
mail_account_view_init (MailAccountView  *shell)
{
	shell->priv = g_new0(MailAccountViewPrivate, 1);

	shell->priv->is_gmail = FALSE;
	shell->priv->is_yahoo = FALSE;
	shell->priv->username = NULL;
}

static void
mail_account_view_finalize (GObject *object)
{
	MailAccountView *shell = (MailAccountView *)object;

	g_free(shell->priv->username);

	G_OBJECT_CLASS (mail_account_view_parent_class)->finalize (object);
}

static void
mail_account_view_class_init (MailAccountViewClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);

	mail_account_view_parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = mail_account_view_finalize;

	signals[VIEW_CLOSE] =
		g_signal_new ("view-close",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (MailAccountViewClass , view_close),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

}

#ifdef NOT_USED

enum {
	GMAIL = 0,
	YAHOO,
	AOL
};
struct _server_prefill {
	gchar *key;
	gchar *recv;
	gchar *send;
	gchar *proto;
	gchar *ssl;
} std_server[] = {
	{"gmail", "imap.gmail.com", "smtp.gmail.com", "imap", "always"},
	{"yahoo", "pop3.yahoo.com", "smtp.yahoo.com", "pop", "never"},
	{"aol", "imap.aol.com", "smtp.aol.com", "pop", "never"},
	{"msn", "pop3.email.msn.com", "smtp.email.msn.com", "pop", "never"}
};
static gint
check_servers (gchar *server)
{
	gint len = G_N_ELEMENTS(std_server), i;

	for (i=0; i<len; i++) {
		if (strstr(server, std_server[i].key) != NULL)
			return i;
	}

	return -1;
}
#endif
static void
save_identity (MailAccountView *view)
{
}

static gint
validate_identity (MailAccountView *view)
{
	gchar *user = (gchar *)e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_ID_NAME);
	gchar *email = (gchar *)e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_ID_ADDRESS);
	gchar *tmp;
	const gchar *pwd = gtk_entry_get_text ((GtkEntry *)view->password);

	if (!user || !*user)
		return ERROR_NO_FULLNAME;
	if (!email || !*email)
		return ERROR_NO_EMAIL;
	if (view->original) /* We don't query/store pwd on edit. */
		return 0;
	if (!pwd || !*pwd)
		return ERROR_NO_PASSWORD;

	tmp = strchr(email, '@');
	if (!tmp || tmp[1] == 0)
		return ERROR_INVALID_EMAIL;

	return 0;
}
#ifdef NOT_USED
static void
save_send (MailAccountView *view)
{
}

static void
save_account (MailAccountView *view)
{
}
#endif

#define PACK_BOX(w) box = gtk_hbox_new(FALSE, 0); gtk_box_pack_start((GtkBox *)box, w, FALSE, FALSE, 12); gtk_widget_show(box);
#define PACK_BOXF(w) box = gtk_hbox_new(FALSE, 0); gtk_box_pack_start((GtkBox *)box, w, FALSE, FALSE, 0); gtk_widget_show(box);

#define CALENDAR_CALDAV_URI "caldav://%s@www.google.com/calendar/dav/%s/events"
#define GMAIL_CALENDAR_LOCATION "://www.google.com/calendar/feeds/"
#define CALENDAR_DEFAULT_PATH "/private/full"
#define SELECTED_CALENDARS "/apps/evolution/calendar/display/selected_calendars"
#define YAHOO_CALENDAR_LOCATION "%s@caldav.calendar.yahoo.com/dav/%s/Calendar/%s"
static gboolean
is_email (const gchar *address)
{
	/* This is supposed to check if the address's domain could be
           an FQDN but alas, it's not worth the pain and suffering. */
	const gchar *at;

	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last gchar */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;

	return TRUE;
}

static gchar *
sanitize_user_mail (const gchar *user)
{
	if (!user)
		return NULL;

	if (strstr (user, "%40") != NULL) {
		return g_strdup (user);
	} else if (!is_email (user)) {
		return g_strconcat (user, "%40gmail.com", NULL);
	} else {
		gchar *tmp = g_malloc0 (sizeof (gchar) * (1 + strlen (user) + 2));
		gchar *at = strchr (user, '@');

		strncpy (tmp, user, at - user);
		strcat (tmp, "%40");
		strcat (tmp, at + 1);

		return tmp;
	}
}

static void
setup_yahoo_account (MailAccountView *mav)
{
	GConfClient *gconf = gconf_client_get_default ();

	mav->priv->do_gcontacts = gtk_toggle_button_get_active((GtkToggleButton *)mav->priv->gcontacts);
	mav->priv->do_calendar = gtk_toggle_button_get_active((GtkToggleButton *)mav->priv->calendar);

	if (mav->priv->do_calendar) {
		ESourceList *slist;
		ESourceGroup *sgrp;
		ESource *calendar;
		gchar *sanitize_uname, *abs_uri, *rel_uri;
		GSList *ids, *temp;
		const gchar *email = e_account_get_string(em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_ID_ADDRESS);

		slist = e_source_list_new_for_gconf (gconf, "/apps/evolution/calendar/sources");
		sgrp = e_source_list_peek_group_by_base_uri (slist, "caldav://");
		if (!sgrp) {
			sgrp = e_source_list_ensure_group (slist, _("CalDAV"), "caldav://", TRUE);
		}

		printf("Setting up Yahoo Calendar: list:%p CalDAVGrp: %p\n", slist, sgrp);

		/* FIXME: Not sure if we should localize 'Calendar' */
		calendar = e_source_new ("Yahoo", "");
		e_source_set_property (calendar, "ssl", "1");
		e_source_set_property (calendar, "refresh", "30");
		e_source_set_property (calendar, "refresh-type", "0");
		e_source_set_property (calendar, "auth", "1");
		e_source_set_property (calendar, "offline_sync", "1");
		e_source_set_property (calendar, "username", email);
		e_source_set_property (calendar, "default", "true");
		e_source_set_property (calendar, "alarm", "true");

		e_source_set_readonly (calendar, FALSE);

		sanitize_uname = sanitize_user_mail (email);

		abs_uri = g_strdup_printf ("caldav://%s@caldav.calendar.yahoo.com/dav/%s/Calendar/%s/", sanitize_uname, email,  gtk_entry_get_text((GtkEntry *)mav->priv->yahoo_cal_entry));
		e_passwords_add_password (abs_uri, gtk_entry_get_text((GtkEntry *)mav->password));
		e_passwords_remember_password ("Calendar", abs_uri);

		rel_uri = g_strdup_printf (YAHOO_CALENDAR_LOCATION, sanitize_uname, email, gtk_entry_get_text((GtkEntry *)mav->priv->yahoo_cal_entry));
		e_source_set_relative_uri (calendar, rel_uri);

		e_source_group_add_source (sgrp, calendar, -1);
		e_source_list_sync (slist, NULL);

		ids = gconf_client_get_list (gconf, SELECTED_CALENDARS, GCONF_VALUE_STRING, NULL);
		ids = g_slist_append (ids, g_strdup (e_source_peek_uid (calendar)));
		gconf_client_set_list (gconf,  SELECTED_CALENDARS, GCONF_VALUE_STRING, ids, NULL);
		temp = ids;

		for (; temp != NULL; temp = g_slist_next (temp))
			g_free (temp->data);
		g_slist_free (ids);

		g_free(abs_uri);
		g_free(rel_uri);
		g_free(sanitize_uname);
		g_object_unref(slist);
		g_object_unref(sgrp);
		g_object_unref(calendar);
	} else
		printf("Not setting up Yahoo Calendar\n");

	if (mav->priv->do_gcontacts) {
		ESourceList *slist;
		ESourceGroup *sgrp;
		ESource *abook;
		gchar *rel_uri;;

		slist = e_source_list_new_for_gconf (gconf, "/apps/evolution/addressbook/sources" );

		sgrp = e_source_list_peek_group_by_base_uri (slist, "google://");

		/* FIXME: Not sure if we should localize 'Contacts' */
		abook = e_source_new ("Contacts", "");
		e_source_set_property (abook, "default", "true");
		e_source_set_property (abook, "offline_sync", "1");
		e_source_set_property (abook, "auth", "plain/password");
		e_source_set_property (abook, "use-ssl", "true");
		e_source_set_property (abook, "remember_password", "true");
		e_source_set_property (abook, "refresh-interval", "86400");
		e_source_set_property (abook, "completion", "true");
		e_source_set_property (abook, "username", mav->priv->username);
		e_source_set_relative_uri (abook, mav->priv->username);

		rel_uri = g_strdup_printf("google://%s/", mav->priv->username);
		e_passwords_add_password (rel_uri, gtk_entry_get_text((GtkEntry *)mav->password));
		e_passwords_remember_password ("Addressbook", rel_uri);
		e_source_group_add_source (sgrp, abook, -1);
		e_source_list_sync (slist, NULL);

		g_free(rel_uri);
		g_object_unref(slist);
		g_object_unref(sgrp);
		g_object_unref(abook);

	}

	g_object_unref (gconf);
}

static void
setup_google_accounts (MailAccountView *mav)
{
	GConfClient *gconf = gconf_client_get_default ();

	mav->priv->do_gcontacts = gtk_toggle_button_get_active((GtkToggleButton *)mav->priv->gcontacts);
	mav->priv->do_calendar = gtk_toggle_button_get_active((GtkToggleButton *)mav->priv->calendar);

	if (mav->priv->do_calendar) {
		ESourceList *slist;
		ESourceGroup *sgrp;
		ESource *calendar;
		gchar *sanitize_uname, *abs_uri, *rel_uri;
		GSList *ids, *temp;

		slist = e_source_list_new_for_gconf (gconf, "/apps/evolution/calendar/sources");
		sgrp = e_source_list_peek_group_by_base_uri (slist, "google://");
		if (!sgrp) {
			sgrp = e_source_list_ensure_group (slist, _("Google"), "google://", TRUE);
		}

		printf("Setting up Google Calendar: list:%p GoogleGrp: %p\n", slist, sgrp);

		/* FIXME: Not sure if we should localize 'Calendar' */
		calendar = e_source_new ("Calendar", "");
		e_source_set_property (calendar, "ssl", "1");
		e_source_set_property (calendar, "refresh", "30");
		e_source_set_property (calendar, "auth", "1");
		e_source_set_property (calendar, "offline_sync", "1");
		e_source_set_property (calendar, "username", mav->priv->username);
		e_source_set_property (calendar, "setup-username", mav->priv->username);
		e_source_set_property (calendar, "default", "true");
		e_source_set_readonly (calendar, FALSE);

		sanitize_uname = sanitize_user_mail (mav->priv->username);

		abs_uri = g_strdup_printf (CALENDAR_CALDAV_URI, sanitize_uname, mav->priv->username);
		e_source_set_absolute_uri (calendar, abs_uri);

		e_passwords_add_password (abs_uri, gtk_entry_get_text((GtkEntry *)mav->password));
		e_passwords_remember_password ("Calendar", abs_uri);
		rel_uri = g_strconcat ("https", GMAIL_CALENDAR_LOCATION, sanitize_uname, CALENDAR_DEFAULT_PATH, NULL);
		e_source_set_relative_uri (calendar, rel_uri);

		e_source_group_add_source (sgrp, calendar, -1);
		e_source_list_sync (slist, NULL);

		ids = gconf_client_get_list (gconf, SELECTED_CALENDARS, GCONF_VALUE_STRING, NULL);
		ids = g_slist_append (ids, g_strdup (e_source_peek_uid (calendar)));
		gconf_client_set_list (gconf,  SELECTED_CALENDARS, GCONF_VALUE_STRING, ids, NULL);
		temp = ids;

		for (; temp != NULL; temp = g_slist_next (temp))
			g_free (temp->data);
		g_slist_free (ids);

		g_free(abs_uri);
		g_free(rel_uri);
		g_free(sanitize_uname);
		g_object_unref(slist);
		g_object_unref(sgrp);
		g_object_unref(calendar);
	} else
		printf("Not setting up Google Calendar\n");

	if (mav->priv->do_gcontacts) {
		ESourceList *slist;
		ESourceGroup *sgrp;
		ESource *abook;
		gchar *rel_uri;;

		slist = e_source_list_new_for_gconf (gconf, "/apps/evolution/addressbook/sources" );

		sgrp = e_source_list_peek_group_by_base_uri (slist, "google://");

		/* FIXME: Not sure if we should localize 'Contacts' */
		abook = e_source_new ("Contacts", "");
		e_source_set_property (abook, "default", "true");
		e_source_set_property (abook, "offline_sync", "1");
		e_source_set_property (abook, "auth", "plain/password");
		e_source_set_property (abook, "use-ssl", "true");
		e_source_set_property (abook, "remember_password", "true");
		e_source_set_property (abook, "refresh-interval", "86400");
		e_source_set_property (abook, "completion", "true");
		e_source_set_property (abook, "username", mav->priv->username);
		e_source_set_relative_uri (abook, mav->priv->username);

		rel_uri = g_strdup_printf("google://%s/", mav->priv->username);
		e_passwords_add_password (rel_uri, gtk_entry_get_text((GtkEntry *)mav->password));
		e_passwords_remember_password ("Addressbook", rel_uri);
		e_source_group_add_source (sgrp, abook, -1);
		e_source_list_sync (slist, NULL);

		g_free(rel_uri);
		g_object_unref(slist);
		g_object_unref(sgrp);
		g_object_unref(abook);

	}

	g_object_unref (gconf);
}

#define INDENTATION 10

static GtkWidget *
create_review (MailAccountView *view)
{
	GtkWidget *table, *box, *label, *entry;
	gchar *uri;
	gchar *enc;
	CamelURL *url;
	gchar *buff;

	uri = (gchar *)e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_SOURCE_URL);
	if (!uri  || (url = camel_url_new(uri, NULL)) == NULL)
		return NULL;

	table = gtk_table_new (4,2, FALSE);
	gtk_table_set_row_spacings ((GtkTable *)table, 4);

	label = gtk_label_new (NULL);
	buff = g_markup_printf_escaped ("<span size=\"large\" weight=\"bold\">%s</span>", _("Personal details:"));
	gtk_label_set_markup ((GtkLabel *)label, buff);
	g_free (buff);
	gtk_widget_show (label);
	PACK_BOXF(label)
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 0, 1, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	label = gtk_label_new (_("Name:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 1, 2, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);
	entry = gtk_label_new(e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_ID_NAME));
	gtk_widget_show(entry);
	PACK_BOX(entry)
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 1, 2, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	label = gtk_label_new (_("Email address:"));
	gtk_widget_show (label);
	PACK_BOX(label)
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 2, 3, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);
	entry = gtk_label_new (e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_ID_ADDRESS));
	gtk_widget_show(entry);
	PACK_BOX(entry)
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 2, 3, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	label = gtk_label_new (NULL);
	buff = g_markup_printf_escaped ("<span size=\"large\" weight=\"bold\">%s</span>", _("Details:"));
	gtk_label_set_markup ((GtkLabel *)label, buff);
	g_free (buff);
	gtk_widget_show (label);
	PACK_BOXF(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 3, 4, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	label = gtk_label_new (NULL);
	buff = g_markup_printf_escaped ("<span size=\"large\" weight=\"bold\">%s</span>", _("Receiving"));
	gtk_label_set_markup ((GtkLabel *)label, buff);
	g_free (buff);
	gtk_widget_show (label);
	PACK_BOXF(label);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 3, 4, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	label = gtk_label_new (_("Server type:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 4, 5, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);
	entry = gtk_label_new (url->protocol);
	gtk_widget_show(entry);
	PACK_BOX(entry)
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 4, 5, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	label = gtk_label_new (_("Server address:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 5, 6, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);
	entry = gtk_label_new (url->host);
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 5, 6, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	label = gtk_label_new (_("Username:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 6, 7, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);
	entry = gtk_label_new (url->user);
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 6, 7, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	label = gtk_label_new (_("Use encryption:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 7, 8, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);
	enc = (gchar *)camel_url_get_param(url, "use_ssl");
	entry = gtk_label_new (enc ? enc : _("never"));
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 7, 8, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	view->priv->username = g_strdup(url->user);
	camel_url_free(url);
	uri =(gchar *) e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_TRANSPORT_URL);
	if (!uri  || (url = camel_url_new(uri, NULL)) == NULL)
		return NULL;

	label = gtk_label_new (NULL);
	buff = g_markup_printf_escaped ("<span size=\"large\" weight=\"bold\">%s</span>", _("Sending"));
	gtk_label_set_markup ((GtkLabel *)label, buff);
	g_free (buff);
	gtk_widget_show (label);
	PACK_BOXF(label);
	gtk_table_attach ((GtkTable *)table, box, 2, 3, 3, 4, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	entry = gtk_label_new (url->protocol);
	gtk_widget_show(entry);
	PACK_BOX(entry)
	gtk_table_attach ((GtkTable *)table, box, 2, 3, 4, 5, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	entry = gtk_label_new (url->host);
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 2, 3, 5, 6, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	entry = gtk_label_new (url->user);
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 2, 3, 6, 7, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

	enc = (gchar *)camel_url_get_param(url, "use_ssl");
	entry = gtk_label_new (enc ? enc : _("never"));
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 2, 3, 7, 8, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);

/*
	label = gtk_label_new (_("Organization:"));
	gtk_widget_show (label);
	entry = gtk_entry_new ();
	gtk_widget_show(entry);
	gtk_table_attach (table, label, 0, 1, 3, 4, GTK_SHRINK, GTK_SHRINK, INDENTATION, 0);
	gtk_table_attach (table, entry, 1, 2, 3, 4, GTK_EXPAND|GTK_FILL, GTK_SHRINK, INDENTATION, 0);
	*/

	gtk_widget_show(table);

	return table;
}

#define IDENTITY_DETAIL N_("To use the email application you'll need to setup an account. Put your email address and password in below and we'll try and work out all the settings. If we can't do it automatically you'll need your server details as well.")

#define RECEIVE_DETAIL N_("Sorry, we can't work out the settings to get your mail automatically. Please enter them below. We've tried to make a start with the details you just entered but you may need to change them.")

#define RECEIVE_OPT_DETAIL N_("You can specify more options to configure the account.")

#define SEND_DETAIL N_("Now we need your settings for sending mail. We've tried to make some guesses but you should check them over to make sure.")
#define DEFAULTS_DETAIL N_("You can specify your default settings for your account.")
#define REVIEW_DETAIL N_("Time to check things over before we try and connect to the server and fetch your mail.")
struct _page_text {
	gint id;
	const gchar *head;
	const gchar *next;
	const gchar *prev;
	const gchar *next_edit;
	const gchar *prev_edit;
	const gchar *detail;
	const gchar *path;
	GtkWidget * (*create_page) (MailAccountView *view);
	void (*fill_page) (MailAccountView *view);
	void (*save_page) (MailAccountView *view);
	gint (*validate_page) (MailAccountView *view);
} mail_account_pages[] = {
	{ MAV_IDENTITY_PAGE, N_("Identity"), N_("Next - Receiving mail"), NULL, N_("Next - Receiving mail"), NULL, IDENTITY_DETAIL, "00.identity",NULL, NULL, save_identity, validate_identity},
	{ MAV_RECV_PAGE, N_("Receiving mail"), N_("Next - Sending mail"), N_("Back - Identity"), N_("Next - Receiving options"), N_("Back - Identity"), RECEIVE_DETAIL, "10.receive", NULL, NULL, NULL, NULL },
	{ MAV_RECV_OPT_PAGE, N_("Receiving options"),  NULL, NULL, N_("Next - Sending mail"), N_("Back - Receiving mail"), RECEIVE_OPT_DETAIL, "10.receive", NULL, NULL, NULL, NULL },

	{ MAV_SEND_PAGE, N_("Sending mail"), N_("Next - Review account"), N_("Back - Receiving mail"), N_("Next - Defaults"), N_("Back - Receiving options"), SEND_DETAIL, "30.send", NULL, NULL, NULL, NULL},
	{ MAV_DEFAULTS_PAGE, N_("Defaults"), NULL, NULL, N_("Next - Review account"), N_("Back - Sending mail"), DEFAULTS_DETAIL, "40.defaults", NULL, NULL, NULL, NULL},

	{ MAV_REVIEW_PAGE, N_("Review account"), N_("Finish"), N_("Back - Sending"), N_("Finish"), N_("Back - Sending"), REVIEW_DETAIL, NULL, create_review, NULL, NULL},
};

static void
mav_next_pressed (GtkButton *button, MailAccountView *mav)
{
	if (mail_account_pages[mav->current_page].validate_page) {
		gint ret = (*mail_account_pages[mav->current_page].validate_page) (mav);
		MAVPage *page = mav->pages[mav->current_page];
		if (ret) {
			gtk_label_set_text ((GtkLabel *)page->error_label, _(dialog_errors[ret-1].detail));
			gtk_widget_show  (page->error);
			return;
		}
		gtk_widget_hide (page->error);
		gtk_label_set_text ((GtkLabel *)page->error_label, "");
	}
	if (mail_account_pages[mav->current_page].save_page) {
		(*mail_account_pages[mav->current_page].save_page) (mav);
	}

	if (mav->current_page == MAV_LAST - 1) {
		gchar *uri = (gchar *)e_account_get_string(em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_SOURCE_URL);
		CamelURL *url;

		e_account_set_string (em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_NAME, e_account_get_string(em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_ID_ADDRESS));
		if (uri != NULL && (url = camel_url_new(uri, NULL)) != NULL) {
			camel_url_set_param(url, "check_all", "1");
			camel_url_set_param(url, "sync_offline", "1");
			if (!mav->original) {
				e_account_set_bool(em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_SOURCE_AUTO_CHECK, TRUE);
			}

			if (!mav->original && strcmp(url->protocol, "pop") == 0) {
				e_account_set_bool (em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_SOURCE_KEEP_ON_SERVER, TRUE);
			}

			uri = camel_url_to_string(url, 0);
			e_account_set_string(em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_SOURCE_URL, uri);
			g_free(uri);
			camel_url_free(url);
		}

		if (!mav->original) {
			EAccount *account = em_account_editor_get_modified_account(mav->edit);
			CamelURL *aurl;
			gchar *surl;
			/* Save the password ahead of time */
			aurl = camel_url_new (account->source->url, NULL);
			surl = camel_url_to_string(aurl, CAMEL_URL_HIDE_ALL);
			e_passwords_add_password (surl, gtk_entry_get_text((GtkEntry *)mav->password));
			e_passwords_remember_password ("Mail", surl);
			camel_url_free(aurl);
			g_free(surl);
		}

		if (mav->priv->is_gmail && !mav->original)
			setup_google_accounts (mav);
		else if (mav->priv->is_yahoo && !mav->original)
			setup_yahoo_account (mav);

		em_account_editor_commit (mav->edit);
		g_signal_emit (mav, signals[VIEW_CLOSE], 0);
		return;
	}

	gtk_widget_hide (mav->pages[mav->current_page]->box);
	mav->current_page++;
	if (mav->current_page == MAV_RECV_OPT_PAGE && mav->original == NULL)
		mav->current_page++; /* Skip recv options in new account creation. */
	if (mav->current_page == MAV_DEFAULTS_PAGE && mav->original == NULL)
		mav->current_page++; /* Skip defaults in new account creation. */

	if (mav->current_page == MAV_LAST - 1) {
		MAVPage *page = mav->pages[mav->current_page];
		GtkWidget *tmp;
		EAccount *account = em_account_editor_get_modified_account(mav->edit);

		if (page->main)
			gtk_widget_destroy (page->main);

		tmp = mail_account_pages[mav->current_page].create_page(mav);
		page->main = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (page->main);
		gtk_box_pack_start((GtkBox *)page->main, tmp, FALSE, FALSE, 0);
		gtk_widget_show(tmp);
		gtk_box_pack_start((GtkBox *)page->box, page->main, FALSE, FALSE, 3);

		if (mav->priv->is_gmail) {
			gtk_widget_destroy (mav->priv->gcontacts);
			gtk_widget_destroy (mav->priv->calendar);
			gtk_widget_destroy (mav->priv->gmail_info_label);
		} else if (mav->priv->is_yahoo) {
			gtk_widget_destroy (mav->priv->calendar);
			gtk_widget_destroy (mav->priv->gmail_info_label);
			gtk_widget_destroy (mav->priv->yahoo_cal_entry);
		}

		if (mav->original == NULL && (g_strrstr(account->source->url, "gmail") ||
				g_strrstr(account->source->url, "googlemail"))) {
			/* Google accounts*/
			GtkWidget *tmp;
			gchar *buff;
			mav->priv->is_gmail = TRUE;

			mav->priv->gcontacts = gtk_check_button_new_with_label (_("Setup Google contacts with Evolution"));
			mav->priv->calendar = gtk_check_button_new_with_label (_("Setup Google calendar with Evolution"));

			gtk_toggle_button_set_active ((GtkToggleButton *)mav->priv->gcontacts, TRUE);
			gtk_toggle_button_set_active ((GtkToggleButton *)mav->priv->calendar, TRUE);

			mav->priv->gmail_info_label = gtk_label_new (_("You may need to enable IMAP access."));
			gtk_label_set_selectable ((GtkLabel *)mav->priv->gmail_info_label, TRUE);

			gtk_widget_show (mav->priv->gcontacts);
			gtk_widget_show (mav->priv->calendar);
			gtk_widget_show (mav->priv->gmail_info_label);

			tmp = gtk_label_new (NULL);
			buff = g_markup_printf_escaped ("<span size=\"large\" weight=\"bold\">%s</span>", _("Google account settings:"));
			gtk_label_set_markup ((GtkLabel *)tmp, buff);
			g_free (buff);
			gtk_widget_show(tmp);

#define PACK_IN_BOX(wid,child,num) { GtkWidget *tbox; tbox = gtk_hbox_new (FALSE, 0); gtk_box_pack_start ((GtkBox *)tbox, child, FALSE, FALSE, num); gtk_widget_show (tbox); gtk_box_pack_start ((GtkBox *)wid, tbox, FALSE, FALSE, 0); }

			PACK_IN_BOX(page->box,tmp,12);
			PACK_IN_BOX(page->box,mav->priv->gcontacts,24);
			PACK_IN_BOX(page->box,mav->priv->calendar,24);
#undef PACK_IN_BOX
#define PACK_IN_BOX(wid,child1,child2,num1,num2) { GtkWidget *tbox; tbox = gtk_hbox_new (FALSE, 0); gtk_box_pack_start ((GtkBox *)tbox, child1, FALSE, FALSE, num1); gtk_box_pack_start ((GtkBox *)tbox, child2, FALSE, FALSE, num2); gtk_widget_show_all (tbox); gtk_box_pack_start ((GtkBox *)wid, tbox, FALSE, FALSE, 0); }

			PACK_IN_BOX(page->box,mav->priv->gmail_info_label,gtk_link_button_new("https://mail.google.com/mail/?ui=2&amp;shva=1#settings/fwdandpop"), 24, 0);
#undef PACK_IN_BOX
		} else if (mav->original == NULL &&
				(g_strrstr(account->source->url, "yahoo.") ||
				 g_strrstr(account->source->url, "ymail.") ||
				 g_strrstr(account->source->url, "rocketmail."))) {
			/* Yahoo accounts*/
			GtkWidget *tmp;
			gchar *cal_name;
			GtkWidget *tmpbox;
			gchar *buff;

			mav->priv->is_yahoo = TRUE;
			printf("Google account: %s\n", account->source->url);
			mav->priv->calendar = gtk_check_button_new_with_label (_("Setup Yahoo calendar with Evolution"));

			gtk_toggle_button_set_active ((GtkToggleButton *)mav->priv->calendar, TRUE);

			mav->priv->gmail_info_label = gtk_label_new (_("Yahoo calendars are named as firstname_lastname. We have tried to form the calendar name. So please confirm and re-enter the calendar name if it is not correct."));
			gtk_label_set_selectable ((GtkLabel *)mav->priv->gmail_info_label, TRUE);

			gtk_widget_show (mav->priv->calendar);
			gtk_widget_show (mav->priv->gmail_info_label);

			tmp = gtk_label_new (NULL);
			buff = g_markup_printf_escaped ("<span size=\"large\" weight=\"bold\">%s</span>", _("Yahoo account settings:"));
			gtk_label_set_markup ((GtkLabel *)tmp, buff);
			g_free (buff);
			gtk_widget_show(tmp);

#define PACK_IN_BOX(wid,child,num) { GtkWidget *tbox; tbox = gtk_hbox_new (FALSE, 0); gtk_box_pack_start ((GtkBox *)tbox, child, FALSE, FALSE, num); gtk_widget_show (tbox); gtk_box_pack_start ((GtkBox *)wid, tbox, FALSE, FALSE, 0); }
#define PACK_IN_BOX_AND_TEXT(txt, child,num) { GtkWidget *txtlbl = gtk_label_new (txt); tmpbox = gtk_hbox_new (FALSE, 12); gtk_box_pack_start ((GtkBox *)tmpbox, txtlbl, FALSE, FALSE, num); gtk_box_pack_start ((GtkBox *)tmpbox, child, FALSE, FALSE, num); gtk_widget_show_all (tmpbox);}

			PACK_IN_BOX(page->box,tmp,12);
			PACK_IN_BOX(page->box,mav->priv->calendar,24);

			mav->priv->yahoo_cal_entry = gtk_entry_new ();
			gtk_widget_show (mav->priv->yahoo_cal_entry);
			PACK_IN_BOX(page->box,mav->priv->gmail_info_label, 24);
			PACK_IN_BOX_AND_TEXT(_("Yahoo Calendar name:"), mav->priv->yahoo_cal_entry, 0);
			PACK_IN_BOX(page->box, tmpbox, 24);
			cal_name = g_strdup(e_account_get_string(em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_ID_NAME));
			cal_name = g_strdelimit(cal_name, " ", '_');
			gtk_entry_set_text ((GtkEntry *)mav->priv->yahoo_cal_entry, cal_name);
			g_free (cal_name);
#undef PACK_IN_BOX
		} else {
			mav->priv->is_gmail = FALSE;
			mav->priv->is_yahoo = FALSE;
		}

	}

	gtk_widget_show (mav->pages[mav->current_page]->box);
	if (!mav->pages[mav->current_page]->done) {
		mav->pages[mav->current_page]->done = TRUE;
		if (mail_account_pages[mav->current_page].path) {

			if (!mav->original && em_account_editor_check(mav->edit, mail_account_pages[mav->current_page].path))
				mav_next_pressed (NULL, mav);
		}
	}
}

static void
mav_prev_pressed (GtkButton *button, MailAccountView *mav)
{
	if (mav->current_page == 0)
		return;

	gtk_widget_hide (mav->pages[mav->current_page]->box);
	mav->current_page--;
	if (mav->current_page == MAV_RECV_OPT_PAGE && mav->original == NULL)
		mav->current_page--; /* Skip recv options in new account creation. */
	if (mav->current_page == MAV_DEFAULTS_PAGE && mav->original == NULL)
		mav->current_page--; /* Skip defaults in new account creation. */
	gtk_widget_show (mav->pages[mav->current_page]->box);

}

static GtkWidget *
mav_construct_page(MailAccountView *view, MAVPageType type)
{
	MAVPage *page = g_new0(MAVPage, 1);
	GtkWidget *box, *tmp, *error_box;
	gchar *str;

	page->type = type;

	page->box = gtk_vbox_new (FALSE, 2);

	error_box = gtk_hbox_new (FALSE, 2);
	page->error_label = gtk_label_new ("");
	tmp = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start ((GtkBox *)error_box, tmp, FALSE, FALSE, 2);
	gtk_box_pack_start ((GtkBox *)error_box, page->error_label, FALSE, FALSE, 2);
	gtk_widget_hide (tmp);
	gtk_widget_show (page->error_label);
	page->error = tmp;
	gtk_widget_show (error_box);

	box = gtk_hbox_new (FALSE, 12);
	gtk_widget_show(box);
	gtk_box_pack_start((GtkBox *)page->box, box, FALSE, FALSE, 12);
	tmp = gtk_label_new (NULL);
	str = g_strdup_printf("<span  size=\"xx-large\" weight=\"heavy\">%s</span>", _(mail_account_pages[type].head));
	gtk_label_set_markup ((GtkLabel *)tmp, str);
	g_free(str);
	gtk_widget_show (tmp);
	gtk_box_pack_start((GtkBox *)box, tmp, FALSE, FALSE, 12);

	box = gtk_hbox_new (FALSE, 12);
	gtk_widget_show(box);
	gtk_box_pack_start((GtkBox *)page->box, box, FALSE, FALSE, 12);
	tmp = gtk_label_new (_(mail_account_pages[type].detail));
	gtk_widget_set_size_request (tmp, 600, -1);
	gtk_label_set_line_wrap ((GtkLabel *)tmp, TRUE);
	gtk_label_set_line_wrap_mode ((GtkLabel *)tmp, PANGO_WRAP_WORD);
	gtk_widget_show(tmp);
	gtk_box_pack_start((GtkBox *)box, tmp, FALSE, FALSE, 12);

	page->main = NULL;
	if (mail_account_pages[type].create_page && mail_account_pages[type].path) {
		tmp = (*mail_account_pages[type].create_page) (view);
		gtk_box_pack_start ((GtkBox *)page->box, tmp, FALSE, FALSE, 3);
		page->main = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (page->main);
		gtk_box_pack_start((GtkBox *)page->main, tmp, FALSE, FALSE, 0);
	}

	if (mail_account_pages[type].fill_page) {
		(*mail_account_pages[type].fill_page) (view);
	}

	if ((view->original && mail_account_pages[type].prev_edit) || mail_account_pages[type].prev) {
		box = gtk_hbox_new(FALSE, 0);
		if (FALSE) {
			tmp = gtk_image_new_from_icon_name ("go-previous", GTK_ICON_SIZE_BUTTON);
			gtk_box_pack_start((GtkBox *)box, tmp, FALSE, FALSE, 0);
		}
		tmp = gtk_label_new (_(view->original ? mail_account_pages[type].prev_edit : mail_account_pages[type].prev));
		gtk_box_pack_start((GtkBox *)box, tmp, FALSE, FALSE, 3);
		page->prev = gtk_button_new ();
		gtk_container_add ((GtkContainer *)page->prev, box);
		gtk_widget_show_all(page->prev);
		g_signal_connect(page->prev, "clicked", G_CALLBACK(mav_prev_pressed), view);
	}

	if ((view->original && mail_account_pages[type].next_edit) || mail_account_pages[type].next) {
		box = gtk_hbox_new(FALSE, 0);
		tmp = gtk_label_new (_(view->original ? mail_account_pages[type].next_edit : mail_account_pages[type].next));
		gtk_box_pack_start((GtkBox *)box, tmp, FALSE, FALSE, 3);
		if (FALSE) {
			tmp = gtk_image_new_from_icon_name ("go-next", GTK_ICON_SIZE_BUTTON);
			gtk_box_pack_start((GtkBox *)box, tmp, FALSE, FALSE, 0);
		}
		page->next = gtk_button_new ();
		gtk_widget_set_can_default (page->next, TRUE);
		g_signal_connect (page->next, "hierarchy-changed",
				  G_CALLBACK (gtk_widget_grab_default), NULL);
		gtk_container_add ((GtkContainer *)page->next, box);
		gtk_widget_show_all(page->next);
		g_signal_connect(page->next, "clicked", G_CALLBACK(mav_next_pressed), view);
	}

	box = gtk_hbox_new (FALSE, 0);
	if (page->prev)
		gtk_box_pack_start ((GtkBox *)box, page->prev, FALSE, FALSE, 12);
	if (page->next)
		gtk_box_pack_end ((GtkBox *)box, page->next, FALSE, FALSE, 12);
	gtk_widget_show (box);
	gtk_box_pack_end ((GtkBox *)page->box, box, FALSE, FALSE, 6);
	gtk_widget_show(page->box);
	gtk_box_pack_end ((GtkBox *)page->box, error_box, FALSE, FALSE, 2);
	return (GtkWidget *)page;
}

static ServerData *
emae_check_servers (const gchar *email)
{
	ServerData *sdata = g_new0(ServerData, 1);
	EmailProvider *provider = g_new0(EmailProvider, 1);
	gchar *dupe = g_strdup(email);
	gchar *tmp;

	/* FIXME: Find a way to free the provider once given to account settings. */
	provider->email = (gchar *)email;
	tmp = strchr(email, '@');
	tmp++;
	provider->domain = tmp;
	tmp = strchr(dupe, '@');
	*tmp = 0;
	provider->username = (gchar *)g_quark_to_string(g_quark_from_string(dupe));
	g_free(dupe);

	if (!mail_guess_servers (provider)) {
		g_free (provider);
		g_free (sdata);
		return NULL;
	}
	/*printf("Recv: %s\n%s(%s), %s by %s \n Send: %s\n%s(%s), %s by %s\n via %s to %s\n",
	  provider->recv_type, provider->recv_hostname, provider->recv_port, provider->recv_username, provider->recv_auth,
	  provider->send_type, provider->send_hostname, provider->send_port, provider->send_username, provider->send_auth,
	  provider->recv_socket_type, provider->send_socket_type); */

	sdata->recv = provider->recv_hostname;
	sdata->recv_port = provider->recv_port;
	sdata->send = provider->send_hostname;
	sdata->send_port = provider->send_port;
	if (strcmp(provider->recv_type, "pop3") == 0)
		sdata->proto = g_strdup("pop");
	else if (strcmp(provider->recv_type, "imap") == 0)
		sdata->proto = g_strdup("imapx");
	else
		sdata->proto = provider->recv_type;
	if (provider->recv_socket_type) {
		if (g_ascii_strcasecmp(provider->recv_socket_type, "SSL") == 0) {
			sdata->ssl = g_strdup("always");
			sdata->recv_sock = g_strdup("always");
		}
		else if (g_ascii_strcasecmp(provider->recv_socket_type, "secure") == 0) {
			sdata->ssl = g_strdup("always");
			sdata->recv_sock = g_strdup("always");
		}
		else if (g_ascii_strcasecmp(provider->recv_socket_type, "STARTTLS") == 0) {
			sdata->ssl = g_strdup("when-possible");
			sdata->recv_sock = g_strdup("when-possible");
		}
		else if (g_ascii_strcasecmp(provider->recv_socket_type, "TLS") == 0) {
			sdata->ssl = g_strdup("when-possible");
			sdata->recv_sock = g_strdup("when-possible");
		}
		else {
			sdata->ssl = g_strdup("never");
			sdata->recv_sock = g_strdup("never");
		}

	}

	if (provider->send_socket_type) {
		if (g_ascii_strcasecmp(provider->send_socket_type, "SSL") == 0)
			sdata->send_sock = g_strdup("always");
		else if (g_ascii_strcasecmp(provider->send_socket_type, "secure") == 0)
			sdata->send_sock = g_strdup("always");
		else if (g_ascii_strcasecmp(provider->send_socket_type, "STARTTLS") == 0)
			sdata->send_sock = g_strdup("when-possible");
		else if (g_ascii_strcasecmp(provider->send_socket_type, "TLS") == 0)
			sdata->send_sock = g_strdup("when-possible");
		else
			sdata->send_sock = g_strdup("never");
	}

	sdata->send_auth = provider->send_auth;
	sdata->recv_auth = provider->recv_auth;
	sdata->send_user = provider->send_username;
	sdata->recv_user = provider->recv_username;

	g_free (provider);

	return sdata;
}

static void
next_page (GtkWidget *entry, MailAccountView *mav)
{
	mav_next_pressed (NULL, mav);
}

static void
mail_account_view_construct (MailAccountView *view)
{
	gint i;
	EShell *shell;

	view->scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy ((GtkScrolledWindow *)view->scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *)view->scroll, GTK_SHADOW_NONE);
	view->page_widget = gtk_vbox_new (FALSE, 3);
	gtk_scrolled_window_add_with_viewport ((GtkScrolledWindow *)view->scroll, view->page_widget);
	gtk_widget_show_all (view->scroll);
	gtk_widget_set_size_request ((GtkWidget *)view, -1, 300);
	for (i=0; i<MAV_LAST; i++) {
		view->pages[i] = (MAVPage *)mav_construct_page (view, i);
		view->pages[i]->done = FALSE;
		view->wpages[i] = view->pages[i]->box;
		gtk_box_pack_start ((GtkBox *)view->page_widget, view->pages[i]->box, TRUE, TRUE, 0);
		gtk_widget_hide (view->pages[i]->box);
	}
	gtk_widget_show (view->pages[0]->box);
	view->current_page = 0;
	gtk_box_pack_start ((GtkBox *)view, view->scroll, TRUE, TRUE, 0);
	view->edit = em_account_editor_new_for_pages (view->original, EMAE_PAGES, "org.gnome.evolution.mail.config.accountWizard", view->wpages);
	gtk_widget_hide (e_config_create_widget (E_CONFIG (view->edit->config)));
	view->edit->emae_check_servers = emae_check_servers;
	if (!view->original) {
		e_account_set_bool (em_account_editor_get_modified_account(view->edit), E_ACCOUNT_SOURCE_SAVE_PASSWD, TRUE);
		e_account_set_bool (em_account_editor_get_modified_account(view->edit), E_ACCOUNT_TRANSPORT_SAVE_PASSWD, TRUE);
	}
	em_account_editor_check (view->edit, mail_account_pages[0].path);
	view->pages[0]->done = TRUE;

	shell = e_shell_get_default ();
	if (!shell || e_shell_get_express_mode (shell)) {
		GtkWidget *table = em_account_editor_get_widget (view->edit, "identity_required_table");
		GtkWidget *label, *pwd;
		gtk_widget_hide (em_account_editor_get_widget (view->edit, "identity_optional_frame"));

		if (!view->original) {
			label = gtk_label_new (_("Password:"));
			pwd = gtk_entry_new ();
			gtk_entry_set_visibility ((GtkEntry *)pwd, FALSE);
/*			gtk_entry_set_activates_default ((GtkEntry *)pwd, TRUE); */
			g_signal_connect (pwd, "activate", G_CALLBACK (next_page), view);
			gtk_widget_show(label);
			gtk_widget_show(pwd);
			gtk_table_attach ((GtkTable *)table, label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
			gtk_table_attach ((GtkTable *)table, pwd, 1, 2, 2, 3, GTK_FILL|GTK_EXPAND, 0, 0, 0);

			view->password = pwd;
		}
	}

	/* assume the full name is known from the system */
	gtk_widget_grab_focus (em_account_editor_get_widget (view->edit, "identity_address"));
}

MailAccountView *
mail_account_view_new (EAccount *account)
{
	MailAccountView *view = g_object_new (MAIL_ACCOUNT_VIEW_TYPE, NULL);
	view->type = MAIL_VIEW_ACCOUNT;
	view->uri = "account://";
	view->original = account;
	mail_account_view_construct (view);

	return view;
}

static gboolean
mav_btn_expose (GtkWidget *w, GdkEventExpose *event, MailAccountView *mfv)
{
	GdkPixbuf *img = g_object_get_data ((GObject *)w, "pbuf");
	cairo_t *cr;

	cr = gdk_cairo_create (gtk_widget_get_window (w));
	cairo_save (cr);
	gdk_cairo_set_source_pixbuf (cr, img, event->area.x-5, event->area.y-4);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_destroy (cr);

	return TRUE;
}

static void
mav_close (GtkButton *w, MailAccountView *mfv)
{
	g_signal_emit (mfv, signals[VIEW_CLOSE], 0);
}

GtkWidget *
mail_account_view_get_tab_widget (MailAccountView *mcv)
{
	GdkPixbuf *pbuf = gtk_widget_render_icon ((GtkWidget *)mcv, "gtk-close", GTK_ICON_SIZE_MENU, NULL);

	GtkWidget *tool, *box, *img;
	gint w=-1, h=-1;
	GtkWidget *tab_label;

	img = (GtkWidget *)gtk_image_new_from_pixbuf (pbuf);
	g_object_set_data ((GObject *)img, "pbuf", pbuf);
	g_signal_connect (img, "expose-event", G_CALLBACK(mav_btn_expose), mcv);

	tool = gtk_button_new ();
	gtk_button_set_relief((GtkButton *)tool, GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click ((GtkButton *)tool, FALSE);
	gtk_widget_set_tooltip_text (tool, _("Close Tab"));
	g_signal_connect (tool, "clicked", G_CALLBACK(mav_close), mcv);

	box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)box, img, FALSE, FALSE, 0);
	gtk_container_add ((GtkContainer *)tool, box);
	gtk_widget_show_all (tool);
	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings(tool) , GTK_ICON_SIZE_MENU, &w, &h);
	gtk_widget_set_size_request (tool, w+2, h+2);

	box = gtk_label_new (_("Account Wizard"));
	tab_label = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)tab_label, box, FALSE, FALSE, 0);
	gtk_box_pack_start ((GtkBox *)tab_label, tool, FALSE, FALSE, 0);
	gtk_widget_show_all (tab_label);

	return tab_label;

}

void
mail_account_view_activate (MailAccountView *mcv, GtkWidget *tree, GtkWidget *folder_tree, GtkWidget *check_mail, GtkWidget *sort_by, gboolean act)
{
	 if (!check_mail || !sort_by)
		  return;
	 //gtk_widget_hide (folder_tree);
	 gtk_widget_set_sensitive (check_mail, TRUE);
	 gtk_widget_set_sensitive (sort_by, FALSE);
}
