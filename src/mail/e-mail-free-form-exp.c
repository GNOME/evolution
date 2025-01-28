/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <string.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"
#include "em-utils.h"

#include "e-mail-free-form-exp.h"

static gchar *
mail_ffe_build_header_sexp (const gchar *word,
			    const gchar *options,
			    const gchar * const *header_names)
{
	GString *sexp = NULL, *encoded_word;
	const gchar *compare_type = NULL;
	gint ii;

	g_return_val_if_fail (header_names != NULL, NULL);
	g_return_val_if_fail (header_names[0] != NULL, NULL);

	if (!word)
		return NULL;

	if (options) {
		struct _KnownOptions {
			const gchar *compare_type;
			const gchar *alt_name;
		} known_options[] = {
			{ "contains",    "c" },
			{ "has-words",   "w" },
			{ "matches",     "m" },
			{ "starts-with", "sw" },
			{ "ends-with",   "ew" },
			{ "soundex",     "se" },
			{ "regex",       "r" },
			{ "full-regex",  "fr" }
		};

		for (ii = 0; ii < G_N_ELEMENTS (known_options); ii++) {
			if (g_ascii_strcasecmp (options, known_options[ii].compare_type) == 0 ||
			    (known_options[ii].alt_name && g_ascii_strcasecmp (options, known_options[ii].alt_name) == 0)) {
				compare_type = known_options[ii].compare_type;
				break;
			}
		}
	}

	if (!compare_type)
		compare_type = "contains";

	encoded_word = g_string_new ("");
	camel_sexp_encode_string (encoded_word, word);

	if (!header_names[1]) {
		sexp = g_string_new ("");
	} else {
		sexp = g_string_new ("(or ");
	}

	for (ii = 0; header_names[ii]; ii++) {
		g_string_append_printf (sexp, "(header-%s \"%s\" %s)", compare_type, header_names[ii], encoded_word->str);
	}

	if (header_names[1])
		g_string_append_c (sexp, ')');

	g_string_free (encoded_word, TRUE);

	return sexp ? g_string_free (sexp, FALSE) : NULL;
}

static gchar *
mail_ffe_default (const gchar *word,
		  const gchar *options,
		  const gchar *hint)
{
	const gchar *header_names[] = { "From", "To", "Cc", "Subject", NULL };

	return mail_ffe_build_header_sexp (word, options, header_names);
}

static gchar *
mail_ffe_recips (const gchar *word,
		 const gchar *options,
		 const gchar *hint)
{
	const gchar *header_names[] = { "To", "Cc", NULL };

	return mail_ffe_build_header_sexp (word, options, header_names);
}

static gchar *
mail_ffe_from (const gchar *word,
	       const gchar *options,
	       const gchar *hint)
{
	const gchar *header_names[] = { "From", NULL };

	return mail_ffe_build_header_sexp (word, options, header_names);
}

static gchar *
mail_ffe_to (const gchar *word,
	     const gchar *options,
	     const gchar *hint)
{
	const gchar *header_names[] = { "To", NULL };

	return mail_ffe_build_header_sexp (word, options, header_names);
}

static gchar *
mail_ffe_cc (const gchar *word,
	       const gchar *options,
	       const gchar *hint)
{
	const gchar *header_names[] = { "Cc", NULL };

	return mail_ffe_build_header_sexp (word, options, header_names);
}

static gchar *
mail_ffe_subject (const gchar *word,
		  const gchar *options,
		  const gchar *hint)
{
	const gchar *header_names[] = { "Subject", NULL };

	return mail_ffe_build_header_sexp (word, options, header_names);
}

static gchar *
mail_ffe_list (const gchar *word,
	       const gchar *options,
	       const gchar *hint)
{
	const gchar *header_names[] = { "x-camel-mlist", NULL };

	return mail_ffe_build_header_sexp (word, options, header_names);
}

static gchar *
mail_ffe_header (const gchar *word,
		 const gchar *options,
		 const gchar *hint)
{
	const gchar *header_names[] = { NULL, NULL };
	const gchar *equal;
	gchar *header_name, *sexp;

	equal = word ? strchr (word, '=') : NULL;
	if (!equal)
		return NULL;

	header_name = g_strndup (word, equal - word);
	header_names[0] = header_name;

	sexp = mail_ffe_build_header_sexp (equal + 1, options, header_names);

	g_free (header_name);

	return sexp;
}

