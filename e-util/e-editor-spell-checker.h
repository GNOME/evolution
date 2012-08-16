/*
 * e-editor-spell-checker.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EDITOR_SPELL_CHECKER_H
#define E_EDITOR_SPELL_CHECKER_H

#include <glib-object.h>
#include <enchant/enchant.h>

#define E_TYPE_EDITOR_SPELL_CHECKER \
	(e_editor_spell_checker_get_type ())
#define E_EDITOR_SPELL_CHECKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_SPELL_CHECKER, EEditorSpellChecker))
#define E_EDITOR_SPELL_CHECKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_SPELL_CHECKER, EEditorSpellCheckerClass))
#define E_IS_EDITOR_SPELL_CHECKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_SPELL_CHECKER))
#define E_IS_EDITOR_SPELL_CHECKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_SPELL_CHECKER))
#define E_EDITOR_SPELL_CHECKER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_SPELL_CHECKER, EEditorSpellCheckerClass))

G_BEGIN_DECLS

typedef struct _EEditorSpellChecker EEditorSpellChecker;
typedef struct _EEditorSpellCheckerClass EEditorSpellCheckerClass;
typedef struct _EEditorSpellCheckerPrivate EEditorSpellCheckerPrivate;

struct _EEditorSpellChecker {
	GObject parent;

	EEditorSpellCheckerPrivate *priv;
};

struct _EEditorSpellCheckerClass {
	GObjectClass parent_class;
};

GType		e_editor_spell_checker_get_type		(void);


void		e_editor_spell_checker_set_dictionaries	(EEditorSpellChecker *checker,
							 GList *dictionaries);

EnchantDict *	e_editor_spell_checker_lookup_dict	(EEditorSpellChecker *checker,
							 const gchar *language_code);
void		e_editor_spell_checker_free_dict	(EEditorSpellChecker *checker,
							 EnchantDict *dict);

gint		e_editor_spell_checker_dict_compare	(const EnchantDict *dict_a,
							 const EnchantDict *dict_b);

GList *		e_editor_spell_checker_get_available_dicts
							(EEditorSpellChecker *checker);

const gchar *	e_editor_spell_checker_get_dict_name	(const EnchantDict *dictionary);
const gchar *	e_editor_spell_checker_get_dict_code	(const EnchantDict *dictionary);

G_END_DECLS

#endif /* E_EDITOR_SPELL_CHECKER_H */
