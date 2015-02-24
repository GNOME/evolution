/*
 * e-spell-checker.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-spell-checker.h"
#include "e-spell-dictionary.h"

#include <libebackend/libebackend.h>
#include <webkit/webkitspellchecker.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include <string.h>

#define E_SPELL_CHECKER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SPELL_CHECKER, ESpellCheckerPrivate))

#define MAX_SUGGESTIONS 10

struct _ESpellCheckerPrivate {
	GHashTable *active_dictionaries;
	GHashTable *dictionaries_cache;
};

enum {
	PROP_0,
	PROP_ACTIVE_LANGUAGES
};

/* Forward Declarations */
static void	e_spell_checker_init_webkit_checker
				(WebKitSpellCheckerInterface *interface);

G_DEFINE_TYPE_EXTENDED (
	ESpellChecker,
	e_spell_checker,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (
		WEBKIT_TYPE_SPELL_CHECKER,
		e_spell_checker_init_webkit_checker))

/**
 * ESpellChecker:
 *
 * #ESpellChecker represents a spellchecker in Evolution. It can be used as a
 * provider for dictionaries. It also implements #WebKitSpellCheckerInterface,
 * so it can be set as a default spell-checker to WebKit editors
 */


/* We retain ownership of the EnchantDict's since they
 * have to be freed through enchant_broker_free_dict()
 * and we also own the EnchantBroker. */
static GHashTable *global_enchant_dicts;
static EnchantBroker *global_broker;
G_LOCK_DEFINE_STATIC (global_memory);

static gboolean
spell_checker_enchant_dicts_foreach_cb (gpointer key,
                                        gpointer value,
                                        gpointer user_data)
{
	EnchantDict *enchant_dict = value;
	EnchantBroker *enchant_broker = user_data;

	enchant_broker_free_dict (enchant_broker, enchant_dict);

	return TRUE;
}

static void
wksc_check_spelling (WebKitSpellChecker *webkit_checker,
                     const gchar *word,
                     gint *misspelling_location,
                     gint *misspelling_length)
{
	ESpellChecker *checker = E_SPELL_CHECKER (webkit_checker);
	GHashTable *active_dictionaries;
	PangoLanguage *language;
	PangoLogAttr *attrs;
	gint length, ii;

	active_dictionaries = checker->priv->active_dictionaries;
	if (g_hash_table_size (active_dictionaries) == 0)
		return;

	length = g_utf8_strlen (word, -1);

	language = pango_language_get_default ();
	attrs = g_new (PangoLogAttr, length + 1);

	pango_get_log_attrs (word, -1, -1, language, attrs, length + 1);

	for (ii = 0; ii < length + 1; ii++) {
		/* We go through each character until we find an is_word_start,
		 * then we get into an inner loop to find the is_word_end
		 * corresponding */
		if (attrs[ii].is_word_start) {
			gboolean word_recognized;
			gint start = ii;
			gint end = ii;
			gint word_length;
			gchar *cstart;
			gint bytes;
			gchar *new_word;

			while (attrs[end].is_word_end < 1)
				end++;

			word_length = end - start;
			/* Set the iterator to be at the current word
			 * end, so we don't check characters twice. */
			ii = end;

			cstart = g_utf8_offset_to_pointer (word, start);
			bytes = g_utf8_offset_to_pointer (word, end) - cstart;
			new_word = g_new0 (gchar, bytes + 1);

			g_utf8_strncpy (new_word, cstart, word_length);

			word_recognized = e_spell_checker_check_word (
				checker, new_word, strlen (new_word));

			if (word_recognized) {
				if (misspelling_location != NULL)
					*misspelling_location = -1;
				if (misspelling_length != NULL)
					*misspelling_length = 0;
			} else {
				if (misspelling_location != NULL)
					*misspelling_location = start;
				if (misspelling_length != NULL)
					*misspelling_length = word_length;
			}

			g_free (new_word);
		}
	}

	g_free (attrs);
}

