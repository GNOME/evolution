/*
 * e-spell-entry.c
 *
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
 */

/* This code is based on libsexy's SexySpellEntry */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <e-util/e-spell-checker.h>

#include "e-misc-utils.h"
#include "e-spell-entry.h"

struct _ESpellEntryPrivate {
	PangoAttrList *attr_list;
	gint mark_character;
	gint entry_scroll_offset;
	gboolean custom_checkers;
	gboolean checking_enabled;
	gchar **words;
	gint *word_starts;
	gint *word_ends;

	ESpellChecker *spell_checker;
	guint active_languages_handler_id;

	gboolean im_in_preedit;
};

enum {
	PROP_0,
	PROP_CHECKING_ENABLED,
	PROP_SPELL_CHECKER
};

G_DEFINE_TYPE_WITH_CODE (ESpellEntry, e_spell_entry, GTK_TYPE_ENTRY,
	G_ADD_PRIVATE (ESpellEntry)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static gboolean
word_misspelled (ESpellEntry *entry,
                 gint start,
                 gint end)
{
	const gchar *text;
	gchar *word;
	ESpellChecker *spell_checker;
	gboolean result;

	if (start == end)
		return FALSE;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	word = g_new0 (gchar, end - start + 2);

	g_strlcpy (word, text + start, end - start + 1);

	spell_checker = e_spell_entry_get_spell_checker (entry);
	result = !e_spell_checker_check_word (spell_checker, word, -1);

	g_free (word);

	return result;
}

static void
insert_underline (ESpellEntry *entry,
                  guint start,
                  guint end)
{
	PangoAttribute *ucolor;
	PangoAttribute *unline;

	ucolor = pango_attr_underline_color_new (65535, 0, 0);
	unline = pango_attr_underline_new (PANGO_UNDERLINE_ERROR);

	ucolor->start_index = start;
	unline->start_index = start;

	ucolor->end_index = end;
	unline->end_index = end;

	pango_attr_list_insert (entry->priv->attr_list, ucolor);
	pango_attr_list_insert (entry->priv->attr_list, unline);
}

static void
check_word (ESpellEntry *entry,
            gint start,
            gint end)
{
	PangoAttrIterator *it;

	/* Check to see if we've got any attributes at this position.
	 * If so, free them, since we'll read it if the word is misspelled */
	it = pango_attr_list_get_iterator (entry->priv->attr_list);

	if (it == NULL)
		return;
	do {
		gint s, e;
		pango_attr_iterator_range (it, &s, &e);
		if (s == start) {
			/* XXX What does this do? */
			GSList *attrs = pango_attr_iterator_get_attrs (it);
			g_slist_free_full (
				attrs, (GDestroyNotify)
				pango_attribute_destroy);
		}
	} while (pango_attr_iterator_next (it));
	pango_attr_iterator_destroy (it);

	if (word_misspelled (entry, start, end))
		insert_underline (entry, start, end);
}

static void
spell_entry_recheck_all (ESpellEntry *entry)
{
	GtkWidget *widget = GTK_WIDGET (entry);
	PangoLayout *layout;
	gint length, i;
	gboolean check_words = FALSE;

	if (entry->priv->words == NULL)
		return;

	/* Remove all existing pango attributes.
	 * These will get read as we check. */
	pango_attr_list_unref (entry->priv->attr_list);
	entry->priv->attr_list = pango_attr_list_new ();

	if (e_spell_entry_get_checking_enabled (entry)) {
		ESpellChecker *spell_checker;

		spell_checker = e_spell_entry_get_spell_checker (entry);
		if (e_spell_checker_count_active_languages (spell_checker) > 0)
			check_words = TRUE;
	}

	if (check_words) {
		/* Loop through words */
		for (i = 0; entry->priv->words[i]; i++) {
			length = strlen (entry->priv->words[i]);
			if (length == 0)
				continue;
			check_word (
				entry,
				entry->priv->word_starts[i],
				entry->priv->word_ends[i]);
		}

		layout = gtk_entry_get_layout (GTK_ENTRY (entry));
		pango_layout_set_attributes (layout, entry->priv->attr_list);
	}

	if (gtk_widget_get_realized (widget))
		gtk_widget_queue_draw (widget);
}

static void
get_word_extents_from_position (ESpellEntry *entry,
                                gint *start,
                                gint *end,
                                guint position)
{
	const gchar *text;
	gint i, bytes_pos;

	*start = -1;
	*end = -1;

	if (entry->priv->words == NULL)
		return;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	bytes_pos = (gint) (g_utf8_offset_to_pointer (text, position) - text);

	for (i = 0; entry->priv->words[i]; i++) {
		if (bytes_pos >= entry->priv->word_starts[i] &&
		    bytes_pos <= entry->priv->word_ends[i]) {
			*start = entry->priv->word_starts[i];
			*end = entry->priv->word_ends[i];
			return;
		}
	}
}

static void
spell_entry_store_word (gchar ***set,
			gint **starts,
			gint **ends,
			const gchar *text,
			gint n_word,
			gint n_strings,
			const gchar *word_start,
			const gchar *word_end)
{
	gint bytes;

	g_return_if_fail (n_word >= 0);
	g_return_if_fail (n_word < n_strings);

	/* Copy sub-string */
	bytes = (gint) (word_end - word_start);
	(*set)[n_word] = g_new0 (gchar, bytes + 1);
	(*starts)[n_word] = (gint) (word_start - text);
	(*ends)[n_word] = (gint) (word_start - text + bytes);
	memcpy ((*set)[n_word], word_start, bytes);
}

static gboolean
entry_is_word_char (gunichar uc,
		    gboolean has_en_language)
{
	return (uc == L'\'' && has_en_language) ||
		g_unichar_isalnum (uc) ||
		g_unichar_ismark (uc);
}

static void
entry_strsplit_utf8 (ESpellEntry *entry,
                     gchar ***set,
                     gint **starts,
                     gint **ends)
{
	const gchar *text, *ptr, *word_start;
	gint n_strings, n_word;
	gchar **active_languages;
	guint n_languages, ii;
	gboolean has_en_language = FALSE;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	g_return_if_fail (g_utf8_validate (text, -1, NULL));

	active_languages = e_spell_checker_list_active_languages (entry->priv->spell_checker, &n_languages);
	for (ii = 0; active_languages && ii < n_languages && !has_en_language; ii++) {
		has_en_language =
			g_ascii_strncasecmp (active_languages[ii], "en", 2) == 0 &&
			(!active_languages[ii][2] || active_languages[ii][2] == '_');
	}

	g_strfreev (active_languages);

	/* Find how many words we have */
	n_strings = 0;
	word_start = NULL;
	for (ptr = text; *ptr; ptr = g_utf8_next_char (ptr)) {
		if (!entry_is_word_char (g_utf8_get_char (ptr), has_en_language)) {
			word_start = NULL;
		} else if (!word_start) {
			n_strings++;
			word_start = ptr;
		}
	}

	*set = g_new0 (gchar *, n_strings + 1);
	*starts = g_new0 (gint, n_strings + 1);
	*ends = g_new0 (gint, n_strings + 1);

	/* Copy out strings */
	word_start = NULL;
	n_word = -1;
	for (ptr = text; *ptr; ptr = g_utf8_next_char (ptr)) {
		if (!entry_is_word_char (g_utf8_get_char (ptr), has_en_language)) {
			if (word_start)
				spell_entry_store_word (set, starts, ends, text, n_word, n_strings, word_start, ptr);
			word_start = NULL;
		} else if (!word_start) {
			n_word++;
			word_start = ptr;
		}
	}

	if (word_start)
		spell_entry_store_word (set, starts, ends, text, n_word, n_strings, word_start, ptr);
}

static gchar *
spell_entry_get_chars_from_byte_pos (ESpellEntry *entry,
				     gint byte_pos_start,
				     gint byte_pos_end)
{
	const gchar *text;
	gint len;

	g_return_val_if_fail (E_IS_SPELL_ENTRY (entry), NULL);
	g_return_val_if_fail (byte_pos_start <= byte_pos_end, NULL);

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (!text)
		return NULL;

	len = strlen (text);

	if (byte_pos_start < 0)
		byte_pos_start = 0;

	if (byte_pos_end > len)
		byte_pos_end = len;

	if (byte_pos_end < 0)
		byte_pos_end = 0;

	return g_strndup (text + byte_pos_start, byte_pos_end - byte_pos_start);
}

static void
spell_entry_byte_pos_to_char_pos (ESpellEntry *entry,
				  gint byte_pos,
				  gint *out_char_pos)
{
	const gchar *text, *ptr;

	g_return_if_fail (E_IS_SPELL_ENTRY (entry));
	g_return_if_fail (out_char_pos != NULL);

	*out_char_pos = 0;

	if (byte_pos <= 0)
		return;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (!text || !g_utf8_validate (text, -1, NULL))
		return;

	for (ptr = text; ptr && *ptr; ptr = g_utf8_next_char (ptr)) {
		if (byte_pos <= ptr - text)
			break;

		*out_char_pos = (*out_char_pos) + 1;
	}
}

static void
add_to_dictionary (GtkWidget *menuitem,
                   ESpellEntry *entry)
{
	gchar *word;
	gint start, end;
	ESpellDictionary *dict;

	get_word_extents_from_position (
		entry, &start, &end, entry->priv->mark_character);
	word = spell_entry_get_chars_from_byte_pos (entry, start, end);

	dict = g_object_get_data (G_OBJECT (menuitem), "spell-entry-checker");
	if (dict != NULL)
		e_spell_dictionary_learn_word (dict, word, -1);

	g_free (word);

	if (entry->priv->words != NULL) {
		g_strfreev (entry->priv->words);
		g_free (entry->priv->word_starts);
		g_free (entry->priv->word_ends);
	}

	entry_strsplit_utf8 (
		entry,
		&entry->priv->words,
		&entry->priv->word_starts,
		&entry->priv->word_ends);

	spell_entry_recheck_all (entry);
}

static void
ignore_all (GtkWidget *menuitem,
            ESpellEntry *entry)
{
	ESpellChecker *spell_checker;
	gchar *word;
	gint start, end;

	get_word_extents_from_position (
		entry, &start, &end, entry->priv->mark_character);
	word = spell_entry_get_chars_from_byte_pos (entry, start, end);

	spell_checker = e_spell_entry_get_spell_checker (entry);
	e_spell_checker_ignore_word (spell_checker, word);

	g_free (word);

	if (entry->priv->words != NULL) {
		g_strfreev (entry->priv->words);
		g_free (entry->priv->word_starts);
		g_free (entry->priv->word_ends);
	}

	entry_strsplit_utf8 (
		entry,
		&entry->priv->words,
		&entry->priv->word_starts,
		&entry->priv->word_ends);

	spell_entry_recheck_all (entry);
}

static void
replace_word (GtkWidget *menuitem,
              ESpellEntry *entry)
{
	gchar *oldword;
	const gchar *newword;
	gint start, end;
	gint cursor;
	ESpellDictionary *dict;

	get_word_extents_from_position (
		entry, &start, &end, entry->priv->mark_character);
	oldword = spell_entry_get_chars_from_byte_pos (entry, start, end);
	newword = gtk_label_get_text (
		GTK_LABEL (gtk_bin_get_child (GTK_BIN (menuitem))));

	spell_entry_byte_pos_to_char_pos (entry, start, &start);
	spell_entry_byte_pos_to_char_pos (entry, end, &end);

	cursor = gtk_editable_get_position (GTK_EDITABLE (entry));
	/* is the cursor at the end? If so, restore it there */
	if (g_utf8_strlen (gtk_entry_get_text (GTK_ENTRY (entry)), -1) == cursor)
		cursor = -1;
	else if (cursor > start && cursor <= end)
		cursor = start;

	gtk_editable_delete_text (GTK_EDITABLE (entry), start, end);
	gtk_editable_set_position (GTK_EDITABLE (entry), start);
	gtk_editable_insert_text (
		GTK_EDITABLE (entry), newword, strlen (newword), &start);
	gtk_editable_set_position (GTK_EDITABLE (entry), cursor);

	dict = g_object_get_data (G_OBJECT (menuitem), "spell-entry-checker");

	if (dict != NULL)
		e_spell_dictionary_store_correction (
			dict, oldword, -1, newword, -1);

	g_free (oldword);
}

static void
build_suggestion_menu (ESpellEntry *entry,
                       GtkWidget *menu,
                       ESpellDictionary *dict,
                       const gchar *word)
{
	GtkWidget *mi;
	GList *suggestions, *iter;

	suggestions = e_spell_dictionary_get_suggestions (dict, word, -1);

	if (suggestions == NULL) {
		/* no suggestions. Put something in the menu anyway... */
		GtkWidget *label = gtk_label_new (_("(no suggestions)"));
		PangoAttribute *attribute;
		PangoAttrList *attribute_list;

		attribute_list = pango_attr_list_new ();
		attribute = pango_attr_style_new (PANGO_STYLE_ITALIC);
		pango_attr_list_insert (attribute_list, attribute);
		gtk_label_set_attributes (GTK_LABEL (label), attribute_list);
		pango_attr_list_unref (attribute_list);

		mi = gtk_separator_menu_item_new ();
		gtk_container_add (GTK_CONTAINER (mi), label);
		gtk_widget_show_all (mi);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), mi);
	} else {
		gint ii = 0;

		/* build a set of menus with suggestions */
		for (iter = suggestions; iter; iter = g_list_next (iter), ii++) {
			if ((ii != 0) && (ii % 10 == 0)) {
				mi = gtk_separator_menu_item_new ();
				gtk_widget_show (mi);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

				mi = gtk_menu_item_new_with_label (_("More…"));
				gtk_widget_show (mi);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

				menu = gtk_menu_new ();
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);
			}

			mi = gtk_menu_item_new_with_label (iter->data);
			g_object_set_data (G_OBJECT (mi), "spell-entry-checker", dict);
			g_signal_connect (mi, "activate", G_CALLBACK (replace_word), entry);
			gtk_widget_show (mi);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
		}
	}

	g_list_free_full (suggestions, (GDestroyNotify) g_free);
}

