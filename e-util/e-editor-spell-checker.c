 /*
 * e-editor-spell-checker.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

 /* Based on webkitspellcheckerenchant.cpp */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-editor-spell-checker.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <pango/pango.h>
#include <enchant/enchant.h>
#include <webkit/webkitspellchecker.h>

#include "e-editor.h"

static void e_editor_spell_checker_interface_init (WebKitSpellCheckerInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EEditorSpellChecker,
	e_editor_spell_checker,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		WEBKIT_TYPE_SPELL_CHECKER,
		e_editor_spell_checker_interface_init));

#define ISO_639_DOMAIN	"iso_639"
#define ISO_3166_DOMAIN	"iso_3166"

struct _EEditorSpellCheckerPrivate {
	EnchantBroker *broker;

	GList *dicts;

	/* Dictionary to which to write new words */
	EnchantDict *write_dictionary;
};

struct _available_dictionaries_data {
	EEditorSpellChecker *checker;
	GList *dicts;
};

struct _enchant_dict_description_data {
	const gchar *language_tag;
	gchar *dict_name;
};

static GHashTable *iso_639_table = NULL;
static GHashTable *iso_3166_table = NULL;

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

	if (g_strcmp0 (element_name, "iso_639_entry") != 0)
		return;

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


static void
get_available_dictionaries (const char *language_tag,
			    const char *provider_name,
			    const char *provider_desc,
			    const char *provider_file,
			    gpointer user_data)
{
	struct _available_dictionaries_data *data = user_data;
	EnchantDict *dict;

	dict = enchant_broker_request_dict (
			data->checker->priv->broker, language_tag);
	if (dict) {
		data->dicts = g_list_append (data->dicts, dict);
	}
}

static void
describe_dictionary (const gchar *language_tag,
		     const gchar *provider_name,
		     const gchar *provider_desc,
		     const gchar *provider_file,
		     gpointer user_data)
{
	struct _enchant_dict_description_data *data = user_data;
	const gchar *iso_639_name;
	const gchar *iso_3166_name;
	gchar *language_name;
	gchar *lowercase;
	gchar **tokens;

	/* Split language code into lowercase tokens. */
	lowercase = g_ascii_strdown (language_tag, -1);
	tokens = g_strsplit (lowercase, "_", -1);
	g_free (lowercase);

	g_return_if_fail (tokens != NULL);

	iso_639_name = g_hash_table_lookup (iso_639_table, tokens[0]);

	if (iso_639_name == NULL) {
		language_name = g_strdup_printf (
		/* Translators: %s is the language ISO code. */
			C_("language", "Unknown (%s)"), language_tag);
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

	data->language_tag = language_tag;
	data->dict_name = language_name;
}

static void
free_dictionary (gpointer data,
		 gpointer user_data)
{
	EEditorSpellChecker *checker = user_data;

	enchant_broker_free_dict (checker->priv->broker, data);
}

static void
update_spell_checking_languages (WebKitSpellChecker *ichecker,
				 const char *languages)
{
	GList* spell_dictionaries = 0;
	EEditorSpellChecker *checker = E_EDITOR_SPELL_CHECKER (ichecker);

	if (languages) {
		gchar **langs;
		gint ii;

		langs = g_strsplit (languages, ",", -1);
		for (ii = 0; langs[ii]; ii++) {
			if (enchant_broker_dict_exists (
					checker->priv->broker, langs[ii])) {

                		EnchantDict* dict =
                			enchant_broker_request_dict (
						checker->priv->broker, langs[ii]);
                		spell_dictionaries = g_list_append (
						spell_dictionaries, dict);
            		}
        	}
        	g_strfreev (langs);
    	} else {
		const char* language;

		language = pango_language_to_string (gtk_get_default_language ());
		if (enchant_broker_dict_exists (checker->priv->broker, language)) {
			EnchantDict* dict;

			dict = enchant_broker_request_dict (
				checker->priv->broker, language);
			spell_dictionaries =
				g_list_append (spell_dictionaries, dict);
		} else {
			spell_dictionaries =
				e_editor_spell_checker_get_available_dicts (checker);
		}
	}

	g_list_foreach (checker->priv->dicts, free_dictionary, checker);
	g_list_free (checker->priv->dicts);
	checker->priv->dicts = spell_dictionaries;
}


static void
check_spelling_of_string (WebKitSpellChecker *webkit_checker,
			  const gchar *word,
			  gint *misspelling_location,
			  gint *misspelling_length)
{
	EEditorSpellChecker *checker = E_EDITOR_SPELL_CHECKER (webkit_checker);
	PangoLanguage *language;
	PangoLogAttr *attrs;
	GList *dicts;
	gint length, ii;

	dicts = checker->priv->dicts;
    	if (!dicts)
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
            		ii = end;

            		cstart = g_utf8_offset_to_pointer (word, start);
            		bytes = g_utf8_offset_to_pointer (word, end) - cstart;
            		new_word = g_new0 (gchar, bytes + 1);

            		g_utf8_strncpy (new_word, cstart, word_length);

            		for (iter = dicts; iter; iter = iter->next) {
                		EnchantDict* dict = iter->data;

				if (enchant_dict_check (dict, new_word, word_length)) {
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
            		}

            		g_free (new_word);
        	}
    	}

    	g_free (attrs);
}

