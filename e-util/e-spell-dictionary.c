<<<<<<< HEAD
/*
 * e-spell-dictionary.c
=======
/* e-spell-dictionary.c
>>>>>>> Import classes for spell checking
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

<<<<<<< HEAD
=======

>>>>>>> Import classes for spell checking
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-spell-dictionary.h"
<<<<<<< HEAD
#include "e-spell-checker.h"

#include <glib/gi18n-lib.h>
#include <string.h>

#define E_SPELL_DICTIONARY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SPELL_DICTIONARY, ESpellDictionaryPrivate))

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

#define ISO_639_DOMAIN	"iso_639"
#define ISO_3166_DOMAIN	"iso_3166"

static GHashTable *iso_639_table = NULL;
static GHashTable *iso_3166_table = NULL;

G_DEFINE_TYPE (
	ESpellDictionary,
	e_spell_dictionary,
	G_TYPE_OBJECT);

=======

#include <string.h>
#include <enchant.h>
#include <glib/gi18n-lib.h>

#define ISO_639_DOMAIN	"iso_639"
#define ISO_3166_DOMAIN	"iso_3166"

struct _ESpellDictionary {
	gchar *code;
	gchar *name;
	gchar *ckey;
};

static GHashTable *iso_639_table = NULL;
static GHashTable *iso_3166_table = NULL;

>>>>>>> Import classes for spell checking
#ifdef HAVE_ISO_CODES

#define ISOCODESLOCALEDIR ISO_CODES_PREFIX "/share/locale"

#ifdef G_OS_WIN32
#ifdef DATADIR
#undef DATADIR
#endif
#include <shlobj.h>
static HMODULE hmodule;

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD fdwReason,
         LPVOID lpvReserved);

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD fdwReason,
         LPVOID lpvReserved)
{
	switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
		hmodule = hinstDLL;
		break;
    }

	return TRUE;
}

static gchar *
_get_iso_codes_prefix (void)
{
	static gchar retval[1000];
	static gint beenhere = 0;
	gchar *temp_dir = 0;

	if (beenhere)
		return retval;

	if (!(temp_dir = g_win32_get_package_installation_directory_of_module ((gpointer) hmodule))) {
		strcpy (retval, ISO_CODES_PREFIX);
		return retval;
	}

	strcpy (retval, temp_dir);
	g_free (temp_dir);
	beenhere = 1;
	return retval;
}

static gchar *
_get_isocodeslocaledir (void)
{
	static gchar retval[1000];
	static gint beenhere = 0;

	if (beenhere)
		return retval;

	strcpy (retval, _get_iso_codes_prefix ());
	strcat (retval, "\\share\\locale" );
	beenhere = 1;
	return retval;
}

#undef ISO_CODES_PREFIX
#define ISO_CODES_PREFIX _get_iso_codes_prefix ()

#undef ISOCODESLOCALEDIR
#define ISOCODESLOCALEDIR _get_isocodeslocaledir ()

#endif

static void
iso_639_start_element (GMarkupParseContext *context,
                       const gchar *element_name,
                       const gchar **attribute_names,
                       const gchar **attribute_values,
                       gpointer data,
                       GError **error)
{
	GHashTable *hash_table = data;
	const gchar *iso_639_1_code = NULL;
	const gchar *iso_639_2_code = NULL;
	const gchar *name = NULL;
	const gchar *code = NULL;
	gint ii;

<<<<<<< HEAD
	if (g_strcmp0 (element_name, "iso_639_entry") != 0) {
		return;
	}
=======
	if (strcmp (element_name, "iso_639_entry") != 0)
		return;
>>>>>>> Import classes for spell checking

	for (ii = 0; attribute_names[ii] != NULL; ii++) {
		if (strcmp (attribute_names[ii], "name") == 0)
			name = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "iso_639_1_code") == 0)
			iso_639_1_code = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "iso_639_2T_code") == 0)
			iso_639_2_code = attribute_values[ii];
	}

	code = (iso_639_1_code != NULL) ? iso_639_1_code : iso_639_2_code;

	if (code != NULL && *code != '\0' && name != NULL && *name != '\0')
		g_hash_table_insert (
			hash_table, g_strdup (code),
			g_strdup (dgettext (ISO_639_DOMAIN, name)));
}

static void
iso_3166_start_element (GMarkupParseContext *context,
                        const gchar *element_name,
                        const gchar **attribute_names,
                        const gchar **attribute_values,
                        gpointer data,
                        GError **error)
{
	GHashTable *hash_table = data;
	const gchar *name = NULL;
	const gchar *code = NULL;
	gint ii;

	if (strcmp (element_name, "iso_3166_entry") != 0)
		return;

	for (ii = 0; attribute_names[ii] != NULL; ii++) {
		if (strcmp (attribute_names[ii], "name") == 0)
			name = attribute_values[ii];
		else if (strcmp (attribute_names[ii], "alpha_2_code") == 0)
			code = attribute_values[ii];
	}

	if (code != NULL && *code != '\0' && name != NULL && *name != '\0')
		g_hash_table_insert (
			hash_table, g_ascii_strdown (code, -1),
			g_strdup (dgettext (ISO_3166_DOMAIN, name)));
}

static GMarkupParser iso_639_parser = {
	iso_639_start_element,
	NULL, NULL, NULL, NULL
};

static GMarkupParser iso_3166_parser = {
	iso_3166_start_element,
	NULL, NULL, NULL, NULL
};

static void
iso_codes_parse (const GMarkupParser *parser,
                 const gchar *basename,
                 GHashTable *hash_table)
{
	GMappedFile *mapped_file;
	gchar *filename;
	GError *error = NULL;

	filename = g_build_filename (
		ISO_CODES_PREFIX, "share", "xml",
		"iso-codes", basename, NULL);
	mapped_file = g_mapped_file_new (filename, FALSE, &error);
	g_free (filename);

	if (mapped_file != NULL) {
		GMarkupParseContext *context;
		const gchar *contents;
		gsize length;

		context = g_markup_parse_context_new (
			parser, 0, hash_table, NULL);
		contents = g_mapped_file_get_contents (mapped_file);
		length = g_mapped_file_get_length (mapped_file);
		g_markup_parse_context_parse (
			context, contents, length, &error);
		g_markup_parse_context_free (context);
#if GLIB_CHECK_VERSION(2,21,3)
		g_mapped_file_unref (mapped_file);
#else
		g_mapped_file_free (mapped_file);
#endif
	}

	if (error != NULL) {
		g_warning ("%s: %s", basename, error->message);
		g_error_free (error);
	}
}

#endif /* HAVE_ISO_CODES */

