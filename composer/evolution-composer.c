/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-composer.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Dan Winship <danw@helixcode.com>
 */

#include <config.h>

#include <bonobo.h>
#include <camel/camel.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <bonobo/bonobo-item-handler.h>
#include "evolution-composer.h"

#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

static void (*send_cb) (EMsgComposer *, gpointer);
static void (*postpone_cb) (EMsgComposer *, gpointer);

/* CORBA interface implementation.  */

static POA_GNOME_Evolution_Composer__vepv Composer_vepv;

static GList *
corba_recipientlist_to_glist (const GNOME_Evolution_Composer_RecipientList *cl)
{
	GNOME_Evolution_Composer_Recipient *recip;
	GList *gl = NULL;
	char *str;
	int i;

	for (i = cl->_length - 1; i >= 0; i--) {
		recip = &(cl->_buffer[i]);

		/* Let's copy this code into yet another place! */
		if (*recip->name) {
			str = g_strdup_printf ("\"%s\" <%s>",
					       recip->name, recip->address);
		} else
			str = g_strdup (recip->address);

		gl = g_list_prepend (gl, str);		
	}

	return gl;
}

static void
free_recipient_glist (GList *gl)
{
	GList *tmp;

	while (gl) {
		tmp = gl->next;
		g_free (gl->data);
		g_list_free_1 (gl);
		gl = tmp;
	}
}

static void
impl_Composer_set_headers (PortableServer_Servant servant,
			   const GNOME_Evolution_Composer_RecipientList *to,
			   const GNOME_Evolution_Composer_RecipientList *cc,
			   const GNOME_Evolution_Composer_RecipientList *bcc,
			   const CORBA_char *subject,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;
	GList *gto, *gcc, *gbcc;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	gto = corba_recipientlist_to_glist (to);
	gcc = corba_recipientlist_to_glist (cc);
	gbcc = corba_recipientlist_to_glist (bcc);

	e_msg_composer_set_headers (composer->composer, gto, gcc, gbcc,
				    subject);

	free_recipient_glist (gto);
	free_recipient_glist (gcc);
	free_recipient_glist (gbcc);
}

static void
impl_Composer_set_body_text (PortableServer_Servant servant,
			     const CORBA_char *text,
			     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	e_msg_composer_set_body_text (composer->composer, text);
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
	camel_object_unref (CAMEL_OBJECT (mem_stream));

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
			   const CORBA_char *data,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionComposer *composer;
	CamelMimePart *attachment;

	bonobo_object = bonobo_object_from_servant (servant);
	composer = EVOLUTION_COMPOSER (bonobo_object);

	attachment = camel_mime_part_new ();
	camel_mime_part_set_content (attachment, data, strlen (data),
				     content_type);

	if (*filename)
		camel_mime_part_set_filename (attachment, filename);
	if (*description)
		camel_mime_part_set_description (attachment, description);
	camel_mime_part_set_disposition (attachment, show_inline ?
					 "inline" : "attachment");

	e_msg_composer_attach (composer->composer, attachment);
	camel_object_unref (CAMEL_OBJECT (attachment));
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

POA_GNOME_Evolution_Composer__epv *
evolution_composer_get_epv (void)
{
	POA_GNOME_Evolution_Composer__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_Composer__epv, 1);
	epv->setHeaders  = impl_Composer_set_headers;
	epv->setBodyText = impl_Composer_set_body_text;
	epv->attachMIME  = impl_Composer_attach_MIME;
	epv->attachData  = impl_Composer_attach_data;
	epv->show        = impl_Composer_show;

	return epv;
}


/* GtkObject stuff */

static void
destroy (GtkObject *object)
{
	EvolutionComposer *composer = EVOLUTION_COMPOSER (object);

	if (composer->composer)
		gtk_object_unref (GTK_OBJECT (composer->composer));

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
class_init (EvolutionComposerClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (bonobo_object_get_type ());

	Composer_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	Composer_vepv.GNOME_Evolution_Composer_epv = evolution_composer_get_epv ();
}

static void
init (EvolutionComposer *composer)
{
	composer->composer = e_msg_composer_new ();
	gtk_signal_connect (GTK_OBJECT (composer->composer), "send",
			    GTK_SIGNAL_FUNC (send_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer->composer), "postpone",
			    GTK_SIGNAL_FUNC (postpone_cb), NULL);
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

	bonobo_object_construct (BONOBO_OBJECT (composer), corba_object);

	item_handler = BONOBO_OBJECT (
		bonobo_item_handler_new (NULL, get_object, composer));
	bonobo_object_add_interface (BONOBO_OBJECT (composer), BONOBO_OBJECT (item_handler));
}

EvolutionComposer *
evolution_composer_new (void)
{
	EvolutionComposer *new;
	POA_GNOME_Evolution_Composer *servant;
	CORBA_Environment ev;
	GNOME_Evolution_Composer corba_object;

	servant = (POA_GNOME_Evolution_Composer *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &Composer_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_Composer__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	new = gtk_type_new (evolution_composer_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (new),
						       servant);
	evolution_composer_construct (new, corba_object);

	return new;
}

E_MAKE_TYPE (evolution_composer, "EvolutionComposer", EvolutionComposer, class_init, init, PARENT_TYPE)


#define GNOME_EVOLUTION_MAIL_COMPOSER_FACTORY_ID "OAFIID:GNOME_Evolution_Mail_ComposerFactory"

static BonoboObject *
factory_fn (BonoboGenericFactory *factory, void *closure)
{
	return BONOBO_OBJECT (evolution_composer_new ());
}

void
evolution_composer_factory_init (void (*send) (EMsgComposer *, gpointer),
				 void (*postpone) (EMsgComposer *, gpointer))
{
	if (bonobo_generic_factory_new (GNOME_EVOLUTION_MAIL_COMPOSER_FACTORY_ID,
					factory_fn, NULL) == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's composer."));
		exit (1);
	}

	send_cb = send;
	postpone_cb = postpone;
}
