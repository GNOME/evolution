/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Lauris Kaplinski <lauris@helixcode.com>
 *         
 */

#include <config.h>
#include <unicode.h>
#include "e-unicode.h"

const gchar *
e_utf8_strstrcase (const gchar *haystack, const gchar *needle)
{
	gchar *p;
	unicode_char_t *huni, *nuni;
	unicode_char_t unival;
	gint hlen, nlen, hp, np;

	if (haystack == NULL) return NULL;
	if (needle == NULL) return NULL;
	if (strlen (needle) == 0) return haystack;

	huni = alloca (sizeof (unicode_char_t) * strlen (haystack));

	for (hlen = 0, p = unicode_get_utf8 (haystack, &unival); p && unival; hlen++, p = unicode_get_utf8 (p, &unival)) {
		huni[hlen] = unicode_tolower (unival);
	}

	if (!p) return NULL;
	if (hlen == 0) return NULL;

	nuni = alloca (sizeof (unicode_char_t) * strlen (needle));

	for (nlen = 0, p = unicode_get_utf8 (needle, &unival); p && unival; nlen++, p = unicode_get_utf8 (p, &unival)) {
		nuni[nlen] = unicode_tolower (unival);
	}

	if (!p) return NULL;
	if (nlen == 0) return NULL;

	if (hlen < nlen) return NULL;

	for (hp = 0; hp <= hlen - nlen; hp++) {
		for (np = 0; np < nlen; np++) {
			if (huni[hp + np] != nuni[np]) break;
		}
		if (np == nlen) return haystack + unicode_offset_to_index (haystack, hp);
	}

	return NULL;
}

gchar *
e_utf8_from_gtk_event_key (GtkWidget *widget, guint keyval, const gchar *string)
{
	/* test it out with iso-8859-1 */

	static gboolean uinit = FALSE;
	static gboolean uerror = FALSE;
	static unicode_iconv_t uiconv = (unicode_iconv_t) -1;
	char *new, *ob;
	size_t ibl, obl;

	if (uerror) return NULL;

	if (!string) return NULL;

	if (!uinit) {
		unicode_init ();
		uiconv = unicode_iconv_open ("UTF-8", "iso-8859-1");
		if (uiconv == (unicode_iconv_t) -1) {
			uerror = TRUE;
			return NULL;
		} else {
			uinit = TRUE;
		}
	}

	ibl = strlen (string);
	new = ob = g_new (gchar, ibl * 6 + 1);
	obl = ibl * 6 + 1;

	unicode_iconv (uiconv, &string, &ibl, &ob, &obl);

	*ob = '\0';

	return new;
}

gchar *
e_utf8_from_gtk_string (GtkWidget *widget, const gchar *string)
{
	/* test it out with iso-8859-1 */

	static gboolean uinit = FALSE;
	static gboolean uerror = FALSE;
	static unicode_iconv_t uiconv = (unicode_iconv_t) -1;
	char *new, *ob;
	size_t ibl, obl;

	if (uerror) return NULL;

	if (!string) return NULL;

	if (!uinit) {
		unicode_init ();
		uiconv = unicode_iconv_open ("UTF-8", "iso-8859-1");
		if (uiconv == (unicode_iconv_t) -1) {
			uerror = TRUE;
			return NULL;
		} else {
			uinit = TRUE;
		}
	}

	ibl = strlen (string);
	new = ob = g_new (gchar, ibl * 6 + 1);
	obl = ibl * 6 + 1;

	unicode_iconv (uiconv, &string, &ibl, &ob, &obl);

	*ob = '\0';

	return new;
}

gchar *
e_utf8_to_gtk_string (GtkWidget *widget, const gchar *string)
{
	/* test it out with iso-8859-1 */

	static gboolean uinit = FALSE;
	static gboolean uerror = FALSE;
	static unicode_iconv_t uiconv = (unicode_iconv_t) -1;
	char *new, *ob;
	size_t ibl, obl;

	if (uerror) return NULL;

	if (!string) return NULL;

	if (!uinit) {
		unicode_init ();
		uiconv = unicode_iconv_open ("iso-8859-1", "UTF-8");
		if (uiconv == (unicode_iconv_t) -1) {
			uerror = TRUE;
			return NULL;
		} else {
			uinit = TRUE;
		}
	}

	ibl = strlen (string);
	new = ob = g_new (gchar, ibl * 2 + 1);
	obl = ibl * 2 + 1;

	unicode_iconv (uiconv, &string, &ibl, &ob, &obl);

	*ob = '\0';

	return new;
}

gchar *
e_utf8_gtk_entry_get_text (GtkEntry *entry)
{
	gchar *s, *u;

	s = gtk_entry_get_text (entry);
	if (!s) return NULL;
	u = e_utf8_from_gtk_string ((GtkWidget *) entry, s);
	return u;
}

gchar *
e_utf8_gtk_editable_get_chars (GtkEditable *editable, gint start, gint end)
{
	gchar *s, *u;

	s = gtk_editable_get_chars (editable, start, end);
	if (!s) return NULL;
	u = e_utf8_from_gtk_string ((GtkWidget *) editable, s);
	g_free (s);
	return u;
}

void
e_utf8_gtk_entry_set_text (GtkEntry *entry, const gchar *text)
{
	gchar *s;

	if (!text) return;

	s = e_utf8_to_gtk_string ((GtkWidget *) entry, text);
	gtk_entry_set_text (entry, s);

	if (s) g_free (s);
}

GtkWidget *
e_utf8_gtk_menu_item_new_with_label (const gchar *label)
{
	GtkWidget *w;
	gchar *s;

	if (!label) return NULL;

	s = e_utf8_to_gtk_string (NULL, label);
	w = gtk_menu_item_new_with_label (s);

	if (s) g_free (s);

	return w;
}


