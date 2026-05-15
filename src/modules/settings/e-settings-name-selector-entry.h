/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_SETTINGS_NAME_SELECTOR_ENTRY_H
#define E_SETTINGS_NAME_SELECTOR_ENTRY_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_NAME_SELECTOR_ENTRY \
	(e_settings_name_selector_entry_get_type ())
#define E_SETTINGS_NAME_SELECTOR_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_NAME_SELECTOR_ENTRY, ESettingsNameSelectorEntry))
#define E_SETTINGS_NAME_SELECTOR_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_NAME_SELECTOR_ENTRY, ESettingsNameSelectorEntryClass))
#define E_IS_SETTINGS_NAME_SELECTOR_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_NAME_SELECTOR_ENTRY))
#define E_IS_SETTINGS_NAME_SELECTOR_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_NAME_SELECTOR_ENTRY))
#define E_SETTINGS_NAME_SELECTOR_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_NAME_SELECTOR_ENTRY, ESettingsNameSelectorEntryClass))

G_BEGIN_DECLS

typedef struct _ESettingsNameSelectorEntry ESettingsNameSelectorEntry;
typedef struct _ESettingsNameSelectorEntryClass ESettingsNameSelectorEntryClass;
typedef struct _ESettingsNameSelectorEntryPrivate ESettingsNameSelectorEntryPrivate;

struct _ESettingsNameSelectorEntry {
	EExtension parent;
	ESettingsNameSelectorEntryPrivate *priv;
};

struct _ESettingsNameSelectorEntryClass {
	EExtensionClass parent_class;
};

GType		e_settings_name_selector_entry_get_type
						(void) G_GNUC_CONST;
void		e_settings_name_selector_entry_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_NAME_SELECTOR_ENTRY_H */