static gchar **
get_guesses_for_word (WebKitSpellChecker *webkit_checker,
		      const gchar *word,
		      const gchar *context)
{
	EEditorSpellChecker *checker = E_EDITOR_SPELL_CHECKER (webkit_checker);
	GList *dicts;
	char** guesses = 0;

	g_return_val_if_fail (E_IS_EDITOR_SPELL_CHECKER (checker), NULL);

    	for (dicts = checker->priv->dicts; dicts; dicts = dicts->next) {
		EnchantDict *dict;
		gchar **suggestions;
        	size_t suggestions_count;
        	size_t ii;

        	dict = dicts->data;
        	suggestions = enchant_dict_suggest (dict, word, -1, &suggestions_count);

        	if (suggestions_count > 0) {
            		if (suggestions_count > 10) {
                		suggestions_count = 10;
			}

            		guesses = g_malloc0 ((suggestions_count + 1) * sizeof (char *));
            		for (ii = 0; ii < suggestions_count && ii < 10; ii++) {
                		guesses[ii] = g_strdup (suggestions[ii]);
			}

            		guesses[ii] = 0;

            		enchant_dict_free_suggestions (dict, suggestions);
        	}
    	}

    	return guesses;
}

static void
ignore_word (WebKitSpellChecker *checker,
	     const gchar *word)
{
	EEditorSpellChecker *editor_spellchecker;

	editor_spellchecker = E_EDITOR_SPELL_CHECKER (checker);
	enchant_dict_add_to_session (
		editor_spellchecker->priv->write_dictionary, word, -1);
}

static void
learn_word (WebKitSpellChecker *checker,
	    const gchar *word)
{
	EEditorSpellChecker *editor_spellchecker;

	editor_spellchecker = E_EDITOR_SPELL_CHECKER (checker);
	enchant_dict_add_to_personal (
		editor_spellchecker->priv->write_dictionary, word, -1);
}

static gchar *
get_autocorrect_suggestions (WebKitSpellChecker *checker,
			     const gchar *word)
{
	/* Not implemented, not needed */
	return 0;
}


static void
editor_spell_checker_finalize (GObject *object)
{
	EEditorSpellCheckerPrivate *priv = E_EDITOR_SPELL_CHECKER (object)->priv;

	if (priv->broker) {
		enchant_broker_free (priv->broker);
		priv->broker = NULL;
	}

	if (priv->dicts) {
		g_list_free (priv->dicts);
		priv->dicts = NULL;
	}

	/* Chain up to parent implementation */
	G_OBJECT_CLASS (e_editor_spell_checker_parent_class)->finalize (object);
}

