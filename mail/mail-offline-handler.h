/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-offline-handler.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _MAIL_OFFLINE_HANDLER_H_
#define _MAIL_OFFLINE_HANDLER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-xobject.h>
#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define MAIL_TYPE_OFFLINE_HANDLER			(mail_offline_handler_get_type ())
#define MAIL_OFFLINE_HANDLER(obj)			(GTK_CHECK_CAST ((obj), MAIL_TYPE_OFFLINE_HANDLER, MailOfflineHandler))
#define MAIL_OFFLINE_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), MAIL_TYPE_OFFLINE_HANDLER, MailOfflineHandlerClass))
#define MAIL_IS_OFFLINE_HANDLER(obj)			(GTK_CHECK_TYPE ((obj), MAIL_TYPE_OFFLINE_HANDLER))
#define MAIL_IS_OFFLINE_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), MAIL_TYPE_OFFLINE_HANDLER))


typedef struct _MailOfflineHandler        MailOfflineHandler;
typedef struct _MailOfflineHandlerPrivate MailOfflineHandlerPrivate;
typedef struct _MailOfflineHandlerClass   MailOfflineHandlerClass;

struct _MailOfflineHandler {
	BonoboXObject parent;

	MailOfflineHandlerPrivate *priv;
};

struct _MailOfflineHandlerClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_Offline__epv epv;
};


GtkType             mail_offline_handler_get_type  (void);
MailOfflineHandler *mail_offline_handler_new       (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MAIL_OFFLINE_HANDLER_H_ */