static GtkWidget *
build_spelling_menu (ESpellEntry *entry,
                     const gchar *word)
{
	ESpellChecker *spell_checker;
	ESpellDictionary *dict;
	GtkWidget *topmenu, *mi;
	GQueue queue = G_QUEUE_INIT;
	gchar **active_languages;
	guint ii, n_active_languages;
	gchar *label;

	topmenu = gtk_menu_new ();

	spell_checker = e_spell_entry_get_spell_checker (entry);

	active_languages = e_spell_checker_list_active_languages (
		spell_checker, &n_active_languages);
	for (ii = 0; ii < n_active_languages; ii++) {
		dict = e_spell_checker_ref_dictionary (
			spell_checker, active_languages[ii]);
		if (dict != NULL)
			g_queue_push_tail (&queue, dict);
	}
	g_strfreev (active_languages);

	if (g_queue_is_empty (&queue))
		goto exit;

	/* Suggestions */
	if (n_active_languages == 1) {
		dict = g_queue_peek_head (&queue);
		build_suggestion_menu (entry, topmenu, dict, word);
	} else {
		GtkWidget *menu;
		GList *list, *link;

		list = g_queue_peek_head_link (&queue);

		for (link = list; link != NULL; link = g_list_next (link)) {
			const gchar *lang_name;

			dict = E_SPELL_DICTIONARY (link->data);

			lang_name = e_spell_dictionary_get_name (dict);
			if (lang_name == NULL)
				lang_name = e_spell_dictionary_get_code (dict);
			if (lang_name == NULL)
				lang_name = "???";

			mi = gtk_menu_item_new_with_label (lang_name);

			gtk_widget_show (mi);
			gtk_menu_shell_append (GTK_MENU_SHELL (topmenu), mi);
			menu = gtk_menu_new ();
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);
			build_suggestion_menu (entry, menu, dict, word);
		}
	}

	/* Separator */
	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (topmenu), mi);

	/* + Add to Dictionary */
	label = g_strdup_printf (_("Add “%s” to Dictionary"), word);
	mi = gtk_image_menu_item_new_with_label (label);
	g_free (label);

	gtk_image_menu_item_set_image (
		GTK_IMAGE_MENU_ITEM (mi),
		gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU));

	if (n_active_languages == 1) {
		dict = g_queue_peek_head (&queue);
		g_object_set_data (G_OBJECT (mi), "spell-entry-checker", dict);
		g_signal_connect (
			mi, "activate",
			G_CALLBACK (add_to_dictionary), entry);
	} else {
		GtkWidget *menu, *submi;
		GList *list, *link;

		menu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

		list = g_queue_peek_head_link (&queue);

		for (link = list; link != NULL; link = g_list_next (link)) {
			const gchar *lang_name;

			dict = E_SPELL_DICTIONARY (link->data);

			lang_name = e_spell_dictionary_get_name (dict);
			if (lang_name == NULL)
				lang_name = e_spell_dictionary_get_code (dict);
			if (lang_name == NULL)
				lang_name = "???";

			submi = gtk_menu_item_new_with_label (lang_name);
			g_object_set_data (G_OBJECT (submi), "spell-entry-checker", dict);
			g_signal_connect (
				submi, "activate",
				G_CALLBACK (add_to_dictionary), entry);

			gtk_widget_show (submi);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), submi);
		}
	}

	gtk_widget_show_all (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (topmenu), mi);

	/* - Ignore All */
	mi = gtk_image_menu_item_new_with_label (_("Ignore All"));
	gtk_image_menu_item_set_image (
		GTK_IMAGE_MENU_ITEM (mi),
		gtk_image_new_from_icon_name ("list-remove", GTK_ICON_SIZE_MENU));
	g_signal_connect (mi, "activate", G_CALLBACK (ignore_all), entry);
	gtk_widget_show_all (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (topmenu), mi);

