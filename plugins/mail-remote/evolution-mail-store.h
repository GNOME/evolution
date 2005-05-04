/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005  Novell, Inc.
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
 * Author: JP Rosevear <jpr@ximian.com>
 */

#ifndef _EVOLUTION_MAIL_STORE_H_
#define _EVOLUTION_MAIL_STORE_H_

#include <bonobo/bonobo-object.h>
#include "Evolution-DataServer-Mail.h"

#define EVOLUTION_MAIL_TYPE_STORE			(evolution_mail_store_get_type ())
#define EVOLUTION_MAIL_STORE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_MAIL_TYPE_STORE, EvolutionMailStore))
#define EVOLUTION_MAIL_STORE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_MAIL_TYPE_STORE, EvolutionMailStoreClass))
#define EVOLUTION_MAIL_IS_STORE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_MAIL_TYPE_STORE))
#define EVOLUTION_MAIL_IS_STORE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_MAIL_TYPE_STORE))

struct _EAccount;

typedef struct _EvolutionMailStore        EvolutionMailStore;
typedef struct _EvolutionMailStoreClass   EvolutionMailStoreClass;

struct _EvolutionMailStore {
	BonoboObject parent;
};

struct _EvolutionMailStoreClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Mail_Store__epv epv;
};

GType           evolution_mail_store_get_type(void);

EvolutionMailStore *evolution_mail_store_new(struct _EAccount *ea);

#endif /* _EVOLUTION_MAIL_STORE_H_ */
