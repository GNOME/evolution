/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-config.c: Mail configuration dialogs/wizard. */

/* 
 * Authors: 
 *  Dan Winship <danw@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
 *  JP Rosevear <jpr@helixcode.com>
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
#include <pwd.h>

#include <gnome.h>
#include <gtkhtml/gtkhtml.h>
#include <glade/glade.h>

#include "e-util/e-html-utils.h"
#include "mail.h"
#include "mail-config.h"

typedef struct 
{
	gboolean configured;
	GSList *ids;
	GSList *sources;
	GSList *news;
	MailConfigService *transport;

	gboolean thread_list;
	gint paned_size;
	gboolean send_html;
	gint seen_timeout;
} MailConfig;

static const char GCONFPATH[] = "/apps/Evolution/Mail";
static MailConfig *config = NULL;

/* Prototypes */
static void config_read (void);

/* Identity struct */
MailConfigIdentity *
identity_copy (MailConfigIdentity *id) 
{
	MailConfigIdentity *newid;
	
	g_return_val_if_fail (id, NULL);
	
	newid = g_new0 (MailConfigIdentity, 1);
	newid->name = g_strdup (id->name);
	newid->address = g_strdup (id->address);
	newid->org = g_strdup (id->org);
	newid->sig = g_strdup (id->sig);
	
	return newid;
}

void
identity_destroy (MailConfigIdentity *id)
{
	if (!id)
		return;
	
	g_free (id->name);
	g_free (id->address);
	g_free (id->org);
	g_free (id->sig);
	
	g_free (id);
}

void
identity_destroy_each (gpointer item, gpointer data)
{
	identity_destroy ((MailConfigIdentity *)item);
}

/* Service struct */
MailConfigService *
service_copy (MailConfigService *source) 
{
	MailConfigService *newsource;
	
	g_return_val_if_fail (source, NULL);
	
	newsource = g_new0 (MailConfigService, 1);
	newsource->url = g_strdup (source->url);
	newsource->keep_on_server = source->keep_on_server;
	
	return newsource;
}

void
service_destroy (MailConfigService *source)
{
	if (!source)
		return;

	g_free (source->url);
	
	g_free (source);
}

void
service_destroy_each (gpointer item, gpointer data)
{
	service_destroy ((MailConfigService *)item);
}

/* Config struct routines */
void
mail_config_init (void)
{
	if (config)
		return;
	
	config = g_new0 (MailConfig, 1);

	config->ids = NULL;
	config->sources = NULL;
	config->transport = NULL;

	config_read ();
}

void
mail_config_clear (void)
{
	if (!config)
		return;
	
	if (config->ids) {
		g_slist_foreach (config->ids, identity_destroy_each, NULL);
		g_slist_free (config->ids);
		config->ids = NULL;
	}
	
	if (config->sources) {
		g_slist_foreach (config->sources, service_destroy_each, NULL);
		g_slist_free (config->sources);
		config->sources = NULL;
	}
	
	service_destroy (config->transport);
	config->transport = NULL;

	if (config->news) {
		g_slist_foreach (config->news, service_destroy_each, NULL);
		g_slist_free (config->news);
		config->news = NULL;
	}
}

