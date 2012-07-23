/* e-spell-dictionary.h
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

/* Based on Marco Barisione's GSpellLanguage. */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SPELL_DICTIONARY_H
#define E_SPELL_DICTIONARY_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ESpellDictionary ESpellDictionary;

#define E_TYPE_SPELL_DICTIONARY \
	(e_spell_dictionary_get_type ())


GType		e_spell_dictionary_get_type		(void);
const GList *	e_spell_dictionary_get_available	(void);

const ESpellDictionary *
		e_spell_dictionary_lookup
					(const gchar *language_code);
const gchar *	e_spell_dictionary_get_language_code
					(const ESpellDictionary *dictionary);
const gchar *	e_spell_dictionary_get_name
					(const ESpellDictionary *dictionary);
gint		e_spell_dictionary_compare
					(const ESpellDictionary *dict_a,
					 const ESpellDictionary *dict_b);

G_END_DECLS

#endif /* E_SPELL_DICTIONARY_H */
