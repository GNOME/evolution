/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-simple-card-bonobo.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Authors:
 * Ettore Perazzoli <ettore@ximian.com>
 * Chris Lahey <clahey@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-simple-card-bonobo.h"

#include <gal/util/e-util.h>

#include "Evolution-Addressbook-SelectNames.h"


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _ESimpleCardBonoboPrivate {
	ECardSimple *card_simple;
};


/* CORBA interface implementation.  */

static POA_GNOME_Evolution_Addressbook_SimpleCard__vepv SimpleCard_vepv;

static POA_GNOME_Evolution_Addressbook_SimpleCard *
create_servant (void)
{
	POA_GNOME_Evolution_Addressbook_SimpleCard *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_Addressbook_SimpleCard *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &SimpleCard_vepv;

	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_Addressbook_SimpleCard__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static GNOME_Evolution_Addressbook_SimpleCard_Arbitrary *
impl_SimpleCard_get_arbitrary (PortableServer_Servant servant,
			       const CORBA_char *key,
			       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;
	GNOME_Evolution_Addressbook_SimpleCard_Arbitrary *ret_val = GNOME_Evolution_Addressbook_SimpleCard_Arbitrary__alloc ();

	bonobo_object = bonobo_object_from_servant (servant);
	simple_card = E_SIMPLE_CARD_BONOBO (bonobo_object);
	priv = simple_card->priv;

	if (priv->card_simple) {
		const ECardArbitrary *arbitrary = e_card_simple_get_arbitrary (priv->card_simple, key);
		ret_val->key = CORBA_string_dup (arbitrary->key);
		ret_val->value = CORBA_string_dup (arbitrary->value);
		ret_val->type = CORBA_string_dup (arbitrary->type);
	} else {
		ret_val->key = CORBA_string_dup ("");
		ret_val->value = CORBA_string_dup ("");
		ret_val->type = CORBA_string_dup ("");
	}

	return ret_val;
}

static void
impl_SimpleCard_set_arbitrary (PortableServer_Servant servant,
			       const CORBA_char *key,
			       const CORBA_char *type,
			       const CORBA_char *value,
			       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	simple_card = E_SIMPLE_CARD_BONOBO (bonobo_object);
	priv = simple_card->priv;

	if (priv->card_simple) {
		e_card_simple_set_arbitrary (priv->card_simple, key, type, value);
	}
}

static CORBA_char *
impl_SimpleCard_get (PortableServer_Servant servant,
		     GNOME_Evolution_Addressbook_SimpleCard_Field field,
		     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	simple_card = E_SIMPLE_CARD_BONOBO (bonobo_object);
	priv = simple_card->priv;

	if (priv->card_simple) {
		char *value = e_card_simple_get (priv->card_simple,
						 field);
		char *ret_val = CORBA_string_dup (value ? value : "");
		g_free (value);
		return ret_val;
	} else {
		return CORBA_string_dup ("");
	}
}

static void
impl_SimpleCard_set (PortableServer_Servant servant,
		     GNOME_Evolution_Addressbook_SimpleCard_Field field,
		     const CORBA_char *value,
		     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	simple_card = E_SIMPLE_CARD_BONOBO (bonobo_object);
	priv = simple_card->priv;

	if (priv->card_simple) {
		e_card_simple_set (priv->card_simple,
				   field,
				   value);
	}
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;

	simple_card = E_SIMPLE_CARD_BONOBO (object);
	priv = simple_card->priv;

	if (priv->card_simple) {
		gtk_object_unref (GTK_OBJECT (priv->card_simple));
	}

	g_free (priv);

	simple_card->priv = NULL;
}


static void
corba_class_init ()
{
	POA_GNOME_Evolution_Addressbook_SimpleCard__vepv *vepv;
	POA_GNOME_Evolution_Addressbook_SimpleCard__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv                 = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private       = NULL;
	base_epv->finalize       = NULL;
	base_epv->default_POA    = NULL;

	epv                      = g_new0 (POA_GNOME_Evolution_Addressbook_SimpleCard__epv, 1);
	epv->getArbitrary        = impl_SimpleCard_get_arbitrary;
	epv->setArbitrary        = impl_SimpleCard_set_arbitrary;
	epv->get                 = impl_SimpleCard_get;
	epv->set                 = impl_SimpleCard_set;

	vepv                     = &SimpleCard_vepv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Addressbook_SimpleCard_epv = epv;
}

static void
class_init (ESimpleCardBonoboClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (bonobo_object_get_type ());

	object_class->destroy = impl_destroy;

	corba_class_init ();
}

static void
init (ESimpleCardBonobo *simple_card)
{
	ESimpleCardBonoboPrivate *priv;

	priv = g_new (ESimpleCardBonoboPrivate, 1);

	priv->card_simple = NULL;

	simple_card->priv = priv;
}


void
e_simple_card_bonobo_construct (ESimpleCardBonobo *simple_card,
				GNOME_Evolution_Addressbook_SimpleCard corba_object,
				ECardSimple *card_simple)
{
	g_return_if_fail (simple_card != NULL);
	g_return_if_fail (E_IS_SIMPLE_CARD_BONOBO (simple_card));

	bonobo_object_construct (BONOBO_OBJECT (simple_card), corba_object);

	simple_card->priv->card_simple = card_simple;
	gtk_object_ref (GTK_OBJECT (card_simple));
}

ESimpleCardBonobo *
e_simple_card_bonobo_new (ECardSimple *card_simple)
{
	POA_GNOME_Evolution_Addressbook_SimpleCard *servant;
	GNOME_Evolution_Addressbook_SimpleCard corba_object;
	ESimpleCardBonobo *simple_card;

	servant = create_servant ();
	if (servant == NULL)
		return NULL;
	
	simple_card = gtk_type_new (e_simple_card_bonobo_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (simple_card), servant);
	e_simple_card_bonobo_construct (simple_card, corba_object, card_simple);

	return simple_card;
}


E_MAKE_TYPE (e_simple_card_bonobo, "ESimpleCardBonobo", ESimpleCardBonobo, class_init, init, PARENT_TYPE)
