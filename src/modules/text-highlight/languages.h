/*
 * languages.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
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
