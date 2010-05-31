/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 *		Srinivasa Ragavan <srini@linux.intel.com>
 *
 * Copyright (C) 2009 Intel Corporation (www.intel.com)
 *
 */

/* Template of the code is taken from libsoup tests/ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libsoup/soup.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>

#include <libedataserver/e-proxy.h>

#include <shell/e-shell.h>
#include <e-util/e-util-private.h>

#include "mail-guess-servers.h"

static gchar *
xml_to_gchar (xmlChar *xml, EmailProvider *provider)
{
	gchar *gxml = NULL;
	gchar *tmp;
	gchar *repl = NULL;
	const gchar *sec_part;

	tmp = xml ? strstr((gchar *) xml, "\%EMAIL") : NULL;

	if (!tmp) {
		gxml = xml ? g_strdup((gchar *) xml) : NULL;
	} else {
	decodepart:
		*tmp = 0;
		tmp+=6;
		if (*tmp == 'A')
			repl = provider->email;
		else if (*tmp == 'L')
			repl = provider->username;
		else if (*tmp == 'D')
			repl = provider->domain;
		sec_part = strstr(tmp, "\%");
		sec_part++;
		if (!*sec_part)
			sec_part = "";

		gxml = g_strdup_printf("%s%s%s", gxml ? gxml : (gchar *)xml, repl, sec_part);
		tmp = strstr (gxml, "\%EMAIL");
		if (tmp) {
			goto decodepart;
		}
	}

	xmlFree(xml);

	return gxml;
}

static SoupMessage *
get_url (SoupSession *session, const gchar *url)
{
	const gchar *name;
	SoupMessage *msg;
	const gchar *header;

	msg = soup_message_new (SOUP_METHOD_GET, url);
	soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);

	soup_session_send_message (session, msg);

	name = soup_message_get_uri (msg)->path;

	if (SOUP_STATUS_IS_REDIRECTION (msg->status_code)) {
		header = soup_message_headers_get_one (msg->response_headers,
						       "Location");
		if (header) {
			return get_url (session, header);
		}
	} else if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
		return msg;
	}

	return NULL;
}

static void
handle_incoming (xmlNodePtr head, EmailProvider *provider)
{
	xmlNodePtr node = head->children;

	provider->recv_type = xml_to_gchar(xmlGetProp(head, (xmlChar *) "type"), provider);

	while (node) {
		if (strcmp ((gchar *)node->name, "hostname") == 0) {
			provider->recv_hostname = xml_to_gchar(xmlNodeGetContent(node), provider);
		} else if (strcmp ((gchar *)node->name, "port") == 0) {
			provider->recv_port = xml_to_gchar(xmlNodeGetContent(node), provider);
		} else if (strcmp ((gchar *)node->name, "socketType") == 0) {
			provider->recv_socket_type = xml_to_gchar(xmlNodeGetContent(node), provider);
		} else if (strcmp ((gchar *)node->name, "username") == 0) {
			provider->recv_username = xml_to_gchar(xmlNodeGetContent(node), provider);
		} else if (strcmp ((gchar *)node->name, "authentication") == 0) {
			provider->recv_auth = xml_to_gchar(xmlNodeGetContent(node), provider);
		}

		node = node->next;
	}
}

static void
handle_outgoing (xmlNodePtr head, EmailProvider *provider)
{
	xmlNodePtr node = head->children;

	provider->send_type = xml_to_gchar(xmlGetProp(head, (xmlChar *) "type"), provider);

	while (node) {
		if (strcmp ((gchar *)node->name, "hostname") == 0) {
			provider->send_hostname = xml_to_gchar(xmlNodeGetContent(node), provider);
		} else if (strcmp ((gchar *)node->name, "port") == 0) {
			provider->send_port = xml_to_gchar(xmlNodeGetContent(node), provider);
		} else if (strcmp ((gchar *)node->name, "socketType") == 0) {
			provider->send_socket_type = xml_to_gchar(xmlNodeGetContent(node), provider);
		} else if (strcmp ((gchar *)node->name, "username") == 0) {
			provider->send_username = xml_to_gchar(xmlNodeGetContent(node), provider);
		} else if (strcmp ((gchar *)node->name, "authentication") == 0) {
			provider->send_auth = xml_to_gchar(xmlNodeGetContent(node), provider);
		}

		node = node->next;
	}
}

static gboolean
parse_message (const gchar *msg, gint length, EmailProvider *provider)
{
	xmlDocPtr doc;
	xmlNodePtr node, top;

	doc = xmlReadMemory (msg, length, "file.xml", NULL, 0);

	node = doc->children;
	while (node) {
		if (strcmp ((gchar *)node->name, "clientConfig") == 0) {
			break;
		}
		node = node->next;
	}

	if (!node) {
		g_warning ("Incorrect data: ClientConfig not found ... Quitting\n");
		return FALSE;
	}

	node = node->children;
	while (node) {
		if (strcmp ((gchar *)node->name, "emailProvider") == 0) {
			break;
		}
		node = node->next;
	}

	if (!node) {
		g_warning ("Incorrect data: ClientConfig not found ... Quitting\n");
		return FALSE;
	}

	top = node;
	node = node->children;
	while (node) {
		if (strcmp ((gchar *)node->name, "incomingServer") == 0) {
			/* Handle Incoming */
			handle_incoming (node, provider);
		} else if (strcmp ((gchar *)node->name, "outgoingServer") == 0) {
			/* Handle Outgoing */
			handle_outgoing (node, provider);
		}

		node = node->next;
	}

	xmlFreeDoc(doc);

	return TRUE;
}

