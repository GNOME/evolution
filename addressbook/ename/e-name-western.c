/*
 * A simple Western name parser.
 *
 * <Nat> Jamie, do you know anything about name parsing?
 * <jwz> Are you going down that rat hole?  Bring a flashlight.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, Helix Code, Inc.
 */

#include <ctype.h>
#include <string.h>
#include <glib.h>
 
#include <ename/e-name-western.h>
#include <ename/e-name-western-tables.h>

typedef struct {
	int prefix_idx;
	int first_idx;
	int middle_idx;
	int nick_idx;
	int last_idx;
	int suffix_idx;
} ENameWesternIdxs;

static int
e_name_western_str_count_words (char *str)
{
	int word_count;
	char *p;

	word_count = 0;

	for (p = str; p != NULL; p = strchr (p, ' ')) {
		word_count ++;
		p ++;
	}

	return word_count;
}

static void
e_name_western_cleanup_string (char **str)
{
	char *newstr;
	char *p;

	if (*str == NULL)
		return;

	/* skip any spaces and commas at the start of the string */
	p = *str;
	while (isspace (*p) || *p == ',')
		p ++;

	/* make the copy we're going to return */
	newstr = g_strdup (p);

	if ( strlen(newstr) > 0) {
		/* now search from the back, skipping over any spaces and commas */
		p = newstr + strlen (newstr) - 1;
		while (isspace (*p) || *p == ',')
			p --;
		/* advance p to after the character that caused us to exit the
		   previous loop, and end the string. */
		if ((! isspace (*p)) && *p != ',')
			p ++;
		*p = '\0';
	}

	g_free (*str);
	*str = newstr;
}

static char *
e_name_western_get_words_at_idx (char *str, int idx, int num_words)
{
	char *words;
	char *p;
	int   word_count;
	int   words_len;

	/*
	 * Walk to the end of the words.
	 */
	word_count = 0;
	p = str + idx;
	while (word_count < num_words && *p != '\0') {
		while (! isspace (*p) && *p != '\0')
			p ++;

		while (isspace (*p) && *p != '\0')
			p ++;

		word_count ++;
	}

	words_len = p - str - idx - 1;

	if (*p == '\0')
		words_len ++;

	words = g_malloc0 (1 + words_len);
	strncpy (words, str + idx, words_len);

	return words;
}

/*
 * What the fuck is wrong with glib's MAX macro.
 */ 
static int
e_name_western_max (const int a, const int b)
{
	if (a > b)
		return a;

	return b;
}

static gboolean
e_name_western_word_is_suffix (char *word)
{
	int i;

	for (i = 0; e_name_western_sfx_table [i] != NULL; i ++) {
		if (g_strcasecmp (word, e_name_western_sfx_table [i]))
			continue;

		return TRUE;
	}

	return FALSE;
}

static char *
e_name_western_get_one_prefix_at_str (char *str)
{
	char *word;
	int   i;

	/*
	 * Check for prefixes from our table.
	 */
	for (i = 0; e_name_western_pfx_table [i] != NULL; i ++) {
		int pfx_words;
		char *words;

		pfx_words = e_name_western_str_count_words (e_name_western_pfx_table [i]);
		words = e_name_western_get_words_at_idx (str, 0, pfx_words);

		if (! g_strcasecmp (words, e_name_western_pfx_table [i]))
			return words;

		g_free (words);
	}

	/*
	 * Check for prefixes we don't know about.  These are always a
	 * sequence of more than one letters followed by a period.
	 */
	word = e_name_western_get_words_at_idx (str, 0, 1);

	if (strlen (word) > 2 && isalpha (word [0]) && isalpha (word [1]) &&
	    word [strlen (word) - 1] == '.')
		return word;

	g_free (word);

	return NULL;
}

static char *
e_name_western_get_prefix_at_str (char *str)
{
	char *pfx;
	char *pfx1;
	char *pfx2;
	char *p;

	/* Get the first prefix. */
	pfx1 = e_name_western_get_one_prefix_at_str (str);

	if (pfx1 == NULL)
		return NULL;

	/* Check for a second prefix. */
	p = str + strlen (pfx1);
	while (isspace (*p) && *p != '\0')
		p ++;

	pfx2 = e_name_western_get_one_prefix_at_str (p);

	if (pfx2 != NULL) {
		int pfx_len;

		pfx_len = (p + strlen (pfx2)) - str;
		pfx = g_malloc0 (pfx_len + 1);
		strncpy (pfx, str, pfx_len);
	} else {
		pfx = g_strdup (pfx1);
	}

	g_free (pfx1);
	g_free (pfx2);

	return pfx;
}

