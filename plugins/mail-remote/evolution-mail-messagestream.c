/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2005  Novell, Inc.
 *
 * Authors: Michael Zucchi <notzed@novell.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include "evolution-mail-messagestream.h"
#include <camel/camel-stream-mem.h>

#include "e-corba-utils.h"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionMailMessageStream *emms = (EvolutionMailMessageStream *)object;

	d(printf("EvolutionMailMessageStream: finalise\n"));

	if (emms->source)
		camel_object_unref(emms->source);
	g_free(emms->buffer);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Evolution.Mail.MessageStream */

static Evolution_Mail_Buffer *
impl_next(PortableServer_Servant _servant, const CORBA_long limit, CORBA_Environment * ev)
{
	EvolutionMailMessageStream *emf = (EvolutionMailMessageStream *)bonobo_object_from_servant(_servant);
	Evolution_Mail_Buffer *buf;
	ssize_t len;

	buf = Evolution_Mail_Buffer__alloc();
	buf->_maximum = limit;
	buf->_buffer = Evolution_Mail_Buffer_allocbuf(buf->_maximum);

	if (emf->source) {
		len = camel_stream_read(emf->source, buf->_buffer, buf->_maximum);
		if (len == -1) {
			Evolution_Mail_MailException *x;

			x = Evolution_Mail_MailException__alloc();
			x->id = Evolution_Mail_SYSTEM_ERROR;
			x->desc = CORBA_string_dup(g_strerror(errno));
			CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_Evolution_Mail_MailException, x);
			CORBA_free(buf);
			buf = CORBA_OBJECT_NIL;
		} else {
			buf->_length = len;
		}
	} else {
		len = MIN(limit, (emf->len - emf->pos));
		memcpy(buf->_buffer, emf->buffer + emf->pos, len);
		emf->pos += len;
		buf->_length = len;
	}

	return buf;
}

static void
impl_mi_dispose(PortableServer_Servant _servant, CORBA_Environment *ev)
{
	EvolutionMailMessageStream *emmi = (EvolutionMailMessageStream *)bonobo_object_from_servant(_servant);

	bonobo_object_set_immortal((BonoboObject *)emmi, FALSE);
	bonobo_object_unref((BonoboObject *)emmi);
}

/* Initialization */

static void
evolution_mail_messagestream_class_init (EvolutionMailMessageStreamClass *klass)
{
	POA_Evolution_Mail_MessageStream__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->next = impl_next;
	epv->dispose = impl_mi_dispose;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
evolution_mail_messagestream_init(EvolutionMailMessageStream *emi, EvolutionMailMessageStreamClass *klass)
{
	bonobo_object_set_immortal((BonoboObject *)emi, TRUE);
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailMessageStream, Evolution_Mail_MessageStream, PARENT_TYPE, evolution_mail_messagestream)

EvolutionMailMessageStream *
evolution_mail_messagestream_new(CamelStream *source)
{
	/* FIXME: use right poa, thread per object? */
	EvolutionMailMessageStream *emf = g_object_new(evolution_mail_messagestream_get_type(), NULL);

	emf->source = source;
	camel_object_ref(source);

	return emf;
}

EvolutionMailMessageStream *
evolution_mail_messagestream_new_buffer(const char *buffer, size_t len)
{
	/* FIXME: use right poa, thread per object? */
	EvolutionMailMessageStream *emf = g_object_new(evolution_mail_messagestream_get_type(), NULL);

	emf->buffer = g_malloc(len);
	memcpy(emf->buffer, buffer, len);
	emf->len = len;
	emf->pos = 0;

	return emf;
}


