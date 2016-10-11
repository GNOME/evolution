/*
 * e-settings-cal-model.h
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

#ifndef E_SETTINGS_CAL_MODEL_H
#define E_SETTINGS_CAL_MODEL_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_CAL_MODEL \
	(e_settings_cal_model_get_type ())
#define E_SETTINGS_CAL_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_CAL_MODEL, ESettingsCalModel))
#define E_SETTINGS_CAL_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_CAL_MODEL, ESettingsCalModelClass))
#define E_IS_SETTINGS_CAL_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_CAL_MODEL))
#define E_IS_SETTINGS_CAL_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_CAL_MODEL))
#define E_SETTINGS_CAL_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_CAL_MODEL, ESettingsCalModelClass))

G_BEGIN_DECLS

typedef struct _ESettingsCalModel ESettingsCalModel;
typedef struct _ESettingsCalModelClass ESettingsCalModelClass;
typedef struct _ESettingsCalModelPrivate ESettingsCalModelPrivate;

struct _ESettingsCalModel {
	EExtension parent;
	ESettingsCalModelPrivate *priv;
};

struct _ESettingsCalModelClass {
	EExtensionClass parent_class;
};

GType		e_settings_cal_model_get_type	(void) G_GNUC_CONST;
void		e_settings_cal_model_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_CAL_MODEL_H */