static void
config_read (void)
{
	gchar *str;
	gint len, i;
	
	mail_config_clear ();

	/* Configured */
	str = g_strdup_printf ("=%s/config/General=/General/configured", evolution_dir);
	config->configured = gnome_config_get_bool (str);
	g_free (str);
	
	/* Identities */
	str = g_strdup_printf ("=%s/config/Mail=/Identities/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);

	len = gnome_config_get_int ("num");
	for (i = 0; i < len; i++) {
		MailConfigIdentity *id;
		gchar *path;
		
		id = g_new0 (MailConfigIdentity, 1);

		path = g_strdup_printf ("name_%d", i);
		id->name = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("address_%d", i);
		id->address = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("org_%d", i);
		id->org = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("sig_%d", i);
		id->sig = gnome_config_get_string (path);
		g_free (path);

		config->ids = g_slist_append (config->ids, id);
	}
	gnome_config_pop_prefix ();

	/* Sources */
	str = g_strdup_printf ("=%s/config/Mail=/Sources/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);

	len = gnome_config_get_int ("num");
	for (i = 0; i < len; i++) {
		MailConfigService *s;
		gchar *path;
		
		s = g_new0 (MailConfigService, 1);
		
		path = g_strdup_printf ("url_%d", i);
		s->url = gnome_config_get_string (path);
		g_free (path);
		path = g_strdup_printf ("keep_on_server_%d", i);
		s->keep_on_server = gnome_config_get_bool (path);
		g_free (path);
		
		config->sources = g_slist_append (config->sources, s);
	}
	gnome_config_pop_prefix ();
	
	/* News */
	str = g_strdup_printf ("=%s/config/News=/Sources/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);

	len = gnome_config_get_int ("num");
	for (i = 0; i < len; i++) {
		MailConfigService *n;
		gchar *path;
		
		n = g_new0 (MailConfigService, 1);

		path = g_strdup_printf ("url_%d", i);
		n->url = gnome_config_get_string (path);
		g_free (path);

		config->news = g_slist_append (config->news, n);
	}
	gnome_config_pop_prefix ();
	
	/* Transport */
	config->transport = g_new0 (MailConfigService, 1);
	str = g_strdup_printf ("=%s/config/Mail=/Transport/url", 
			       evolution_dir);
	config->transport->url = gnome_config_get_string (str);
	g_free (str);
	
	/* Format */
	str = g_strdup_printf ("=%s/config/Mail=/Format/send_html", 
			       evolution_dir);
	config->send_html = gnome_config_get_bool (str);
	g_free (str);

	/* Mark as seen timeout */
	str = g_strdup_printf ("=%s/config/Mail=/Display/seen_timeout=1500", 
			       evolution_dir);
	config->seen_timeout = gnome_config_get_int (str);
	g_free (str);

	/* Show Messages Threaded */
	str = g_strdup_printf ("=%s/config/Mail=/Display/thread_list", 
			       evolution_dir);
	config->thread_list = gnome_config_get_bool (str);
	g_free (str);

	/* Size of vpaned in mail view */
	str = g_strdup_printf ("=%s/config/Mail=/Display/paned_size=200", 
			       evolution_dir);
	config->paned_size = gnome_config_get_int (str);
	g_free (str);

	gnome_config_sync ();
}

void
mail_config_write (void)
{
	gchar *str;
	gint len, i;

	/* Configured switch */
	str = g_strdup_printf ("=%s/config/General=/General/configured", 
			       evolution_dir);
	config->configured = TRUE;
	gnome_config_set_bool (str, config->configured);
	g_free (str);
	
	/* Identities */
	str = g_strdup_printf ("=%s/config/Mail=/Identities/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);

	len = g_slist_length (config->ids);
	gnome_config_set_int ("num", len);
	for (i = 0; i < len; i++) {
		MailConfigIdentity *id;
		gchar *path;
		
		id = (MailConfigIdentity *)g_slist_nth_data (config->ids, i);
		
		path = g_strdup_printf ("name_%d", i);
		gnome_config_set_string (path, id->name);
		g_free (path);
		path = g_strdup_printf ("address_%d", i);
		gnome_config_set_string (path, id->address);
		g_free (path);
		path = g_strdup_printf ("org_%d", i);
		gnome_config_set_string (path, id->org);
		g_free (path);
		path = g_strdup_printf ("sig_%d", i);
		gnome_config_set_string (path, id->sig);
		g_free (path);
	}
	gnome_config_pop_prefix ();
	
	/* Sources */
	str = g_strdup_printf ("=%s/config/Mail=/Sources/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);

	len = g_slist_length (config->sources);
	gnome_config_set_int ("num", len);
	for (i=0; i<len; i++) {
		MailConfigService *s;
		gchar *path;
		
		s = (MailConfigService *)g_slist_nth_data (config->sources, i);
		
		path = g_strdup_printf ("url_%d", i);
		gnome_config_set_string (path, s->url);
		g_free (path);
		path = g_strdup_printf ("keep_on_server_%d", i);
		gnome_config_set_bool (path, s->keep_on_server);
		g_free (path);
	}
	gnome_config_pop_prefix ();

	/* News */
	str = g_strdup_printf ("=%s/config/News=/Sources/", evolution_dir);
	gnome_config_push_prefix (str);
	g_free (str);

  	len = g_slist_length (config->news);
	gnome_config_set_int ("num", len);
	for (i=0; i<len; i++) {
		MailConfigService *n;
		gchar *path;
		
		n = (MailConfigService *)g_slist_nth_data (config->news, i);
		
		path = g_strdup_printf ("url_%d", i);
		gnome_config_set_string (path, n->url);
		g_free (path);
	}
	gnome_config_pop_prefix ();

	/* Transport */
	str = g_strdup_printf ("=%s/config/Mail=/Transport/url", 
			       evolution_dir);
	gnome_config_set_string (str, config->transport->url);
	g_free (str);
	
	/* Mark as seen timeout */
	str = g_strdup_printf ("=%s/config/Mail=/Display/seen_timeout", 
			       evolution_dir);
	gnome_config_set_int (str, config->seen_timeout);
	g_free (str);

	/* Format */
	str = g_strdup_printf ("=%s/config/Mail=/Format/send_html", 
			       evolution_dir);
	gnome_config_set_bool (str, config->send_html);
	g_free (str);

	gnome_config_sync ();
}

void
mail_config_write_on_exit (void)
{
	gchar *str;

	/* Show Messages Threaded */
	str = g_strdup_printf ("=%s/config/Mail=/Display/thread_list", 
			       evolution_dir);
	gnome_config_set_bool (str, config->thread_list);
	g_free (str);

	/* Size of vpaned in mail view */
	str = g_strdup_printf ("=%s/config/Mail=/Display/paned_size", 
			       evolution_dir);
	gnome_config_set_int (str, config->paned_size);
	g_free (str);

	gnome_config_sync ();
}

/* Accessor functions */
gboolean
mail_config_is_configured (void)
{
	return config->configured;
}

gboolean
mail_config_thread_list (void)
{
	return config->thread_list;
}

void
mail_config_set_thread_list (gboolean value)
{
	config->thread_list = value;
}

gint
mail_config_paned_size (void)
{
	return config->paned_size;
}

void
mail_config_set_paned_size (gint value)
{
	config->paned_size = value;
}

gboolean
mail_config_send_html (void)
{
	return config->send_html;
}

void
mail_config_set_send_html (gboolean send_html)
{
	config->send_html = send_html;
}

gint
mail_config_mark_as_seen_timeout (void)
{
	return config->seen_timeout;
}

void
mail_config_set_mark_as_seen_timeout (gint timeout)
{
	config->seen_timeout = timeout;
}

MailConfigIdentity *
mail_config_get_default_identity (void)
{
	if (!config->ids)
		return NULL;
	
	return (MailConfigIdentity *)config->ids->data;
}

GSList *
mail_config_get_identities (void)
{
	return config->ids;
}

void
mail_config_add_identity (MailConfigIdentity *id)
{
	MailConfigIdentity *new_id = identity_copy (id);
	
	config->ids = g_slist_append (config->ids, new_id);
}

MailConfigService *
mail_config_get_default_source (void)
{
	if (!config->sources)
		return NULL;
	
	return (MailConfigService *)config->sources->data;
}

GSList *
mail_config_get_sources (void)
{
	return config->sources;
}

void
mail_config_add_source (MailConfigService *source) 
{
	MailConfigService *new_source = service_copy (source);
	
	config->sources = g_slist_append (config->sources, new_source);
}

MailConfigService *
mail_config_get_transport (void)
{
	return config->transport;
}

void
mail_config_set_transport (MailConfigService *transport)
{
	if (config->transport)
		service_destroy (config->transport);

	config->transport = transport;
}

MailConfigService *
mail_config_get_default_news (void)
{
	if (!config->news)
		return NULL;
	
	return (MailConfigService *)config->news->data;
}

GSList *
mail_config_get_news (void)
{
	return config->news;
}

void
mail_config_add_news (MailConfigService *news) 
{
	MailConfigService *new_news = service_copy (news);

	config->news = g_slist_append (config->news, new_news);
}




