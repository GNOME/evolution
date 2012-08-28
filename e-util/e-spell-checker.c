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

<<<<<<< HEAD
#include <libebackend/libebackend.h>
=======
>>>>>>> Move spell-checking parts to e-util
#include <webkit/webkitspellchecker.h>
#include <pango/pango.h>
#include <gtk/gtk.h>

<<<<<<< HEAD
#define E_SPELL_CHECKER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SPELL_CHECKER, ESpellCheckerPrivate))

#define MAX_SUGGESTIONS 10

struct _ESpellCheckerPrivate {
	EnchantBroker *broker;
	GHashTable *active_dictionaries;
	GHashTable *dictionaries_cache;
	gboolean dictionaries_loaded;

	/* We retain ownership of the EnchantDict's since they
	 * have to be freed through enchant_broker_free_dict()
	 * and we also own the EnchantBroker. */
	GHashTable *enchant_dicts;
};

enum {
	PROP_0,
	PROP_ACTIVE_LANGUAGES
};

/* Forward Declarations */
static void	e_spell_checker_init_webkit_checker
				(WebKitSpellCheckerInterface *interface);
=======
static void e_spell_checker_init_webkit_checker (WebKitSpellCheckerInterface *iface);
>>>>>>> Move spell-checking parts to e-util

G_DEFINE_TYPE_EXTENDED (
	ESpellChecker,
	e_spell_checker,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
<<<<<<< HEAD
		E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (
		WEBKIT_TYPE_SPELL_CHECKER,
		e_spell_checker_init_webkit_checker))

=======
		WEBKIT_TYPE_SPELL_CHECKER,
		e_spell_checker_init_webkit_checker))

struct _ESpellCheckerPrivate {
	GList *active;

	EnchantBroker *broker;
};

enum {
	PROP_0,
	PROP_ACTIVE_DICTIONARIES
};

>>>>>>> Move spell-checking parts to e-util
/**
 * ESpellChecker:
 *
 * #ESpellChecker represents a spellchecker in Evolution. It can be used as a
 * provider for dictionaries. It also implements #WebKitSpellCheckerInterface,
 * so it can be set as a default spell-checker to WebKit editors
 */

<<<<<<< HEAD
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
=======
static void
wksc_check_spelling (WebKitSpellChecker *webkit_checker,
		     const char         *word,
		     int                *misspelling_location,
		     int                *misspelling_length)
{
	ESpellChecker *checker = E_SPELL_CHECKER (webkit_checker);
	PangoLanguage *language;
	PangoLogAttr *attrs;
	GList *dicts;
	gint length, ii;

	dicts = checker->priv->active;
	if (!dicts)
>>>>>>> Move spell-checking parts to e-util
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
<<<<<<< HEAD
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
=======
			int start = ii;
			int end = ii;
			int word_length;
			gchar *cstart;
			gint bytes;
			gchar *new_word;
			GList *iter;

			while (attrs[end].is_word_end < 1) {
				end++;
			}

			word_length = end - start;
			/* Set the iterator to be at the current word end, so we don't
			 * check characters twice. */
>>>>>>> Move spell-checking parts to e-util
			ii = end;

			cstart = g_utf8_offset_to_pointer (word, start);
			bytes = g_utf8_offset_to_pointer (word, end) - cstart;
			new_word = g_new0 (gchar, bytes + 1);

			g_utf8_strncpy (new_word, cstart, word_length);

<<<<<<< HEAD
			word_recognized = e_spell_checker_check_word (
				checker, new_word, word_length);

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
=======
			for (iter = dicts; iter; iter = iter->next) {
				ESpellDictionary *dict = iter->data;

				if (e_spell_dictionary_check (dict, new_word, word_length)) {
					if (misspelling_location)
						*misspelling_location = start;
					if (misspelling_length)
						*misspelling_length = word_length;
				} else {
					/* Stop checking, this word is ok in at
					 * least one dict. */
					if (misspelling_location)
						*misspelling_location = -1;
					if (misspelling_length)
						*misspelling_length = 0;
					break;
				}
>>>>>>> Move spell-checking parts to e-util
			}

			g_free (new_word);
		}
	}

	g_free (attrs);
}

