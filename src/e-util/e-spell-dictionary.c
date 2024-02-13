/*
 * e-spell-dictionary.c
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

#include "e-misc-utils.h"
#include "e-spell-dictionary.h"
#include "e-spell-checker.h"

#include <glib/gi18n-lib.h>
#include <string.h>

/**
 * ESpellDictionary:
 *
 * The #ESpellDictionary is a wrapper around #EnchantDict.
 */

enum {
	PROP_0,
	PROP_SPELL_CHECKER
};

struct _ESpellDictionaryPrivate {
	GWeakRef spell_checker;

	gchar *name;
	gchar *code;
	gchar *collate_key;
};

G_DEFINE_TYPE_WITH_PRIVATE (ESpellDictionary, e_spell_dictionary, G_TYPE_OBJECT)

struct _enchant_dict_description_data {
	gchar *language_tag;
	gchar *dict_name;
};

static void
describe_dictionary (const gchar *language_tag,
                     const gchar *provider_name,
                     const gchar *provider_desc,
                     const gchar *provider_file,
                     gpointer user_data)
{
	struct _enchant_dict_description_data *data = user_data;

	data->language_tag = g_strdup (language_tag);
	data->dict_name = e_util_get_language_name (language_tag);
}

static void
spell_dictionary_set_enchant_dict (ESpellDictionary *dictionary,
                                   EnchantDict *enchant_dict)
{
	struct _enchant_dict_description_data data;

	enchant_dict_describe (enchant_dict, describe_dictionary, &data);

	dictionary->priv->code = data.language_tag;
	dictionary->priv->name = data.dict_name;
	dictionary->priv->collate_key = g_utf8_collate_key (data.dict_name, -1);
}

static void
spell_dictionary_set_spell_checker (ESpellDictionary *dictionary,
                                    ESpellChecker *spell_checker)
{
	g_return_if_fail (E_IS_SPELL_CHECKER (spell_checker));

	g_weak_ref_set (&dictionary->priv->spell_checker, spell_checker);
}

static void
spell_dictionary_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SPELL_CHECKER:
			spell_dictionary_set_spell_checker (
				E_SPELL_DICTIONARY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_dictionary_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SPELL_CHECKER:
			g_value_take_object (
				value,
				e_spell_dictionary_ref_spell_checker (
				E_SPELL_DICTIONARY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_dictionary_dispose (GObject *object)
{
	ESpellDictionary *self = E_SPELL_DICTIONARY (object);

	g_weak_ref_set (&self->priv->spell_checker, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_spell_dictionary_parent_class)->dispose (object);
}

static void
spell_dictionary_finalize (GObject *object)
{
	ESpellDictionary *self = E_SPELL_DICTIONARY (object);

	g_free (self->priv->name);
	g_free (self->priv->code);
	g_free (self->priv->collate_key);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_spell_dictionary_parent_class)->finalize (object);
}

static void
e_spell_dictionary_class_init (ESpellDictionaryClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = spell_dictionary_set_property;
	object_class->get_property = spell_dictionary_get_property;
	object_class->dispose = spell_dictionary_dispose;
	object_class->finalize = spell_dictionary_finalize;

	g_object_class_install_property (
		object_class,
		PROP_SPELL_CHECKER,
		g_param_spec_object (
			"spell-checker",
			NULL,
			"Parent spell checker",
			E_TYPE_SPELL_CHECKER,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_spell_dictionary_init (ESpellDictionary *dictionary)
{
	dictionary->priv = e_spell_dictionary_get_instance_private (dictionary);
}

ESpellDictionary *
e_spell_dictionary_new (ESpellChecker *spell_checker,
                        gpointer enchant_dict)
{
	ESpellDictionary *dictionary;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (spell_checker), NULL);
	g_return_val_if_fail (enchant_dict != NULL, NULL);

	dictionary = g_object_new (
		E_TYPE_SPELL_DICTIONARY,
		"spell-checker", spell_checker, NULL);

	/* Since EnchantDict is not reference counted, ESpellChecker
	 * is loaning us the EnchantDict pointer.  We do not own it. */
	spell_dictionary_set_enchant_dict (dictionary, enchant_dict);

	return dictionary;
}

ESpellDictionary *
e_spell_dictionary_new_bare (ESpellChecker *spell_checker,
			     const gchar *language_tag)
{
	ESpellDictionary *dictionary;
	struct _enchant_dict_description_data descr_data;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (spell_checker), NULL);
	g_return_val_if_fail (language_tag != NULL, NULL);

	dictionary = g_object_new (
		E_TYPE_SPELL_DICTIONARY,
		"spell-checker", spell_checker, NULL);

	descr_data.language_tag = NULL;
	descr_data.dict_name = NULL;

	describe_dictionary (language_tag, NULL, NULL, NULL, &descr_data);

	dictionary->priv->code = descr_data.language_tag;
	dictionary->priv->name = descr_data.dict_name;
	dictionary->priv->collate_key = g_utf8_collate_key (descr_data.dict_name, -1);

	return dictionary;
}

/**
 * e_spell_dictionary_hash:
 * @dictionary: an #ESpellDictionary
 *
 * Generates a hash value for @dictionary based on its ISO code.
 * This function is intended for easily hashing an #ESpellDictionary
 * to add to a #GHashTable or similar data structure.
 *
 * Returns: a hash value for @dictionary
 **/
guint
e_spell_dictionary_hash (ESpellDictionary *dictionary)
{
	const gchar *code;

	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary), 0);

	code = e_spell_dictionary_get_code (dictionary);

	return g_str_hash (code);
}

/**
 * e_spell_dictionary_equal:
 * @dictionary1: an #ESpellDictionary
 * @dictionary2: another #ESpellDictionary
 *
 * Checks two #ESpellDictionary instances for equality based on their
 * ISO codes.
 *
 * Returns: %TRUE if @dictionary1 and @dictionary2 are equal
 **/
gboolean
e_spell_dictionary_equal (ESpellDictionary *dictionary1,
                          ESpellDictionary *dictionary2)
{
	const gchar *code1, *code2;

	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary1), FALSE);
	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary2), FALSE);

	if (dictionary1 == dictionary2)
		return TRUE;

	code1 = e_spell_dictionary_get_code (dictionary1);
	code2 = e_spell_dictionary_get_code (dictionary2);

	return g_str_equal (code1, code2);
}

