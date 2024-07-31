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

static gchar *
e_charset_labelize (const ECharset *charset)
{
	gchar *escaped_name;
	gchar *charset_label;
	gchar **str_array;

	/* Escape underlines in the character set name so
	 * they're not treated as GtkLabel mnemonics. */
	str_array = g_strsplit (charset->name, "_", -1);
	escaped_name = g_strjoinv ("__", str_array);
	g_strfreev (str_array);

	if (charset->subclass != NULL)
		charset_label = g_strdup_printf (
			"%s, %s (%s)",
			gettext (classnames[charset->class]),
			gettext (charset->subclass),
			escaped_name);
	else if (charsets->class != E_CHARSET_UNKNOWN)
		charset_label = g_strdup_printf (
			"%s (%s)",
			gettext (classnames[charset->class]),
			escaped_name);
	else
		charset_label = g_steal_pointer (&escaped_name);

	g_free (escaped_name);

	return charset_label;
}

/**
 * e_charset_add_to_g_menu:
 * @menu: a #GMenu to add the character sets to
 * @action_name: what action name should be used
 *
 * Adds a new section with all predefined character sets into the @menu,
 * naming all of them as the @action_name, only with different target,
 * thus they will construct a radio menu.
 *
 * This does not add a "Default" option.
 *
 * Since: 3.56
 **/
void
e_charset_add_to_g_menu (GMenu *menu,
			 const gchar *action_name)
{
	GMenu *section;
	gint ii;

	g_return_if_fail (G_IS_MENU (menu));
	g_return_if_fail (action_name != NULL);

	section = g_menu_new ();

	for (ii = 0; ii < G_N_ELEMENTS (charsets); ii++) {
		GMenuItem *menu_item;
		const gchar *charset_name;
		gchar *charset_label;

		charset_name = charsets[ii].name;
		charset_label = e_charset_labelize (&(charsets[ii]));

		menu_item = g_menu_item_new (charset_label, NULL);
		g_menu_item_set_action_and_target (menu_item, action_name, "s", charset_name);
		g_menu_append_item (section, menu_item);

		g_object_unref (menu_item);

		g_free (charset_label);
	}

	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));

	g_clear_object (&section);
}

/**
 * e_charset_create_list_store:
 *
 * Creates a new #GtkListStore containing two columns, E_CHARSET_COLUMN_LABEL with the label
 * of the charset, to be shown tin the GUI, and E_CHARSET_COLUMN_VALUE with the actual
 * charset value.
 *
 * Returns: (transfer full): newly created #GtkListStore
 *
 * Since: 3.56
 **/
GtkListStore *
e_charset_create_list_store (void)
{
	GtkListStore *list_store;
	guint ii;

	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	for (ii = 0; ii < G_N_ELEMENTS (charsets); ii++) {
		GtkTreeIter iter;
		const gchar *charset_name;
		gchar *charset_label;

		charset_name = charsets[ii].name;
		charset_label = e_charset_labelize (&(charsets[ii]));

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
			E_CHARSET_COLUMN_LABEL, charset_label,
			E_CHARSET_COLUMN_VALUE, charset_name,
			-1);

		g_free (charset_label);
	}

	return list_store;
}
