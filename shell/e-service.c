/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-service.c: Abstract class for Evolution services
 *
 * Author:
 *   Bertrand Guiheneuf (bg@aful.org)
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

/* 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */




#include <config.h>
#include <libgnome/libgnome.h>
#include "e-util/e-util.h"
#include "e-service.h"

#define PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *parent_class;

#define EFC(o) E_SERVICE_CLASS (GTK_OBJECT (o)->klass)



static void
e_service_destroy (GtkObject *object)
{
	EService *eservice = E_SERVICE (object);
	
	if (eservice->uri)
		g_free (eservice->uri);

	if (eservice->desc)
		g_free (eservice->desc);

	if (eservice->home_page)
		g_free (eservice->home_page);
	
	parent_class->destroy (object);
}

static void
e_service_class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = e_service_destroy;

	
}

static void
e_service_init (GtkObject *object)
{
}

E_MAKE_TYPE (e_service, "EService", EService, e_service_class_init, e_service_init, PARENT_TYPE)


EFolder *
e_service_get_root_efolder   (EService *eservice)
{
	g_return_val_if_fail (eservice != NULL, NULL);
	g_return_val_if_fail (E_IS_SERVICE (eservice), NULL);
	
	return eservice->root_efolder;
}



void
e_service_set_uri (EService *eservice, const char *uri)
{
	g_return_if_fail (eservice != NULL);
	g_return_if_fail (E_IS_SERVICE (eservice));
	g_return_if_fail (uri != NULL);

	if (eservice->uri)
		g_free (eservice->uri);
	
	eservice->uri = g_strdup (uri);
}

const char *
e_service_get_uri (EService *eservice)
{
	g_return_val_if_fail (eservice != NULL, NULL);
	g_return_val_if_fail (E_IS_SERVICE (eservice), NULL);

	return eservice->uri;
}

void
e_service_set_description (EService *eservice, const char *desc)
{
	g_return_if_fail (eservice != NULL);
	g_return_if_fail (E_IS_SERVICE (eservice));
	g_return_if_fail (desc != NULL);

	if (eservice->desc)
		g_free (eservice->desc);
	
	eservice->desc = g_strdup (desc);
}

const char *
e_service_get_description (EService *eservice)
{
	g_return_val_if_fail (eservice != NULL, NULL);
	g_return_val_if_fail (E_IS_SERVICE (eservice), NULL);

	return eservice->desc;
}

void
e_service_set_home_page (EService *eservice, const char *home_page)
{
	g_return_if_fail (eservice != NULL);
	g_return_if_fail (E_IS_SERVICE (eservice));
	g_return_if_fail (home_page != NULL);

	if (eservice->home_page)
		g_free (eservice->home_page);
	
	eservice->home_page = g_strdup (home_page);
}

const char *
e_service_get_home_page   (EService *eservice)
{
	g_return_val_if_fail (eservice != NULL, NULL);
	g_return_val_if_fail (E_IS_SERVICE (eservice), NULL);

	return eservice->home_page;
}

const char *
e_service_get_type_name (EService *eservice)
{
	g_return_val_if_fail (eservice != NULL, NULL);
	g_return_val_if_fail (E_IS_SERVICE (eservice), NULL);

	switch (eservice->type){
	case E_SERVICE_MAIL:
		return _("A service containing mail items");

	case E_SERVICE_CONTACTS:
		return _("A service containing contacts");
		
	case E_SERVICE_CALENDAR:
		return _("A service containing calendar entries");
		
	case E_SERVICE_TASKS:
		return _("A service containing tasks");

	default:
		g_assert_not_reached ();
	}

	return NULL;
}

void
e_service_construct (EService *eservice, EServiceType type,
		    const char *uri, const char *name,
		    const char *desc, const char *home_page)
{
	g_return_if_fail (eservice != NULL);
	g_return_if_fail (E_IS_SERVICE (eservice));

	
	/* EServices are self-owned */
	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (eservice), GTK_FLOATING);

	if (uri)
		eservice->uri = g_strdup (uri);
	if (name)
		eservice->name = g_strdup (name);
	if (desc)
		eservice->desc = g_strdup (desc);
	if (home_page)
		eservice->home_page = g_strdup (home_page);

	eservice->type = type;
}

EService *
e_service_new (EServiceType type,
	      const char *uri, const char *name,
	      const char *desc, const char *home_page)
{
	EService *eservice;
	
	eservice = gtk_type_new (e_service_get_type ());

	e_service_construct (eservice, type, uri, name, desc, home_page);
	return eservice;
}

const char *
e_service_get_name (EService *eservice)
{
	g_return_val_if_fail (eservice != NULL, NULL);
	g_return_val_if_fail (E_IS_SERVICE (eservice), NULL);

	return eservice->name;
}

void
e_service_set_name (EService *eservice, const char *name)
{
	g_return_if_fail (eservice != NULL);
	g_return_if_fail (E_IS_SERVICE (eservice));

	if (eservice->name)
		g_free (eservice->name);

	eservice->name = g_strdup (name);
}


