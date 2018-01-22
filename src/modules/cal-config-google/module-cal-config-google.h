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
