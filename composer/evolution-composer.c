/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-composer.c
 *
 * Copyright (C) 2000  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
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
 * Author: Dan Winship <danw@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-item-handler.h>
#include <bonobo/bonobo-generic-factory.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include "evolution-composer.h"
#include "mail/mail-config.h"
#include "e-util/e-account-list.h"
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-stream-mem.h>

#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionComposerPrivate {
	int send_id;
	int save_draft_id;

	void (*send_cb) (EMsgComposer *, gpointer);
	void (*save_draft_cb) (EMsgComposer *, int, gpointer);
};

/* CORBA interface implementation.  */
static EDestination **
corba_recipientlist_to_destv (const GNOME_Evolution_Composer_RecipientList *cl)
{
	GNOME_Evolution_Composer_Recipient *recip;
	EDestination **destv;
	int i;

	if (cl->_length == 0)
		return NULL;

	destv = g_new (EDestination *, cl->_length+1);

	for (i = 0; i < cl->_length; ++i) {
		recip = &(cl->_buffer[i]);

		destv[i] = e_destination_new ();

		if (*recip->name)
			e_destination_set_name (destv[i], recip->name);
		e_destination_set_email (destv[i], recip->address);
		
	}
	destv[cl->_length] = NULL;

	return destv;
}

static void
impl_Composer_set_headers (PortableServer_Servant servant,
			   const CORBA_char *from,
			   const GNOME_Evolution_Composer_RecipientList *to,
			   const GNOME_Evolution_Composer_RecipientList *cc,
			   const GNOME_Evolution_Composer_RecipientList *bcc,
			   const CORBA_char *subject,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;
	EDestination **tov, **ccv, **bccv;
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	int found = 0;
	
	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);
	
	account = mail_config_get_account_by_name (from);
	if (!account) {
		accounts = mail_config_get_accounts ();
		iter = e_list_get_iterator ((EList *) accounts);
		while (e_iterator_is_valid (iter)) {
			account = (EAccount *) e_iterator_get (iter);
			
			if (!g_ascii_strcasecmp (account->id->address, from)) {
				found = TRUE;
				break;
			}
			
			e_iterator_next (iter);
		}
		
		g_object_unref (iter);
		
		if (!found)
			account = mail_config_get_default_account ();
	}
	
	tov  = corba_recipientlist_to_destv (to);
	ccv  = corba_recipientlist_to_destv (cc);
	bccv = corba_recipientlist_to_destv (bcc);
	
	e_msg_composer_set_headers (composer->composer, account->name,
				    tov, ccv, bccv, subject);
	
	e_destination_freev (tov);
	e_destination_freev (ccv);
	e_destination_freev (bccv);
}

static void
impl_Composer_set_multipart_type (PortableServer_Servant servant,
				  GNOME_Evolution_Composer_MultipartType type,
				  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	if (type == GNOME_Evolution_Composer_ALTERNATIVE) {
		composer->composer->is_alternative = TRUE;
		composer->composer->send_html = FALSE;
	}
}

static void
impl_Composer_set_body (PortableServer_Servant servant,
			const CORBA_char *body,
			const CORBA_char *mime_type,
			CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	if (!g_ascii_strcasecmp (mime_type, "text/plain")) {
		char *htmlbody = camel_text_to_html (body, CAMEL_MIME_FILTER_TOHTML_PRE, 0);
		
		e_msg_composer_set_body_text (composer->composer, htmlbody, -1);
		g_free (htmlbody);
	} else if (!g_ascii_strcasecmp (mime_type, "text/html"))
		e_msg_composer_set_body_text (composer->composer, body, -1);
	else
		e_msg_composer_set_body (composer->composer, body, mime_type);
}

static void
impl_Composer_attach_MIME (PortableServer_Servant servant,
			   const CORBA_char *data,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;
	CamelMimePart *attachment;
	CamelStream *mem_stream;
	int status;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	mem_stream = camel_stream_mem_new_with_buffer (data, strlen (data));
	attachment = camel_mime_part_new ();
	status = camel_data_wrapper_construct_from_stream (
		CAMEL_DATA_WRAPPER (attachment), mem_stream);
	camel_object_unref (mem_stream);

	if (status == -1) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Composer_CouldNotParse,
				     NULL);
		return;
	}

	e_msg_composer_attach (composer->composer, attachment);
	camel_object_unref (CAMEL_OBJECT (attachment));
}

static void
impl_Composer_attach_data (PortableServer_Servant servant,
			   const CORBA_char *content_type,
			   const CORBA_char *filename,
			   const CORBA_char *description,
			   const CORBA_boolean show_inline,
			   const GNOME_Evolution_Composer_AttachmentData *data,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;
	CamelMimePart *attachment;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	attachment = camel_mime_part_new ();
	camel_mime_part_set_content (attachment, data->_buffer, data->_length,
				     content_type);

	if (*filename)
		camel_mime_part_set_filename (attachment, filename);
	if (*description)
		camel_mime_part_set_description (attachment, description);
	camel_mime_part_set_disposition (attachment, show_inline ?
					 "inline" : "attachment");

	e_msg_composer_attach (composer->composer, attachment);
	camel_object_unref (attachment);
}

