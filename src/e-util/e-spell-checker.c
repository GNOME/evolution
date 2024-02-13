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

#include "evolution-config.h"

#include <enchant.h>

#include "e-spell-checker.h"
#include "e-spell-dictionary.h"

#include <libebackend/libebackend.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include <string.h>

#define MAX_SUGGESTIONS 10

struct _ESpellCheckerPrivate {
	GHashTable *active_dictionaries;
	GHashTable *dictionaries_cache;
};

enum {
	PROP_0,
	PROP_ACTIVE_LANGUAGES
};

G_DEFINE_TYPE_WITH_CODE (ESpellChecker, e_spell_checker, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ESpellChecker)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

/**
 * ESpellChecker:
 *
 * #ESpellChecker represents a spellchecker in Evolution. It can be used as a
 * provider for dictionaries.
 */


/* We retain ownership of the EnchantDict's since they
 * have to be freed through enchant_broker_free_dict()
 * and we also own the EnchantBroker. */
static GHashTable *global_enchant_dicts;
static GHashTable *global_language_tags; /* gchar * ~> NULL */
static EnchantBroker *global_broker;
G_LOCK_DEFINE_STATIC (global_memory);

static gboolean
spell_checker_enchant_dicts_foreach_cb (gpointer key,
                                        gpointer value,
                                        gpointer user_data)
{
	EnchantDict *enchant_dict = value;
	EnchantBroker *enchant_broker = user_data;

	if (enchant_dict)
		enchant_broker_free_dict (enchant_broker, enchant_dict);

	return TRUE;
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
	ESpellChecker *self = E_SPELL_CHECKER (object);

	g_hash_table_remove_all (self->priv->active_dictionaries);
	g_hash_table_remove_all (self->priv->dictionaries_cache);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_spell_checker_parent_class)->dispose (object);
}

static void
spell_checker_finalize (GObject *object)
{
	ESpellChecker *self = E_SPELL_CHECKER (object);

	g_hash_table_destroy (self->priv->active_dictionaries);
	g_hash_table_destroy (self->priv->dictionaries_cache);

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

	checker->priv = e_spell_checker_get_instance_private (checker);

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
list_enchant_dicts (const gchar * const language_tag,
                    const gchar * const provider_name,
                    const gchar * const provider_desc,
                    const gchar * const provider_file,
                    gpointer user_data)
{
	g_hash_table_insert (
		global_language_tags,
		g_strdup (language_tag), NULL);
}

static void
copy_enchant_dicts (gpointer planguage_tag,
		    gpointer punused,
		    gpointer user_data)
{
	ESpellChecker *checker = user_data;

	if (planguage_tag) {
		ESpellDictionary *dictionary;
		const gchar *code;

		/* Note that we retain ownership of the EnchantDict.
		 * Since EnchantDict is not reference counted, we're
		 * merely loaning the pointer to ESpellDictionary. */
		dictionary = e_spell_dictionary_new_bare (checker, planguage_tag);
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
		global_language_tags = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			g_free, NULL);

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

	g_clear_pointer (&global_language_tags, g_hash_table_destroy);

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
 * using g_list_free() when not needed anymore. [transfer-list]
 */
GList *
e_spell_checker_list_available_dicts (ESpellChecker *checker)
{
	GList *list;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);

	if (g_hash_table_size (checker->priv->dictionaries_cache) == 0) {
		e_spell_checker_init_global_memory ();
		G_LOCK (global_memory);
		g_hash_table_foreach (global_language_tags, copy_enchant_dicts, checker);
		G_UNLOCK (global_memory);
	}

	list = g_hash_table_get_values (checker->priv->dictionaries_cache);

	return g_list_sort (list, (GCompareFunc) e_spell_dictionary_compare);
}

/**
 * e_spell_checker_count_available_dicts:
 * @checker: An #ESpellChecker
 *
 * Returns: Count of available dictionaries.
 **/
guint
e_spell_checker_count_available_dicts (ESpellChecker *checker)
{
	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), 0);

	if (g_hash_table_size (checker->priv->dictionaries_cache) == 0) {
		e_spell_checker_init_global_memory ();
		G_LOCK (global_memory);
		g_hash_table_foreach (global_language_tags, copy_enchant_dicts, checker);
		G_UNLOCK (global_memory);
	}

	return g_hash_table_size (checker->priv->dictionaries_cache);
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
gpointer
e_spell_checker_get_enchant_dict (ESpellChecker *checker,
                                  const gchar *language_code)
{
	EnchantDict *dict;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);
	g_return_val_if_fail (language_code != NULL, NULL);

	e_spell_checker_init_global_memory ();

	G_LOCK (global_memory);

	dict = g_hash_table_lookup (global_enchant_dicts, language_code);
	if (((gpointer) dict) == GINT_TO_POINTER (1)) {
		dict = NULL;
	} else if (!dict) {
		dict = enchant_broker_request_dict (global_broker, language_code);
		if (dict)
			g_hash_table_insert (global_enchant_dicts, g_strdup (language_code), dict);
		else
			g_hash_table_insert (global_enchant_dicts, g_strdup (language_code), GINT_TO_POINTER (1));
	}

	G_UNLOCK (global_memory);

	return dict;
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
	if (!dictionary)
		return FALSE;

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
	if (!dictionary)
		return;

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
 * e_spell_checker_set_active_languages:
 * @checker: An #ESpellChecker
 * @languages: A list of languages to have activated
 *
 * Activates only the languages from @languages, all others will
 * be deactivated after this function is finished.
 **/
