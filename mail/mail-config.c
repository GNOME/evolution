/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *           Radek Doulik     <rodo@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include <string.h>
#include <ctype.h>

#include <glib.h>
#include <gtk/gtkdialog.h>
#include <gtkhtml/gtkhtml.h>
#include <glade/glade.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

#include <e-util/e-url.h>
#include <e-util/e-passwords.h>
#include <e-util/e-account-list.h>
#include <e-util/e-signature-list.h>

#include <camel/camel-service.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-stream-filter.h>

#include "mail-component.h"
#include "mail-session.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-tools.h"

/* Note, the first element of each MailConfigLabel must NOT be translated */
MailConfigLabel label_defaults[5] = {
	{ "important", N_("Important"), "#ff0000" },  /* red */
	{ "work",      N_("Work"),      "#ff8c00" },  /* orange */
	{ "personal",  N_("Personal"),  "#008b00" },  /* forest green */
	{ "todo",      N_("To Do"),     "#0000ff" },  /* blue */
	{ "later",     N_("Later"),     "#8b008b" }   /* magenta */
};

typedef struct {
	GConfClient *gconf;
	
	gboolean corrupt;
	
	EAccountList *accounts;
	ESignatureList *signatures;
	
	GSList *labels;
	guint label_notify_id;
	
	guint font_notify_id;
	guint spell_notify_id;

	GPtrArray *mime_types;
	guint mime_types_notify_id;
} MailConfig;

static MailConfig *config = NULL;
static guint config_write_timeout = 0;

#define MAIL_CONFIG_RC "/gtkrc-mail-fonts"
#define MAIL_CONFIG_RC_DIR ".evolution/mail/config"

void
mail_config_save_accounts (void)
{
	e_account_list_save (config->accounts);
}

void
mail_config_save_signatures (void)
{
	e_signature_list_save (config->signatures);
}


static void
config_clear_labels (void)
{
	MailConfigLabel *label;
	GSList *list, *n;
	
	list = config->labels;
	while (list != NULL) {
		label = list->data;
		g_free(label->tag);
		g_free (label->name);
		g_free (label->colour);
		g_free (label);
		
		n = list->next;
		g_slist_free_1 (list);
		list = n;
	}
	
	config->labels = NULL;
}

static void
config_cache_labels (void)
{
	GSList *labels, *list, *tail, *n;
	MailConfigLabel *label;
	char *buf, *colour;
	int num = 0;
	
	tail = labels = NULL;
	
	list = gconf_client_get_list (config->gconf, "/apps/evolution/mail/labels", GCONF_VALUE_STRING, NULL);
	
	while (list != NULL) {
		buf = list->data;
		
		if (num < 5 && (colour = strrchr (buf, ':'))) {
			label = g_new (MailConfigLabel, 1);
			
			*colour++ = '\0';
			label->tag = g_strdup(label_defaults[num].tag);
			label->name = g_strdup (buf);
			label->colour = g_strdup (colour);
			
			n = g_slist_alloc ();
			n->next = NULL;
			n->data = label;
			
			if (tail == NULL)
				labels = n;
			else
				tail->next = n;
			
			tail = n;
			
			num++;
		}
		
		g_free (buf);
		
		n = list->next;
		g_slist_free_1 (list);
		list = n;
	}
	
	while (num < 5) {
		/* complete the list with defaults */
		label = g_new (MailConfigLabel, 1);
		label->tag = g_strdup (label_defaults[num].tag);
		label->name = g_strdup (_(label_defaults[num].name));
		label->colour = g_strdup (label_defaults[num].colour);
		
		n = g_slist_alloc ();
		n->next = NULL;
		n->data = label;
		
		if (tail == NULL)
			labels = n;
		else
			tail->next = n;
		
		tail = n;
		
		num++;
	}
	
	config->labels = labels;
}

static void
config_clear_mime_types (void)
{
	int i;
	
	for (i = 0; i < config->mime_types->len; i++)
		g_free (config->mime_types->pdata[i]);
	
	g_ptr_array_set_size (config->mime_types, 0);
}