static gboolean
parse_soup_message (SoupMessage *msg, EmailProvider *provider)
{
	return parse_message (msg->response_body->data, msg->response_body->length, provider);
}

static gboolean
is_online (void)
{
	EShell *shell;

	/* FIXME Pass this in. */
	shell = e_shell_get_default ();

	return e_shell_get_online (shell);
}

static gboolean
guess_when_online (EmailProvider *provider)
{
	const gchar *cafile = NULL;
	gchar *url;
	EProxy *proxy;
	SoupURI *parsed;
	SoupMessage *msg;
	SoupSession *session;

	proxy = e_proxy_new ();
	e_proxy_setup_proxy (proxy);

	url = g_strdup_printf (
		"%s/%s", "http://api.gnome.org/evolution/autoconfig",
		provider->domain);
	parsed = soup_uri_new (url);
	soup_uri_free (parsed);

	session = soup_session_sync_new_with_options (
		SOUP_SESSION_SSL_CA_FILE, cafile,
		SOUP_SESSION_USER_AGENT, "get ",
		NULL);

	if (e_proxy_require_proxy_for_uri (proxy, url)) {
		SoupURI *proxy_uri = e_proxy_peek_uri_for (proxy, url);
/*		fprintf (stderr, "URL '%s' requires a proxy: '%s'\n",
			 url, soup_uri_to_string (proxy_uri, FALSE)); */
		g_object_set (session, SOUP_SESSION_PROXY_URI, proxy_uri, NULL);
	}

	msg = get_url (session, url);
	if (!msg)
		return FALSE;

	parse_soup_message (msg, provider);

	g_object_unref (proxy);
	g_object_unref (msg);
	g_object_unref(session);
	g_free(url);

	return TRUE;

}

static gchar *
get_filename_for_offline_autoconfig (const gchar *domain)
{
	return g_build_filename (EVOLUTION_PRIVDATADIR, "mail-autoconfig", domain, NULL);
}

static gboolean
guess_when_offline (EmailProvider *provider)
{
	gchar *filename;
	gchar *contents;
	gsize length;
	gboolean success;

	if (!provider->domain || provider->domain[0] == 0)
		return FALSE;

	success = FALSE;

	filename = get_filename_for_offline_autoconfig (provider->domain);
	if (!g_file_get_contents (filename, &contents, &length, NULL)) /* NULL-GError */
		goto out;

	success = parse_message (contents, (gint) length, provider);

out:
	g_free (filename);
	g_free (contents);

	return success;
}

gboolean
mail_guess_servers(EmailProvider *provider)
{
	if (is_online () && guess_when_online (provider))
		return TRUE;
	else
		return guess_when_offline (provider);
}

#ifdef TEST
gint
main (gint argc, gchar **argv)
{
	EmailProvider *provider;
	g_thread_init (NULL);
	g_type_init ();

	provider = g_new0(EmailProvider, 1);

	provider->email = "sragavan@iijmio-mail.jp";
	provider->domain = "iijmio-mail.jp";
	provider->username = "sragavan";

	mail_guess_servers (provider);

	printf (
		"Recv: %s\n%s(%s), %s by %s \n "
		"Send: %s\n%s(%s), %s by %s\n via %s to %s\n",
		provider->recv_type, provider->recv_hostname,
		provider->recv_port, provider->recv_username,
		provider->recv_auth,
		provider->send_type, provider->send_hostname,
		provider->send_port, provider->send_username,
		provider->send_auth,
		provider->recv_socket_type,
		provider->send_socket_type);
	return 0;
}
#endif
