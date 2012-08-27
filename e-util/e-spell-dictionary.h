/*
 * e-spell-dictionary.h
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

#ifndef E_SPELL_DICTIONARY_H
#define E_SPELL_DICTIONARY_H

#include <glib-object.h>
#include <enchant/enchant.h>

/* Standard GObject macros */
#define E_TYPE_SPELL_DICTIONARY \
	(e_spell_dictionary_get_type ())
#define E_SPELL_DICTIONARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SPELL_DICTIONARY, ESpellDictionary))
#define E_SPELL_DICTIONARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SPELL_DICTIONARY, ESpellDictionaryClass))
#define E_IS_SPELL_DICTIONARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SPELL_DICTIONARY))
#define E_IS_SPELL_DICTIONARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SPELL_DICTIONARY))
#define E_SPELL_DICTIONARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SPELL_DICTIONARY, ESpellDictionaryClass))


G_BEGIN_DECLS

typedef struct _ESpellDictionary ESpellDictionary;
typedef struct _ESpellDictionaryPrivate ESpellDictionaryPrivate;
typedef struct _ESpellDictionaryClass ESpellDictionaryClass;
typedef struct _ESpellChecker ESpellChecker;

struct _ESpellDictionary {
	GObject parent;

	ESpellDictionaryPrivate *priv;
};

struct _ESpellDictionaryClass {
	GObjectClass parent_class;

};

GType			e_spell_dictionary_get_type	(void);

ESpellDictionary *	e_spell_dictionary_new		(ESpellChecker *parent_checker,
		 					 EnchantDict *dict);

const gchar *		e_spell_dictionary_get_name	(ESpellDictionary *dict);
const gchar *		e_spell_dictionary_get_code	(ESpellDictionary *dict);


gboolean		e_spell_dictionary_check	(ESpellDictionary *dict,
							 const gchar *word,
							 gsize len);

void			e_spell_dictionary_learn_word	(ESpellDictionary *dict,
							 const gchar *word,
							 gsize len);
void			e_spell_dictionary_ignore_word	(ESpellDictionary *dict,
							 const gchar *word,
							 gsize len);
GList *			e_spell_dictionary_get_suggestions
							(ESpellDictionary *dict,
							 const gchar *word,
							 gsize len);
void			e_spell_dictionary_free_suggestions
							(GList *suggestions);
void			e_spell_dictionary_store_correction
							(ESpellDictionary *dict,
							 const gchar *misspelled,
							 gsize misspelled_len,
							 const gchar *correction,
							 gsize correction_len);

ESpellChecker *		e_spell_dictionary_get_parent_checker
							(ESpellDictionary *dict);

gint			e_spell_dictionary_compare	(ESpellDictionary *dict1,
							 ESpellDictionary *dict2);

G_END_DECLS


#endif /* E_SPELL_DICTIONARY_H */