static void
config_cache_mime_types (void)
{
	GSList *n, *nn;
	
	n = gconf_client_get_list (config->gconf, "/apps/evolution/mail/display/mime_types", GCONF_VALUE_STRING, NULL);
	while (n != NULL) {
		nn = n->next;
		g_ptr_array_add (config->mime_types, n->data);
		g_slist_free_1 (n);
		n = nn;
	}
	
	g_ptr_array_add (config->mime_types, NULL);
}

#define CONFIG_GET_SPELL_VALUE(t,x,prop,f,c) G_STMT_START { \
        val = gconf_client_get_without_default (config->gconf, "/GNOME/Spell" x, NULL); \
        if (val) { f; prop = c (gconf_value_get_ ## t (val)); \
        gconf_value_free (val); } } G_STMT_END

static void
config_write_style (void)
{
	GConfValue *val;
	char *filename;
	FILE *rc;
	gboolean custom;
	char *fix_font;
	char *var_font;
	gint red = 0xffff, green = 0, blue = 0;
	
	/*
	 * This is the wrong way to get the path but it needs to 
	 * always be the same as the gtk_rc_parse call and evolution_dir 
	 * may not have been set yet
	 *
	 * filename = g_build_filename (evolution_dir, MAIL_CONFIG_RC, NULL);
	 *
	 * EPFIXME this kludge needs to go away.
	 */
	filename = g_build_filename (g_get_home_dir (), MAIL_CONFIG_RC_DIR, MAIL_CONFIG_RC, NULL);

	rc = fopen (filename, "w");

	if (!rc) {
		g_warning ("unable to open %s", filename);
		g_free (filename);
		return;
	}
	g_free (filename);

	custom = gconf_client_get_bool (config->gconf, "/apps/evolution/mail/display/fonts/use_custom", NULL);
	var_font = gconf_client_get_string (config->gconf, "/apps/evolution/mail/display/fonts/variable", NULL);
	fix_font = gconf_client_get_string (config->gconf, "/apps/evolution/mail/display/fonts/monospace", NULL);

 	CONFIG_GET_SPELL_VALUE (int, "/spell_error_color_red",   red, (void)0, (int));
 	CONFIG_GET_SPELL_VALUE (int, "/spell_error_color_green", green, (void)0, (int));
 	CONFIG_GET_SPELL_VALUE (int, "/spell_error_color_blue",  blue, (void)0, (int));

	fprintf (rc, "style \"evolution-mail-custom-fonts\" {\n");
	fprintf (rc, "        GtkHTML::spell_error_color = \"#%02x%02x%02x\"\n",
		 0xff & (red >> 8), 0xff & (green >> 8), 0xff & (blue >> 8));

	if (custom && var_font && fix_font) {
		fprintf (rc,
			 "        GtkHTML::fixed_font_name = \"%s\"\n"
			 "        font_name = \"%s\"\n",
			 fix_font, var_font); 
	}
	fprintf (rc, "}\n\n"); 
	
	fprintf (rc, "widget \"*.EMFolderView.*.GtkHTML\" style \"evolution-mail-custom-fonts\"\n");
	fprintf (rc, "widget \"*.EMFolderBrowser.*.GtkHTML\" style \"evolution-mail-custom-fonts\"\n");
	fprintf (rc, "widget \"*.EMMessageBrowser.*.GtkHTML\" style \"evolution-mail-custom-fonts\"\n");
	fprintf (rc, "widget \"*.BonoboPlug.*.GtkHTML\" style \"evolution-mail-custom-fonts\"\n");
	fprintf (rc, "widget \"*.EvolutionMailPrintHTMLWidget\" style \"evolution-mail-custom-fonts\"\n");

	if (fclose (rc) == 0)
		gtk_rc_reparse_all ();
}

static void
gconf_labels_changed (GConfClient *client, guint cnxn_id,
		      GConfEntry *entry, gpointer user_data)
{
	config_clear_labels ();
	config_cache_labels ();
}

static void
gconf_style_changed (GConfClient *client, guint cnxn_id,
		     GConfEntry *entry, gpointer user_data)
{
	config_write_style ();
}

static void
gconf_mime_types_changed (GConfClient *client, guint cnxn_id,
			  GConfEntry *entry, gpointer user_data)
{
	config_clear_mime_types ();
	config_cache_mime_types ();
}

