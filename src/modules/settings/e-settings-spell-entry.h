/*
 * e-settings-spell-entry.h
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

#ifndef E_SETTINGS_SPELL_ENTRY_H
#define E_SETTINGS_SPELL_ENTRY_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_SPELL_ENTRY \
	(e_settings_spell_entry_get_type ())
#define E_SETTINGS_SPELL_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_SPELL_ENTRY, ESettingsSpellEntry))
#define E_SETTINGS_SPELL_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_SPELL_ENTRY, ESettingsSpellEntryClass))
#define E_IS_SETTINGS_SPELL_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_SPELL_ENTRY))
#define E_IS_SETTINGS_SPELL_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_SPELL_ENTRY))
#define E_SETTINGS_SPELL_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_SPELL_ENTRY, ESettingsSpellEntryClass))

G_BEGIN_DECLS

typedef struct _ESettingsSpellEntry ESettingsSpellEntry;
typedef struct _ESettingsSpellEntryClass ESettingsSpellEntryClass;
typedef struct _ESettingsSpellEntryPrivate ESettingsSpellEntryPrivate;

struct _ESettingsSpellEntry {
	EExtension parent;
	ESettingsSpellEntryPrivate *priv;
};

struct _ESettingsSpellEntryClass {
	EExtensionClass parent_class;
};

GType		e_settings_spell_entry_get_type	(void) G_GNUC_CONST;
void		e_settings_spell_entry_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_SPELL_ENTRY_H */

