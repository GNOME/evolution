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

