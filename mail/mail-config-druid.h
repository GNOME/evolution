/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef MAIL_CONFIG_DRUID_H
#define MAIL_CONFIG_DRUID_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>

typedef struct _GtkWindow MailConfigDruid;

MailConfigDruid *mail_config_druid_new (void);

char *mail_config_druid_get_account_name (MailConfigDruid *druid);
gboolean mail_config_druid_get_default_account (MailConfigDruid *druid);

char *mail_config_druid_get_source_url (MailConfigDruid *druid);

gboolean mail_config_druid_get_keep_mail_on_server (MailConfigDruid *druid);
gboolean mail_config_druid_get_save_source_password (MailConfigDruid *druid);
gboolean mail_config_druid_get_auto_check (MailConfigDruid *druid);
gint mail_config_druid_get_auto_check_minutes (MailConfigDruid *druid);

char *mail_config_druid_get_transport_url (MailConfigDruid *druid);
gboolean mail_config_druid_get_save_transport_password (MailConfigDruid *druid);
gboolean mail_config_druid_get_transport_requires_auth (MailConfigDruid *druid);

struct _BonoboObject *evolution_mail_config_wizard_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_CONFIG_DRUID_H */