static gchar **
wksc_get_guesses (WebKitSpellChecker *webkit_checker,
                  const gchar *word,
                  const gchar *context)
{
	ESpellChecker *checker = E_SPELL_CHECKER (webkit_checker);
	GHashTable *active_dictionaries;
	GList *list, *link;
	gchar ** guesses;
	gint ii = 0;

	guesses = g_new0 (gchar *, MAX_SUGGESTIONS + 1);

	active_dictionaries = checker->priv->active_dictionaries;
	list = g_hash_table_get_keys (active_dictionaries);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary;
		GList *suggestions;

		dictionary = E_SPELL_DICTIONARY (link->data);
		suggestions = e_spell_dictionary_get_suggestions (
			dictionary, word, -1);

		while (suggestions != NULL && ii < MAX_SUGGESTIONS) {
			guesses[ii++] = suggestions->data;
			suggestions->data = NULL;

			suggestions = g_list_delete_link (
				suggestions, suggestions);
		}

		g_list_free_full (suggestions, (GDestroyNotify) g_free);

		if (ii >= MAX_SUGGESTIONS)
			break;
	}

	g_list_free (list);

	return guesses;
}

static gchar *
wksc_get_autocorrect_suggestions (WebKitSpellChecker *webkit_checker,
                                  const gchar *word)
{
	/* Not supported/needed */
	return NULL;
}

static void
spell_checker_learn_word (WebKitSpellChecker *webkit_checker,
                          const gchar *word)
{
	/* Carefully, this will add the word to all active dictionaries! */

	ESpellChecker *checker;
	GHashTable *active_dictionaries;
	GList *list, *link;

	checker = E_SPELL_CHECKER (webkit_checker);
	active_dictionaries = checker->priv->active_dictionaries;
	list = g_hash_table_get_keys (active_dictionaries);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary;

		dictionary = E_SPELL_DICTIONARY (link->data);
		e_spell_dictionary_learn_word (dictionary, word, -1);
	}

	g_list_free (list);
}

static void
spell_checker_ignore_word (WebKitSpellChecker *webkit_checker,
                           const gchar *word)
{
	/* Carefully, this will add the word to all active dictionaries */

	ESpellChecker *checker;
	GHashTable *active_dictionaries;
	GList *list, *link;

	checker = E_SPELL_CHECKER (webkit_checker);
	active_dictionaries = checker->priv->active_dictionaries;
	list = g_hash_table_get_keys (active_dictionaries);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary;

		dictionary = E_SPELL_DICTIONARY (link->data);
		e_spell_dictionary_ignore_word (dictionary, word, -1);
	}

	g_list_free (list);
}

static void
wksc_update_languages (WebKitSpellChecker *webkit_checker,
                       const gchar *languages)
{
	ESpellChecker *checker;
	GHashTable *active_dictionaries;
	GQueue queue = G_QUEUE_INIT;

	checker = E_SPELL_CHECKER (webkit_checker);
	active_dictionaries = checker->priv->active_dictionaries;

	if (languages != NULL) {
		gchar **langs;
		gint ii;

		langs = g_strsplit (languages, ",", -1);
		for (ii = 0; langs[ii] != NULL; ii++) {
			ESpellDictionary *dictionary;

			dictionary = e_spell_checker_ref_dictionary (
				checker, langs[ii]);
			if (dictionary != NULL)
				g_queue_push_tail (&queue, dictionary);
		}
		g_strfreev (langs);
	} else {
		ESpellDictionary *dictionary;
		PangoLanguage *pango_language;
		const gchar *language;

		pango_language = gtk_get_default_language ();
		language = pango_language_to_string (pango_language);
		dictionary = e_spell_checker_ref_dictionary (checker, language);

		if (dictionary == NULL) {
			GList *list;

			list = e_spell_checker_list_available_dicts (checker);
			if (list != NULL) {
				dictionary = g_object_ref (list->data);
				g_list_free (list);
			}
		}

		if (dictionary != NULL)
			g_queue_push_tail (&queue, dictionary);
	}

	g_hash_table_remove_all (active_dictionaries);

	while (!g_queue_is_empty (&queue)) {
		ESpellDictionary *dictionary;

		dictionary = g_queue_pop_head (&queue);
		g_hash_table_add (active_dictionaries, dictionary);
	}

	g_object_notify (G_OBJECT (checker), "active-languages");
}

static void
spell_checker_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_LANGUAGES:
			g_value_take_boxed (
				value,
				e_spell_checker_list_active_languages (
				E_SPELL_CHECKER (object), NULL));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_checker_dispose (GObject *object)
{
	ESpellCheckerPrivate *priv;

	priv = E_SPELL_CHECKER_GET_PRIVATE (object);

	g_hash_table_remove_all (priv->active_dictionaries);
	g_hash_table_remove_all (priv->dictionaries_cache);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_spell_checker_parent_class)->dispose (object);
}

