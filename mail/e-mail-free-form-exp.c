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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <string.h>

#include <camel/camel.h>
#include <e-util/e-util.h>
#include <libedataserver/libedataserver.h>

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
		if (!sexp)
			sexp = g_string_new ("");
	} else if (!sexp) {
		sexp = g_string_new ("(or ");
	} else {
		g_string_append (sexp, "(or ");
	}

	for (ii = 0; header_names[ii]; ii++) {
		g_string_append_printf (sexp, "(match-all (header-%s \"%s\" %s))", compare_type, header_names[ii], encoded_word->str);
	}

	if (header_names[1])
		g_string_append (sexp, ")");

	g_string_free (encoded_word, TRUE);

	return sexp ? g_string_free (sexp, FALSE) : NULL;
}

static gchar *
mail_ffe_recips (const gchar *word,
		 const gchar *options,
		 const gchar *hint)
{
	const gchar *header_names[] = { "To", "Cc", "Subject", NULL };

	/* Include Subject only in the default expression. */
	if (!hint)
		header_names[2] = NULL;

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

	sexp = g_strdup_printf ("(match-all (header-exists %s))", encoded_word->str);

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

	sexp = g_strdup_printf ("(match-all (not (= (user-tag %s) \"\")))", encoded_word->str);

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
		if (g_ascii_strcasecmp (word, system_flags[ii]) == 0 ||
		    g_ascii_strcasecmp (word, g_dpgettext2 (NULL, "ffe", system_flags[ii])) == 0) {
			sexp = g_strdup_printf ("(match-all (system-flag \"%s\"))", system_flags[ii]);
			break;
		}
	}

	if (!sexp)
		sexp = g_strdup_printf ("(match-all (not (= (user-tag %s) \"\")))", encoded_word->str);

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

	sexp = g_strdup_printf ("(match-all (or ((= (user-tag \"label\") %s) (user-flag (+ \"$Label\" %s)) (user-flag  %s)))",
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

	sexp = g_strdup_printf ("(match-all (%s (get-size) (cast-int %s)))", cmp, encoded_word->str);

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

	sexp = g_strdup_printf ("(match-all (%s (cast-int (user-tag \"score\")) (cast-int %s)))", cmp, encoded_word->str);

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

	sexp = g_strdup_printf ("(match-all (body-%s %s))", cmp, encoded_word->str);

	g_string_free (encoded_word, TRUE);

	return sexp;
}

static gboolean
mail_ffe_decode_date_time (const gchar *word,
			   GTimeVal *tv)
{
	struct tm tm;

	g_return_val_if_fail (word != NULL, FALSE);
	g_return_val_if_fail (tv != NULL, FALSE);

	/* YYYY-MM-DD */
	if (strlen (word) == 10 && word[4] == '-' && word[7] == '-') {
		gint yy, mm, dd;

		yy = atoi (word);
		mm = atoi (word + 5);
		dd = atoi (word + 8);

		if (g_date_valid_dmy (dd, mm, yy)) {
			GDate *date;

			date = g_date_new_dmy (dd, mm, yy);
			g_date_to_struct_tm (date, &tm);
			g_date_free (date);

			tv->tv_sec = mktime (&tm);
			tv->tv_usec = 0;

			return TRUE;
		}
	}

	if (g_time_val_from_iso8601 (word, tv))
		return TRUE;

	if (e_time_parse_date_and_time (word, &tm) == E_TIME_PARSE_OK ||
	    e_time_parse_date (word, &tm) == E_TIME_PARSE_OK) {
		tv->tv_sec = mktime (&tm);
		tv->tv_usec = 0;

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
	GTimeVal tv;

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
		return g_strdup_printf ("(match-all (%s (%s) (%s (get-current-date) %" G_GINT64_FORMAT ")))", op, get_date_fnc,
			rel_days < 0 ? "+" : "-", (rel_days < 0 ? -1 : 1) * rel_days * 24 * 60 * 60);
	}

	if (!mail_ffe_decode_date_time (word, &tv))
		return g_strdup_printf ("(match-all (%s (%s) (get-current-date)))", op, get_date_fnc);

	return g_strdup_printf ("(match-all (%s (%s) %" G_GINT64_FORMAT "))", op, get_date_fnc, (gint64) tv.tv_sec);
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

static gchar *
mail_ffe_attachment (const gchar *word,
		     const gchar *options,
		     const gchar *hint)
{
	gboolean is_neg = FALSE;

	if (!word)
		return NULL;

	if (g_ascii_strcasecmp (word, "no") == 0 ||
	    g_ascii_strcasecmp (word, "false") == 0 ||
	    g_ascii_strcasecmp (word, C_("ffe", "no")) == 0 ||
	    g_ascii_strcasecmp (word, C_("ffe", "false")) == 0 ||
	    g_ascii_strcasecmp (word, "0") == 0) {
		is_neg = TRUE;
	}

	return g_strdup_printf ("(match-all %s(system-flag \"Attachment\")%s)", is_neg ? "(not " : "", is_neg ? ")" : "");
}

static const EFreeFormExpSymbol mail_ffe_symbols[] = {
	{ "",		"1",	mail_ffe_recips },
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
