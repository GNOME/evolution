/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Miguel Angel Lopez Hernandex <miguel@gulev.org.mx>
 *
 *  Copyright 2004 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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

#ifndef __NMN_H__
#define __NMN_H__

#define GCONF_KEY "/apps/evolution/mail/notify/gen_dbus_msg"
#define DBUS_PATH "/org/gnome/evolution/mail/newmail"
#define DBUS_INTERFACE "org.gnome.evolution.mail.dbus.Signal"

GtkWidget *
org_gnome_new_mail_config (EPlugin *ep, EConfigHookItemFactoryData *hook_data);

void
org_gnome_new_mail_notify (EMEventTargetFolder *t);


#endif /* __NMN_H__ */
