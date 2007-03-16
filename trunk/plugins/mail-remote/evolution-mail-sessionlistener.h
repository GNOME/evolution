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

#ifndef _EVOLUTION_MAIL_SESSIONLISTENER_H_
#define _EVOLUTION_MAIL_SESSIONLISTENER_H_

#include <bonobo/bonobo-object.h>
#include "Evolution-DataServer-Mail.h"

#define EVOLUTION_MAIL_TYPE_SESSIONLISTENER			(evolution_mail_sessionlistener_get_type ())
#define EVOLUTION_MAIL_SESSIONLISTENER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_MAIL_TYPE_LISTENER, EvolutionMailSessionListener))
#define EVOLUTION_MAIL_SESSIONLISTENER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_MAIL_TYPE_LISTENER, EvolutionMailSessionListenerClass))
#define EVOLUTION_MAIL_IS_SESSIONLISTENER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_MAIL_TYPE_LISTENER))
#define EVOLUTION_MAIL_IS_SESSIONLISTENER_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_MAIL_TYPE_LISTENER))

typedef struct _EvolutionMailSessionListener        EvolutionMailSessionListener;
typedef struct _EvolutionMailSessionListenerClass   EvolutionMailSessionListenerClass;

struct _EvolutionMailSessionListener {
	BonoboObject parent;
};

struct _EvolutionMailSessionListenerClass {
	BonoboObjectClass parent_class;

	POA_Evolution_Mail_SessionListener__epv epv;

	void (*changed)(EvolutionMailSessionListener *, const Evolution_Mail_Session session, const Evolution_Mail_SessionChanges *);
};

GType           evolution_mail_sessionlistener_get_type(void);

EvolutionMailSessionListener *evolution_mail_sessionlistener_new(void);

#endif /* _EVOLUTION_MAIL_SESSIONLISTENER_H_ */
