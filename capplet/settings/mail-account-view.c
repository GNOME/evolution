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
#include <libedataserver/e-account-list.h>
#include "mail-view.h"
#include "e-util/e-config.h"
#include "mail/mail-config.h"
#include "mail-guess-servers.h"

struct _MailAccountViewPrivate {
	GtkWidget *tab_str;
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
};

struct _dialog_errors {
	int error;
	char *detail;
} dialog_errors[] = {
	{ ERROR_NO_FULLNAME, N_("Please enter your full name.") },
	{ ERROR_NO_EMAIL, N_("Please enter your email address.") },
	{ ERROR_INVALID_EMAIL, N_("The email addres you have entered is invalid.") },
};
static guint signals[LAST_SIGNAL] = { 0 };

static void
mail_account_view_init (MailAccountView  *shell)
{
	shell->priv = g_new0(MailAccountViewPrivate, 1);

}

static void
mail_account_view_finalize (GObject *object)
{
	/*MailAccountView *shell = (MailAccountView *)object;*/

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
	char *key;
	char *recv;
	char *send;
	char *proto;
	char *ssl;
} std_server [] = {
	{"gmail", "imap.gmail.com", "smtp.gmail.com", "imap", "always"},
	{"yahoo", "pop3.yahoo.com", "smtp.yahoo.com", "pop", "never"},
	{"aol", "imap.aol.com", "smtp.aol.com", "pop", "never"},
	{"msn", "pop3.email.msn.com", "smtp.email.msn.com", "pop", "never"}
};
static int
check_servers (char *server)
{
	int len = G_N_ELEMENTS(std_server), i;

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
#if 0	
	if (!view->original) {
		char *tmp = e_account_get_string(view->edit->account, E_ACCOUNT_ID_ADDRESS);
		char **token;
		int index; 

		if (tmp && *tmp) {
			token = g_strsplit (tmp, "@", 2);
			index = check_servers(token[1]);
	
			if (index != -1) {
				char *uri = e_account_get_string(view->edit->account, E_ACCOUNT_SOURCE_URL);
				CamelURL *url;
				if (uri == NULL || (url = camel_url_new(uri, NULL)) == NULL)
					return;
				
				if (strcmp(url->protocol, std_server[index].proto)) {
					camel_url_set_protocol (url, std_server[index].proto);
					g_datalist_clear (&url->params);
				}
				camel_url_set_param(url, "use_ssl", std_server[index].ssl);
				camel_url_set_host (url, std_server[index].recv);
				camel_url_set_user (url, token[0]);
				uri = camel_url_to_string(url, 0);
				e_account_set_string(view->edit->account, E_ACCOUNT_SOURCE_URL, uri);
				g_free(uri);

				uri = e_account_get_string(view->edit->account, E_ACCOUNT_TRANSPORT_URL);
				if (uri == NULL || (url = camel_url_new(uri, NULL)) == NULL)
					return;
				
				camel_url_set_protocol (url, "smtp");
				camel_url_set_param(url, "use_ssl", std_server[index].ssl);
				camel_url_set_host (url, std_server[index].recv);
				camel_url_set_user (url, token[0]);
				uri = camel_url_to_string(url, 0);
				e_account_set_string(view->edit->account, E_ACCOUNT_TRANSPORT_URL, uri);
				g_free(uri);
			}
			g_strfreev(token);
		}
	}
#endif
}

static int
validate_identity (MailAccountView *view)
{
	char *user = (char *)e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_ID_NAME);
	char *email = (char *)e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_ID_ADDRESS);
	char *tmp;

	if (!user || !*user)
		return ERROR_NO_FULLNAME;
	if (!email || !*email)	
		return ERROR_NO_EMAIL;
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

