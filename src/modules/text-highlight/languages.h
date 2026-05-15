/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef LANGUAGES_H
#define LANGUAGES_H

#include <glib.h>

typedef struct Language {
	const gchar *action_name;
	const gchar *action_label;
	const gchar **extensions;
	const gchar **mime_types;
} Language;

const gchar *	get_syntax_for_ext		(const gchar *extension);
const gchar *	get_syntax_for_mime_type	(const gchar *mime_type);

Language *	get_default_langauges		(gsize *len);
Language *	get_additinal_languages		(gsize *len);

const gchar **	get_mime_types			(void);

#endif /* LANGUAGES_H */
