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

#ifndef __EAB_CONFIG_H__
#define __EAB_CONFIG_H__

#include <glib-object.h>

#include "e-util/e-config.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

typedef struct _EABConfig EABConfig;
typedef struct _EABConfigClass EABConfigClass;

struct _EABConfig {
	EConfig config;
};

struct _EABConfigClass {
	EConfigClass config_class;
};

enum _eab_config_target_t {
	EAB_CONFIG_TARGET_SOURCE,
};

typedef struct _EABConfigTargetSource EABConfigTargetSource;

struct _EABConfigTargetSource {
	EConfigTarget target;

	struct _ESource *source;
};

typedef struct _EConfigItem EABConfigItem;

GType eab_config_get_type (void);
EABConfig *eab_config_new (int type, const char *menuid);

EABConfigTargetSource *eab_config_target_new_source (EABConfig *ecp, struct _ESource *source);

/* ********************************************************************** */

typedef struct _EABConfigHook EABConfigHook;
typedef struct _EABConfigHookClass EABConfigHookClass;

struct _EABConfigHook {
	EConfigHook hook;
};

struct _EABConfigHookClass {
	EConfigHookClass hook_class;
};

GType eab_config_hook_get_type (void);

#ifdef __cplusplus
}
#endif

#endif
