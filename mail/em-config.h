/*
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
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_CONFIG_H
#define EM_CONFIG_H

#include <camel/camel.h>
#include <e-util/e-util.h>

/* Standard GObject macros */
#define EM_TYPE_CONFIG \
	(em_config_get_type ())
#define EM_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_CONFIG, EMConfig))
#define EM_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_CONFIG, EMConfigClass))
#define EM_IS_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_CONFIG))
#define EM_IS_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_CONFIG, EMConfigClass))
#define EM_CONFIG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_CONFIG, EMConfigClass))

G_BEGIN_DECLS

typedef struct _EMConfig EMConfig;
typedef struct _EMConfigClass EMConfigClass;
typedef struct _EMConfigPrivate EMConfigPrivate;

/* Current target description */
/* Types of popup tagets */
enum _em_config_target_t {
	EM_CONFIG_TARGET_FOLDER,
	EM_CONFIG_TARGET_PREFS,
	EM_CONFIG_TARGET_SETTINGS
};

typedef struct _EMConfigTargetFolder EMConfigTargetFolder;
typedef struct _EMConfigTargetPrefs EMConfigTargetPrefs;
typedef struct _EMConfigTargetSettings EMConfigTargetSettings;

struct _EMConfigTargetFolder {
	EConfigTarget target;

	CamelFolder *folder;
};

struct _EMConfigTargetPrefs {
	EConfigTarget target;
};

struct _EMConfigTargetSettings {
	EConfigTarget target;

	gchar *email_address;

	const gchar *storage_protocol;
	CamelSettings *storage_settings;

	const gchar *transport_protocol;
	CamelSettings *transport_settings;
};

typedef struct _EConfigItem EMConfigItem;

struct _EMConfig {
	EConfig config;
	EMConfigPrivate *priv;
};

struct _EMConfigClass {
	EConfigClass config_class;
};

GType		em_config_get_type		(void);
EMConfig *	em_config_new			(const gchar *menuid);
EMConfigTargetFolder *
		em_config_target_new_folder	(EMConfig *emp,
						 CamelFolder *folder);
EMConfigTargetPrefs *
		em_config_target_new_prefs	(EMConfig *emp);
EMConfigTargetSettings *
		em_config_target_new_settings	(EMConfig *emp,
						 const gchar *email_address,
						 const gchar *storage_protocol,
						 CamelSettings *storage_settings,
						 const gchar *transport_protocol,
						 CamelSettings *transport_settings);
void		em_config_target_update_settings
						(EConfig *ep,
						 EMConfigTargetSettings *target,
						 const gchar *email_address,
						 const gchar *storage_protocol,
						 CamelSettings *storage_settings,
						 const gchar *transport_protocol,
						 CamelSettings *transport_settings);

G_END_DECLS

#endif /* EM_CONFIG_H */