static GtkWidget *
create_review (MailAccountView *view)
{
	GtkWidget *table, *box, *label, *entry;
	char *uri; 
	char *enc;
	CamelURL *url;

	uri = (char *)e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_SOURCE_URL);
	if (!uri  || (url = camel_url_new(uri, NULL)) == NULL)
		return NULL;

	table = gtk_table_new (4,2, FALSE);
	gtk_table_set_row_spacings ((GtkTable *)table, 4);

	label = gtk_label_new (NULL);
	gtk_label_set_markup ((GtkLabel *)label, _("<span size=\"large\" weight=\"bold\">Personal details:</span>"));
	gtk_widget_show (label);
	PACK_BOXF(label)
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 0, 1, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (_("Name:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 1, 2, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	entry = gtk_label_new(e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_ID_NAME));
	gtk_widget_show(entry);
	PACK_BOX(entry)
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 1, 2, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (_("Email address:"));
	gtk_widget_show (label);
	PACK_BOX(label)
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 2, 3, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	entry = gtk_label_new (e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_ID_ADDRESS));
	gtk_widget_show(entry);
	PACK_BOX(entry)
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 2, 3, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (NULL);
	gtk_label_set_markup ((GtkLabel *)label, _("<span size=\"large\" weight=\"bold\">Receiving details:</span>"));
	gtk_widget_show (label);
	PACK_BOXF(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 3, 4, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (_("Server type:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 4, 5, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	entry = gtk_label_new (url->protocol);
	gtk_widget_show(entry);
	PACK_BOX(entry)
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 4, 5, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (_("Server address:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 5, 6, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	entry = gtk_label_new (url->host);
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 5, 6, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);


	label = gtk_label_new (_("Username:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 6, 7, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	entry = gtk_label_new (url->user);
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 6, 7, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (_("Use encryption:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 7, 8, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	enc = (char *)camel_url_get_param(url, "use_ssl");
	entry = gtk_label_new (enc ? enc : _("never"));
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 7, 8, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);


	camel_url_free(url);
	uri =(char *) e_account_get_string(em_account_editor_get_modified_account(view->edit), E_ACCOUNT_TRANSPORT_URL);
	if (!uri  || (url = camel_url_new(uri, NULL)) == NULL)
		return NULL;

	label = gtk_label_new (NULL);
	gtk_label_set_markup ((GtkLabel *)label, _("<span size=\"large\" weight=\"bold\">Sending details:</span>"));
	gtk_widget_show (label);
	PACK_BOXF(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 8, 9, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (_("Server type:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 9, 10, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	entry = gtk_label_new (url->protocol);
	gtk_widget_show(entry);
	PACK_BOX(entry)
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 9, 10, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (_("Server address:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 10, 11, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	entry = gtk_label_new (url->host);
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 10, 11, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);


	label = gtk_label_new (_("Username:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 11, 12, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	entry = gtk_label_new (url->user);
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 11, 12, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);

	label = gtk_label_new (_("Use encryption:"));
	gtk_widget_show (label);
	PACK_BOX(label);
	gtk_table_attach ((GtkTable *)table, box, 0, 1, 12, 13, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	enc = (char *)camel_url_get_param(url, "use_ssl");
	entry = gtk_label_new (enc ? enc : _("never"));
	gtk_widget_show(entry);
	PACK_BOX(entry);
	gtk_table_attach ((GtkTable *)table, box, 1, 2, 12, 13, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	
/*
	label = gtk_label_new (_("Organization:"));
	gtk_widget_show (label);
	entry = gtk_entry_new ();
	gtk_widget_show(entry);
	gtk_table_attach (table, label, 0, 1, 3, 4, GTK_SHRINK, GTK_SHRINK, 10, 3);
	gtk_table_attach (table, entry, 1, 2, 3, 4, GTK_EXPAND|GTK_FILL, GTK_SHRINK, 10, 3);
	*/

	gtk_widget_show(table);	

	return table;
}

#define IDENTITY_DETAIL N_("To use the email application you'll need to setup an account. Put your email address and password in below and we'll try and work out all the settings. If we can't do it automatically you'll need your server details as well.")

#define RECEIVE_DETAIL N_("Sorry, we can't work out the settings to get your mail automatically. Please enter them below. We've tried  to make a start with the details you just entered but you may need to change them.")

#define RECEIVE_OPT_DETAIL N_("You can specify more options to configure the account.")

#define SEND_DETAIL N_("Now we need your settings for sending mail. We've tried to make some guesses but you should check them over to make sure.")
#define DEFAULTS_DETAIL N_("You can specify your default settings for your account.")
#define REVIEW_DETAIL N_("Time to check things over before we try and connect to the server and fetch your mail.")
struct _page_text {
	int id;
	char *head;
	char *next;
	char *prev;
	char *next_edit;
	char *prev_edit;
	char *detail;
	char *path;
	GtkWidget * (*create_page) (MailAccountView *view);
	void (*fill_page) (MailAccountView *view);
	void (*save_page) (MailAccountView *view);
	int (*validate_page) (MailAccountView *view);
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
		int ret = (*mail_account_pages[mav->current_page].validate_page) (mav);
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
		char *uri = (char *)e_account_get_string(em_account_editor_get_modified_account(mav->edit), E_ACCOUNT_SOURCE_URL);
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

		if (page->main)
			gtk_widget_destroy (page->main);

		tmp = mail_account_pages[mav->current_page].create_page(mav);
		page->main = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (page->main);
		gtk_box_pack_start((GtkBox *)page->main, tmp, FALSE, FALSE, 0);
		gtk_widget_show(tmp);
		gtk_box_pack_start((GtkBox *)page->box, page->main, FALSE, FALSE, 3);
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
	char *str;

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
	char *dupe = g_strdup(email);
	char *tmp;

	/* FIXME: Find a way to free the provider once given to account settings. */
	provider->email = (char *)email;
	tmp = strchr(email, '@');
	tmp++;
	provider->domain = tmp;
	tmp = strchr(dupe, '@');
	*tmp = 0;
	provider->username = (char *)g_quark_to_string(g_quark_from_string(dupe));
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
	else
		sdata->proto = provider->recv_type;
	if (provider->recv_socket_type) {
		if(g_ascii_strcasecmp(provider->recv_socket_type, "SSL") == 0)
			sdata->ssl = g_strdup("always");
		else if(g_ascii_strcasecmp(provider->recv_socket_type, "secure") == 0)
			sdata->ssl = g_strdup("always");
		else if(g_ascii_strcasecmp(provider->recv_socket_type, "STARTTLS") == 0)
			sdata->ssl = g_strdup("when-possible");		
		else if(g_ascii_strcasecmp(provider->recv_socket_type, "TLS") == 0)
			sdata->ssl = g_strdup("when-possible");
		else 
			sdata->ssl = g_strdup("never");

	}
	sdata->send_user = provider->send_username;
	sdata->recv_user = provider->recv_username;

	
	g_free (provider);

	return sdata;
}

void
mail_account_view_construct (MailAccountView *view)
{
	int i;
	
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
	view->edit->emae_check_servers = emae_check_servers;
	if (!view->original) {
		e_account_set_bool (em_account_editor_get_modified_account(view->edit), E_ACCOUNT_SOURCE_SAVE_PASSWD, TRUE);
		e_account_set_bool (em_account_editor_get_modified_account(view->edit), E_ACCOUNT_TRANSPORT_SAVE_PASSWD, TRUE);
	}
	em_account_editor_check (view->edit, mail_account_pages[0].path);
	view->pages[0]->done = TRUE;

	if (e_shell_get_express_mode (e_shell_get_default ()))
		gtk_widget_hide (em_account_editor_get_widget (view->edit, "identity_optional_frame"));
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
	cairo_t *cr = gdk_cairo_create (w->window);
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
	int w=-1, h=-1;
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
