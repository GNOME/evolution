/*
 * e-cal-config-model.h
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

#ifndef E_CAL_CONFIG_MODEL_H
#define E_CAL_CONFIG_MODEL_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_CAL_CONFIG_MODEL \
	(e_cal_config_model_get_type ())
#define E_CAL_CONFIG_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_CONFIG_MODEL, ECalConfigModel))
#define E_CAL_CONFIG_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_CONFIG_MODEL, ECalConfigModelClass))
#define E_IS_CAL_CONFIG_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_CONFIG_MODEL))
#define E_IS_CAL_CONFIG_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_CONFIG_MODEL))
#define E_CAL_CONFIG_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_CONFIG_MODEL, ECalConfigModelClass))

G_BEGIN_DECLS

typedef struct _ECalConfigModel ECalConfigModel;
typedef struct _ECalConfigModelClass ECalConfigModelClass;
typedef struct _ECalConfigModelPrivate ECalConfigModelPrivate;

struct _ECalConfigModel {
	EExtension parent;
	ECalConfigModelPrivate *priv;
};

struct _ECalConfigModelClass {
	EExtensionClass parent_class;
};

GType		e_cal_config_model_get_type	(void) G_GNUC_CONST;
void		e_cal_config_model_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_CAL_CONFIG_MODEL_H */

