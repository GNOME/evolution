/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2001 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef MAIL_CONFIG_DRUID_H
#define MAIL_CONFIG_DRUID_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gnome.h>
#include <glade/glade.h>
#include <camel.h>
#include "shell/Evolution.h"
#include "mail-account-gui.h"

#define MAIL_CONFIG_DRUID_TYPE        (mail_config_druid_get_type ())
#define MAIL_CONFIG_DRUID(o)          (GTK_CHECK_CAST ((o), MAIL_CONFIG_DRUID_TYPE, MailConfigDruid))
#define MAIL_CONFIG_DRUID_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_CONFIG_DRUID_TYPE, MailConfigDruidClass))
#define MAIL_IS_CONFIG_DRUID(o)       (GTK_CHECK_TYPE ((o), MAIL_CONFIG_DRUID_TYPE))
#define MAIL_IS_CONFIG_DRUID_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_CONFIG_DRUID_TYPE))

typedef struct {
	GtkWindow parent;
	
	GnomeDruid *druid;
	MailAccountGui *gui;

	GNOME_Evolution_Shell shell;
	gboolean identity_copied;
	CamelProvider *last_source;
} MailConfigDruid;

typedef struct {
	GtkWindowClass parent_class;
	
	/* signals */
	
} MailConfigDruidClass;

GtkType mail_config_druid_get_type (void);

MailConfigDruid *mail_config_druid_new (GNOME_Evolution_Shell shell);

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_CONFIG_DRUID_H */
