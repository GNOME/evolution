/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * eab-util.c
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Jon Trowbridge <trow@ximian.com>
 *          Chris Toshok <toshok@ximian.com>
 */

/*
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
 * USA.
 */

#include <config.h>
#include "eab-book-util.h"

#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <e-util/e-config-listener.h>

EConfigListener *
eab_get_config_database ()
{
	static EConfigListener *config_db;

	if (config_db == NULL)
		config_db = e_config_listener_new ();

	return config_db;
}

/*
 *
 * Specialized Queries
 *
 */

static char*
escape (const char *str)
{
	GString *s = g_string_new ("");
	const char *p = str;

	while (*p) {
		if (*p == '\\')
			g_string_append (s, "\\\\");
		else if (*p == '"')
			g_string_append (s, "\\\"");
		else
			g_string_append_c (s, *p);

		p ++;
	}

	return g_string_free (s, FALSE);
}

guint
eab_name_and_email_query (EBook *book,
			  const gchar *name,
			  const gchar *email,
			  EBookListCallback cb,
			  gpointer closure)
{
	gchar *email_query=NULL, *name_query=NULL;
	EBookQuery *query;
	guint tag;
	char *escaped_name, *escaped_email;

	g_return_val_if_fail (book && E_IS_BOOK (book), 0);
	g_return_val_if_fail (cb != NULL, 0);

	if (name && !*name)
		name = NULL;
	if (email && !*email)
		email = NULL;

	if (name == NULL && email == NULL)
		return 0;

	escaped_name = name ? escape (name) : NULL;
	escaped_email = email ? escape (email) : NULL;

	/* Build our e-mail query.
	 * We only query against the username part of the address, to avoid not matching
	 * fred@foo.com and fred@mail.foo.com.  While their may be namespace collisions
	 * in the usernames of everyone out there, it shouldn't be that bad.  (Famous last words.)
	 */
	if (escaped_email) {
		const gchar *t = escaped_email;
		while (*t && *t != '@')
			++t;
		if (*t == '@') {
			email_query = g_strdup_printf ("(beginswith \"email\" \"%.*s@\")", t-escaped_email, escaped_email);

		} else {
			email_query = g_strdup_printf ("(beginswith \"email\" \"%s\")", escaped_email);
		}
	}

	/* Build our name query.
	 * We only do name-query stuff if we don't have an e-mail address.  Our basic assumption
	 * is that the username part of the email is good enough to keep the amount of stuff returned
	 * in the query relatively small.
	 */
	if (escaped_name && !escaped_email)
		name_query = g_strdup_printf ("(or (beginswith \"file_as\" \"%s\") (beginswith \"full_name\" \"%s\"))", escaped_name, escaped_name);

	/* Assemble our e-mail & name queries */
	if (email_query && name_query) {
		char *full_query = g_strdup_printf ("(and %s %s)", email_query, name_query);
		query = e_book_query_from_string (full_query);
		g_free (full_query);
	}
	else if (email_query) {
		query = e_book_query_from_string (email_query);
	}
	else if (name_query) {
		query = e_book_query_from_string (name_query);
	}
	else
		return 0;

	tag = e_book_async_get_contacts (book, query, cb, closure);

	g_free (email_query);
	g_free (name_query);
	g_free (escaped_email);
	g_free (escaped_name);
	e_book_query_unref (query);

	return tag;
}

/*
 * Simple nickname query
 */
guint
eab_nickname_query (EBook                 *book,
		    const char            *nickname,
		    EBookListCallback      cb,
		    gpointer               closure)
{
	EBookQuery *query;
	char *query_string;
	guint retval;

	g_return_val_if_fail (E_IS_BOOK (book), 0);
	g_return_val_if_fail (nickname != NULL, 0);

	/* The empty-string case shouldn't generate a warning. */
	if (! *nickname)
		return 0;

	query_string = g_strdup_printf ("(is \"nickname\" \"%s\")", nickname);

	query = e_book_query_from_string (query_string);

	retval = e_book_async_get_contacts (book, query, cb, closure);

	g_free (query_string);
	g_object_unref (query);

	return retval;
}