<<<<<<< HEAD
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
=======
static void
spell_dictionary_describe_cb (const gchar * const language_code,
			      const gchar * const provider_name,
			      const gchar * const provider_desc,
			      const gchar * const provider_file,
			      GTree *tree)
{
>>>>>>> Import classes for spell checking
	const gchar *iso_639_name;
	const gchar *iso_3166_name;
	gchar *language_name;
	gchar *lowercase;
	gchar **tokens;

	/* Split language code into lowercase tokens. */
<<<<<<< HEAD
	lowercase = g_ascii_strdown (language_tag, -1);
=======
	lowercase = g_ascii_strdown (language_code, -1);
>>>>>>> Import classes for spell checking
	tokens = g_strsplit (lowercase, "_", -1);
	g_free (lowercase);

	g_return_if_fail (tokens != NULL);

	iso_639_name = g_hash_table_lookup (iso_639_table, tokens[0]);

	if (iso_639_name == NULL) {
		language_name = g_strdup_printf (
		/* Translators: %s is the language ISO code. */
<<<<<<< HEAD
			C_("language", "Unknown (%s)"), language_tag);
=======
			C_("language", "Unknown (%s)"), language_code);
>>>>>>> Import classes for spell checking
		goto exit;
	}

	if (g_strv_length (tokens) < 2) {
		language_name = g_strdup (iso_639_name);
		goto exit;
	}

	iso_3166_name = g_hash_table_lookup (iso_3166_table, tokens[1]);

	if (iso_3166_name != NULL)
		language_name = g_strdup_printf (
		 /* Translators: The first %s is the language name, and the
		 * second is the country name. Example: "French (France)" */
			C_("language", "%s (%s)"), iso_639_name, iso_3166_name);
	else
		language_name = g_strdup_printf (
		 /* Translators: The first %s is the language name, and the
		 * second is the country name. Example: "French (France)" */
			C_("language", "%s (%s)"), iso_639_name, tokens[1]);

