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
 *		Jon Trowbridge <trow@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "eab-book-util.h"

/* Copied from camel_strstrcase */
static gchar *
eab_strstrcase (const gchar *haystack,
                const gchar *needle)
{
	/* find the needle in the haystack neglecting case */
	const gchar *ptr;
	guint len;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	len = strlen (needle);
	if (len > strlen (haystack))
		return NULL;

	if (len == 0)
		return (gchar *) haystack;

	for (ptr = haystack; *(ptr + len - 1) != '\0'; ptr++)
		if (!g_ascii_strncasecmp (ptr, needle, len))
			return (gchar *) ptr;

	return NULL;
}

GSList *
eab_contact_list_from_string (const gchar *str)
{
	GSList *contacts = NULL;
	GString *gstr = g_string_new (NULL);
	gchar *str_stripped;
	gchar *p = (gchar *) str;
	gchar *q;

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

	p = str_stripped = g_string_free (gstr, FALSE);

	/* Note: The vCard standard says
	 *
	 * vcard = "BEGIN" [ws] ":" [ws] "VCARD" [ws] 1*CRLF
	 *         items *CRLF "END" [ws] ":" [ws] "VCARD"
	 *
	 * which means we can have whitespace (e.g. "BEGIN : VCARD"). So we're not being
	 * fully compliant here, although I'm not sure it matters. The ideal solution
	 * would be to have a vcard parsing function that returned the end of the vcard
	 * parsed. Arguably, contact list parsing should all be in libebook's e-vcard.c,
	 * where we can do proper parsing and validation without code duplication. */

	for (p = eab_strstrcase (p, "BEGIN:VCARD"); p; p = eab_strstrcase (q, "\nBEGIN:VCARD")) {
		gchar *card_str;

		if (*p == '\n')
			p++;

		for (q = eab_strstrcase (p, "END:VCARD"); q; q = eab_strstrcase (q, "END:VCARD")) {
			gchar *temp;

			q += 9;
			temp = q;
			temp += strspn (temp, "\r\n\t ");

			if (*temp == '\0' || !g_ascii_strncasecmp (temp, "BEGIN:VCARD", 11))
				break;  /* Found the outer END:VCARD */
		}

		if (!q)
			break;

		card_str = g_strndup (p, q - p);
		contacts = g_slist_prepend (contacts, e_contact_new_from_vcard (card_str));
		g_free (card_str);
	}

	g_free (str_stripped);

	return g_slist_reverse (contacts);
}

gchar *
eab_contact_list_to_string (const GSList *contacts)
{
	GString *str = g_string_new ("");
	const GSList *l;

	for (l = contacts; l; l = l->next) {
		EContact *contact = l->data;
		gchar *vcard_str;

		vcard_str = e_vcard_to_string (
			E_VCARD (contact), EVC_FORMAT_VCARD_30);

		g_string_append (str, vcard_str);
		if (l->next)
			g_string_append (str, "\r\n\r\n");
	}

	return g_string_free (str, FALSE);
}

gboolean
eab_book_and_contact_list_from_string (const gchar *str,
                                       EBookClient **book_client,
                                       GSList **contacts)
{
	const gchar *s0, *s1;
	gchar *uri;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (book_client != NULL, FALSE);
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
		*book_client = NULL;
		return FALSE;
	}

	uri = g_strndup (s0, s1 - s0);
	*book_client = e_book_client_new_from_uri (uri, NULL);
	g_free (uri);

	return (*book_client != NULL);
}

gchar *
eab_book_and_contact_list_to_string (EBookClient *book_client,
                                     const GSList *contacts)
{
	gchar *s0, *s1;

	s0 = eab_contact_list_to_string (contacts);
	if (!s0)
		s0 = g_strdup ("");

	if (book_client != NULL) {
		s1 = g_strconcat (
			"Book: ", e_client_get_uri (
			E_CLIENT (book_client)), "\r\n", s0, NULL);
	} else
		s1 = g_strdup (s0);

	g_free (s0);
	return s1;
}

/* bad place for this i know. */
gint
e_utf8_casefold_collate_len (const gchar *str1,
                             const gchar *str2,
                             gint len)
{
	gchar *s1 = g_utf8_casefold (str1, len);
	gchar *s2 = g_utf8_casefold (str2, len);
	gint rv;

	rv = g_utf8_collate (s1, s2);

	g_free (s1);
	g_free (s2);

	return rv;
}

gint
e_utf8_casefold_collate (const gchar *str1,
                         const gchar *str2)
{
	return e_utf8_casefold_collate_len (str1, str2, -1);
}