static void
spell_checker_finalize (GObject *object)
{
	ESpellCheckerPrivate *priv;

	priv = E_SPELL_CHECKER_GET_PRIVATE (object);

	g_hash_table_destroy (priv->active_dictionaries);
	g_hash_table_destroy (priv->dictionaries_cache);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_spell_checker_parent_class)->finalize (object);
}

static void
spell_checker_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_spell_checker_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_spell_checker_class_init (ESpellCheckerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ESpellCheckerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = spell_checker_get_property;
	object_class->dispose = spell_checker_dispose;
	object_class->finalize = spell_checker_finalize;
	object_class->constructed = spell_checker_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_LANGUAGES,
		g_param_spec_boxed (
			"active-languages",
			"Active Languages",
			"Active spell check language codes",
			G_TYPE_STRV,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_spell_checker_init_webkit_checker (WebKitSpellCheckerInterface *interface)
{
	interface->check_spelling_of_string = wksc_check_spelling;
	interface->get_autocorrect_suggestions_for_misspelled_word =
		wksc_get_autocorrect_suggestions;
	interface->get_guesses_for_word = wksc_get_guesses;
	interface->ignore_word = spell_checker_ignore_word;
	interface->learn_word = spell_checker_learn_word;
	interface->update_spell_checking_languages = wksc_update_languages;
}

static void
e_spell_checker_init (ESpellChecker *checker)
{
	GHashTable *active_dictionaries;
	GHashTable *dictionaries_cache;

	active_dictionaries = g_hash_table_new_full (
		(GHashFunc) e_spell_dictionary_hash,
		(GEqualFunc) e_spell_dictionary_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) NULL);

	dictionaries_cache = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) g_object_unref);

	checker->priv = E_SPELL_CHECKER_GET_PRIVATE (checker);

	checker->priv->active_dictionaries = active_dictionaries;
	checker->priv->dictionaries_cache = dictionaries_cache;
}

/**
 * e_spell_checker_new:
 *
 * Creates a new #ESpellChecker instance.
 *
 * Returns: a new #ESpellChecker
 **/
ESpellChecker *
e_spell_checker_new (void)
{
	return g_object_new (E_TYPE_SPELL_CHECKER, NULL);
}

static void
list_enchant_dicts (const gchar * const lang_tag,
                    const gchar * const provider_name,
                    const gchar * const provider_desc,
                    const gchar * const provider_file,
                    gpointer user_data)
{
	EnchantBroker *broker = user_data;
	EnchantDict *enchant_dict;

	enchant_dict = enchant_broker_request_dict (broker, lang_tag);
	if (enchant_dict != NULL) {
		g_hash_table_insert (
			global_enchant_dicts,
			g_strdup (lang_tag), enchant_dict);
	}
}

static void
copy_enchant_dicts (gpointer pcode,
		    gpointer pdict,
		    gpointer user_data)
{
	EnchantDict *enchant_dict = pdict;
	ESpellChecker *checker = user_data;

	if (enchant_dict) {
		ESpellDictionary *dictionary;
		const gchar *code;

		/* Note that we retain ownership of the EnchantDict.
		 * Since EnchantDict is not reference counted, we're
		 * merely loaning the pointer to ESpellDictionary. */
		dictionary = e_spell_dictionary_new (checker, enchant_dict);
		code = e_spell_dictionary_get_code (dictionary);

		g_hash_table_insert (
			checker->priv->dictionaries_cache,
			(gpointer) code, dictionary);
	}
}

static void
e_spell_checker_init_global_memory (void)
{
	G_LOCK (global_memory);

	if (!global_broker) {
		global_broker = enchant_broker_init ();
		global_enchant_dicts = g_hash_table_new_full (
			(GHashFunc) g_str_hash,
			(GEqualFunc) g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) NULL);

		enchant_broker_list_dicts (
			global_broker,
			list_enchant_dicts, global_broker);
	}

	G_UNLOCK (global_memory);
}

/**
 * e_spell_checker_free_global_memory:
 *
 * Frees global memory used by the ESpellChecker. This should be called at
 * the end of main(), to avoid memory leaks.
 *
 * Since: 3.16
 **/