static gchar **
wksc_get_guesses (WebKitSpellChecker *webkit_checker,
<<<<<<< HEAD
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

=======
		  const gchar *word,
		  const gchar *context)
{
	ESpellChecker *checker = E_SPELL_CHECKER (webkit_checker);
	GList *dicts;
	char** guesses;
	gint ii;

	guesses = g_malloc0_n (sizeof (gchar *), 11);
	ii = 0;
	for (dicts = checker->priv->active; dicts && ii < 10; dicts = dicts->next) {
		ESpellDictionary *dict;
		GList *suggestions, *iter;
		gint suggestions_count;

		dict = dicts->data;
		suggestions = e_spell_dictionary_get_suggestions (dict, word, -1);

		suggestions_count = g_list_length (suggestions);
		if (suggestions_count > 0) {
			if (suggestions_count > 10) {
				suggestions_count = 10;
			}

			for (iter = suggestions; iter && ii < 10; iter = iter->next, ii++) {
				guesses[ii] = g_strdup (iter->data);
			}

			guesses[ii] = 0;

			e_spell_dictionary_free_suggestions (suggestions);
		}
	}

>>>>>>> Move spell-checking parts to e-util
	return guesses;
}

static gchar *
wksc_get_autocorrect_suggestions (WebKitSpellChecker *webkit_checker,
<<<<<<< HEAD
                                  const gchar *word)
=======
				  const gchar *word)
>>>>>>> Move spell-checking parts to e-util
{
	/* Not supported/needed */
	return NULL;
}

static void
spell_checker_learn_word (WebKitSpellChecker *webkit_checker,
<<<<<<< HEAD
                          const gchar *word)
=======
			  const gchar *word)
>>>>>>> Move spell-checking parts to e-util
{
	/* Carefully, this will add the word to all active dictionaries! */

	ESpellChecker *checker;
<<<<<<< HEAD
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
=======
	GList *iter;

	checker = E_SPELL_CHECKER (webkit_checker);
	for (iter = checker->priv->active; iter; iter = iter->next) {
		ESpellDictionary *dict = iter->data;

		e_spell_dictionary_learn_word (dict, word, -1);
	}
>>>>>>> Move spell-checking parts to e-util
}

static void
spell_checker_ignore_word (WebKitSpellChecker *webkit_checker,
<<<<<<< HEAD
                           const gchar *word)
=======
			   const gchar *word)
>>>>>>> Move spell-checking parts to e-util
{
	/* Carefully, this will add the word to all active dictionaries */

	ESpellChecker *checker;
<<<<<<< HEAD
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
=======
	GList *iter;

	checker = E_SPELL_CHECKER (webkit_checker);
	for (iter = checker->priv->active; iter; iter = iter->next) {
		ESpellDictionary *dict = iter->data;

		e_spell_dictionary_ignore_word (dict, word, -1);
	}
>>>>>>> Move spell-checking parts to e-util
}

static void
wksc_update_languages (WebKitSpellChecker *webkit_checker,
<<<<<<< HEAD
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
=======
		       const char *languages)
{
	ESpellChecker *checker;
	GList *dictionaries = NULL;
	gchar **langs;
	gint ii;

	checker = E_SPELL_CHECKER (webkit_checker);
	if (languages) {
		langs = g_strsplit (languages, ",", -1);
		for (ii = 0; langs[ii] != NULL; ii++) {
			ESpellDictionary *dict;

			dict = e_spell_checker_lookup_dictionary (checker, langs[ii]);
			dictionaries = g_list_append (dictionaries, dict);
		}
		g_strfreev (langs);
	} else {
		const gchar *language;
		ESpellDictionary *dict;

		language = pango_language_to_string (gtk_get_default_language ());
		dict = e_spell_checker_lookup_dictionary (checker, language);
		if (dict) {
			dictionaries = g_list_append (dictionaries, dict);
		} else {
			dictionaries = e_spell_checker_list_available_dicts (checker);
		}
	}

	e_spell_checker_set_active_dictionaries (checker, dictionaries);
	g_list_free_full (dictionaries, g_object_unref);
}