static gchar *
mail_ffe_exists (const gchar *word,
		 const gchar *options,
		 const gchar *hint)
{
	GString *encoded_word;
	gchar *sexp;

	if (!word)
		return NULL;

	encoded_word = g_string_new ("");
	camel_sexp_encode_string (encoded_word, word);

	sexp = g_strdup_printf ("(header-exists %s)", encoded_word->str);

	g_string_free (encoded_word, TRUE);

	return sexp;
}

static gchar *
mail_ffe_tag (const gchar *word,
	      const gchar *options,
	      const gchar *hint)
{
	GString *encoded_word;
	gchar *sexp;

	if (!word)
		return NULL;

	encoded_word = g_string_new ("");
	camel_sexp_encode_string (encoded_word, word);

	sexp = g_strdup_printf ("(not (= (user-tag %s) \"\"))", encoded_word->str);

	g_string_free (encoded_word, TRUE);

	return sexp;
}

static gchar *
mail_ffe_flag (const gchar *word,
	       const gchar *options,
	       const gchar *hint)
{
	const gchar *system_flags[] = {
		/* Translators: This is a name of a flag, the same as all strings in the 'ffe' context.
		   The translated value should not contain spaces. */
		NC_("ffe", "Answered"),
		NC_("ffe", "Deleted"),
		NC_("ffe", "Draft"),
		NC_("ffe", "Flagged"),
		NC_("ffe", "Seen"),
		NC_("ffe", "Attachment")
	};
	GString *encoded_word;
	gchar *sexp = NULL;
	gint ii;

	if (!word)
		return NULL;

	encoded_word = g_string_new ("");
	camel_sexp_encode_string (encoded_word, word);

	for (ii = 0; ii < G_N_ELEMENTS (system_flags); ii++) {
		const gchar *flag = system_flags[ii];

		if (g_ascii_strcasecmp (word, flag) == 0 ||
		    g_ascii_strcasecmp (word, g_dpgettext2 (NULL, "ffe", flag)) == 0) {
			if (g_ascii_strcasecmp (flag, "Attachment") == 0)
				flag = "Attachments";

			sexp = g_strdup_printf ("(system-flag \"%s\")", flag);
			break;
		}
	}

	if (!sexp)
		sexp = g_strdup_printf ("(not (= (user-tag %s) \"\"))", encoded_word->str);

	g_string_free (encoded_word, TRUE);

	return sexp;
}

static gchar *
mail_ffe_label (const gchar *word,
		const gchar *options,
		const gchar *hint)
{
	GString *encoded_word;
	gchar *sexp;

	if (!word)
		return NULL;

	encoded_word = g_string_new ("");
	camel_sexp_encode_string (encoded_word, word);

	sexp = g_strdup_printf ("(or (= (user-tag \"label\") %s) (user-flag (+ \"$Label\" %s)) (user-flag %s))",
		encoded_word->str, encoded_word->str, encoded_word->str);

	g_string_free (encoded_word, TRUE);

	return sexp;
}

static gchar *
mail_ffe_size (const gchar *word,
	       const gchar *options,
	       const gchar *hint)
{
	GString *encoded_word;
	gchar *sexp;
	const gchar *cmp = "=";

	if (!word)
		return NULL;

	if (options) {
		if (g_ascii_strcasecmp (options, "<") == 0 ||
		    g_ascii_strcasecmp (options, ">") == 0)
			cmp = options;
	}

	encoded_word = g_string_new ("");
	camel_sexp_encode_string (encoded_word, word);

	sexp = g_strdup_printf ("(%s (get-size) (cast-int %s))", cmp, encoded_word->str);

	g_string_free (encoded_word, TRUE);

	return sexp;
}

static gchar *
mail_ffe_score (const gchar *word,
		const gchar *options,
		const gchar *hint)
{
	GString *encoded_word;
	gchar *sexp;
	const gchar *cmp = "=";

	if (!word)
		return NULL;

	if (options) {
		if (g_ascii_strcasecmp (options, "<") == 0 ||
		    g_ascii_strcasecmp (options, ">") == 0)
			cmp = options;
	}

	encoded_word = g_string_new ("");
	camel_sexp_encode_string (encoded_word, word);

	sexp = g_strdup_printf ("(%s (cast-int (user-tag \"score\")) (cast-int %s))", cmp, encoded_word->str);

	g_string_free (encoded_word, TRUE);

	return sexp;
}

