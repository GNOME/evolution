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

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <editor/gtkhtml-spell-language.h>
#include <editor/gtkhtml-spell-checker.h>

#include "e-misc-utils.h"
#include "e-spell-entry.h"

#define E_SPELL_ENTRY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SPELL_ENTRY, ESpellEntryPrivate))

struct _ESpellEntryPrivate {
	PangoAttrList *attr_list;
	gint mark_character;
	gint entry_scroll_offset;
	GSettings *settings;
	gboolean custom_checkers;
	gboolean checking_enabled;
	GSList *checkers;
	gchar **words;
	gint *word_starts;
	gint *word_ends;
};

enum {
	PROP_0,
	PROP_CHECKING_ENABLED
};

G_DEFINE_TYPE_WITH_CODE (
	ESpellEntry,
	e_spell_entry,
	GTK_TYPE_ENTRY,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static gboolean
word_misspelled (ESpellEntry *entry,
                 gint start,
                 gint end)
{
	const gchar *text;
	gchar *word;
	gboolean result = TRUE;
	GSList *li;
	gssize wlen;

	if (start == end)
		return FALSE;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	word = g_new0 (gchar, end - start + 2);

	g_strlcpy (word, text + start, end - start + 1);

	wlen = strlen (word);

	for (li = entry->priv->checkers; li; li = g_slist_next (li)) {
		GtkhtmlSpellChecker *checker = li->data;
		if (gtkhtml_spell_checker_check_word (checker, word, wlen)) {
			result = FALSE;
			break;
		}
	}

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

	if (e_spell_entry_get_checking_enabled (entry))
		check_words = (entry->priv->checkers != NULL);

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
entry_strsplit_utf8 (GtkEntry *entry,
                     gchar ***set,
                     gint **starts,
                     gint **ends)
{
	PangoLayout *layout;
	PangoLogAttr *log_attrs;
	const gchar *text;
	gint n_attrs, n_strings, i, j;

	layout = gtk_entry_get_layout (GTK_ENTRY (entry));
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

	/* Find how many words we have */
	n_strings = 0;
	for (i = 0; i < n_attrs; i++)
		if (log_attrs[i].is_word_start)
			n_strings++;

	*set = g_new0 (gchar *, n_strings + 1);
	*starts = g_new0 (gint, n_strings);
	*ends = g_new0 (gint, n_strings);

	/* Copy out strings */
	for (i = 0, j = 0; i < n_attrs; i++) {
		if (log_attrs[i].is_word_start) {
			gint cend, bytes;
			gchar *start;

			/* Find the end of this string */
			cend = i;
			while (!(log_attrs[cend].is_word_end))
				cend++;

			/* Copy sub-string */
			start = g_utf8_offset_to_pointer (text, i);
			bytes = (gint) (g_utf8_offset_to_pointer (text, cend) - start);
			(*set)[j] = g_new0 (gchar, bytes + 1);
			(*starts)[j] = (gint) (start - text);
			(*ends)[j] = (gint) (start - text + bytes);
			g_utf8_strncpy ((*set)[j], start, cend - i);

			/* Move on to the next word */
			j++;
		}
	}

	g_free (log_attrs);
}

static void
add_to_dictionary (GtkWidget *menuitem,
                   ESpellEntry *entry)
{
	gchar *word;
	gint start, end;
	GtkhtmlSpellChecker *checker;

	get_word_extents_from_position (
		entry, &start, &end, entry->priv->mark_character);
	word = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);

	checker = g_object_get_data (G_OBJECT (menuitem), "spell-entry-checker");
	if (checker != NULL)
		gtkhtml_spell_checker_add_word (checker, word, -1);

	g_free (word);

	if (entry->priv->words != NULL) {
		g_strfreev (entry->priv->words);
		g_free (entry->priv->word_starts);
		g_free (entry->priv->word_ends);
	}

	entry_strsplit_utf8 (
		GTK_ENTRY (entry),
		&entry->priv->words,
		&entry->priv->word_starts,
		&entry->priv->word_ends);

	spell_entry_recheck_all (entry);
}

static void
ignore_all (GtkWidget *menuitem,
            ESpellEntry *entry)
{
	gchar *word;
	gint start, end;
	GSList *li;

	get_word_extents_from_position (
		entry, &start, &end, entry->priv->mark_character);
	word = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);

	for (li = entry->priv->checkers; li; li = g_slist_next (li)) {
		GtkhtmlSpellChecker *checker = li->data;
		gtkhtml_spell_checker_add_word_to_session (checker, word, -1);
	}

	g_free (word);

	if (entry->priv->words != NULL) {
		g_strfreev (entry->priv->words);
		g_free (entry->priv->word_starts);
		g_free (entry->priv->word_ends);
	}

	entry_strsplit_utf8 (
		GTK_ENTRY (entry),
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
	GtkhtmlSpellChecker *checker;

	get_word_extents_from_position (
		entry, &start, &end, entry->priv->mark_character);
	oldword = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);
	newword = gtk_label_get_text (
		GTK_LABEL (gtk_bin_get_child (GTK_BIN (menuitem))));

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

	checker = g_object_get_data (G_OBJECT (menuitem), "spell-entry-checker");

	if (checker != NULL)
		gtkhtml_spell_checker_store_replacement (
			checker, oldword, -1, newword, -1);

	g_free (oldword);
}