/* Config struct routines */
void
mail_config_init (void)
{
	char *filename;
	
	if (config)
		return;
	
	config = g_new0 (MailConfig, 1);
	config->gconf = gconf_client_get_default ();
	config->mime_types = g_ptr_array_new ();
	
	mail_config_clear ();
	
	filename = g_build_filename (g_get_home_dir (), MAIL_CONFIG_RC_DIR, MAIL_CONFIG_RC, NULL);
	gtk_rc_parse (filename);
	g_free (filename);
	
	gconf_client_add_dir (config->gconf, "/apps/evolution/mail/display/fonts", 			      
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (config->gconf, "/GNOME/Spell", 			      
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	config->font_notify_id = gconf_client_notify_add (config->gconf, "/apps/evolution/mail/display/fonts",
							  gconf_style_changed, NULL, NULL, NULL);
	config->spell_notify_id = gconf_client_notify_add (config->gconf, "/GNOME/Spell",
							   gconf_style_changed, NULL, NULL, NULL);
	
	gconf_client_add_dir (config->gconf, "/apps/evolution/mail/labels",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	config->label_notify_id =
		gconf_client_notify_add (config->gconf, "/apps/evolution/mail/labels",
					 gconf_labels_changed, NULL, NULL, NULL);
	
	gconf_client_add_dir (config->gconf, "/apps/evolution/mail/mime_types",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	config->mime_types_notify_id =
		gconf_client_notify_add (config->gconf, "/apps/evolution/mail/mime_types",
					 gconf_mime_types_changed, NULL, NULL, NULL);
	
	config_cache_labels ();
	config_cache_mime_types ();
	
	config->accounts = e_account_list_new (config->gconf);
	config->signatures = e_signature_list_new (config->gconf);
}


void
mail_config_clear (void)
{
	if (!config)
		return;
	
	if (config->accounts) {
		g_object_unref (config->accounts);
		config->accounts = NULL;
	}
	
	if (config->signatures) {
		g_object_unref (config->signatures);
		config->signatures = NULL;
	}
	
	config_clear_labels ();
	config_clear_mime_types ();
}


void
mail_config_write (void)
{
	if (!config)
		return;
	
	e_account_list_save (config->accounts);
	e_signature_list_save (config->signatures);
	
	gconf_client_suggest_sync (config->gconf, NULL);
}

void
mail_config_write_on_exit (void)
{
	EAccount *account;
	EIterator *iter;
	
	if (config_write_timeout) {
		g_source_remove (config_write_timeout);
		config_write_timeout = 0;
		mail_config_write ();
	}
	
	/* Passwords */
	
	/* then we make sure the ones we want to remember are in the
           session cache */
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		char *passwd;
		
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->source->save_passwd && account->source->url) {
			passwd = mail_session_get_password (account->source->url);
			mail_session_forget_password (account->source->url);
			mail_session_add_password (account->source->url, passwd);
			g_free (passwd);
		}
		
		if (account->transport->save_passwd && account->transport->url) {
			passwd = mail_session_get_password (account->transport->url);
			mail_session_forget_password (account->transport->url);
			mail_session_add_password (account->transport->url, passwd);
			g_free (passwd);
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	/* then we clear out our component passwords */
	e_passwords_clear_component_passwords ("Mail");
	
	/* then we remember them */
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->source->save_passwd && account->source->url)
			mail_session_remember_password (account->source->url);
		
		if (account->transport->save_passwd && account->transport->url)
			mail_session_remember_password (account->transport->url);
		
		e_iterator_next (iter);
	}
	
	/* now do cleanup */
	mail_config_clear ();
	
	g_object_unref (config->gconf);
	g_ptr_array_free (config->mime_types, TRUE);
	
	g_free (config);
}

/* Accessor functions */
GConfClient *
mail_config_get_gconf_client (void)
{
	return config->gconf;
}

gboolean
mail_config_is_configured (void)
{
	return e_list_length ((EList *) config->accounts) > 0;
}

gboolean
mail_config_is_corrupt (void)
{
	return config->corrupt;
}

GSList *
mail_config_get_labels (void)
{
	return config->labels;
}

const char *
mail_config_get_label_color_by_name (const char *name)
{
	MailConfigLabel *label;
	GSList *node;
	
	node = config->labels;
	while (node != NULL) {
		label = node->data;
		if (!strcmp (label->tag, name))
			return label->colour;
		node = node->next;
	}
	
	return NULL;
}

const char *
mail_config_get_label_color_by_index (int index)
{
	MailConfigLabel *label;
	
	label = g_slist_nth_data (config->labels, index);
	
	if (label)
		return label->colour;
	
	return NULL;
}

const char **
mail_config_get_allowable_mime_types (void)
{
	return (const char **) config->mime_types->pdata;
}

gboolean
mail_config_find_account (EAccount *account)
{
	EAccount *acnt;
	EIterator *iter;
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		acnt = (EAccount *) e_iterator_get (iter);
		if (acnt == account) {
			g_object_unref (iter);
			return TRUE;
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	return FALSE;
}

EAccount *
mail_config_get_default_account (void)
{
	if (config == NULL)
		mail_config_init ();
	
	if (!config->accounts)
		return NULL;

	/* should probably return const */
	return (EAccount *)e_account_list_get_default(config->accounts);
}

EAccount *
mail_config_get_account_by_name (const char *account_name)
{
	return (EAccount *)e_account_list_find(config->accounts, E_ACCOUNT_FIND_NAME, account_name);
}

EAccount *
mail_config_get_account_by_uid (const char *uid)
{
	return (EAccount *) e_account_list_find (config->accounts, E_ACCOUNT_FIND_UID, uid);
}

EAccount *
mail_config_get_account_by_source_url (const char *source_url)
{
	CamelProvider *provider;
	EAccount *account;
	CamelURL *source;
	EIterator *iter;
	
	g_return_val_if_fail (source_url != NULL, NULL);
	
	provider = camel_provider_get(source_url, NULL);
	if (!provider)
		return NULL;
	
	source = camel_url_new (source_url, NULL);
	if (!source)
		return NULL;
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->source && account->source->url) {
			CamelURL *url;
			
			url = camel_url_new (account->source->url, NULL);
			if (url && provider->url_equal (url, source)) {
				camel_url_free (url);
				camel_url_free (source);
				g_object_unref (iter);
				
				return account;
			}
			
			if (url)
				camel_url_free (url);
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	camel_url_free (source);
	
	return NULL;
}

EAccount *
mail_config_get_account_by_transport_url (const char *transport_url)
{
	CamelProvider *provider;
	CamelURL *transport;
	EAccount *account;
	EIterator *iter;
	
	g_return_val_if_fail (transport_url != NULL, NULL);
	
	provider = camel_provider_get(transport_url, NULL);
	if (!provider)
		return NULL;
	
	transport = camel_url_new (transport_url, NULL);
	if (!transport)
		return NULL;
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->transport && account->transport->url) {
			CamelURL *url;
			
			url = camel_url_new (account->transport->url, NULL);
			if (url && provider->url_equal (url, transport)) {
				camel_url_free (url);
				camel_url_free (transport);
				g_object_unref (iter);
				
				return account;
			}
			
			if (url)
				camel_url_free (url);
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	camel_url_free (transport);
	
	return NULL;
}

EAccountList *
mail_config_get_accounts (void)
{
	g_assert (config != NULL);
	
	return config->accounts;
}

void
mail_config_add_account (EAccount *account)
{
	e_account_list_add(config->accounts, account);
	mail_config_save_accounts ();
}

void
mail_config_remove_account (EAccount *account)
{
	e_account_list_remove(config->accounts, account);
	mail_config_save_accounts ();
}

void
mail_config_set_default_account (EAccount *account)
{
	e_account_list_set_default(config->accounts, account);
}

EAccountIdentity *
mail_config_get_default_identity (void)
{
	EAccount *account;
	
	account = mail_config_get_default_account ();
	if (account)
		return account->id;
	else
		return NULL;
}

EAccountService *
mail_config_get_default_transport (void)
{
	EAccount *account;
	EIterator *iter;
	
	account = mail_config_get_default_account ();
	if (account && account->transport && account->transport->url)
		return account->transport;
	
	/* return the first account with a transport? */
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->transport && account->transport->url) {
			g_object_unref (iter);
			
			return account->transport;
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	return NULL;
}

static char *
uri_to_evname (const char *uri, const char *prefix)
{
	const char *base_directory = mail_component_peek_base_directory (mail_component_peek ());
	char *safe;
	char *tmp;
	
	safe = g_strdup (uri);
	e_filename_make_safe (safe);
	/* blah, easiest thing to do */
	if (prefix[0] == '*')
		tmp = g_strdup_printf ("%s/mail/%s%s.xml", base_directory, prefix + 1, safe);
	else
		tmp = g_strdup_printf ("%s/mail/%s%s", base_directory, prefix, safe);
	g_free (safe);
	return tmp;
}

void
mail_config_uri_renamed (GCompareFunc uri_cmp, const char *old, const char *new)
{
	EAccount *account;
	EIterator *iter;
	int i, work = 0;
	char *oldname, *newname;
	char *cachenames[] = { "config/hidestate-", 
			       "config/et-expanded-", 
			       "config/et-header-", 
			       "*views/current_view-",
			       "*views/custom_view-",
			       NULL };
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->sent_folder_uri && uri_cmp (account->sent_folder_uri, old)) {
			g_free (account->sent_folder_uri);
			account->sent_folder_uri = g_strdup (new);
			work = 1;
		}
		
		if (account->drafts_folder_uri && uri_cmp (account->drafts_folder_uri, old)) {
			g_free (account->drafts_folder_uri);
			account->drafts_folder_uri = g_strdup (new);
			work = 1;
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	/* ignore return values or if the files exist or
	 * not, doesn't matter */
	
	for (i = 0; cachenames[i]; i++) {
		oldname = uri_to_evname (old, cachenames[i]);
		newname = uri_to_evname (new, cachenames[i]);
		/*printf ("** renaming %s to %s\n", oldname, newname);*/
		rename (oldname, newname);
		g_free (oldname);
		g_free (newname);
	}
	
	/* nasty ... */
	if (work)
		mail_config_write ();
}

void
mail_config_uri_deleted (GCompareFunc uri_cmp, const char *uri)
{
	EAccount *account;
	EIterator *iter;
	int work = 0;
	/* assumes these can't be removed ... */
	const char *default_sent_folder_uri = mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT);
	const char *default_drafts_folder_uri = mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS);
	
	iter = e_list_get_iterator ((EList *) config->accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->sent_folder_uri && uri_cmp (account->sent_folder_uri, uri)) {
			g_free (account->sent_folder_uri);
			account->sent_folder_uri = g_strdup (default_sent_folder_uri);
			work = 1;
		}
		
		if (account->drafts_folder_uri && uri_cmp (account->drafts_folder_uri, uri)) {
			g_free (account->drafts_folder_uri);
			account->drafts_folder_uri = g_strdup (default_drafts_folder_uri);
			work = 1;
		}
		
		e_iterator_next (iter);
	}
	
	/* nasty again */
	if (work)
		mail_config_write ();
}