static void
spell_checker_set_property (GObject *object,
			    guint property_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_DICTIONARIES:
			e_spell_checker_set_active_dictionaries (
				E_SPELL_CHECKER (object),
				g_value_get_pointer (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
>>>>>>> Move spell-checking parts to e-util
}

static void
spell_checker_get_property (GObject *object,
<<<<<<< HEAD
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
=======
			    guint property_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_DICTIONARIES:
			g_value_set_pointer (
				value,
				e_spell_checker_get_active_dictionaries (
					E_SPELL_CHECKER (object)));
>>>>>>> Move spell-checking parts to e-util
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spell_checker_dispose (GObject *object)
{
<<<<<<< HEAD
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

	/* Freeing EnchantDicts requires help from EnchantBroker. */
	g_hash_table_foreach_remove (
		priv->enchant_dicts,
		spell_checker_enchant_dicts_foreach_cb,
		priv->broker);
	g_hash_table_destroy (priv->enchant_dicts);

	enchant_broker_free (priv->broker);

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
	GHashTable *enchant_dicts;

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

	enchant_dicts = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	checker->priv = E_SPELL_CHECKER_GET_PRIVATE (checker);

	checker->priv->broker = enchant_broker_init ();
	checker->priv->active_dictionaries = active_dictionaries;
	checker->priv->dictionaries_cache = dictionaries_cache;
	checker->priv->enchant_dicts = enchant_dicts;
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
	ESpellChecker *checker = user_data;
	EnchantDict *enchant_dict;

	enchant_dict = enchant_broker_request_dict (
		checker->priv->broker, lang_tag);
	if (enchant_dict != NULL) {
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

		g_hash_table_insert (
			checker->priv->enchant_dicts,
			g_strdup (code), enchant_dict);
	}
}

=======
	ESpellCheckerPrivate *priv = E_SPELL_CHECKER (object)->priv;

	g_list_free_full (priv->active, g_object_unref);
	priv->active = NULL;

	enchant_broker_free (priv->broker);
	priv->broker = NULL;

	/* Chain up to parent implementation */
	G_OBJECT_CLASS (e_spell_checker_parent_class)->dispose (object);
}

static void
e_spell_checker_class_init (ESpellCheckerClass *klass)
{
	GObjectClass *object_class;

	e_spell_checker_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (ESpellCheckerPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = spell_checker_set_property;
	object_class->get_property = spell_checker_get_property;
	object_class->dispose = spell_checker_dispose;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_DICTIONARIES,
		g_param_spec_pointer (
			"active-dictionaries",
			NULL,
			"List of active dictionaries to use for spell-checking",
			G_PARAM_READWRITE));
}

static void
e_spell_checker_init_webkit_checker (WebKitSpellCheckerInterface *iface)
{
	iface->check_spelling_of_string = wksc_check_spelling;
	iface->get_autocorrect_suggestions_for_misspelled_word = wksc_get_autocorrect_suggestions;
	iface->get_guesses_for_word = wksc_get_guesses;
	iface->ignore_word = spell_checker_ignore_word;
	iface->learn_word = spell_checker_learn_word;
	iface->update_spell_checking_languages = wksc_update_languages;
}


static void
e_spell_checker_init (ESpellChecker *checker)
{
	checker->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		checker, E_TYPE_SPELL_CHECKER, ESpellCheckerPrivate);

	checker->priv->broker = enchant_broker_init ();
}

ESpellChecker *
e_spell_checker_new (void)
{
	return g_object_new (E_TYPE_SPELL_CHECKER, NULL);
}


typedef struct  {
	ESpellChecker *checker;
	GList *dicts;
} ListAvailDictsData;

static void
list_enchant_dicts (const char * const lang_tag,
		    const char * const provider_name,
		    const char * const provider_desc,
		    const char * const provider_file,
		    void * user_data)
{
	ListAvailDictsData *data = user_data;
	EnchantDict *dict;

	dict = enchant_broker_request_dict (data->checker->priv->broker, lang_tag);
	if (dict) {
		ESpellDictionary *e_dict;

		e_dict = e_spell_dictionary_new (data->checker, dict);

		data->dicts = g_list_prepend (data->dicts, e_dict);
	}
}


>>>>>>> Move spell-checking parts to e-util
/**
 * e_spell_checker_list_available_dicts:
 * @checker: An #ESpellChecker
 *
 * Returns list of all dictionaries available to the actual
 * spell-checking backend.
 *
<<<<<<< HEAD
 * Returns: new copy of #GList of #ESpellDictionary. The dictionaries are
 * owned by the @checker and should not be free'd. The list should be freed
 * using g_list_free() when not neede anymore. [transfer-list]
=======
 * Return value: a #GList of #ESpellDictionary. Free the list using g_list_free()
 * 		when not needed anymore.
>>>>>>> Move spell-checking parts to e-util
 */
GList *
e_spell_checker_list_available_dicts (ESpellChecker *checker)
{
<<<<<<< HEAD
	GList *list;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);

	if (!checker->priv->dictionaries_loaded) {
		enchant_broker_list_dicts (
			checker->priv->broker,
			list_enchant_dicts, checker);
		checker->priv->dictionaries_loaded = TRUE;
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

	return g_hash_table_lookup (
		checker->priv->enchant_dicts, language_code);
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
=======
	ESpellChecker *e_checker;
	ListAvailDictsData data = { 0 };

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);

	e_checker = E_SPELL_CHECKER (checker);

	data.checker = e_checker;
	enchant_broker_list_dicts (
		e_checker->priv->broker, list_enchant_dicts, &data);

	return g_list_reverse (data.dicts);
}

/**
 * e_spell_checker_lookup_dictionary:
 * @checker: an #ESpellChecker
 * @language_code: (allow-none) language code for which to lookup the dictionary
 *
 * Tries to find an #ESpellDictionary for given @language_code. When @language_code
 * is #NULL, this function will return a default #ESpellDictionary.
 *
 * Return value: an #ESpellDictionary for @language_code
 */