static void
build_suggestion_menu (ESpellEntry *entry,
                       GtkWidget *menu,
                       GtkhtmlSpellChecker *checker,
                       const gchar *word)
{
	GtkWidget *mi;
	GList *suggestions, *iter;

	suggestions = gtkhtml_spell_checker_get_suggestions (checker, word, -1);

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

				mi = gtk_menu_item_new_with_label (_("More..."));
				gtk_widget_show (mi);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

				menu = gtk_menu_new ();
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);
			}

			mi = gtk_menu_item_new_with_label (iter->data);
			g_object_set_data (G_OBJECT (mi), "spell-entry-checker", checker);
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
	GtkhtmlSpellChecker *checker;
	GtkWidget *topmenu, *mi;
	gchar *label;

	topmenu = gtk_menu_new ();

	if (entry->priv->checkers == NULL)
		return topmenu;

	/* Suggestions */
	if (entry->priv->checkers->next == NULL) {
		checker = entry->priv->checkers->data;
		build_suggestion_menu (entry, topmenu, checker, word);
	} else {
		GSList *li;
		GtkWidget *menu;
		const gchar *lang_name;

		for (li = entry->priv->checkers; li; li = g_slist_next (li)) {
			const GtkhtmlSpellLanguage *language;

			checker = li->data;
			language = gtkhtml_spell_checker_get_language (checker);
			if (language == NULL)
				continue;

			lang_name = gtkhtml_spell_language_get_name (language);
			if (lang_name == NULL)
				lang_name = gtkhtml_spell_language_get_code (language);
			if (lang_name == NULL)
				lang_name = "???";

			mi = gtk_menu_item_new_with_label (lang_name);

			gtk_widget_show (mi);
			gtk_menu_shell_append (GTK_MENU_SHELL (topmenu), mi);
			menu = gtk_menu_new ();
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);
			build_suggestion_menu (entry, menu, checker, word);
		}
	}

	/* Separator */
	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (topmenu), mi);

	/* + Add to Dictionary */
	label = g_strdup_printf (_("Add \"%s\" to Dictionary"), word);
	mi = gtk_image_menu_item_new_with_label (label);
	g_free (label);

	gtk_image_menu_item_set_image (
		GTK_IMAGE_MENU_ITEM (mi),
		gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU));

	if (entry->priv->checkers->next == NULL) {
		checker = entry->priv->checkers->data;
		g_object_set_data (G_OBJECT (mi), "spell-entry-checker", checker);
		g_signal_connect (
			mi, "activate",
			G_CALLBACK (add_to_dictionary), entry);
	} else {
		GSList *li;
		GtkWidget *menu, *submi;
		const gchar *lang_name;

		menu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

		for (li = entry->priv->checkers; li; li = g_slist_next (li)) {
			const GtkhtmlSpellLanguage *language;

			checker = li->data;
			language = gtkhtml_spell_checker_get_language (checker);
			if (language == NULL)
				continue;

			lang_name = gtkhtml_spell_language_get_name (language);
			if (lang_name == NULL)
				lang_name = gtkhtml_spell_language_get_code (language);
			if (lang_name == NULL)
				lang_name = "???";

			submi = gtk_menu_item_new_with_label (lang_name);
			g_object_set_data (G_OBJECT (submi), "spell-entry-checker", checker);
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
	gint start, end;
	gchar *word;

	if (entry->priv->checkers == NULL)
		return;

	get_word_extents_from_position (
		entry, &start, &end, entry->priv->mark_character);
	if (start == end)
		return;

	if (!word_misspelled (entry, start, end))
		return;

	word = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);
	g_return_if_fail (word != NULL);

	spell_entry_add_suggestions_menu (entry, menu, word);

	g_free (word);
}