void 
mail_config_service_set_save_passwd (EAccountService *service, gboolean save_passwd)
{
	service->save_passwd = save_passwd;
}

char *
mail_config_folder_to_safe_url (CamelFolder *folder)
{
	char *url;
	
	url = mail_tools_folder_to_url (folder);
	e_filename_make_safe (url);
	
	return url;
}

char *
mail_config_folder_to_cachename (CamelFolder *folder, const char *prefix)
{
	char *url, *basename, *filename;
	const char *evolution_dir;
	
	evolution_dir = mail_component_peek_base_directory (mail_component_peek ());
	
	url = mail_config_folder_to_safe_url (folder);
	basename = g_strdup_printf ("%s%s", prefix, url);
	filename = g_build_filename (evolution_dir, "mail", "config", basename, NULL);
	g_free (basename);
	g_free (url);
	
	return filename;
}


/* Async service-checking/authtype-lookup code. */
struct _check_msg {
	struct _mail_msg msg;

	const char *url;
	CamelProviderType type;
	GList **authtypes;
	gboolean *success;
};

static char *
check_service_describe (struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Checking Service"));
}

static void
check_service_check (struct _mail_msg *mm)
{
	struct _check_msg *m = (struct _check_msg *)mm;
	CamelService *service = NULL;

	camel_operation_register(mm->cancel);

	service = camel_session_get_service (session, m->url, m->type, &mm->ex);
	if (!service) {
		camel_operation_unregister(mm->cancel);
		return;
	}

	if (m->authtypes)
		*m->authtypes = camel_service_query_auth_types (service, &mm->ex);
	else
		camel_service_connect (service, &mm->ex);

	camel_object_unref (service);
	*m->success = !camel_exception_is_set(&mm->ex);

	camel_operation_unregister(mm->cancel);
}

