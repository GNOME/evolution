/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_SETTINGS_DATE_EDIT_H
#define E_SETTINGS_DATE_EDIT_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_DATE_EDIT \
	(e_settings_date_edit_get_type ())
#define E_SETTINGS_DATE_EDIT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_DATE_EDIT, ESettingsDateEdit))
#define E_SETTINGS_DATE_EDIT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_DATE_EDIT, ESettingsDateEditClass))
#define E_IS_SETTINGS_DATE_EDIT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_DATE_EDIT))
#define E_IS_SETTINGS_DATE_EDIT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_DATE_EDIT))
#define E_SETTINGS_DATE_EDIT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_DATE_EDIT, ESettingsDateEditClass))

G_BEGIN_DECLS

typedef struct _ESettingsDateEdit ESettingsDateEdit;
typedef struct _ESettingsDateEditClass ESettingsDateEditClass;
typedef struct _ESettingsDateEditPrivate ESettingsDateEditPrivate;

struct _ESettingsDateEdit {
	EExtension parent;
	ESettingsDateEditPrivate *priv;
};

struct _ESettingsDateEditClass {
	EExtensionClass parent_class;
};

GType		e_settings_date_edit_get_type	(void) G_GNUC_CONST;
void		e_settings_date_edit_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_DATE_EDIT_H */

