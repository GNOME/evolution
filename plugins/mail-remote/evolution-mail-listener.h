/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005  Novell, Inc.
 *
 * Author: Michael Zucchi <notzed@novell.com>
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

#ifndef _EVOLUTION_MAIL_LISTENER_H_
#define _EVOLUTION_MAIL_LISTENER_H_

#include <bonobo/bonobo-object.h>
#include "Evolution-DataServer-Mail.h"

#define EVOLUTION_MAIL_TYPE_LISTENER			(evolution_mail_listener_get_type ())
#define EVOLUTION_MAIL_LISTENER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_MAIL_TYPE_LISTENER, EvolutionMailListener))
#define EVOLUTION_MAIL_LISTENER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_MAIL_TYPE_LISTENER, EvolutionMailListenerClass))
#define EVOLUTION_MAIL_IS_LISTENER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_MAIL_TYPE_LISTENER))
#define EVOLUTION_MAIL_IS_LISTENER_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_MAIL_TYPE_LISTENER))

typedef struct _EvolutionMailListener        EvolutionMailListener;
typedef struct _EvolutionMailListenerClass   EvolutionMailListenerClass;

struct _EvolutionMailListener {
	BonoboObject parent;
};

struct _EvolutionMailListenerClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Mail_Listener__epv epv;

	void (*session_changed)(const GNOME_Evolution_Mail_Session session, const GNOME_Evolution_Mail_SessionChange *change);
	void (*store_changed)(const GNOME_Evolution_Mail_Session session, const GNOME_Evolution_Mail_Store store, const GNOME_Evolution_Mail_StoreChanges *);
	void (*folder_changed)(const GNOME_Evolution_Mail_Session session, const GNOME_Evolution_Mail_Store store, const GNOME_Evolution_Mail_Folder folder, const GNOME_Evolution_Mail_FolderChanges *);
};

GType           evolution_mail_listener_get_type(void);

EvolutionMailListener *evolution_mail_listener_new(void);

#endif /* _EVOLUTION_MAIL_LISTENER_H_ */