exit:
	g_strfreev (tokens);

<<<<<<< HEAD
	data->language_tag = g_strdup (language_tag);
	data->dict_name = language_name;
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
	ESpellDictionaryPrivate *priv;

	priv = E_SPELL_DICTIONARY_GET_PRIVATE (object);

	g_weak_ref_set (&priv->spell_checker, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_spell_dictionary_parent_class)->dispose (object);
}

static void
spell_dictionary_finalize (GObject *object)
{
	ESpellDictionaryPrivate *priv;

	priv = E_SPELL_DICTIONARY_GET_PRIVATE (object);

	g_free (priv->name);
	g_free (priv->code);
	g_free (priv->collate_key);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_spell_dictionary_parent_class)->finalize (object);
}

static void
e_spell_dictionary_class_init (ESpellDictionaryClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ESpellDictionaryPrivate));

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
	dictionary->priv = E_SPELL_DICTIONARY_GET_PRIVATE (dictionary);

	if (!iso_639_table && !iso_3166_table) {
#if defined (ENABLE_NLS) && defined (HAVE_ISO_CODES)
		bindtextdomain (ISO_639_DOMAIN, ISOCODESLOCALEDIR);
		bind_textdomain_codeset (ISO_639_DOMAIN, "UTF-8");

		bindtextdomain (ISO_3166_DOMAIN, ISOCODESLOCALEDIR);
		bind_textdomain_codeset (ISO_3166_DOMAIN, "UTF-8");
#endif /* ENABLE_NLS && HAVE_ISO_CODES */

		iso_639_table = g_hash_table_new_full (
			(GHashFunc) g_str_hash,
			(GEqualFunc) g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) g_free);

		iso_3166_table = g_hash_table_new_full (
			(GHashFunc) g_str_hash,
			(GEqualFunc) g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) g_free);

#ifdef HAVE_ISO_CODES
		iso_codes_parse (
			&iso_639_parser, "iso_639.xml", iso_639_table);
		iso_codes_parse (
			&iso_3166_parser, "iso_3166.xml", iso_3166_table);
#endif /* HAVE_ISO_CODES */
	}
}

ESpellDictionary *
e_spell_dictionary_new (ESpellChecker *spell_checker,
                        EnchantDict *enchant_dict)
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

	enchant_dict_add_to_personal (enchant_dict, word, length);

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
	enchant_dict_free_suggestions (enchant_dict, suggestions);

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

=======
	g_tree_replace (tree, g_strdup (language_code), language_name);
}

static const ESpellDictionary *
spell_dictionary_copy (const ESpellDictionary *language)
{
	return language;
}

static void
spell_dictionary_free (const ESpellDictionary *language)
{
	/* do nothing */
}

static const ESpellDictionary *
spell_dictionary_lookup (const gchar *language_code)
{
	const ESpellDictionary *closest_match = NULL;
	const GList *available_dicts;

	available_dicts = e_spell_dictionary_get_available ();

	while (available_dicts != NULL && language_code != NULL) {
		ESpellDictionary *dict = available_dicts->data;
		const gchar *code = dict->code;
		gsize length = strlen (code);

		if (g_ascii_strcasecmp (language_code, code) == 0)
			return dict;

		if (g_ascii_strncasecmp (language_code, code, length) == 0)
			closest_match = dict;

		available_dicts = g_list_next (available_dicts);
	}

	return closest_match;
}

