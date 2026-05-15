/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef MODULE_CAL_CONFIG_GOOGLE_H
#define MODULE_CAL_CONFIG_GOOGLE_H

#include <glib.h>
#include <libebackend/libebackend.h>
#include "e-util/e-util.h"

gboolean	e_module_cal_config_google_is_supported	(ESourceConfigBackend *backend,
							 ESourceRegistry *registry);
void		e_cal_config_google_type_register	(GTypeModule *type_module);
void		e_cal_config_gtasks_type_register	(GTypeModule *type_module);

#endif /* MODULE_CAL_CONFIG_GOOGLE_H */