exit:
	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	return topmenu;
}

static void
spell_entry_add_suggestions_menu (ESpellEntry *entry,
                                  GtkMenu *menu,
                                  const gchar *word)
{
	GtkWidget *icon, *mi;

	g_return_if_fail (menu != NULL);
	g_return_if_fail (word != NULL);

	/* separator */
	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), mi);

	/* Above the separator, show the suggestions menu */
	icon = gtk_image_new_from_icon_name ("tools-check-spelling", GTK_ICON_SIZE_MENU);
	mi = gtk_image_menu_item_new_with_label (_("Spelling Suggestions"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), icon);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), build_spelling_menu (entry, word));

	gtk_widget_show_all (mi);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), mi);
}

static gboolean
spell_entry_popup_menu (ESpellEntry *entry)
{
	/* Menu popped up from a keybinding (menu key or <shift>+F10).
	 * Use the cursor position as the mark position. */
	entry->priv->mark_character =
		gtk_editable_get_position (GTK_EDITABLE (entry));

	return FALSE;
}

static void
spell_entry_populate_popup (ESpellEntry *entry,
                            GtkMenu *menu,
                            gpointer data)
{
	ESpellChecker *spell_checker;
	gint start, end;
	gchar *word;

	spell_checker = e_spell_entry_get_spell_checker (entry);
	if (e_spell_checker_count_active_languages (spell_checker) == 0)
		return;

	get_word_extents_from_position (
		entry, &start, &end, entry->priv->mark_character);
	if (start == end)
		return;

	if (!word_misspelled (entry, start, end))
		return;

	word = spell_entry_get_chars_from_byte_pos (entry, start, end);
	g_return_if_fail (word != NULL);

	spell_entry_add_suggestions_menu (entry, menu, word);

	g_free (word);
}