static void
e_editor_spell_checker_class_init (EEditorSpellCheckerClass *klass)
{
	GObjectClass *object_class;

	e_editor_spell_checker_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorSpellCheckerPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = editor_spell_checker_finalize;
}

static void
e_editor_spell_checker_interface_init (WebKitSpellCheckerInterface *iface)
{
	iface->check_spelling_of_string = check_spelling_of_string;
	iface->get_autocorrect_suggestions_for_misspelled_word = get_autocorrect_suggestions;
	iface->get_guesses_for_word = get_guesses_for_word;
	iface->update_spell_checking_languages = update_spell_checking_languages;
	iface->ignore_word = ignore_word;
	iface->learn_word = learn_word;
}

static void
e_editor_spell_checker_init (EEditorSpellChecker *checker)
{
	checker->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		checker, E_TYPE_EDITOR_SPELL_CHECKER, EEditorSpellCheckerPrivate);

	checker->priv->broker = enchant_broker_init ();

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

}

void
e_editor_spell_checker_set_dictionaries (EEditorSpellChecker *checker,
					 GList *dictionaries)
{
	g_return_if_fail (E_IS_EDITOR_SPELL_CHECKER (checker));

	g_list_foreach (checker->priv->dicts, free_dictionary, checker);
	g_list_free (checker->priv->dicts);

	checker->priv->dicts = g_list_copy (dictionaries);
}

EnchantDict *
e_editor_spell_checker_lookup_dict (EEditorSpellChecker *checker,
				    const gchar *language_code)
{
	g_return_val_if_fail (E_IS_EDITOR_SPELL_CHECKER (checker), NULL);
	g_return_val_if_fail (language_code != NULL, NULL);

	if (!enchant_broker_dict_exists (checker->priv->broker, language_code)) {
		return NULL;
	}

	return enchant_broker_request_dict (checker->priv->broker, language_code);
}

void
e_editor_spell_checker_free_dict (EEditorSpellChecker *checker,
				  EnchantDict *dict)
{
	g_return_if_fail (E_IS_EDITOR_SPELL_CHECKER (checker));
	g_return_if_fail (dict != NULL);

	enchant_broker_free_dict (checker->priv->broker, dict);
}


gint
e_editor_spell_checker_dict_compare (const EnchantDict *dict_a,
				     const EnchantDict *dict_b)
{
	const gchar *dict_a_name, *dict_b_name;
	gchar *dict_a_ckey, *dict_b_ckey;
	gint result;

	dict_a_name = e_editor_spell_checker_get_dict_name (dict_a);
	dict_b_name = e_editor_spell_checker_get_dict_name (dict_b);

	dict_a_ckey = g_utf8_collate_key (dict_a_name, -1);
	dict_b_ckey = g_utf8_collate_key (dict_b_name, -1);

	result = g_strcmp0 (dict_a_ckey, dict_b_ckey);

	g_free (dict_a_ckey);
	g_free (dict_b_ckey);

	return result;
}

GList *
e_editor_spell_checker_get_available_dicts (EEditorSpellChecker *checker)
{
	struct _available_dictionaries_data data;

	g_return_val_if_fail (E_IS_EDITOR_SPELL_CHECKER (checker), NULL);

	data.checker = checker;
	data.dicts = NULL;

	enchant_broker_list_dicts (
		checker->priv->broker, get_available_dictionaries, &data);

	return data.dicts;
}

const gchar *
e_editor_spell_checker_get_dict_name (const EnchantDict *dictionary)
{
	struct _enchant_dict_description_data data;

	enchant_dict_describe (
		(EnchantDict *) dictionary, describe_dictionary, &data);

	return data.dict_name;
}

const gchar *
e_editor_spell_checker_get_dict_code (const EnchantDict *dictionary)
{
	struct _enchant_dict_description_data data;

	enchant_dict_describe (
		(EnchantDict *) dictionary, describe_dictionary, &data);

	return data.language_tag;
}