void
e_spell_checker_free_global_memory (void)
{
	G_LOCK (global_memory);

	if (global_enchant_dicts) {
		/* Freeing EnchantDicts requires help from EnchantBroker. */
		g_hash_table_foreach_remove (
			global_enchant_dicts,
			spell_checker_enchant_dicts_foreach_cb,
			global_broker);
		g_hash_table_destroy (global_enchant_dicts);
		global_enchant_dicts = NULL;

		enchant_broker_free (global_broker);
		global_broker = NULL;
	}

	G_UNLOCK (global_memory);
}

/**
 * e_spell_checker_list_available_dicts:
 * @checker: An #ESpellChecker
 *
 * Returns list of all dictionaries available to the actual
 * spell-checking backend.
 *
 * Returns: new copy of #GList of #ESpellDictionary. The dictionaries are
 * owned by the @checker and should not be free'd. The list should be freed
 * using g_list_free() when not neede anymore. [transfer-list]
 */
GList *
e_spell_checker_list_available_dicts (ESpellChecker *checker)
{
	GList *list;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);

	if (g_hash_table_size (checker->priv->dictionaries_cache) == 0) {
		e_spell_checker_init_global_memory ();
		g_hash_table_foreach (global_enchant_dicts, copy_enchant_dicts, checker);
	}

	list = g_hash_table_get_values (checker->priv->dictionaries_cache);

	return g_list_sort (list, (GCompareFunc) e_spell_dictionary_compare);
}

/**
 * e_spell_checker_ref_dictionary:
 * @checker: an #ESpellChecker
 * @language_code: (allow-none): language code of a dictionary, or %NULL
 *
 * Tries to find an #ESpellDictionary for given @language_code.
 * If @language_code is %NULL, the function will return a default
 * #ESpellDictionary.
 *
 * Returns: an #ESpellDictionary for @language_code
 */
ESpellDictionary *
e_spell_checker_ref_dictionary (ESpellChecker *checker,
                                const gchar *language_code)
{
	ESpellDictionary *dictionary;
	GList *list;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);

	/* If the cache has not yet been initialized, do so - we will need
	 * it anyway, Otherwise is this call very cheap */
	list = e_spell_checker_list_available_dicts (checker);

	if (language_code == NULL) {
		dictionary = (list != NULL) ? list->data : NULL;
	} else {
		dictionary = g_hash_table_lookup (
			checker->priv->dictionaries_cache,
			language_code);
	}

	if (dictionary != NULL)
		g_object_ref (dictionary);

	g_list_free (list);

	return dictionary;
}

/**
 * e_spell_checker_get_enchant_dict:
 * @checker: an #ESpellChecker
 * @language_code: language code of a dictionary, or %NULL
 *
 * Returns the #EnchantDict for @language_code, or %NULL if there is none.
 *
 * Returns: the #EnchantDict for @language_code, or %NULL
 **/
EnchantDict *
e_spell_checker_get_enchant_dict (ESpellChecker *checker,
                                  const gchar *language_code)
{
	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);
	g_return_val_if_fail (language_code != NULL, NULL);

	e_spell_checker_init_global_memory ();

	return g_hash_table_lookup (global_enchant_dicts, language_code);
}

gboolean
e_spell_checker_get_language_active (ESpellChecker *checker,
                                     const gchar *language_code)
{
	ESpellDictionary *dictionary;
	GHashTable *active_dictionaries;
	gboolean active;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), FALSE);
	g_return_val_if_fail (language_code != NULL, FALSE);

	dictionary = e_spell_checker_ref_dictionary (checker, language_code);
	g_return_val_if_fail (dictionary != NULL, FALSE);

	active_dictionaries = checker->priv->active_dictionaries;
	active = g_hash_table_contains (active_dictionaries, dictionary);

	g_object_unref (dictionary);

	return active;
}