static struct _mail_msg_op check_service_op = {
	check_service_describe,
	check_service_check,
	NULL,
	NULL
};

static void
check_response (GtkDialog *dialog, int button, gpointer data)
{
	int *msg_id = data;

	mail_msg_cancel (*msg_id);
}

/**
 * mail_config_check_service:
 * @url: service url
 * @type: provider type
 * @authtypes: set to list of supported authtypes on return if non-%NULL.
 *
 * Checks the service for validity. If @authtypes is non-%NULL, it will
 * be filled in with a list of supported authtypes.
 *
 * Return value: %TRUE on success or %FALSE on error.
 **/
gboolean
mail_config_check_service (const char *url, CamelProviderType type, GList **authtypes, GtkWindow *window)
{
	static GtkWidget *dialog = NULL;
	gboolean ret = FALSE;
	struct _check_msg *m;
	GtkWidget *label;
	int id;
	
	if (dialog) {
		gdk_window_raise (dialog->window);
		*authtypes = NULL;
		return FALSE;
	}
	
	m = mail_msg_new (&check_service_op, NULL, sizeof(*m));
	m->url = url;
	m->type = type;
	m->authtypes = authtypes;
	m->success = &ret;
	
	id = m->msg.seq;
	e_thread_put(mail_thread_new, (EMsg *)m);

	dialog = gtk_dialog_new_with_buttons(_("Connecting to server..."), window,
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     NULL);
	label = gtk_label_new (_("Connecting to server..."));
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG (dialog)->vbox),
			    label, TRUE, TRUE, 10);
	g_signal_connect(dialog, "response", G_CALLBACK (check_response), &id);
	gtk_widget_show_all (dialog);
	
	mail_msg_wait(id);
	
	gtk_widget_destroy (dialog);
	dialog = NULL;
	
	return ret;
}

