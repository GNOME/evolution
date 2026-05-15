/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_SETTINGS_DEPRECATED_H
#define E_SETTINGS_DEPRECATED_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_DEPRECATED \
	(e_settings_deprecated_get_type ())
#define E_SETTINGS_DEPRECATED(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_DEPRECATED, ESettingsDeprecated))
#define E_SETTINGS_DEPRECATED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_DEPRECATED, ESettingsDeprecatedClass))
#define E_IS_SETTINGS_DEPRECATED(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_DEPRECATED))
#define E_IS_SETTINGS_DEPRECATED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_DEPRECATED))
#define E_SETTINGS_DEPRECATED_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_DEPRECATED, ESettingsDeprecatedClass))

G_BEGIN_DECLS

typedef struct _ESettingsDeprecated ESettingsDeprecated;
typedef struct _ESettingsDeprecatedClass ESettingsDeprecatedClass;
typedef struct _ESettingsDeprecatedPrivate ESettingsDeprecatedPrivate;

struct _ESettingsDeprecated {
	EExtension parent;
	ESettingsDeprecatedPrivate *priv;
};

struct _ESettingsDeprecatedClass {
	EExtensionClass parent_class;
};

GType		e_settings_deprecated_get_type	(void) G_GNUC_CONST;
void		e_settings_deprecated_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_DEPRECATED_H */