void
e_spell_checker_set_language_active (ESpellChecker *checker,
                                     const gchar *language_code,
                                     gboolean active)
{
	ESpellDictionary *dictionary;
	GHashTable *active_dictionaries;
	gboolean is_active;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));
	g_return_if_fail (language_code != NULL);

	dictionary = e_spell_checker_ref_dictionary (checker, language_code);
	g_return_if_fail (dictionary != NULL);

	active_dictionaries = checker->priv->active_dictionaries;
	is_active = g_hash_table_contains (active_dictionaries, dictionary);

	if (active && !is_active) {
		g_object_ref (dictionary);
		g_hash_table_add (active_dictionaries, dictionary);
		g_object_notify (G_OBJECT (checker), "active-languages");
	} else if (!active && is_active) {
		g_hash_table_remove (active_dictionaries, dictionary);
		g_object_notify (G_OBJECT (checker), "active-languages");
	}

	g_object_unref (dictionary);
}

/**
 * e_spell_checker_list_active_languages:
 * @checker: an #ESpellChecker
 * @n_languages: return location for the number of active languages, or %NULL
 *
 * Returns a %NULL-terminated array of language codes actively being used
 * for spell checking.  Free the returned array with g_strfreev().
 *
 * Returns: a %NULL-teriminated array of language codes
 **/
gchar **
e_spell_checker_list_active_languages (ESpellChecker *checker,
                                       guint *n_languages)
{
	GHashTable *active_dictionaries;
	GList *list, *link;
	gchar **active_languages;
	guint size;
	gint ii = 0;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);

	active_dictionaries = checker->priv->active_dictionaries;
	list = g_hash_table_get_keys (active_dictionaries);
	size = g_hash_table_size (active_dictionaries);

	active_languages = g_new0 (gchar *, size + 1);

	list = g_list_sort (list, (GCompareFunc) e_spell_dictionary_compare);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary;
		const gchar *language_code;

		dictionary = E_SPELL_DICTIONARY (link->data);
		language_code = e_spell_dictionary_get_code (dictionary);
		active_languages[ii++] = g_strdup (language_code);
	}

	if (n_languages != NULL)
		*n_languages = size;

	g_list_free (list);

	return active_languages;
}

/**
 * e_spell_checker_count_active_languages:
 * @checker: an #ESpellChecker
 *
 * Returns the number of languages actively being used for spell checking.
 *
 * Returns: number of active spell checking languages
 **/
guint
e_spell_checker_count_active_languages (ESpellChecker *checker)
{
	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), 0);

	return g_hash_table_size (checker->priv->active_dictionaries);
}

/**
 * e_spell_checker_check_word:
 * @checker: an #SpellChecker
 * @word: a word to spell-check
 * @length: length of @word in bytes or -1 when %NULL-terminated
 *
 * Calls e_spell_dictionary_check_word() on all active dictionaries in
 * @checker, and returns %TRUE if @word is recognized by any of them.
 *
 * Returns: %TRUE if @word is recognized, %FALSE otherwise
 **/
gboolean
e_spell_checker_check_word (ESpellChecker *checker,
                            const gchar *word,
                            gsize length)
{
	GList *list, *link;
	gboolean recognized = FALSE;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), TRUE);
	g_return_val_if_fail (word != NULL && *word != '\0', TRUE);

	list = g_hash_table_get_keys (checker->priv->active_dictionaries);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary;

		dictionary = E_SPELL_DICTIONARY (link->data);
		if (e_spell_dictionary_check_word (dictionary, word, length)) {
			recognized = TRUE;
			break;
		}
	}

	g_list_free (list);

	return recognized;
}

/**
 * e_spell_checker_ignore_word:
 * @checker: an #ESpellChecker
 * @word: word to ignore for the rest of session
 *
 * Calls e_spell_dictionary_ignore_word() on all active dictionaries in
 * the @checker.
 */
void
e_spell_checker_ignore_word (ESpellChecker *checker,
                             const gchar *word)
{
	WebKitSpellCheckerInterface *interface;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	interface = WEBKIT_SPELL_CHECKER_GET_IFACE (checker);
	interface->ignore_word (WEBKIT_SPELL_CHECKER (checker), word);
}

/**
 * e_spell_checker_learn_word:
 * @checker: an #ESpellChecker
 * @word: word to learn
 *
 * Calls e_spell_dictionary_learn_word() on all active dictionaries in
 * the @checker.
 */
void
e_spell_checker_learn_word (ESpellChecker *checker,
                            const gchar *word)
{
	WebKitSpellCheckerInterface *interface;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	interface = WEBKIT_SPELL_CHECKER_GET_IFACE (checker);
	interface->learn_word (WEBKIT_SPELL_CHECKER (checker), word);
}
