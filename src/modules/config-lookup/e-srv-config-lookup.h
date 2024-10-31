/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_SRV_CONFIG_LOOKUP_H
#define E_SRV_CONFIG_LOOKUP_H

#include <glib-object.h>

G_BEGIN_DECLS

void e_srv_config_lookup_type_register (GTypeModule *type_module);

G_END_DECLS

#endif /* E_SRV_CONFIG_LOOKUP_H */
