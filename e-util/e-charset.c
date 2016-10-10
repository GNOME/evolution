/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-charset.h"

#include <string.h>
#include <iconv.h>

#include <glib/gi18n-lib.h>

typedef enum {
	E_CHARSET_UNKNOWN,
	E_CHARSET_ARABIC,
	E_CHARSET_BALTIC,
	E_CHARSET_CENTRAL_EUROPEAN,
	E_CHARSET_CHINESE,
	E_CHARSET_CYRILLIC,
	E_CHARSET_GREEK,
	E_CHARSET_HEBREW,
	E_CHARSET_JAPANESE,
	E_CHARSET_KOREAN,
	E_CHARSET_THAI,
	E_CHARSET_TURKISH,
	E_CHARSET_UNICODE,
	E_CHARSET_WESTERN_EUROPEAN,
	E_CHARSET_WESTERN_EUROPEAN_NEW
} ECharsetClass;

static const gchar *classnames[] = {
	N_("Unknown"),
	N_("Arabic"),
	N_("Baltic"),
	N_("Central European"),
	N_("Chinese"),
	N_("Cyrillic"),
	N_("Greek"),
	N_("Hebrew"),
	N_("Japanese"),
	N_("Korean"),
	N_("Thai"),
	N_("Turkish"),
	N_("Unicode"),
	N_("Western European"),
	N_("Western European, New"),
};

typedef struct {
	const gchar *name;
	ECharsetClass class;
	const gchar *subclass;
} ECharset;

/* This list is based on what other mailers/browsers support. There's
 * not a lot of point in using, say, ISO-8859-3, if anything that can
 * read that can read UTF8 too.
 */
static ECharset charsets[] = {
	{ "ISO-8859-6", E_CHARSET_ARABIC, NULL },
	{ "ISO-8859-13", E_CHARSET_BALTIC, NULL },
	{ "ISO-8859-4", E_CHARSET_BALTIC, NULL },
	{ "ISO-8859-2", E_CHARSET_CENTRAL_EUROPEAN, NULL },
	/* Translators: Character set "Chinese, Traditional" */
	{ "Big5", E_CHARSET_CHINESE, N_("Traditional") },
	/* Translators: Character set "Chinese, Traditional" */
	{ "BIG5HKSCS", E_CHARSET_CHINESE, N_("Traditional") },
	/* Translators: Character set "Chinese, Traditional" */
	{ "EUC-TW", E_CHARSET_CHINESE, N_("Traditional") },
	/* Translators: Character set "Chinese, Simplified" */
	{ "GB18030", E_CHARSET_CHINESE, N_("Simplified") },
	/* Translators: Character set "Chinese, Simplified" */
	{ "GB2312", E_CHARSET_CHINESE, N_("Simplified") },
	/* Translators: Character set "Chinese, Simplified" */
	{ "HZ", E_CHARSET_CHINESE, N_("Simplified") },
	/* Translators: Character set "Chinese, Simplified" */
	{ "ISO-2022-CN", E_CHARSET_CHINESE, N_("Simplified") },
	{ "KOI8-R", E_CHARSET_CYRILLIC, NULL },
	{ "Windows-1251", E_CHARSET_CYRILLIC, NULL },
	/* Translators: Character set "Cyrillic, Ukrainian" */
	{ "KOI8-U", E_CHARSET_CYRILLIC, N_("Ukrainian") },
	{ "ISO-8859-5", E_CHARSET_CYRILLIC, NULL },
	{ "ISO-8859-7", E_CHARSET_GREEK, NULL },
	/* Translators: Character set "Hebrew, Visual" */
	{ "ISO-8859-8", E_CHARSET_HEBREW, N_("Visual") },
	{ "ISO-2022-JP", E_CHARSET_JAPANESE, NULL },
	{ "EUC-JP", E_CHARSET_JAPANESE, NULL },
	{ "Shift_JIS", E_CHARSET_JAPANESE, NULL },
	{ "EUC-KR", E_CHARSET_KOREAN, NULL },
	{ "TIS-620", E_CHARSET_THAI, NULL },
	{ "ISO-8859-9", E_CHARSET_TURKISH, NULL },
	{ "UTF-8", E_CHARSET_UNICODE, NULL },
	{ "UTF-7", E_CHARSET_UNICODE, NULL },
	{ "ISO-8859-1", E_CHARSET_WESTERN_EUROPEAN, NULL },
	{ "ISO-8859-15", E_CHARSET_WESTERN_EUROPEAN_NEW, NULL },
};