static void
spell_entry_changed (GtkEditable *editable)
{
	ESpellEntry *entry = E_SPELL_ENTRY (editable);
	ESpellChecker *spell_checker;

	spell_checker = e_spell_entry_get_spell_checker (entry);
	if (e_spell_checker_count_active_languages (spell_checker) == 0)
		return;

	if (entry->priv->words != NULL) {
		g_strfreev (entry->priv->words);
		g_free (entry->priv->word_starts);
		g_free (entry->priv->word_ends);
	}

	entry_strsplit_utf8 (
		entry,
		&entry->priv->words,
		&entry->priv->word_starts,
		&entry->priv->word_ends);

	spell_entry_recheck_all (entry);
}

static void
spell_entry_notify_scroll_offset (ESpellEntry *spell_entry)
{
	g_object_get (
		G_OBJECT (spell_entry), "scroll-offset",
		&spell_entry->priv->entry_scroll_offset, NULL);
}

static gint
spell_entry_find_position (ESpellEntry *spell_entry,
                           gint x)
{
	PangoLayout *layout;
	PangoLayoutLine *line;
	gint index;
	gint pos;
	gint trailing;
	const gchar *text;
	GtkEntry *entry = GTK_ENTRY (spell_entry);

	layout = gtk_entry_get_layout (entry);
	text = pango_layout_get_text (layout);

	line = pango_layout_get_lines_readonly (layout)->data;
	pango_layout_line_x_to_index (line, x * PANGO_SCALE, &index, &trailing);

	pos = g_utf8_pointer_to_offset (text, text + index);
	pos += trailing;

	return pos;
}