static void
e_name_western_extract_prefix (ENameWestern *name, ENameWesternIdxs *idxs)
{
	char *pfx;

	pfx = e_name_western_get_prefix_at_str (name->full);

	if (pfx == NULL)
		return;

	idxs->prefix_idx = 0;
	name->prefix     = pfx;
}

static gboolean
e_name_western_is_complex_last_beginning (char *word)
{
	int i;

	for (i = 0; e_name_western_complex_last_table [i] != NULL; i ++) {

		if (! g_strcasecmp (
			word, e_name_western_complex_last_table [i]))
			return TRUE;
	}

	return FALSE;
}

static void
e_name_western_extract_first (ENameWestern *name, ENameWesternIdxs *idxs)
{
	/*
	 * If there's a prefix, then the first name is right after it.
	 */
	if (idxs->prefix_idx != -1) {
		int   first_idx;
		char *p;

		first_idx = idxs->prefix_idx + strlen (name->prefix);

		/* Skip past white space. */
		p = name->full + first_idx;
		while (isspace (*p) && *p != '\0')
			p++;

		if (*p == '\0')
			return;

		idxs->first_idx = p - name->full;
		name->first = e_name_western_get_words_at_idx (
			name->full, idxs->first_idx, 1);

	} else {

		/*
		 * Otherwise, the first name is probably the first string.
		 */
		idxs->first_idx = 0;
		name->first = e_name_western_get_words_at_idx (
			name->full, idxs->first_idx, 1);
	}

	/*
	 * Check that we didn't just assign the beginning of a
	 * compound last name to the first name.
	 */
	if (name->first != NULL) {
		if (e_name_western_is_complex_last_beginning (name->first)) {
			g_free (name->first);
			name->first = NULL;
			idxs->first_idx = -1;
		}
	}
}

static void
e_name_western_extract_middle (ENameWestern *name, ENameWesternIdxs *idxs)
{
	char *word;
	int   middle_idx;

	/*
	 * Middle names can only exist if you have a first name.
	 */
	if (idxs->first_idx == -1)
		return;

	middle_idx = idxs->first_idx + strlen (name->first) + 1;

	if (middle_idx > strlen (name->full))
		return;
	
	/*
	 * Search for the first space (or the terminating \0)
	 */
	while (isspace (name->full [middle_idx]) &&
	       name->full [middle_idx] != '\0')
		middle_idx ++;
		
	if (name->full [middle_idx] == '\0')
		return;

	/*
	 * Skip past the nickname, if it's there.
	 */
	if (name->full [middle_idx] == '\"') {
		if (idxs->nick_idx == -1)
			return;

		middle_idx = idxs->nick_idx + strlen (name->nick) + 1;
		
		while (isspace (name->full [middle_idx]) &&
		       name->full [middle_idx] != '\0')
			middle_idx ++;

		if (name->full [middle_idx] == '\0')
			return;
	}

	/*
	 * Make sure this isn't the beginning of a complex last name.
	 */
	word = e_name_western_get_words_at_idx (name->full, middle_idx, 1);
	if (e_name_western_is_complex_last_beginning (word)) {
		g_free (word);
		return;
	}

	/*
	 * Make sure this isn't a suffix.
	 */
	e_name_western_cleanup_string (& word);
	if (e_name_western_word_is_suffix (word)) {
		g_free (word);
		return;
	}

	/*
	 * Make sure we didn't just grab a cute nickname.
	 */
	if (word [0] == '\"') {
		g_free (word);
		return;
	}
	
	idxs->middle_idx = middle_idx;
	name->middle = word;
}

static void
e_name_western_extract_nickname (ENameWestern *name, ENameWesternIdxs *idxs)
{
	int   idx;
	int   start_idx;
	char *str;

	if (idxs->first_idx == -1)
		return;

	if (idxs->middle_idx > idxs->first_idx)
		idx = idxs->middle_idx + strlen (name->middle);
	else
		idx = idxs->first_idx + strlen (name->first);

	while (name->full [idx] != '\"' && name->full [idx] != '\0')
		idx ++;

	if (name->full [idx] != '\"')
		return;

	start_idx = idx;

	/*
	 * Advance to the next double quote.
	 */
	idx ++;
	
	while (name->full [idx] != '\"' && name->full [idx] != '\0')
		idx ++;

	if (name->full [idx] == '\0')
		return;

	str = g_malloc0 (idx - start_idx + 2);
	strncpy (str, name->full + start_idx, idx - start_idx + 1);

	name->nick = str;
	idxs->nick_idx = start_idx;
}