/**
 * e_spell_dictionary_compare:
 * @dictionary1: an #ESpellDictionary
 * @dictionary2: another #ESpellDictionary
 *
 * Compares @dictionary1 and @dictionary2 by their display names for
 * the purpose of lexicographical sorting.  Use this function where a
 * #GCompareFunc callback is required, such as g_list_sort().
 *
 * Returns: 0 if the names match,
 *          a negative value if @dictionary1 < @dictionary2,
 *          or a positive value of @dictionary1 > @dictionary2
 **/
gint
e_spell_dictionary_compare (ESpellDictionary *dictionary1,
                            ESpellDictionary *dictionary2)
{
	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary1), 0);
	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary2), 0);

	return strcmp (
		dictionary1->priv->collate_key,
		dictionary2->priv->collate_key);
}

/**
 * e_spell_dictionary_get_name:
 * @dictionary: an #ESpellDictionary
 *
 * Returns the display name of the dictionary (for example
 * "English (British)")
 *
 * Returns: the display name of the @dictionary
 */
const gchar *
e_spell_dictionary_get_name (ESpellDictionary *dictionary)
{
	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary), NULL);

	return dictionary->priv->name;
}

/**
 * e_spell_dictionary_get_code:
 * @dictionary: an #ESpellDictionary
 *
 * Returns the ISO code of the spell-checking language for
 * @dictionary (for example "en_US").
 *
 * Returns: the language code of the @dictionary
 */
const gchar *
e_spell_dictionary_get_code (ESpellDictionary *dictionary)
{
	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary), NULL);

	return dictionary->priv->code;
}

/**
 * e_spell_dictionary_ref_spell_checker:
 * @dictionary: an #ESpellDictionary
 *
 * Returns a new reference to the #ESpellChecker which owns the dictionary.
 * Unreference the #ESpellChecker with g_object_unref() when finished with it.
 *
 * Returns: an #ESpellChecker
 **/
ESpellChecker *
e_spell_dictionary_ref_spell_checker (ESpellDictionary *dictionary)
{
	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary), NULL);

	return g_weak_ref_get (&dictionary->priv->spell_checker);
}

/**
 * e_spell_dictionary_check_word:
 * @dictionary: an #ESpellDictionary
 * @word: a word to spell-check
 * @length: length of @word in bytes or -1 when %NULL-terminated
 *
 * Tries to lookup the @word in the @dictionary to check whether
 * it's spelled correctly or not.
 *
 * Returns: %TRUE if @word is recognized, %FALSE otherwise
 */
gboolean
e_spell_dictionary_check_word (ESpellDictionary *dictionary,
                               const gchar *word,
                               gsize length)
{
	ESpellChecker *spell_checker;
	EnchantDict *enchant_dict;
	gboolean recognized;

	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary), TRUE);
	g_return_val_if_fail (word != NULL && *word != '\0', TRUE);

	spell_checker = e_spell_dictionary_ref_spell_checker (dictionary);
	g_return_val_if_fail (spell_checker != NULL, TRUE);

	enchant_dict = e_spell_checker_get_enchant_dict (
		spell_checker, e_spell_dictionary_get_code (dictionary));
	g_return_val_if_fail (enchant_dict != NULL, TRUE);

	recognized = (enchant_dict_check (enchant_dict, word, length) == 0);

	g_object_unref (spell_checker);

	return recognized;
}

/**
 * e_spell_dictionary_learn_word:
 * @dictionary: an #ESpellDictionary
 * @word: a word to add to @dictionary
 * @length: length of @word in bytes or -1 when %NULL-terminated
 *
 * Permanently adds @word to @dictionary so that next time calling
 * e_spell_dictionary_check() on the @word will return %TRUE.
 */
