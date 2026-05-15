/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_SETTINGS_SPELL_CHECKER_H
#define E_SETTINGS_SPELL_CHECKER_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_SPELL_CHECKER \
	(e_settings_spell_checker_get_type ())
#define E_SETTINGS_SPELL_CHECKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_SPELL_CHECKER, ESettingsSpellChecker))
#define E_SETTINGS_SPELL_CHECKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_SPELL_CHECKER, ESettingsSpellCheckerClass))
#define E_IS_SETTINGS_SPELL_CHECKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_SPELL_CHECKER))
#define E_IS_SETTINGS_SPELL_CHECKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_SPELL_CHECKER))
#define E_SETTINGS_SPELL_CHECKER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_SPELL_CHECKER, ESettingsSpellCheckerClass))

G_BEGIN_DECLS

typedef struct _ESettingsSpellChecker ESettingsSpellChecker;
typedef struct _ESettingsSpellCheckerClass ESettingsSpellCheckerClass;
typedef struct _ESettingsSpellCheckerPrivate ESettingsSpellCheckerPrivate;

struct _ESettingsSpellChecker {
	EExtension parent;
	ESettingsSpellCheckerPrivate *priv;
};

struct _ESettingsSpellCheckerClass {
	EExtensionClass parent_class;
};

GType		e_settings_spell_checker_get_type
						(void) G_GNUC_CONST;
void		e_settings_spell_checker_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_SPELL_CHECKER_H */
