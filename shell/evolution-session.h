/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-session.h
 *
 * Copyright (C) 2000, 2001, 2002  Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifndef __EVOLUTION_SESSION_H__
#define __EVOLUTION_SESSION_H__

#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_SESSION			(evolution_session_get_type ())
#define EVOLUTION_SESSION(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_SESSION, EvolutionSession))
#define EVOLUTION_SESSION_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_SESSION, EvolutionSessionClass))
#define EVOLUTION_IS_SESSION(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_SESSION))
#define EVOLUTION_IS_SESSION_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_SESSION))


typedef struct _EvolutionSession        EvolutionSession;
typedef struct _EvolutionSessionPrivate EvolutionSessionPrivate;
typedef struct _EvolutionSessionClass   EvolutionSessionClass;

struct _EvolutionSession {
	BonoboObject parent;

	EvolutionSessionPrivate *priv;
};

struct _EvolutionSessionClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Session__epv epv;

	void (* save_configuration) (EvolutionSession *session, const char *prefix);
	void (* load_configuration) (EvolutionSession *session, const char *prefix);
};


GtkType           evolution_session_get_type   (void);
EvolutionSession *evolution_session_new        (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_SESSION_H__ */