ESignatureList *
mail_config_get_signatures (void)
{
	return config->signatures;
}

static char *
get_new_signature_filename (void)
{
	const char *base_directory;
	char *filename, *id;
	struct stat st;
	int i;

	base_directory = mail_component_peek_base_directory (mail_component_peek ());
	filename = g_build_filename (base_directory, "signatures", NULL);
	if (lstat (filename, &st)) {
		if (errno == ENOENT) {
			if (mkdir (filename, 0700))
				g_warning ("Fatal problem creating %s directory.", filename);
		} else
			g_warning ("Fatal problem with %s directory.", filename);
	}
	g_free (filename);
	
	filename = g_malloc (strlen (base_directory) + sizeof ("/signatures/signature-") + 12);
	id = g_stpcpy (filename, base_directory);
	id = g_stpcpy (id, "/signatures/signature-");
	
	for (i = 0; i < (INT_MAX - 1); i++) {
		sprintf (id, "%d", i);
		if (lstat (filename, &st) == -1 && errno == ENOENT) {
			int fd;
			
			fd = creat (filename, 0600);
			if (fd >= 0) {
				close (fd);
				return filename;
			}
		}
	}
	
	g_free (filename);
	
	return NULL;
}


ESignature *
mail_config_signature_new (const char *filename, gboolean script, gboolean html)
{
	ESignature *sig;
	
	sig = e_signature_new ();
	sig->name = g_strdup (_("Unnamed"));
	sig->script = script;
	sig->html = html;
	
	if (filename == NULL)
		sig->filename = get_new_signature_filename ();
	else
		sig->filename = g_strdup (filename);
	
	return sig;
}

ESignature *
mail_config_get_signature_by_uid (const char *uid)
{
	return (ESignature *) e_signature_list_find (config->signatures, E_SIGNATURE_FIND_UID, uid);
}