void
e_spell_dictionary_learn_word (ESpellDictionary *dictionary,
                               const gchar *word,
                               gsize length)
{
	ESpellChecker *spell_checker;
	EnchantDict *enchant_dict;

	g_return_if_fail (E_IS_SPELL_DICTIONARY (dictionary));
	g_return_if_fail (word != NULL && *word != '\0');

	spell_checker = e_spell_dictionary_ref_spell_checker (dictionary);
	g_return_if_fail (spell_checker != NULL);

	enchant_dict = e_spell_checker_get_enchant_dict (
		spell_checker, e_spell_dictionary_get_code (dictionary));
	g_return_if_fail (enchant_dict != NULL);

	enchant_dict_add (enchant_dict, word, length);

	g_object_unref (spell_checker);
}

/**
 * e_spell_dictionary_ignore_word:
 * @dictionary: an #ESpellDictionary
 * @word: a word to add to ignore list
 * @length: length of @word in bytes or -1 when %NULL-terminated
 *
 * Adds @word to temporary ignore list of the @dictionary, so that
 * e_spell_dictionary_check() on the @word will return %TRUE. The
 * list is cleared when the dictionary is freed.
 */
void
e_spell_dictionary_ignore_word (ESpellDictionary *dictionary,
                                const gchar *word,
                                gsize length)
{
	ESpellChecker *spell_checker;
	EnchantDict *enchant_dict;

	g_return_if_fail (E_IS_SPELL_DICTIONARY (dictionary));
	g_return_if_fail (word != NULL && *word != '\0');

	spell_checker = e_spell_dictionary_ref_spell_checker (dictionary);
	g_return_if_fail (spell_checker != NULL);

	enchant_dict = e_spell_checker_get_enchant_dict (
		spell_checker, e_spell_dictionary_get_code (dictionary));
	g_return_if_fail (enchant_dict != NULL);

	enchant_dict_add_to_session (enchant_dict, word, length);

	g_object_unref (spell_checker);
}

/**
 * e_spell_dictionary_get_suggestions:
 * @dictionary: an #ESpellDictionary
 * @word: a word to which to find suggestions
 * @length: length of @word in bytes or -1 when %NULL-terminated
 *
 * Provides list of alternative spellings of @word.
 *
 * Free the returned spelling suggestions with g_free(), and the list
 * itself with g_list_free().  An easy way to free the list properly in
 * one step is as follows:
 *
 * |[
 *   g_list_free_full (list, (GDestroyNotify) g_free);
 * ]|
 *
 * Returns: a list of spelling suggestions for @word
 */
GList *
e_spell_dictionary_get_suggestions (ESpellDictionary *dictionary,
                                    const gchar *word,
                                    gsize length)
{
	ESpellChecker *spell_checker;
	EnchantDict *enchant_dict;
	GList *list = NULL;
	gchar **suggestions;
	gsize ii, count = 0;

	g_return_val_if_fail (E_IS_SPELL_DICTIONARY (dictionary), NULL);
	g_return_val_if_fail (word != NULL && *word != '\0', NULL);

	spell_checker = e_spell_dictionary_ref_spell_checker (dictionary);
	g_return_val_if_fail (spell_checker != NULL, NULL);

	enchant_dict = e_spell_checker_get_enchant_dict (
		spell_checker, e_spell_dictionary_get_code (dictionary));
	g_return_val_if_fail (enchant_dict != NULL, NULL);

	suggestions = enchant_dict_suggest (enchant_dict, word, length, &count);
	for (ii = 0; ii < count; ii++)
		list = g_list_prepend (list, g_strdup (suggestions[ii]));
	enchant_dict_free_string_list (enchant_dict, suggestions);

	g_object_unref (spell_checker);

	return g_list_reverse (list);
}

/**
 * e_spell_dictionary_add_correction
 * @dictionary: an #ESpellDictionary
 * @misspelled: a misspelled word
 * @misspelled_length: length of @misspelled in bytes or -1 when
 *                     %NULL-terminated
 * @correction: the corrected word
 * @correction_length: length of @correction in bytes or -1 when
 *                     %NULL-terminated
 *
 * Learns a new @correction of @misspelled word.
 */
void
e_spell_dictionary_store_correction (ESpellDictionary *dictionary,
                                     const gchar *misspelled,
                                     gsize misspelled_length,
                                     const gchar *correction,
                                     gsize correction_length)
{
	ESpellChecker *spell_checker;
	EnchantDict *enchant_dict;

	g_return_if_fail (E_IS_SPELL_DICTIONARY (dictionary));
	g_return_if_fail (misspelled != NULL && *misspelled != '\0');
	g_return_if_fail (correction != NULL && *correction != '\0');

	spell_checker = e_spell_dictionary_ref_spell_checker (dictionary);
	g_return_if_fail (spell_checker != NULL);

	enchant_dict = e_spell_checker_get_enchant_dict (
		spell_checker, e_spell_dictionary_get_code (dictionary));
	g_return_if_fail (enchant_dict != NULL);

	enchant_dict_store_replacement (
		enchant_dict,
		misspelled, misspelled_length,
		correction, correction_length);

	g_object_unref (spell_checker);
}
