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

#ifndef _EVOLUTION_MAIL_MESSAGEITERATOR_H_
#define _EVOLUTION_MAIL_MESSAGEITERATOR_H_

#include <bonobo/bonobo-object.h>
#include "Evolution-DataServer-Mail.h"

struct _CamelFolder;

typedef struct _EvolutionMailMessageIterator        EvolutionMailMessageIterator;
typedef struct _EvolutionMailMessageIteratorClass   EvolutionMailMessageIteratorClass;

struct _EvolutionMailMessageIterator {
	BonoboObject parent;
};

struct _EvolutionMailMessageIteratorClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Mail_MessageIterator__epv epv;
};

GType           evolution_mail_messageiterator_get_type(void);

EvolutionMailMessageIterator *evolution_mail_messageiterator_new(struct _CamelFolder *folder, const char *expr);

#endif /* _EVOLUTION_MAIL_MESSAGEITERATOR_H_ */