static int
e_name_western_last_get_max_idx (ENameWestern *name, ENameWesternIdxs *idxs)
{
	int max_idx = -1;

	if (name->prefix != NULL)
		max_idx = e_name_western_max (
			max_idx, idxs->prefix_idx + strlen (name->prefix));

	if (name->first != NULL)
		max_idx = e_name_western_max (
			max_idx, idxs->first_idx + strlen (name->first));

	if (name->middle != NULL)
		max_idx = e_name_western_max (
			max_idx, idxs->middle_idx + strlen (name->middle));

	if (name->nick != NULL)
		max_idx = e_name_western_max (
			max_idx, idxs->nick_idx + strlen (name->nick));

	return max_idx;
}

static void
e_name_western_extract_last (ENameWestern *name, ENameWesternIdxs *idxs)
{
	char *word;
	int   idx = -1;

	idx = e_name_western_last_get_max_idx (name, idxs);

	/*
	 * In the case where there is no preceding name element, the
	 * name is either just a first name ("Nat", "John"), is a
	 * single-element name ("Cher", which we treat as a first
	 * name), or is just a last name.  The only time we can
	 * differentiate a last name alone from a single-element name
	 * or a first name alone is if it's a complex last name ("de
	 * Icaza", "van Josephsen").  So if there is no preceding name
	 * element, we check to see whether or not the first part of
	 * the name is the beginning of a complex name.  If it is,
	 * we subsume the entire string.  If we accidentally subsume
	 * the suffix, this will get fixed in the fixup routine.
	 */
	if (idx == -1) {
		word = e_name_western_get_words_at_idx (name->full, 0, 1);
		if (! e_name_western_is_complex_last_beginning (word)) {
			g_free (word);
			return;
		}

		name->last     = g_strdup (name->full);
		idxs->last_idx = 0;
		return;
	}

	/* Skip past the white space. */
	while (isspace (name->full [idx]) && name->full [idx] != '\0')
		idx ++;

	if (name->full [idx] == '\0')
		return;

	word = e_name_western_get_words_at_idx (name->full, idx, 1);
	e_name_western_cleanup_string (& word);
	if (e_name_western_word_is_suffix (word)) {
		g_free (word);
		return;
	}
	g_free (word);

	/*
	 * Subsume the rest of the string into the last name.  If we
	 * accidentally include the prefix, it will get fixed later.
	 * This is the only way to handle things like "Miguel de Icaza
	 * Amozorrutia" without dropping data and forcing the user
	 * to retype it.
	 */
	name->last = g_strdup (name->full + idx);
	idxs->last_idx = idx;
}

static char *
e_name_western_get_preceding_word (char *str, int idx)
{
	int   word_len;
	char *word;
	char *p;

	p = str + idx;

	while (isspace (*p) && p > str)
		p --;

	while (! isspace (*p) && p > str)
		p --;

	if (isspace (*p))
	    p ++;

	word_len = (str + idx) - p;
	word = g_malloc0 (word_len + 1);
	if (word_len > 0)
		strncpy (word, p, word_len);

	return word;
}

static char *
e_name_western_get_suffix_at_str_end (char *str)
{
	char *suffix;
	char *p;

	/*
	 * Walk backwards till we reach the beginning of the
	 * (potentially-comma-separated) list of suffixes.
	 */
	p = str + strlen (str);
	while (1) {
		char *nextp;
		char *word;

		word = e_name_western_get_preceding_word (str, p - str);
		nextp = p - strlen (word) - 1;
		
		e_name_western_cleanup_string (& word);

		if (e_name_western_word_is_suffix (word)) {
			p = nextp;
			g_free (word);
		} else {
			g_free (word);
			break;
		}
	}

	if (p == (str + strlen (str)))
		return NULL;

	suffix = g_strdup (p);
	e_name_western_cleanup_string (& suffix);

	if (strlen (suffix) == 0) {
		g_free (suffix);
		return NULL;
	}

	return suffix;
}

