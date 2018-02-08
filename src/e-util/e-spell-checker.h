/*
 * e-spell-checker.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SPELL_CHECKER_H
#define E_SPELL_CHECKER_H

#include <glib-object.h>
#include <e-util/e-spell-dictionary.h>

/* Standard GObject macros */
#define E_TYPE_SPELL_CHECKER \
	(e_spell_checker_get_type ())
#define E_SPELL_CHECKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SPELL_CHECKER, ESpellChecker))
#define E_SPELL_CHECKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SPELL_CHECKER, ESpellCheckerClass))
#define E_IS_SPELL_CHECKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SPELL_CHECKER))
#define E_IS_SPELL_CHECKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SPELL_CHECKER))
#define E_SPELL_CHECKER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SPELL_CHECKER, ESpellCheckerClass))

G_BEGIN_DECLS

typedef struct _ESpellChecker ESpellChecker;
typedef struct _ESpellCheckerPrivate ESpellCheckerPrivate;
typedef struct _ESpellCheckerClass ESpellCheckerClass;

struct _ESpellChecker {
	GObject parent;
	ESpellCheckerPrivate *priv;
};

struct _ESpellCheckerClass {
	GObjectClass parent_class;
};

GType		e_spell_checker_get_type	(void) G_GNUC_CONST;
void		e_spell_checker_free_global_memory
						(void);
ESpellChecker *	e_spell_checker_new		(void);
GList *		e_spell_checker_list_available_dicts
						(ESpellChecker *checker);
guint		e_spell_checker_count_available_dicts
						(ESpellChecker *checker);
ESpellDictionary *
		e_spell_checker_ref_dictionary	(ESpellChecker *checker,
						 const gchar *language_code);
gpointer /* EnchantDict * */
		e_spell_checker_get_enchant_dict
						(ESpellChecker *checker,
						 const gchar *language_code);
gboolean	e_spell_checker_get_language_active
						(ESpellChecker *checker,
						 const gchar *language_code);
void		e_spell_checker_set_language_active
						(ESpellChecker *checker,
						 const gchar *language_code,
						 gboolean active);
void		e_spell_checker_set_active_languages
						(ESpellChecker *checker,
						 const gchar * const *languages);
gchar **	e_spell_checker_list_active_languages
						(ESpellChecker *checker,
						 guint *n_languages);
guint		e_spell_checker_count_active_languages
						(ESpellChecker *checker);
gboolean	e_spell_checker_check_word	(ESpellChecker *checker,
						 const gchar *word,
						 gsize length);
void		e_spell_checker_learn_word	(ESpellChecker *checker,
						 const gchar *word);
void		e_spell_checker_ignore_word	(ESpellChecker *checker,
						 const gchar *word);
gchar **	e_spell_checker_get_guesses_for_word
						(ESpellChecker *checker,
						 const gchar *word);

G_END_DECLS

#endif /* E_SPELL_CHECKER_H */
