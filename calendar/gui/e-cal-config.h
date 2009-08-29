/*
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
 *
 * Authors:
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CAL_CONFIG_H__
#define __E_CAL_CONFIG_H__

#include <glib-object.h>

#include <libecal/e-cal.h>
#include "e-util/e-config.h"

G_BEGIN_DECLS

typedef struct _ECalConfig ECalConfig;
typedef struct _ECalConfigClass ECalConfigClass;

struct _ECalConfig {
	EConfig config;
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

	struct _ESource *source;
        ECalSourceType source_type;
};

struct _ECalConfigTargetPrefs {
	EConfigTarget target;

	struct _GConfClient *gconf;
};

typedef struct _EConfigItem ECalConfigItem;

GType e_cal_config_get_type (void);
ECalConfig *e_cal_config_new (gint type, const gchar *menuid);

ECalConfigTargetSource *e_cal_config_target_new_source (ECalConfig *ecp, struct _ESource *source);
ECalConfigTargetPrefs *e_cal_config_target_new_prefs (ECalConfig *ecp, struct _GConfClient *gconf);

G_END_DECLS

#endif