static void
e_name_western_extract_suffix (ENameWestern *name, ENameWesternIdxs *idxs)
{

	name->suffix = e_name_western_get_suffix_at_str_end (name->full);

	if (name->suffix == NULL)
		return;

	idxs->suffix_idx = strlen (name->full) - strlen (name->suffix);
}

static gboolean
e_name_western_detect_backwards (ENameWestern *name, ENameWesternIdxs *idxs)
{
	char *comma;
	char *word;

	comma = strchr (name->full, ',');

	if (comma == NULL)
		return FALSE;

	/*
	 * If there's a comma, we need to detect whether it's
	 * separating the last name from the first or just separating
	 * suffixes.  So we grab the word which comes before the
	 * comma and check if it's a suffix.
	 */
	word = e_name_western_get_preceding_word (name->full, comma - name->full);

	if (e_name_western_word_is_suffix (word)) {
		g_free (word);
		return FALSE;
	}

	g_free (word);
	return TRUE;
}

static void
e_name_western_reorder_asshole (ENameWestern *name, ENameWesternIdxs *idxs)
{
	char *prefix;
	char *last;
	char *suffix;
	char *firstmidnick;
	char *newfull;

	char *comma;
	char *p;

	if (! e_name_western_detect_backwards (name, idxs))
		return;

	/*
	 * Convert
	 *    <Prefix> <Last name>, <First name> <Middle[+nick] name> <Suffix>
	 * to
	 *    <Prefix> <First name> <Middle[+nick] name> <Last name> <Suffix>
	 */
	
	/*
	 * Grab the prefix from the beginning.
	 */
	prefix = e_name_western_get_prefix_at_str (name->full);

	/*
	 * Everything from the end of the prefix to the comma is the
	 * last name.
	 */
	comma = strchr (name->full, ',');
	if (comma == NULL)
		return;

	p = name->full + (prefix == NULL ? 0 : strlen (prefix));

	while (isspace (*p) && *p != '\0')
		p ++;

	last = g_malloc0 (comma - p + 1);
	strncpy (last, p, comma - p);

	/*
	 * Get the suffix off the end.
	 */
	suffix = e_name_western_get_suffix_at_str_end (name->full);

	/*
	 * Firstmidnick is everything from the comma to the beginning
	 * of the suffix.
	 */
	p = comma + 1;

	while (isspace (*p) && *p != '\0')
		p ++;

	if (suffix != NULL) {
		char *q;

		/*
		 * Point q at the beginning of the suffix.
		 */
		q = name->full + strlen (name->full) - strlen (suffix) - 1;

		/*
		 * Walk backwards until we hit the space which
		 * separates the suffix from firstmidnick.
		 */
		while (! isspace (*q) && q > comma)
			q --;

		if ((q - p + 1) > 0) {
			firstmidnick = g_malloc0 (q - p + 1);
			strncpy (firstmidnick, p, q - p);
		} else
			firstmidnick = NULL;
	} else {
		firstmidnick = g_strdup (p);
	}

	/*
	 * Create our new reordered version of the name.
	 */
#define NULLSTR(a) ((a) == NULL ? "" : (a))
	newfull = g_strdup_printf ("%s %s %s %s", NULLSTR (prefix), NULLSTR (firstmidnick),
				   NULLSTR (last), NULLSTR (suffix));
	g_strstrip (newfull);
	g_free (name->full);
	name->full = newfull;


	g_free (prefix);
	g_free (firstmidnick);
	g_free (last);
	g_free (suffix);
}

static void
e_name_western_zap_nil (char **str, int *idx)
{
	if (*str == NULL)
		return;

	if (strlen (*str) != 0)
		return;

	*idx = -1;
	g_free (*str);
	*str = NULL;
}