static void
spell_entry_active_languages_cb (ESpellChecker *spell_checker,
                                 GParamSpec *pspec,
                                 ESpellEntry *spell_entry)
{
	if (gtk_widget_get_realized (GTK_WIDGET (spell_entry)))
		spell_entry_recheck_all (spell_entry);
}

static void
spell_entry_preedit_changed_cb (ESpellEntry *spell_entry,
				const gchar *preedit_text,
				gpointer user_data)
{
	g_return_if_fail (E_IS_SPELL_ENTRY (spell_entry));

	spell_entry->priv->im_in_preedit = preedit_text && *preedit_text;
}

static void
spell_entry_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHECKING_ENABLED:
			e_spell_entry_set_checking_enabled (
				E_SPELL_ENTRY (object),
				g_value_get_boolean (value));
			return;

		case PROP_SPELL_CHECKER:
			e_spell_entry_set_spell_checker (
				E_SPELL_ENTRY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_entry_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHECKING_ENABLED:
			g_value_set_boolean (
				value,
				e_spell_entry_get_checking_enabled (
				E_SPELL_ENTRY (object)));
			return;

		case PROP_SPELL_CHECKER:
			g_value_set_object (
				value,
				e_spell_entry_get_spell_checker (
				E_SPELL_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_entry_dispose (GObject *object)
{
	ESpellEntry *self = E_SPELL_ENTRY (object);

	if (self->priv->active_languages_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->spell_checker,
			self->priv->active_languages_handler_id);
		self->priv->active_languages_handler_id = 0;
	}

	g_clear_object (&self->priv->spell_checker);
	g_clear_pointer (&self->priv->attr_list, pango_attr_list_unref);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_spell_entry_parent_class)->dispose (object);
}

static void
spell_entry_finalize (GObject *object)
{
	ESpellEntry *self = E_SPELL_ENTRY (object);

	g_strfreev (self->priv->words);
	g_free (self->priv->word_starts);
	g_free (self->priv->word_ends);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_spell_entry_parent_class)->finalize (object);
}

