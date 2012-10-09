/*
 * e-cal-config-view.h
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

#ifndef E_CAL_CONFIG_VIEW_H
#define E_CAL_CONFIG_VIEW_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_CAL_CONFIG_VIEW \
	(e_cal_config_view_get_type ())
#define E_CAL_CONFIG_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_CONFIG_VIEW, ECalConfigView))
#define E_CAL_CONFIG_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_CONFIG_VIEW, ECalConfigViewClass))
#define E_IS_CAL_CONFIG_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_CONFIG_VIEW))
#define E_IS_CAL_CONFIG_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_CONFIG_VIEW))
#define E_CAL_CONFIG_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_CONFIG_VIEW, ECalConfigViewClass))

G_BEGIN_DECLS

typedef struct _ECalConfigView ECalConfigView;
typedef struct _ECalConfigViewClass ECalConfigViewClass;
typedef struct _ECalConfigViewPrivate ECalConfigViewPrivate;

struct _ECalConfigView {
	EExtension parent;
	ECalConfigViewPrivate *priv;
};

struct _ECalConfigViewClass {
	EExtensionClass parent_class;
};

GType		e_cal_config_view_get_type	(void) G_GNUC_CONST;
void		e_cal_config_view_type_register	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_CAL_CONFIG_VIEW_H */