ESignature *
mail_config_get_signature_by_name (const char *name)
{
	return (ESignature *) e_signature_list_find (config->signatures, E_SIGNATURE_FIND_NAME, name);
}

void
mail_config_add_signature (ESignature *signature)
{
	e_signature_list_add (config->signatures, signature);
	mail_config_save_signatures ();
}

void
mail_config_remove_signature (ESignature *signature)
{
	if (signature->filename && !signature->script)
		unlink (signature->filename);
	
	e_signature_list_remove (config->signatures, signature);
	mail_config_save_signatures ();
}

char *
mail_config_signature_run_script (const char *script)
{
	int result, status;
	int in_fds[2];
	pid_t pid;
	
	if (pipe (in_fds) == -1) {
		g_warning ("Failed to create pipe to '%s': %s", script, g_strerror (errno));
		return NULL;
	}
	
	if (!(pid = fork ())) {
		/* child process */
		int maxfd, i;
		
		close (in_fds [0]);
		if (dup2 (in_fds[1], STDOUT_FILENO) < 0)
			_exit (255);
		close (in_fds [1]);
		
		setsid ();
		
		maxfd = sysconf (_SC_OPEN_MAX);
		for (i = 3; i < maxfd; i++) {
			if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO)
				fcntl (i, F_SETFD, FD_CLOEXEC);
		}
		
		execlp (script, script, NULL);
		g_warning ("Could not execute %s: %s\n", script, g_strerror (errno));
		_exit (255);
	} else if (pid < 0) {
		g_warning ("Failed to create create child process '%s': %s", script, g_strerror (errno));
		return NULL;
	} else {
		CamelStreamFilter *filtered_stream;
		CamelStreamMem *memstream;
		CamelMimeFilter *charenc;
		CamelStream *stream;
		GByteArray *buffer;
		char *charset;
		char *content;
		
		/* parent process */
		close (in_fds[1]);
		
		stream = camel_stream_fs_new_with_fd (in_fds[0]);
		
		memstream = (CamelStreamMem *) camel_stream_mem_new ();
		buffer = g_byte_array_new ();
		camel_stream_mem_set_byte_array (memstream, buffer);
		
		camel_stream_write_to_stream (stream, (CamelStream *) memstream);
		camel_object_unref (stream);
		
		/* signature scripts are supposed to generate UTF-8 content, but because users
		   are known to not ever read the manual... we try to do our best if the
                   content isn't valid UTF-8 by assuming that the content is in the user's
		   preferred charset. */
		if (!g_utf8_validate (buffer->data, buffer->len, NULL)) {
			stream = (CamelStream *) memstream;
			memstream = (CamelStreamMem *) camel_stream_mem_new ();
			camel_stream_mem_set_byte_array (memstream, g_byte_array_new ());
			
			filtered_stream = camel_stream_filter_new_with_stream (stream);
			camel_object_unref (stream);
			
			charset = gconf_client_get_string (config->gconf, "/apps/evolution/mail/composer/charset", NULL);
			if (charset && *charset) {
				if ((charenc = (CamelMimeFilter *) camel_mime_filter_charset_new_convert (charset, "utf-8"))) {
					camel_stream_filter_add (filtered_stream, charenc);
					camel_object_unref (charenc);
				}
			}
			g_free (charset);
			
			camel_stream_write_to_stream ((CamelStream *) filtered_stream, (CamelStream *) memstream);
			camel_object_unref (filtered_stream);
			g_byte_array_free (buffer, TRUE);
			
			buffer = memstream->buffer;
		}
		
		camel_object_unref (memstream);
		
		g_byte_array_append (buffer, "", 1);
		content = buffer->data;
		g_byte_array_free (buffer, FALSE);
		
		/* wait for the script process to terminate */
		result = waitpid (pid, &status, 0);
		
		if (result == -1 && errno == EINTR) {
			/* child process is hanging... */
			kill (pid, SIGTERM);
			sleep (1);
			result = waitpid (pid, &status, WNOHANG);
			if (result == 0) {
				/* ...still hanging, set phasers to KILL */
				kill (pid, SIGKILL);
				sleep (1);
				result = waitpid (pid, &status, WNOHANG);
			}
		}
		
		return content;
	}
}