GList*
eab_contact_list_from_string (const char *str)
{
	GList *contacts = NULL;
	GString *gstr = g_string_new ("");
	char *p = (char*)str;
	char *q;
	char *blank_line;

	if (!p)
		return NULL;

	if (!strncmp (p, "Book: ", 6)) {
		p = strchr (p, '\n');
		if (!p) {
			g_warning (G_STRLOC ": Got book but no newline!");
			return NULL;
		}
		p++;
	}

	while (*p) {
		if (*p != '\r') g_string_append_c (gstr, *p);
		
		p++;
	}

	p = g_string_free (gstr, FALSE);
	q = p;
	do {
		char *temp;

		blank_line = strstr (q, "\n\n");
		if (blank_line) {
			temp = g_strndup (q, blank_line - q);
		}
		else {
			temp = g_strdup (q);
		}

		/* Do a minimal well-formedness test, since
		 * e_contact_new_from_vcard () always returns a contact */
		if (!strstr (p, "BEGIN:VCARD")) {
			g_free (temp);
			break;
		}

		contacts = g_list_append (contacts, e_contact_new_from_vcard (temp));

		g_free (temp);

		if (blank_line)
			q = blank_line + 2;
		else
			q = NULL;
	} while (blank_line);

	g_free (p);

	return contacts;
}

char*
eab_contact_list_to_string (GList *contacts)
{
	GString *str = g_string_new ("");
	GList *l;

	for (l = contacts; l; l = l->next) {
		EContact *contact = l->data;
		char *vcard_str = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

		g_string_append (str, vcard_str);
		if (l->next)
			g_string_append (str, "\r\n\r\n");
	}

	return g_string_free (str, FALSE);
}

gboolean
eab_book_and_contact_list_from_string (const char *str, EBook **book, GList **contacts)
{
	const char *s0, *s1;
	char *uri;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (book != NULL, FALSE);
	g_return_val_if_fail (contacts != NULL, FALSE);

	*contacts = eab_contact_list_from_string (str);

	if (!strncmp (str, "Book: ", 6)) {
		s0 = str + 6;
		s1 = strchr (str, '\r');

		if (!s1)
			s1 = strchr (str, '\n');
	} else {
		s0 = NULL;
		s1 = NULL;
	}

	if (!s0 || !s1) {
		*book = NULL;
		return FALSE;
	}

	uri = g_strndup (s0, s1 - s0);
	*book = e_book_new_from_uri (uri, NULL);
	g_free (uri);

	return *book ? TRUE : FALSE;
}

char *
eab_book_and_contact_list_to_string (EBook *book, GList *contacts)
{
	char *s0, *s1;

	s0 = eab_contact_list_to_string (contacts);
	if (!s0)
		s0 = g_strdup ("");

	if (book)
		s1 = g_strconcat ("Book: ", e_book_get_uri (book), "\r\n", s0, NULL);
	else
		s1 = g_strdup (s0);

	g_free (s0);
	return s1;
}

#if notyet
/*
 *  Convenience routine to check for addresses in the local address book.
 */

typedef struct _HaveAddressInfo HaveAddressInfo;
struct _HaveAddressInfo {
	gchar *email;
	EBookHaveAddressCallback cb;
	gpointer closure;
};

static void
have_address_query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *contacts, gpointer closure)
{
	HaveAddressInfo *info = (HaveAddressInfo *) closure;
	
	info->cb (book, 
		  info->email,
		  contacts && (status == E_BOOK_ERROR_OK) ? E_CONTACT (contacts->data) : NULL,
		  info->closure);

	g_free (info->email);
	g_free (info);
}

static void
have_address_book_open_cb (EBook *book, gpointer closure)
{
	HaveAddressInfo *info = (HaveAddressInfo *) closure;

	if (book) {

		e_book_name_and_email_query (book, NULL, info->email, have_address_query_cb, info);

	} else {

		info->cb (NULL, info->email, NULL, info->closure);

		g_free (info->email);
		g_free (info);

	}
}

void
eab_query_address_default (const gchar *email,
			   EABHaveAddressCallback cb,
			   gpointer closure)
{
	HaveAddressInfo *info;

	g_return_if_fail (email != NULL);
	g_return_if_fail (cb != NULL);

	info = g_new0 (HaveAddressInfo, 1);
	info->email = g_strdup (email);
	info->cb = cb;
	info->closure = closure;

	e_book_use_default_book (have_address_book_open_cb, info);
}
#endif

/* bad place for this i know. */
int
e_utf8_casefold_collate_len (const gchar *str1, const gchar *str2, int len)
{
	gchar *s1 = g_utf8_casefold(str1, len);
	gchar *s2 = g_utf8_casefold(str2, len);
	int rv;

	rv = g_utf8_collate (s1, s2);

	g_free (s1);
	g_free (s2);

	return rv;
}

int
e_utf8_casefold_collate (const gchar *str1, const gchar *str2)
{
	return e_utf8_casefold_collate_len (str1, str2, -1);
}