static void
spell_entry_constructed (GObject *object)
{
	ESpellEntry *spell_entry;
	ESpellChecker *spell_checker;

	spell_entry = E_SPELL_ENTRY (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_spell_entry_parent_class)->constructed (object);

	g_signal_connect (spell_entry, "preedit-changed", G_CALLBACK (spell_entry_preedit_changed_cb), NULL);

	/* Install a default spell checker if there is not one already. */
	spell_checker = e_spell_entry_get_spell_checker (spell_entry);
	if (spell_checker == NULL) {
		spell_checker = e_spell_checker_new ();
		e_spell_entry_set_spell_checker (spell_entry, spell_checker);
		g_object_unref (spell_checker);
	}

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static gboolean
spell_entry_draw (GtkWidget *widget,
                  cairo_t *cr)
{
	ESpellEntry *spell_entry = E_SPELL_ENTRY (widget);

	if (!spell_entry->priv->im_in_preedit) {
		GtkEntry *entry = GTK_ENTRY (widget);
		PangoLayout *layout;

		layout = gtk_entry_get_layout (entry);
		pango_layout_set_attributes (layout, spell_entry->priv->attr_list);
	}

	/* Chain up to parent's draw() method. */
	return GTK_WIDGET_CLASS (e_spell_entry_parent_class)->
		draw (widget, cr);
}

static gboolean
spell_entry_button_press (GtkWidget *widget,
                          GdkEventButton *event)
{
	ESpellEntry *spell_entry = E_SPELL_ENTRY (widget);

	spell_entry->priv->mark_character = spell_entry_find_position (
		spell_entry, event->x + spell_entry->priv->entry_scroll_offset);

	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_spell_entry_parent_class)->
		button_press_event (widget, event);
}

