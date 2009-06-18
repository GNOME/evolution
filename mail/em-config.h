/*
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
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_CONFIG_H__
#define __EM_CONFIG_H__

#include <glib-object.h>
#include <gconf/gconf-client.h>
#include <camel/camel-folder.h>
#include <libedataserver/e-account.h>

#include "e-util/e-config.h"

G_BEGIN_DECLS

typedef struct _EMConfig EMConfig;
typedef struct _EMConfigClass EMConfigClass;
typedef struct _EMConfigPrivate EMConfigPrivate;

/* Current target description */
/* Types of popup tagets */
enum _em_config_target_t {
	EM_CONFIG_TARGET_FOLDER,
	EM_CONFIG_TARGET_PREFS,
	EM_CONFIG_TARGET_ACCOUNT
};

typedef struct _EMConfigTargetFolder EMConfigTargetFolder;
typedef struct _EMConfigTargetPrefs EMConfigTargetPrefs;
typedef struct _EMConfigTargetAccount EMConfigTargetAccount;

struct _EMConfigTargetFolder {
	EConfigTarget target;

	CamelFolder *folder;
	gchar *uri;
};

struct _EMConfigTargetPrefs {
	EConfigTarget target;

	/* preferences are global from gconf */
	GConfClient *gconf;
};

struct _EMConfigTargetAccount {
	EConfigTarget target;

	EAccount *account;
	/* Need also: working account, not just real account, so changes can be propagated around
	   And some mechamism for controlling the gui if we're running inside a druid, e.g. enabling 'next' */
};

typedef struct _EConfigItem EMConfigItem;

/* The object */
struct _EMConfig {
	EConfig config;

	EMConfigPrivate *priv;
};

struct _EMConfigClass {
	EConfigClass config_class;
};

GType em_config_get_type(void);

EMConfig *em_config_new(gint type, const gchar *menuid);

EMConfigTargetFolder *em_config_target_new_folder(EMConfig *emp, CamelFolder *folder, const gchar *uri);
EMConfigTargetPrefs *em_config_target_new_prefs(EMConfig *emp, GConfClient *gconf);
EMConfigTargetAccount *em_config_target_new_account(EMConfig *emp, EAccount *account);

/* ********************************************************************** */

typedef struct _EMConfigHook EMConfigHook;
typedef struct _EMConfigHookClass EMConfigHookClass;

struct _EMConfigHook {
	EConfigHook hook;
};

struct _EMConfigHookClass {
	EConfigHookClass hook_class;
};

GType em_config_hook_get_type(void);

G_END_DECLS

#endif /* __EM_CONFIG_H__ */