static void
e_name_western_fixup (ENameWestern *name, ENameWesternIdxs *idxs)
{
	/*
	 * The middle and last names cannot be the same.
	 */
	if (idxs->middle_idx != -1 && idxs->middle_idx == idxs->last_idx) {
		idxs->middle_idx = -1;
		g_free (name->middle);
		name->middle = NULL;
	}

	/*
	 * If we have a middle name and no last name, then we mistook
	 * the last name for the middle name.
	 */
	if (idxs->last_idx == -1 && idxs->middle_idx != -1) {
		idxs->last_idx   = idxs->middle_idx;
		name->last       = name->middle;
		name->middle     = NULL;
		idxs->middle_idx = -1;
	}

	/*
	 * Check to see if we accidentally included the suffix in the
	 * last name.
	 */
	if (idxs->suffix_idx != -1 && idxs->last_idx != -1 &&
	    idxs->suffix_idx < (idxs->last_idx + strlen (name->last))) {
		char *sfx;

		sfx = name->last + (idxs->suffix_idx - idxs->last_idx);
		if (sfx != NULL) {
			char *newlast;
			char *p;

			p = sfx - 1;
			while (isspace (*p) && p > name->last)
				p --;
			p ++;

			newlast = g_malloc0 (p - name->last + 1);
			strncpy (newlast, name->last, p - name->last);
			g_free (name->last);
			name->last = newlast;
		}
	}

	/*
	 * If we have a prefix and a first name, but no last name,
	 * then we need to assign the first name to the last name.
	 * This way we get things like "Mr Friedman" correctly.
	 */
	if (idxs->first_idx != -1 && idxs->prefix_idx != -1 &&
	    idxs->last_idx == -1) {
		name->last      = name->first;
		idxs->last_idx  = idxs->first_idx;
		idxs->first_idx = -1;
		name->first     = NULL;
	}

	/*
	 * Remove stray spaces and commas (although there don't seem
	 * to be any in the test cases, they might show up later).
	 */
	e_name_western_cleanup_string (& name->prefix);
	e_name_western_cleanup_string (& name->first);
	e_name_western_cleanup_string (& name->middle);
	e_name_western_cleanup_string (& name->nick);
	e_name_western_cleanup_string (& name->last);
	e_name_western_cleanup_string (& name->suffix);

	/*
	 * Make zero-length strings just NULL.
	 */
	e_name_western_zap_nil (& name->prefix, & idxs->prefix_idx);
	e_name_western_zap_nil (& name->first,  & idxs->first_idx);
	e_name_western_zap_nil (& name->middle, & idxs->middle_idx);
	e_name_western_zap_nil (& name->nick,   & idxs->nick_idx);
	e_name_western_zap_nil (& name->last,   & idxs->last_idx);
	e_name_western_zap_nil (& name->suffix, & idxs->suffix_idx);
}

/**
 * e_name_western_western_parse_fullname:
 * @full_name: A string containing a Western name.
 *
 * Parses @full_name and returns an #ENameWestern object filled with
 * the component parts of the name.
 */
ENameWestern *
e_name_western_parse (const char *full_name)
{
	ENameWesternIdxs *idxs;
	ENameWestern *wname;

	wname = g_new0 (ENameWestern, 1);

	wname->full = g_strdup (full_name);

	idxs = g_new0 (ENameWesternIdxs, 1);

	idxs->prefix_idx = -1;
	idxs->first_idx  = -1;
	idxs->middle_idx = -1;
	idxs->nick_idx   = -1;
	idxs->last_idx   = -1;
	idxs->suffix_idx = -1;
	
	/*
	 * An extremely simple algorithm.
	 *
	 * The goal here is to get it right 95% of the time for
	 * Western names.
	 *
	 * First we check to see if this is an ass-backwards name
	 * ("Prefix Last, First Middle Suffix").  These names really
	 * suck (imagine "Dr von Johnson, Albert Roderick Jr"), so
	 * we reorder them first and then parse them.
	 *
	 * Next, we grab the most obvious assignments for the various
	 * parts of the name.  Once this is done, we check for stupid
	 * errors and fix them up.
	 */
	e_name_western_reorder_asshole  (wname, idxs);

	e_name_western_extract_prefix   (wname, idxs);
	e_name_western_extract_first    (wname, idxs);
	e_name_western_extract_nickname (wname, idxs);
	e_name_western_extract_middle   (wname, idxs);
	e_name_western_extract_last     (wname, idxs);
	e_name_western_extract_suffix   (wname, idxs);

	e_name_western_fixup            (wname, idxs);

	g_free (idxs);

	return wname;
}

/**
 * e_name_western_free:
 * @name: An ENameWestern object which needs to be freed.
 *
 * Deep-frees @name
 */
void
e_name_western_free (ENameWestern *w)
{

	g_free (w->prefix);
	g_free (w->first);
	g_free (w->middle);
	g_free (w->nick);
	g_free (w->last);
	g_free (w->suffix);
	
	g_free (w->full);

	g_free (w);
}