ESpellDictionary *
e_spell_checker_lookup_dictionary (ESpellChecker *checker,
				   const gchar *language_code)
{
	ESpellChecker *e_checker;
	ESpellDictionary *e_dict;

	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);

	e_checker = E_SPELL_CHECKER (checker);

	e_dict = NULL;

	if (!language_code) {
		GList *dicts = e_spell_checker_list_available_dicts (checker);

		if (dicts) {
			e_dict = g_object_ref (dicts->data);
			g_list_free_full (dicts, g_object_unref);
		}
	} else {
		EnchantDict *dict;
		dict = enchant_broker_request_dict (
			e_checker->priv->broker, language_code);
		if (dict) {
			e_dict = e_spell_dictionary_new (checker, dict);
		}
	}

	return e_dict;
}

/**
 * e_spell_checker_get_active_dictionaries:
 * @checker: an #ESpellChecker
 *
 * Returns a list of #ESpellDictionary that are to be used for spell-checking.
 *
 * Return value: a #GList of #ESpellDictionary. Free the list using g_list_fre()
 * 		 when no longer needed.
 */
GList *
e_spell_checker_get_active_dictionaries (ESpellChecker *checker)
{
	g_return_val_if_fail (E_IS_SPELL_CHECKER (checker), NULL);

	return g_list_copy (checker->priv->active);
}

/**
 * e_spell_checker_set_active_dictionaries:
 * @checker: an #ESpellChecker
 * @active_dict: a #GList of #ESpellDictionary to use for spell-checking
 *
 * Set dictionaries to be actively used for spell-checking.
 */
void
e_spell_checker_set_active_dictionaries (ESpellChecker *checker,
					 GList *active_dicts)
{
	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	g_list_free_full (checker->priv->active, (GDestroyNotify) g_object_unref);

	checker->priv->active = g_list_copy (active_dicts);
	g_list_foreach (checker->priv->active, (GFunc) g_object_ref, NULL);
}

void
e_spell_checker_free_dict (ESpellChecker *checker,
			   EnchantDict *dict)
{
	g_return_if_fail (E_IS_SPELL_CHECKER (checker));
	g_return_if_fail (dict != NULL);

	enchant_broker_free_dict (checker->priv->broker, dict);
>>>>>>> Move spell-checking parts to e-util
}

/**
 * e_spell_checker_ignore_word:
 * @checker: an #ESpellChecker
 * @word: word to ignore for the rest of session
 *
<<<<<<< HEAD
 * Calls e_spell_dictionary_ignore_word() on all active dictionaries in
=======
 * Calls #e_spell_dictionary_ignore_word() on all active dictionaries in
>>>>>>> Move spell-checking parts to e-util
 * the @checker.
 */
void
e_spell_checker_ignore_word (ESpellChecker *checker,
<<<<<<< HEAD
                             const gchar *word)
{
	WebKitSpellCheckerInterface *interface;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	interface = WEBKIT_SPELL_CHECKER_GET_IFACE (checker);
	interface->ignore_word (WEBKIT_SPELL_CHECKER (checker), word);
=======
			     const gchar *word)
{
	WebKitSpellCheckerInterface *webkit_checker_iface;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	webkit_checker_iface = WEBKIT_SPELL_CHECKER_GET_IFACE (checker);
	webkit_checker_iface->ignore_word (WEBKIT_SPELL_CHECKER (checker), word);
>>>>>>> Move spell-checking parts to e-util
}

/**
 * e_spell_checker_learn_word:
 * @checker: an #ESpellChecker
 * @word: word to learn
 *
<<<<<<< HEAD
 * Calls e_spell_dictionary_learn_word() on all active dictionaries in
=======
 * Calls #e_spell_dictionary_learn_word() on all active dictionaries in
>>>>>>> Move spell-checking parts to e-util
 * the @checker.
 */
void
e_spell_checker_learn_word (ESpellChecker *checker,
<<<<<<< HEAD
                            const gchar *word)
{
	WebKitSpellCheckerInterface *interface;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	interface = WEBKIT_SPELL_CHECKER_GET_IFACE (checker);
	interface->learn_word (WEBKIT_SPELL_CHECKER (checker), word);
=======
			    const gchar *word)
{
	WebKitSpellCheckerInterface *webkit_checker_iface;

	g_return_if_fail (E_IS_SPELL_CHECKER (checker));

	webkit_checker_iface = WEBKIT_SPELL_CHECKER_GET_IFACE (checker);
	webkit_checker_iface->learn_word (WEBKIT_SPELL_CHECKER (checker), word);
>>>>>>> Move spell-checking parts to e-util
}
