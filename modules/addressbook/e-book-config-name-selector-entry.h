/*
 * e-book-config-name-selector-entry.h
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

#ifndef E_BOOK_CONFIG_NAME_SELECTOR_ENTRY_H
#define E_BOOK_CONFIG_NAME_SELECTOR_ENTRY_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_BOOK_CONFIG_NAME_SELECTOR_ENTRY \
	(e_book_config_name_selector_entry_get_type ())
#define E_BOOK_CONFIG_NAME_SELECTOR_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOOK_CONFIG_NAME_SELECTOR_ENTRY, EBookConfigNameSelectorEntry))
#define E_BOOK_CONFIG_NAME_SELECTOR_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BOOK_CONFIG_NAME_SELECTOR_ENTRY, EBookConfigNameSelectorEntryClass))
#define E_IS_BOOK_CONFIG_NAME_SELECTOR_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BOOK_CONFIG_NAME_SELECTOR_ENTRY))
#define E_IS_BOOK_CONFIG_NAME_SELECTOR_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BOOK_CONFIG_NAME_SELECTOR_ENTRY))
#define E_BOOK_CONFIG_NAME_SELECTOR_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BOOK_CONFIG_NAME_SELECTOR_ENTRY, EBookConfigNameSelectorEntryClass))

G_BEGIN_DECLS

typedef struct _EBookConfigNameSelectorEntry EBookConfigNameSelectorEntry;
typedef struct _EBookConfigNameSelectorEntryClass EBookConfigNameSelectorEntryClass;
typedef struct _EBookConfigNameSelectorEntryPrivate EBookConfigNameSelectorEntryPrivate;

struct _EBookConfigNameSelectorEntry {
	EExtension parent;
	EBookConfigNameSelectorEntryPrivate *priv;
};

struct _EBookConfigNameSelectorEntryClass {
	EExtensionClass parent_class;
};

GType		e_book_config_name_selector_entry_get_type
						(void) G_GNUC_CONST;
void		e_book_config_name_selector_entry_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_BOOK_CONFIG_NAME_SELECTOR_ENTRY_H */

