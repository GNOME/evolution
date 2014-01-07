/*
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
 *
 * Authors:
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CAL_CONFIG_H
#define E_CAL_CONFIG_H

#include <libecal/libecal.h>
#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_CAL_CONFIG \
	(e_cal_config_get_type ())
#define E_CAL_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_CONFIG, ECalConfig))
#define E_CAL_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_CONFIG, ECalConfigClass))
#define E_IS_CAL_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_CONFIG))
#define E_IS_CAL_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_CONFIG))
#define E_CAL_CONFIG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_CONFIG, ECalConfigClass))

G_BEGIN_DECLS

typedef struct _ECalConfig ECalConfig;
typedef struct _ECalConfigClass ECalConfigClass;
typedef struct _ECalConfigPrivate ECalConfigPrivate;

struct _ECalConfig {
	EConfig config;

	ECalConfigPrivate *priv;
};

struct _ECalConfigClass {
	EConfigClass config_class;
};

enum _e_cal_config_target_t {
	EC_CONFIG_TARGET_SOURCE,
	EC_CONFIG_TARGET_PREFS
};

typedef struct _ECalConfigTargetSource ECalConfigTargetSource;
typedef struct _ECalConfigTargetPrefs ECalConfigTargetPrefs;

struct _ECalConfigTargetSource {
	EConfigTarget target;

	ESource *source;
        ECalClientSourceType source_type;
};

struct _ECalConfigTargetPrefs {
	EConfigTarget target;

	GSettings *settings;
};

typedef struct _EConfigItem ECalConfigItem;

GType		e_cal_config_get_type (void);
ECalConfig *	e_cal_config_new		(const gchar *menuid);
ECalConfigTargetSource *
		e_cal_config_target_new_source	(ECalConfig *ecp,
						 ESource *source);
ECalConfigTargetPrefs *
		e_cal_config_target_new_prefs	(ECalConfig *ecp);

G_END_DECLS

#endif
