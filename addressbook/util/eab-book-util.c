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

guint
eab_name_and_email_query (EBook *book,
			  const gchar *name,
			  const gchar *email,
			  EBookContactsCallback cb,
			  gpointer closure)
{
	gchar *email_query=NULL, *name_query=NULL, *query;
	guint tag;

	g_return_val_if_fail (book && E_IS_BOOK (book), 0);
	g_return_val_if_fail (cb != NULL, 0);

	if (name && !*name)
		name = NULL;
	if (email && !*email)
		email = NULL;

	if (name == NULL && email == NULL)
		return 0;

	/* Build our e-mail query.
	 * We only query against the username part of the address, to avoid not matching
	 * fred@foo.com and fred@mail.foo.com.  While their may be namespace collisions
	 * in the usernames of everyone out there, it shouldn't be that bad.  (Famous last words.)
	 */
	if (email) {
		const gchar *t = email;
		while (*t && *t != '@')
			++t;
		if (*t == '@') {
			email_query = g_strdup_printf ("(beginswith \"email\" \"%.*s@\")", t-email, email);

		} else {
			email_query = g_strdup_printf ("(beginswith \"email\" \"%s\")", email);
		}
	}

	/* Build our name query.
	 * We only do name-query stuff if we don't have an e-mail address.  Our basic assumption
	 * is that the username part of the email is good enough to keep the amount of stuff returned
	 * in the query relatively small.
	 */
	if (name && !email)
		name_query = g_strdup_printf ("(or (beginswith \"file_as\" \"%s\") (beginswith \"full_name\" \"%s\"))", name, name);

	/* Assemble our e-mail & name queries */
	if (email_query && name_query) {
		query = g_strdup_printf ("(and %s %s)", email_query, name_query);
	} else if (email_query) {
		query = email_query;
		email_query = NULL;
	} else if (name_query) {
		query = name_query;
		name_query = NULL;
	} else
		return 0;

	tag = e_book_async_get_contacts (book, query, cb, closure);

	g_free (email_query);
	g_free (name_query);
	g_free (query);

	return tag;
}

/*
 * Simple nickname query
 */
guint
eab_nickname_query (EBook                 *book,
		    const char            *nickname,
		    EBookContactsCallback  cb,
		    gpointer               closure)
{
	gchar *query;
	guint retval;

	g_return_val_if_fail (E_IS_BOOK (book), 0);
	g_return_val_if_fail (nickname != NULL, 0);

	/* The empty-string case shouldn't generate a warning. */
	if (! *nickname)
		return 0;

	query = g_strdup_printf ("(is \"nickname\" \"%s\")", nickname);

	retval = e_book_async_get_contacts (book, query, cb, closure);

	g_free (query);

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