static void
e_spell_entry_class_init (ESpellEntryClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = spell_entry_set_property;
	object_class->get_property = spell_entry_get_property;
	object_class->dispose = spell_entry_dispose;
	object_class->finalize = spell_entry_finalize;
	object_class->constructed = spell_entry_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->draw = spell_entry_draw;
	widget_class->button_press_event = spell_entry_button_press;

	g_object_class_install_property (
		object_class,
		PROP_CHECKING_ENABLED,
		g_param_spec_boolean (
			"checking-enabled",
			"checking enabled",
			"Spell Checking is Enabled",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SPELL_CHECKER,
		g_param_spec_object (
			"spell-checker",
			"Spell Checker",
			"The spell checker object",
			E_TYPE_SPELL_CHECKER,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_spell_entry_init (ESpellEntry *spell_entry)
{
	spell_entry->priv = e_spell_entry_get_instance_private (spell_entry);
	spell_entry->priv->attr_list = pango_attr_list_new ();
	spell_entry->priv->checking_enabled = TRUE;
	spell_entry->priv->im_in_preedit = FALSE;

	g_signal_connect (
		spell_entry, "popup-menu",
		G_CALLBACK (spell_entry_popup_menu), NULL);
	g_signal_connect (
		spell_entry, "populate-popup",
		G_CALLBACK (spell_entry_populate_popup), NULL);
	g_signal_connect (
		spell_entry, "changed",
		G_CALLBACK (spell_entry_changed), NULL);
	e_signal_connect_notify (
		spell_entry, "notify::scroll-offset",
		G_CALLBACK (spell_entry_notify_scroll_offset), NULL);
}

GtkWidget *
e_spell_entry_new (void)
{
	return g_object_new (E_TYPE_SPELL_ENTRY, NULL);
}

gboolean
e_spell_entry_get_checking_enabled (ESpellEntry *spell_entry)
{
	g_return_val_if_fail (E_IS_SPELL_ENTRY (spell_entry), FALSE);

	return spell_entry->priv->checking_enabled;
}

void
e_spell_entry_set_checking_enabled (ESpellEntry *spell_entry,
                                    gboolean enable_checking)
{
	g_return_if_fail (E_IS_SPELL_ENTRY (spell_entry));

	if (spell_entry->priv->checking_enabled == enable_checking)
		return;

	spell_entry->priv->checking_enabled = enable_checking;
	spell_entry_recheck_all (spell_entry);

	g_object_notify (G_OBJECT (spell_entry), "checking-enabled");
}

/**
 * e_spell_entry_get_spell_checker:
 * @spell_entry: an #ESpellEntry
 *
 * Returns the #ESpellChecker being used for spell checking.  By default,
 * #ESpellEntry creates its own #ESpellChecker, but this can be overridden
 * through e_spell_entry_set_spell_checker().
 *
 * Returns: an #ESpellChecker
 **/
ESpellChecker *
e_spell_entry_get_spell_checker (ESpellEntry *spell_entry)
{
	g_return_val_if_fail (E_IS_SPELL_ENTRY (spell_entry), NULL);

	return spell_entry->priv->spell_checker;
}

/**
 * e_spell_entry_set_spell_checker:
 * @spell_entry: an #ESpellEntry
 * @spell_checker: an #ESpellChecker
 *
 * Sets the #ESpellChecker to use for spell checking.  By default,
 * #ESpellEntry creates its own #ESpellChecker.  This function can be
 * useful for sharing an #ESpellChecker across multiple spell-checking
 * widgets, so the active spell checking languages stay synchronized.
 **/
void
e_spell_entry_set_spell_checker (ESpellEntry *spell_entry,
                                 ESpellChecker *spell_checker)
{
	gulong handler_id;

	g_return_if_fail (E_IS_SPELL_ENTRY (spell_entry));
	g_return_if_fail (E_IS_SPELL_CHECKER (spell_checker));

	if (spell_checker == spell_entry->priv->spell_checker)
		return;

	if (spell_entry->priv->spell_checker != NULL) {
		g_signal_handler_disconnect (
			spell_entry->priv->spell_checker,
			spell_entry->priv->active_languages_handler_id);
		g_object_unref (spell_entry->priv->spell_checker);
	}

	spell_entry->priv->spell_checker = g_object_ref (spell_checker);

	handler_id = g_signal_connect (
		spell_checker, "notify::active-languages",
		G_CALLBACK (spell_entry_active_languages_cb),
		spell_entry);

	spell_entry->priv->active_languages_handler_id = handler_id;

	g_object_notify (G_OBJECT (spell_entry), "spell-checker");

	if (gtk_widget_get_realized (GTK_WIDGET (spell_entry)))
		spell_entry_recheck_all (spell_entry);
}

