/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 2004 Novell, Inc (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef __E_CAL_CONFIG_H__
#define __E_CAL_CONFIG_H__

#include <glib-object.h>

#include "e-util/e-config.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

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
};

typedef struct _ECalConfigTargetSource ECalConfigTargetSource;

struct _ECalConfigTargetSource {
	EConfigTarget target;

	struct _ESource *source;
};

typedef struct _EConfigItem ECalConfigItem;

GType e_cal_config_get_type (void);
ECalConfig *e_cal_config_new (int type, const char *menuid);

ECalConfigTargetSource *e_cal_config_target_new_source (ECalConfig *ecp, struct _ESource *source);

/* ********************************************************************** */

typedef struct _ECalConfigHook ECalConfigHook;
typedef struct _ECalConfigHookClass ECalConfigHookClass;

struct _ECalConfigHook {
	EConfigHook hook;
};

struct _ECalConfigHookClass {
	EConfigHookClass hook_class;
};

GType e_cal_config_hook_get_type (void);

#ifdef __cplusplus
}
#endif

#endif
