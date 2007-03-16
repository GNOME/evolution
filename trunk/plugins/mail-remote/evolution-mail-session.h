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

#ifndef _EVOLUTION_MAIL_SESSION_H_
#define _EVOLUTION_MAIL_SESSION_H_

#include <bonobo/bonobo-object.h>
#include "Evolution-DataServer-Mail.h"

#define EVOLUTION_MAIL_TYPE_SESSION			(evolution_mail_session_get_type ())
#define EVOLUTION_MAIL_SESSION(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_MAIL_TYPE_SESSION, EvolutionMailSession))
#define EVOLUTION_MAIL_SESSION_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_MAIL_TYPE_SESSION, EvolutionMailSessionClass))
#define EVOLUTION_MAIL_IS_SESSION(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_MAIL_TYPE_SESSION))
#define EVOLUTION_MAIL_IS_SESSION_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_MAIL_TYPE_SESSION))

typedef struct _EvolutionMailSession        EvolutionMailSession;
typedef struct _EvolutionMailSessionClass   EvolutionMailSessionClass;

struct _EvolutionMailSession {
	BonoboObject parent;

	struct _CamelSession *session;
};

struct _EvolutionMailSessionClass {
	BonoboObjectClass parent_class;

	POA_Evolution_Mail_Session__epv epv;
};

GType           evolution_mail_session_get_type(void);

void evolution_mail_session_changed(EvolutionMailSession *ems, Evolution_Mail_SessionChanges *changes);

#endif /* _EVOLUTION_MAIL_SESSION_H_ */