/**
 * e_charset_add_radio_actions:
 * @action_group: a #GtkActionGroup
 * @action_prefix: a prefix for action names, or %NULL
 * @default_charset: the default character set, or %NULL to use the
 *                   locale character set
 * @callback: a callback function for actions in the group, or %NULL
 * @user_data: user data to be passed to @callback, or %NULL
 *
 * Adds a set of #GtkRadioActions for available character sets to
 * @action_group.  The @default_charset (or locale character set if
 * @default_charset is %NULL) will be added first, and selected by
 * default (except that ISO-8859-1 will always be used instead of
 * US-ASCII).  Any other character sets of the same language class as
 * the default will be added next, followed by the remaining character
 * sets.
 *
 * Returns: the radio action group
 **/
GSList *
e_charset_add_radio_actions (GtkActionGroup *action_group,
                             const gchar *action_prefix,
                             const gchar *default_charset,
                             GCallback callback,
                             gpointer user_data)
{
	GtkRadioAction *action = NULL;
	GSList *group = NULL;
	const gchar *locale_charset;
	gint def, ii;

	g_return_val_if_fail (GTK_IS_ACTION_GROUP (action_group), NULL);

	if (action_prefix == NULL)
		action_prefix = "";

	g_get_charset (&locale_charset);
	if (!g_ascii_strcasecmp (locale_charset, "US-ASCII"))
		locale_charset = "ISO-8859-1";

	if (default_charset == NULL)
		default_charset = locale_charset;
	for (def = 0; def < G_N_ELEMENTS (charsets); def++)
		if (!g_ascii_strcasecmp (charsets[def].name, default_charset))
			break;

	for (ii = 0; ii < G_N_ELEMENTS (charsets); ii++) {
		const gchar *charset_name;
		gchar *action_name;
		gchar *escaped_name;
		gchar *charset_label;
		gchar **str_array;

		charset_name = charsets[ii].name;
		action_name = g_strconcat (action_prefix, charset_name, NULL);

		/* Escape underlines in the character set name so
		 * they're not treated as GtkLabel mnemonics. */
		str_array = g_strsplit (charset_name, "_", -1);
		escaped_name = g_strjoinv ("__", str_array);
		g_strfreev (str_array);

		if (charsets[ii].subclass != NULL)
			charset_label = g_strdup_printf (
				"%s, %s (%s)",
				gettext (classnames[charsets[ii].class]),
				gettext (charsets[ii].subclass),
				escaped_name);
		else if (charsets[ii].class != E_CHARSET_UNKNOWN)
			charset_label = g_strdup_printf (
				"%s (%s)",
				gettext (classnames[charsets[ii].class]),
				escaped_name);
		else
			charset_label = g_strdup (escaped_name);

		/* XXX Add a tooltip! */
		action = gtk_radio_action_new (
			action_name, charset_label, NULL, NULL, ii);

		/* Character set name is static so no need to free it. */
		g_object_set_data (
			G_OBJECT (action), "charset",
			(gpointer) charset_name);

		gtk_radio_action_set_group (action, group);
		group = gtk_radio_action_get_group (action);

		if (callback != NULL)
			g_signal_connect (
				action, "changed", callback, user_data);

		gtk_action_group_add_action (
			action_group, GTK_ACTION (action));

		g_object_unref (action);

		g_free (action_name);
		g_free (escaped_name);
		g_free (charset_label);
	}

	if (def == G_N_ELEMENTS (charsets)) {
		const gchar *charset_name;
		gchar *action_name;
		gchar *charset_label;
		gchar **str_array;

		charset_name = default_charset;
		action_name = g_strconcat (action_prefix, charset_name, NULL);

		/* Escape underlines in the character set name so
		 * they're not treated as GtkLabel mnemonics. */
		str_array = g_strsplit (charset_name, "_", -1);
		charset_label = g_strjoinv ("__", str_array);
		g_strfreev (str_array);

		/* XXX Add a tooltip! */
		action = gtk_radio_action_new (
			action_name, charset_label, NULL, NULL, def);

		/* Character set name may NOT be static,
		 * so we do need to duplicate the string. */
		g_object_set_data_full (
			G_OBJECT (action), "charset",
			g_strdup (charset_name),
			(GDestroyNotify) g_free);

		gtk_radio_action_set_group (action, group);
		group = gtk_radio_action_get_group (action);

		if (callback != NULL)
			g_signal_connect (
				action, "changed", callback, user_data);

		gtk_action_group_add_action (
			action_group, GTK_ACTION (action));

		g_object_unref (action);

		g_free (action_name);
		g_free (charset_label);
	}

	/* Any of the actions in the action group will do. */
	if (action != NULL)
		gtk_radio_action_set_current_value (action, def);

	return group;
}
