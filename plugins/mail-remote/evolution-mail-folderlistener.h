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

#ifndef _EVOLUTION_MAIL_FOLDERLISTENER_H_
#define _EVOLUTION_MAIL_FOLDERLISTENER_H_

#include <bonobo/bonobo-object.h>
#include "Evolution-DataServer-Mail.h"

#define EVOLUTION_MAIL_TYPE_FOLDERLISTENER			(evolution_mail_folderlistener_get_type ())
#define EVOLUTION_MAIL_FOLDERLISTENER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_MAIL_TYPE_LISTENER, EvolutionMailFolderListener))
#define EVOLUTION_MAIL_FOLDERLISTENER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_MAIL_TYPE_LISTENER, EvolutionMailFolderListenerClass))
#define EVOLUTION_MAIL_IS_FOLDERLISTENER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_MAIL_TYPE_LISTENER))
#define EVOLUTION_MAIL_IS_FOLDERLISTENER_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_MAIL_TYPE_LISTENER))

typedef struct _EvolutionMailFolderListener        EvolutionMailFolderListener;
typedef struct _EvolutionMailFolderListenerClass   EvolutionMailFolderListenerClass;

struct _EvolutionMailFolderListener {
	BonoboObject parent;
};

struct _EvolutionMailFolderListenerClass {
	BonoboObjectClass parent_class;

	POA_Evolution_Mail_FolderListener__epv epv;

	void (*changed)(EvolutionMailFolderListener *, const Evolution_Mail_Folder folder, const Evolution_Mail_FolderChanges *);
};

GType           evolution_mail_folderlistener_get_type(void);

EvolutionMailFolderListener *evolution_mail_folderlistener_new(void);

#endif /* _EVOLUTION_MAIL_FOLDERLISTENER_H_ */