void
e_spell_checker_set_active_languages (ESpellChecker *checker,
				      const gchar * const *languages)
{
	gint ii;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	g_object_freeze_notify (G_OBJECT (checker));

	for (ii = 0; languages && languages[ii]; ii++) {
		e_spell_checker_set_language_active (checker, languages[ii], TRUE);
	}

	if (ii == g_hash_table_size (checker->priv->active_dictionaries)) {
		g_object_thaw_notify (G_OBJECT (checker));
		return;
	}

	g_hash_table_remove_all (checker->priv->active_dictionaries);
	for (ii = 0; languages && languages[ii]; ii++) {
		e_spell_checker_set_language_active (checker, languages[ii], TRUE);
	}

	g_object_notify (G_OBJECT (checker), "active-languages");
	g_object_thaw_notify (G_OBJECT (checker));
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
 * @checker: a #SpellChecker
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
	/* Carefully, this will add the word to all active dictionaries */

	GHashTable *active_dictionaries;
	GList *list, *link;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	active_dictionaries = checker->priv->active_dictionaries;
	list = g_hash_table_get_keys (active_dictionaries);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary;

		dictionary = E_SPELL_DICTIONARY (link->data);
		e_spell_dictionary_ignore_word (dictionary, word, -1);
	}

	g_list_free (list);
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
	/* Carefully, this will add the word to all active dictionaries! */

	GHashTable *active_dictionaries;
	GList *list, *link;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	active_dictionaries = checker->priv->active_dictionaries;
	list = g_hash_table_get_keys (active_dictionaries);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary;

		dictionary = E_SPELL_DICTIONARY (link->data);
		e_spell_dictionary_learn_word (dictionary, word, -1);
	}

	g_list_free (list);
}

/**
 * e_spell_checker_get_guesses_for_word:
 * @checker: an #ESpellChecker
 * @word: word to get guesses for
 *
 * Returns: a NULL-terminated array of guesses for the @word. Free the returned
 *    pointer with g_strfreev() when done with it.
 **/
gchar **
e_spell_checker_get_guesses_for_word (ESpellChecker *checker,
				      const gchar *word)
{
	GHashTable *active_dictionaries;
	GList *list, *link;
	gchar **guesses;
	gint ii = 0;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);
	g_return_val_if_fail (word != NULL, NULL);

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