static void
impl_Composer_show (PortableServer_Servant servant,
		    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	gtk_widget_show (GTK_WIDGET (composer->composer));
}

static void
impl_Composer_send (PortableServer_Servant servant,
		    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	composer->priv->send_cb (composer->composer, NULL);
}

POA_GNOME_Evolution_Composer__epv *
evolution_composer_get_epv (void)
{
	POA_GNOME_Evolution_Composer__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_Composer__epv, 1);
	epv->setHeaders       = impl_Composer_set_headers;
	epv->setMultipartType = impl_Composer_set_multipart_type;
	epv->setBody          = impl_Composer_set_body;
	epv->attachMIME       = impl_Composer_attach_MIME;
	epv->attachData       = impl_Composer_attach_data;
	epv->show             = impl_Composer_show;
	epv->send             = impl_Composer_send;

	return epv;
}


/* GObject stuff */

static void
finalise (GObject *object)
{
	EvolutionComposer *composer = EVOLUTION_COMPOSER (object);
	struct _EvolutionComposerPrivate *p = composer->priv;

	g_signal_handler_disconnect(composer->composer, p->send_id);
	g_signal_handler_disconnect(composer->composer, p->save_draft_id);
	g_free(p);

	if (composer->composer) {
		g_object_unref(composer->composer);
		composer->composer = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
evolution_composer_class_init (EvolutionComposerClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Composer__epv *epv;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = finalise;

	parent_class = g_type_class_ref(bonobo_object_get_type ());

	epv = &klass->epv;

	epv->setHeaders       = impl_Composer_set_headers;
	epv->setMultipartType = impl_Composer_set_multipart_type;
	epv->setBody          = impl_Composer_set_body;
	epv->attachMIME       = impl_Composer_attach_MIME;
	epv->attachData       = impl_Composer_attach_data;
	epv->show             = impl_Composer_show;
	epv->send             = impl_Composer_send;
}

static void
evolution_composer_init (EvolutionComposer *composer)
{
	EAccount *account;
	
	account = mail_config_get_default_account ();
	composer->composer = e_msg_composer_new ();
	composer->priv = g_malloc0(sizeof(*composer->priv));
}

#if 0
static Bonobo_ItemContainer_ObjectNames *
enum_objects (BonoboItemHandler *handler, gpointer data, CORBA_Environment *ev)
{
}
#endif

static Bonobo_Unknown 
get_object (BonoboItemHandler *h, const char *item_name, gboolean only_if_exists,
	    gpointer data, CORBA_Environment *ev)
{
	EvolutionComposer *composer = data;
	GSList *options, *l;
	
	options = bonobo_item_option_parse (item_name);
	for (l = options; l; l = l->next){
		BonoboItemOption *option = l->data;

		if (strcmp (option->key, "visible") == 0){
			gboolean show = 1;
			
			if (option->value)
				show = atoi (option->value);

			if (show)
				gtk_widget_show (GTK_WIDGET (composer->composer));
			else
				gtk_widget_hide (GTK_WIDGET (composer->composer));
		}
	}
	return bonobo_object_dup_ref (
		BONOBO_OBJECT (composer)->corba_objref, ev);
}

void
evolution_composer_construct (EvolutionComposer *composer,
			      GNOME_Evolution_Composer corba_object)
{
	BonoboObject *item_handler;
	
	g_return_if_fail (composer != NULL);
	g_return_if_fail (EVOLUTION_IS_COMPOSER (composer));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	/*bonobo_object_construct (BONOBO_OBJECT (composer), corba_object);*/

	item_handler = BONOBO_OBJECT (
		bonobo_item_handler_new (NULL, get_object, composer));
	bonobo_object_add_interface (BONOBO_OBJECT (composer), BONOBO_OBJECT (item_handler));
}

EvolutionComposer *
evolution_composer_new (void (*send) (EMsgComposer *, gpointer),
			void (*save_draft) (EMsgComposer *, int, gpointer))
{
	EvolutionComposer *new;
	struct _EvolutionComposerPrivate *p;

	new = g_object_new(EVOLUTION_TYPE_COMPOSER, NULL);
	evolution_composer_construct (new, bonobo_object_corba_objref((BonoboObject *)new));
	p = new->priv;
	p->send_cb = send;
	p->save_draft_cb = save_draft;
	p->send_id = g_signal_connect (new->composer, "send", G_CALLBACK (send), NULL);
	p->save_draft_id = g_signal_connect (new->composer, "save-draft", G_CALLBACK (save_draft), NULL);

	return new;
}

BONOBO_TYPE_FUNC_FULL(EvolutionComposer, GNOME_Evolution_Composer, BONOBO_TYPE_OBJECT, evolution_composer)
