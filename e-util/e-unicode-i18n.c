/*
 * e-unicode-i18n.c
 *
 * Author: Zbigniew Chyla  <cyba@gnome.pl>
 *
 * Copyright (C) 2001 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gal/widgets/e-unicode.h>
#include "e-unicode-i18n.h"

static GHashTable *locale_to_utf8_hash = NULL;

static const char *
locale_to_utf8 (const char *string)
{
	char *utf;

	if (locale_to_utf8_hash == NULL) {
		locale_to_utf8_hash = g_hash_table_new (&g_str_hash, &g_str_equal);
	}

	utf = g_hash_table_lookup (locale_to_utf8_hash, string);
	if (utf == NULL) {
		utf = e_utf8_from_locale_string (string);
		g_hash_table_insert (locale_to_utf8_hash, g_strdup (string), utf);
	}

	return utf;
}

const char *
e_utf8_gettext (const char *string)
{
	if (string == NULL) {
		return NULL;
	} else if (string[0] == '\0') {
		return "";
	} else {
		return locale_to_utf8 (gettext (string));
	}
}

const char *
e_utf8_dgettext (const char *domain, const char *string)
{
	if (string == NULL) {
		return NULL;
	} else if (string[0] == '\0') {
		return "";
	} else {
		return locale_to_utf8 (dgettext (domain, string));
	}
}