static void
spell_entry_changed (GtkEditable *editable)
{
	ESpellEntry *entry = E_SPELL_ENTRY (editable);

	if (entry->priv->checkers == NULL)
		return;

	if (entry->priv->words != NULL) {
		g_strfreev (entry->priv->words);
		g_free (entry->priv->word_starts);
		g_free (entry->priv->word_ends);
	}

	entry_strsplit_utf8 (
		GTK_ENTRY (entry),
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

static GList *
spell_entry_load_spell_languages (void)
{
	GSettings *settings;
	GList *spell_languages = NULL;
	gchar **strv;
	gint ii;

	/* Ask GSettings for a list of spell check language codes. */
	settings = g_settings_new ("org.gnome.evolution.mail");
	strv = g_settings_get_strv (settings, "composer-spell-languages");
	g_object_unref (settings);

	/* Convert the codes to spell language structs. */
	for (ii = 0; strv[ii] != NULL; ii++) {
		gchar *language_code = strv[ii];
		const GtkhtmlSpellLanguage *language;

		language = gtkhtml_spell_language_lookup (language_code);
		if (language != NULL)
			spell_languages = g_list_prepend (
				spell_languages, (gpointer) language);
	}

	g_strfreev (strv);

	spell_languages = g_list_reverse (spell_languages);

	/* Pick a default spell language if it came back empty. */
	if (spell_languages == NULL) {
		const GtkhtmlSpellLanguage *language;

		language = gtkhtml_spell_language_lookup (NULL);

		if (language) {
			spell_languages = g_list_prepend (
				spell_languages, (gpointer) language);
		}
	}

	return spell_languages;
}

static void
spell_entry_settings_changed (ESpellEntry *spell_entry,
                              const gchar *key)
{
	GList *languages;

	g_return_if_fail (spell_entry != NULL);

	if (spell_entry->priv->custom_checkers)
		return;

	if (key && !g_str_equal (key, "composer-spell-languages"))
		return;

	languages = spell_entry_load_spell_languages ();
	e_spell_entry_set_languages (spell_entry, languages);
	g_list_free (languages);

	spell_entry->priv->custom_checkers = FALSE;
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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_entry_dispose (GObject *object)
{
	ESpellEntryPrivate *priv;

	priv = E_SPELL_ENTRY_GET_PRIVATE (object);

	g_slist_free_full (priv->checkers, (GDestroyNotify) g_object_unref);
	priv->checkers = NULL;

	g_clear_object (&priv->settings);

	if (priv->attr_list != NULL) {
		pango_attr_list_unref (priv->attr_list);
		priv->attr_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_spell_entry_parent_class)->dispose (object);
}

static void
spell_entry_finalize (GObject *object)
{
	ESpellEntryPrivate *priv;

	priv = E_SPELL_ENTRY_GET_PRIVATE (object);

	g_strfreev (priv->words);
	g_free (priv->word_starts);
	g_free (priv->word_ends);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_spell_entry_parent_class)->finalize (object);
}

static void
spell_entry_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_spell_entry_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static gboolean
spell_entry_draw (GtkWidget *widget,
                  cairo_t *cr)
{
	ESpellEntry *spell_entry = E_SPELL_ENTRY (widget);
	GtkEntry *entry = GTK_ENTRY (widget);
	PangoLayout *layout;

	layout = gtk_entry_get_layout (entry);
	pango_layout_set_attributes (layout, spell_entry->priv->attr_list);

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

	g_type_class_add_private (class, sizeof (ESpellEntryPrivate));

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
}

static void
e_spell_entry_init (ESpellEntry *spell_entry)
{
	spell_entry->priv = E_SPELL_ENTRY_GET_PRIVATE (spell_entry);
	spell_entry->priv->attr_list = pango_attr_list_new ();
	spell_entry->priv->checkers = NULL;
	spell_entry->priv->checking_enabled = TRUE;

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

	/* listen for languages changes */
	spell_entry->priv->settings = g_settings_new ("org.gnome.evolution.mail");
	g_signal_connect_swapped (
		spell_entry->priv->settings, "changed",
		G_CALLBACK (spell_entry_settings_changed), spell_entry);

	/* load current settings */
	spell_entry_settings_changed (spell_entry, NULL);
}

GtkWidget *
e_spell_entry_new (void)
{
	return g_object_new (E_TYPE_SPELL_ENTRY, NULL);
}

/* 'languages' consists of 'const GtkhtmlSpellLanguage *' */
void
e_spell_entry_set_languages (ESpellEntry *spell_entry,
                             GList *languages)
{
	GList *iter;

	g_return_if_fail (spell_entry != NULL);

	spell_entry->priv->custom_checkers = TRUE;

	if (spell_entry->priv->checkers)
		g_slist_free_full (spell_entry->priv->checkers, g_object_unref);
	spell_entry->priv->checkers = NULL;

	for (iter = languages; iter; iter = g_list_next (iter)) {
		const GtkhtmlSpellLanguage *language = iter->data;

		if (language)
			spell_entry->priv->checkers = g_slist_prepend (
				spell_entry->priv->checkers,
				gtkhtml_spell_checker_new (language));
	}

	spell_entry->priv->checkers = g_slist_reverse (spell_entry->priv->checkers);

	if (gtk_widget_get_realized (GTK_WIDGET (spell_entry)))
		spell_entry_recheck_all (spell_entry);
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
