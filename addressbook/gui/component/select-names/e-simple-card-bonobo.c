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


#define PARENT_TYPE BONOBO_TYPE_OBJECT
static BonoboObjectClass *parent_class = NULL;

struct _ESimpleCardBonoboPrivate {
	ECardSimple *card_simple;
};



static GNOME_Evolution_Addressbook_SimpleCard_Arbitrary *
impl_SimpleCard_get_arbitrary (PortableServer_Servant servant,
			       const CORBA_char *key,
			       CORBA_Environment *ev)
{
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;
	GNOME_Evolution_Addressbook_SimpleCard_Arbitrary *ret_val = GNOME_Evolution_Addressbook_SimpleCard_Arbitrary__alloc ();

	simple_card = E_SIMPLE_CARD_BONOBO (bonobo_object (servant));
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
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;

	simple_card = E_SIMPLE_CARD_BONOBO (bonobo_object (servant));
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
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;

	simple_card = E_SIMPLE_CARD_BONOBO (bonobo_object (servant));
	priv = simple_card->priv;

	if (priv->card_simple) {
		char *value = e_card_simple_get (priv->card_simple,
						 field);
		char *ret_val = CORBA_string_dup (value);
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

	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;

	simple_card = E_SIMPLE_CARD_BONOBO (bonobo_object (servant));
	priv = simple_card->priv;

	if (priv->card_simple) {
		e_card_simple_set (priv->card_simple,
				   field,
				   value);
	}
}


/* GtkObject methods.  */

static void
impl_dispose (GObject *object)
{
	ESimpleCardBonobo *simple_card;
	ESimpleCardBonoboPrivate *priv;

	simple_card = E_SIMPLE_CARD_BONOBO (object);
	priv = simple_card->priv;

	if (priv->card_simple) {
		g_object_unref (priv->card_simple);
	}

	g_free (priv);

	simple_card->priv = NULL;
}


static void
e_simple_card_bonobo_class_init (ESimpleCardBonoboClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Addressbook_SimpleCard__epv *epv;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_ref (BONOBO_TYPE_OBJECT);

	object_class->dispose = impl_dispose;

	epv                      = &klass->epv;
	epv->getArbitrary        = impl_SimpleCard_get_arbitrary;
	epv->setArbitrary        = impl_SimpleCard_set_arbitrary;
	epv->get                 = impl_SimpleCard_get;
	epv->set                 = impl_SimpleCard_set;
}

static void
e_simple_card_bonobo_init (ESimpleCardBonobo *simple_card)
{
	ESimpleCardBonoboPrivate *priv;

	priv = g_new (ESimpleCardBonoboPrivate, 1);

	priv->card_simple = NULL;

	simple_card->priv = priv;
}


void
e_simple_card_bonobo_construct (ESimpleCardBonobo *simple_card,
				ECardSimple *card_simple)
{
	g_return_if_fail (simple_card != NULL);
	g_return_if_fail (E_IS_SIMPLE_CARD_BONOBO (simple_card));

	simple_card->priv->card_simple = card_simple;
	g_object_ref (card_simple);
}

ESimpleCardBonobo *
e_simple_card_bonobo_new (ECardSimple *card_simple)
{
	ESimpleCardBonobo *simple_card;

	simple_card = g_object_new (E_TYPE_SIMPLE_CARD_BONOBO, NULL);

	e_simple_card_bonobo_construct (simple_card, card_simple);

	return simple_card;
}


BONOBO_TYPE_FUNC_FULL (
		       ESimpleCardBonobo,
		       GNOME_Evolution_Addressbook_SimpleCard,
		       PARENT_TYPE,
		       e_simple_card_bonobo);
