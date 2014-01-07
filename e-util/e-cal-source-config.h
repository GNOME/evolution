/*
 * e-cal-source-config.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CAL_SOURCE_CONFIG_H
#define E_CAL_SOURCE_CONFIG_H

#include <libecal/libecal.h>
#include <e-util/e-source-config.h>

/* Standard GObject macros */
#define E_TYPE_CAL_SOURCE_CONFIG \
	(e_cal_source_config_get_type ())
#define E_CAL_SOURCE_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_SOURCE_CONFIG, ECalSourceConfig))
#define E_CAL_SOURCE_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_SOURCE_CONFIG, ECalSourceConfigClass))
#define E_IS_CAL_SOURCE_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_SOURCE_CONFIG))
#define E_IS_CAL_SOURCE_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_SOURCE_CONFIG))
#define E_CAL_SOURCE_CONFIG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_SOURCE_CONFIG, ECalSourceConfigClass))

G_BEGIN_DECLS

typedef struct _ECalSourceConfig ECalSourceConfig;
typedef struct _ECalSourceConfigClass ECalSourceConfigClass;
typedef struct _ECalSourceConfigPrivate ECalSourceConfigPrivate;

struct _ECalSourceConfig {
	ESourceConfig parent;
	ECalSourceConfigPrivate *priv;
};

struct _ECalSourceConfigClass {
	ESourceConfigClass parent_class;
};

GType		e_cal_source_config_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_cal_source_config_new		(ESourceRegistry *registry,
						 ESource *original_source,
						 ECalClientSourceType source_type);
ECalClientSourceType
		e_cal_source_config_get_source_type
						(ECalSourceConfig *config);
void		e_cal_source_config_add_offline_toggle
						(ECalSourceConfig *config,
						 ESource *scratch_source);

G_END_DECLS

#endif /* E_CAL_SOURCE_CONFIG_H */
