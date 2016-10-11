/*
 * e-mail-config-hook.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_CONFIG_HOOK_H
#define E_MAIL_CONFIG_HOOK_H

#include <glib-object.h>

G_BEGIN_DECLS

void e_mail_config_hook_register_type (GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_CONFIG_HOOK_H */