static gchar *
mail_ffe_body (const gchar *word,
	       const gchar *options,
	       const gchar *hint)
{
	GString *encoded_word;
	gchar *sexp;
	const gchar *cmp = "contains";

	if (!word)
		return NULL;

	if (options) {
		if (g_ascii_strcasecmp (options, "regex") == 0 ||
		    g_ascii_strcasecmp (options, "re") == 0 ||
		    g_ascii_strcasecmp (options, "r") == 0)
			cmp = "regex";
	}

	encoded_word = g_string_new ("");
	camel_sexp_encode_string (encoded_word, word);

	sexp = g_strdup_printf ("(body-%s %s)", cmp, encoded_word->str);

	g_string_free (encoded_word, TRUE);

	return sexp;
}

static gboolean
mail_ffe_decode_date_time (const gchar *word,
			   gint64 *unix_time)
{
	struct tm tm;
	GDateTime *datetime;

	g_return_val_if_fail (word != NULL, FALSE);
	g_return_val_if_fail (unix_time != NULL, FALSE);

	/* YYYY-MM-DD */
	if (strlen (word) == 10 && word[4] == '-' && word[7] == '-') {
		gint yy, mm, dd;

		yy = atoi (word);
		mm = atoi (word + 5);
		dd = atoi (word + 8);

		datetime = g_date_time_new_local (yy, mm, dd, 0, 0, 0);
		if (datetime) {
			*unix_time = g_date_time_to_unix (datetime);
			g_date_time_unref (datetime);
			return TRUE;
		}
	}

	datetime = g_date_time_new_from_iso8601 (word, NULL);
	if (datetime) {
		*unix_time = g_date_time_to_unix (datetime);
		g_date_time_unref (datetime);
		return TRUE;
	}

	memset (&tm, 0, sizeof (tm));
	if (e_time_parse_date_and_time (word, &tm) == E_TIME_PARSE_OK ||
	    e_time_parse_date (word, &tm) == E_TIME_PARSE_OK) {
		datetime = g_date_time_new_local (1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
		*unix_time = g_date_time_to_unix (datetime);
		g_date_time_unref (datetime);
		return TRUE;
	}

	return FALSE;
}

static gchar *
mail_ffe_process_date (const gchar *get_date_fnc,
		       const gchar *word,
		       const gchar *options)
{
	gint64 rel_days;
	gchar *endptr = NULL;
	const gchar *op = ">";
	gint64 unix_time;

	g_return_val_if_fail (get_date_fnc != NULL, NULL);

	if (options) {
		if (g_ascii_strcasecmp (options, "<") == 0) {
			op = "<";
		} else if (g_ascii_strcasecmp (options, "=") == 0) {
			op = "=";
		} else if (g_ascii_strcasecmp (options, ">") == 0) {
			op = ">";
		}
	}

	rel_days = g_ascii_strtoll (word, &endptr, 10);
	if (rel_days != 0 && endptr && !*endptr) {
		return g_strdup_printf ("(%s (compare-date (%s) (%s (get-current-date) %" G_GINT64_FORMAT ")) 0)", op, get_date_fnc,
			rel_days < 0 ? "+" : "-", (rel_days < 0 ? -1 : 1) * rel_days * 24 * 60 * 60);
	}

	if (!mail_ffe_decode_date_time (word, &unix_time))
		return g_strdup_printf ("(%s (compare-date (%s) (get-current-date)) 0)", op, get_date_fnc);

	return g_strdup_printf ("(%s (compare-date (%s) %" G_GINT64_FORMAT ") 0)", op, get_date_fnc, unix_time);
}

static gchar *
mail_ffe_sent (const gchar *word,
	       const gchar *options,
	       const gchar *hint)
{
	if (!word)
		return NULL;

	return mail_ffe_process_date ("get-sent-date", word, options);
}

static gchar *
mail_ffe_received (const gchar *word,
		   const gchar *options,
		   const gchar *hint)
{
	if (!word)
		return NULL;

	return mail_ffe_process_date ("get-received-date", word, options);
}

static gboolean
mail_ffe_is_neg (const gchar *value)
{
	return value &&
	       (g_ascii_strcasecmp (value, "!") == 0 ||
		g_ascii_strcasecmp (value, "0") == 0 ||
		g_ascii_strcasecmp (value, "no") == 0 ||
		g_ascii_strcasecmp (value, "not") == 0 ||
		g_ascii_strcasecmp (value, "false") == 0 ||
		g_ascii_strcasecmp (value, C_("ffe", "no")) == 0 ||
		g_ascii_strcasecmp (value, C_("ffe", "not")) == 0 ||
		g_ascii_strcasecmp (value, C_("ffe", "false")) == 0);
}

static gchar *
mail_ffe_attachment (const gchar *word,
		     const gchar *options,
		     const gchar *hint)
{
	gboolean is_neg;

	if (!word)
		return NULL;

	is_neg = mail_ffe_is_neg (word);

	return g_strdup_printf ("%s(system-flag \"Attachments\")%s", is_neg ? "(not " : "", is_neg ? ")" : "");
}

static gchar *
mail_ffe_location (const gchar *word,
		   const gchar *options,
		   const gchar *hint)
{
	GString *encoded_uri;
	gchar *sexp, *folder_uri;
	gboolean is_neg;

	if (!word)
		return NULL;

	is_neg = mail_ffe_is_neg (options);

	folder_uri = em_utils_account_path_to_folder_uri (NULL, word);

	if (!folder_uri)
		return NULL;

	encoded_uri = g_string_new ("");
	camel_sexp_encode_string (encoded_uri, folder_uri);

	sexp = g_strdup_printf ("%s(message-location %s)%s", is_neg ? "(not " : "", encoded_uri->str, is_neg ? ")" : "");

	g_string_free (encoded_uri, TRUE);
	g_free (folder_uri);

	return sexp;
}

static gchar *
mail_ffe_message_id (const gchar *word,
		     const gchar *options,
		     const gchar *hint)
{
	GString *encoded_mid;
	gchar *sexp;

	if (!word)
		return NULL;

	encoded_mid = g_string_new ("");
	camel_sexp_encode_string (encoded_mid, word);

	sexp = g_strdup_printf ("(header-matches \"MESSAGE-ID\" %s)", encoded_mid->str);

	g_string_free (encoded_mid, TRUE);

	return sexp;
}

static const EFreeFormExpSymbol mail_ffe_symbols[] = {
	{ "",		NULL,	mail_ffe_default },
	{ "from:f",	NULL,	mail_ffe_from },
	{ "to:t",	NULL,	mail_ffe_to },
	{ "cc:c:",	NULL,	mail_ffe_cc },
	{ "recips:r",	NULL,	mail_ffe_recips },
	{ "subject:s",	NULL,	mail_ffe_subject },
	{ "list",	NULL,	mail_ffe_list },
	{ "header:h",	NULL,	mail_ffe_header },
	{ "exists:e",	NULL,	mail_ffe_exists },
	{ "tag",	NULL,	mail_ffe_tag },
	{ "flag",	NULL,	mail_ffe_flag },
	{ "label:l",	NULL,	mail_ffe_label },
	{ "size:sz",	NULL,	mail_ffe_size },
	{ "score:sc",	NULL,	mail_ffe_score },
	{ "body:b",	NULL,	mail_ffe_body },
	{ "sent",	NULL,	mail_ffe_sent },
	{ "received:rcv", NULL,	mail_ffe_received },
	{ "attachment:a", NULL,	mail_ffe_attachment },
	{ "location:m",	NULL,	mail_ffe_location },
	{ "mid",	NULL,	mail_ffe_message_id },
	{ NULL,		NULL,	NULL}
};

static gchar *
get_filter_input_value (EFilterPart *part,
			const gchar *name)
{
	EFilterElement *elem;
	EFilterInput *input;
	GString *value;
	GList *link;

	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	elem = e_filter_part_find_element (part, name);
	g_return_val_if_fail (elem != NULL, NULL);
	g_return_val_if_fail (E_IS_FILTER_INPUT (elem), NULL);

	input = E_FILTER_INPUT (elem);
	value = g_string_new ("");

	for (link = input->values; link; link = g_list_next (link)) {
		const gchar *val = link->data;

		if (val && *val) {
			if (value->len > 0)
				g_string_append_c (value, ' ');
			g_string_append (value, val);
		}
	}

	return g_string_free (value, FALSE);
}

void
e_mail_free_form_exp_to_sexp (EFilterElement *element,
			      GString *out,
			      EFilterPart *part)
{
	gchar *ffe, *sexp;

	ffe = get_filter_input_value (part, "ffe");
	g_return_if_fail (ffe != NULL);

	sexp = e_free_form_exp_to_sexp (ffe, mail_ffe_symbols);
	if (sexp)
		g_string_append (out, sexp);

	g_free (sexp);
	g_free (ffe);
}