static gboolean
spell_dictionary_traverse_cb (const gchar *code,
                            const gchar *name,
                            GList **available_languages)
{
	ESpellDictionary *dict;

	dict = g_slice_new (ESpellDictionary);
	dict->code = g_strdup (code);
	dict->name = g_strdup (name);
	dict->ckey = g_utf8_collate_key (name, -1);

	*available_languages = g_list_insert_sorted (
		*available_languages, dict,
		(GCompareFunc) e_spell_dictionary_compare);

	return FALSE;
}

GType
e_spell_dictionary_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
		type = g_boxed_type_register_static (
			"ESpellDictionary",
			(GBoxedCopyFunc) spell_dictionary_copy,
			(GBoxedFreeFunc) spell_dictionary_free);

	return type;
}

const GList *
e_spell_dictionary_get_available (void)
{
	static gboolean initialized = FALSE;
	static GList *available_dicts = NULL;
	EnchantBroker *broker;
	GTree *tree;

	if (initialized)
		return available_dicts;

	initialized = TRUE;

#if defined (ENABLE_NLS) && defined (HAVE_ISO_CODES)
	bindtextdomain (ISO_639_DOMAIN, ISOCODESLOCALEDIR);
	bind_textdomain_codeset (ISO_639_DOMAIN, "UTF-8");

	bindtextdomain (ISO_3166_DOMAIN, ISOCODESLOCALEDIR);
	bind_textdomain_codeset (ISO_3166_DOMAIN, "UTF-8");
#endif

	iso_639_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	iso_3166_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

#ifdef HAVE_ISO_CODES
	iso_codes_parse (&iso_639_parser, "iso_639.xml", iso_639_table);
	iso_codes_parse (&iso_3166_parser, "iso_3166.xml", iso_3166_table);
#endif

	tree = g_tree_new_full (
		(GCompareDataFunc) strcmp, NULL,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	broker = enchant_broker_init ();
	enchant_broker_list_dicts (
		broker, (EnchantDictDescribeFn)
		spell_dictionary_describe_cb, tree);
	enchant_broker_free (broker);

	g_tree_foreach (
		tree, (GTraverseFunc)
		spell_dictionary_traverse_cb,
		&available_dicts);

	g_tree_destroy (tree);

	return available_dicts;
}

static const ESpellDictionary *
spell_dictionary_pick_default (void)
{
	const ESpellDictionary *dictionary = NULL;
	const gchar * const *language_names;
	const GList *available_dicts;
	gint ii;

	language_names = g_get_language_names ();
	available_dicts = e_spell_dictionary_get_available ();

	for (ii = 0; dictionary == NULL && language_names[ii] != NULL; ii++)
		dictionary = spell_dictionary_lookup (language_names[ii]);

	if (dictionary == NULL)
		dictionary = spell_dictionary_lookup ("en_US");

	if (dictionary == NULL && available_dicts != NULL)
		dictionary = available_dicts->data;

	return dictionary;
}

const ESpellDictionary *
e_spell_dictionary_lookup (const gchar *language_code)
{
	const ESpellDictionary *dictionary = NULL;

	dictionary = spell_dictionary_lookup (language_code);

	if (dictionary == NULL)
		dictionary = spell_dictionary_pick_default ();

	return dictionary;
}

const gchar *
e_spell_dictionary_get_language_code (const ESpellDictionary *dictionary)
{
	g_return_val_if_fail (dictionary != NULL, NULL);

	return dictionary->code;
}

const gchar *
e_spell_dictionary_get_name (const ESpellDictionary *dictionary)
{
	if (dictionary == NULL)
		 /* Translators: This refers to the default language used
		 * by the spell checker. */
		return C_("language", "Default");

	return dictionary->name;
}

gint
e_spell_dictionary_compare (const ESpellDictionary *dict_a,
			    const ESpellDictionary *dict_b)
{
	return strcmp (dict_a->ckey, dict_b->ckey);
}
>>>>>>> Import classes for spell checking
