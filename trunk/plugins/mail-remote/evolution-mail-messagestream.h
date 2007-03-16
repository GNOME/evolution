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
 * Author: Michael Zucchi <notzed@novell.com>
 */

#ifndef _EVOLUTION_MAIL_MESSAGESTREAM_H_
#define _EVOLUTION_MAIL_MESSAGESTREAM_H_

#include <bonobo/bonobo-object.h>
#include "Evolution-DataServer-Mail.h"

typedef struct _EvolutionMailMessageStream        EvolutionMailMessageStream;
typedef struct _EvolutionMailMessageStreamClass   EvolutionMailMessageStreamClass;

struct _EvolutionMailMessageStream {
	BonoboObject parent;

	/* only one or the other is set */
	struct _CamelStream *source;

	char *buffer;
	size_t len;
	size_t pos;
};

struct _EvolutionMailMessageStreamClass {
	BonoboObjectClass parent_class;

	POA_Evolution_Mail_MessageStream__epv epv;
};

GType           evolution_mail_messagestream_get_type(void);

EvolutionMailMessageStream *evolution_mail_messagestream_new(struct _CamelStream *source);
EvolutionMailMessageStream *evolution_mail_messagestream_new_buffer(const char *buffer, size_t len);

#endif /* _EVOLUTION_MAIL_MESSAGESTREAM_H_ */
