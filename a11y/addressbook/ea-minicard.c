/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * ea-minicard.c
 * Copyright (C) 2000  Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author:  Leon Zhang <leon.zhang@sun.com> Sun Microsystem Inc., 2003
 */

#include <config.h>
#include <string.h>
#include <libgnome/gnome-i18n.h>
#include "ea-minicard.h"
#include "ea-minicard-view.h"
#include "e-minicard.h"

static const char * action_name[] = {
	N_("Open")
};

static G_CONST_RETURN gchar* ea_minicard_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_minicard_get_description (AtkObject *accessible);

static void ea_minicard_class_init (EaMinicardClass *klass);

static gint ea_minicard_get_n_children (AtkObject *obj);
static AtkObject* ea_minicard_ref_child(AtkObject *obj, gint i);

static AtkStateSet *ea_minicard_ref_state_set (AtkObject *obj);

static void atk_action_interface_init (AtkActionIface *iface);
static gboolean atk_action_interface_do_action (AtkAction *iface, gint i);
static gint atk_action_interface_get_n_action (AtkAction *iface);
static G_CONST_RETURN gchar* atk_action_interface_get_description (AtkAction *iface, gint i);
static G_CONST_RETURN gchar* atk_action_interface_get_name (AtkAction *iface, gint i);

static gpointer parent_class = NULL;

GType
ea_minicard_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static  GTypeInfo tinfo =  {
			sizeof (EaMinicardClass),
			(GBaseInitFunc) NULL,  /* base_init */
			(GBaseFinalizeFunc) NULL,  /* base_finalize */
			(GClassInitFunc) ea_minicard_class_init,
			(GClassFinalizeFunc) NULL, /* class_finalize */
			NULL,	/* class_data */
			sizeof (EaMinicard),
			0,	/* n_preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL	/* value table */
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (GailWidget, in this case) 
		 */

		factory = atk_registry_get_factory (atk_get_default_registry (),
						    GNOME_TYPE_CANVAS_GROUP);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static ( derived_atk_type,
						"EaMinicard", &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_ACTION,
					     &atk_action_info);	
	}

	return type;
}

static void
ea_minicard_class_init (EaMinicardClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_minicard_get_name;
	class->get_description = ea_minicard_get_description;
	class->ref_state_set = ea_minicard_ref_state_set;
	class->get_n_children = ea_minicard_get_n_children;
	class->ref_child = ea_minicard_ref_child;
}

/*
 *  we access the main content of current minicard, including 
 *  header text, label(field, field name)
 */
static G_CONST_RETURN gchar*
ea_minicard_get_name (AtkObject *accessible)
{
#define BUFFERSIZE 500
	
	static gchar name[BUFFERSIZE];
	GString *new_str = g_string_new (NULL);
	gchar *string;
	EMinicard *card;

	g_return_val_if_fail (EA_IS_MINICARD(accessible), NULL);
	memset (name, '\0', BUFFERSIZE);

	card = E_MINICARD(atk_gobject_accessible_get_object 
			 (ATK_GOBJECT_ACCESSIBLE(accessible)));
	g_object_get (card->header_text, "text", &string, NULL);

	if (e_contact_get (card->contact, E_CONTACT_IS_LIST))
		g_string_append (new_str, _("Contact List: "));
	else    g_string_append (new_str, _("Contact: "));
	
	/* get header of current card */
	g_string_append (new_str, string);
	g_free (string);

	/* if there exist no enough space for remain info, return */
	if (new_str->len >= BUFFERSIZE) {
		strncpy (name, new_str->str, BUFFERSIZE);
		return name;
	}

	strcpy (name, new_str->str);
	g_string_free (new_str, TRUE);

	ATK_OBJECT_CLASS (parent_class)->set_name (accessible, name);

	return accessible->name;
}

static G_CONST_RETURN gchar*
ea_minicard_get_description (AtkObject *accessible)
{
	if (accessible->description)
		return accessible->description;

	return _("evolution minicard");
}

AtkObject* 
ea_minicard_new (GObject *obj)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail(obj != NULL, NULL);
	g_return_val_if_fail (E_IS_MINICARD(obj), NULL);

	object = g_object_new (EA_TYPE_MINICARD, NULL);
	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, obj);

	accessible->role = ATK_ROLE_UNKNOWN;
	return accessible;
}

static AtkStateSet *ea_minicard_ref_state_set (AtkObject *obj)
{
	AtkStateSet *state_set = NULL;
	GObject *gobj = NULL;

	state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (obj);
	if( !state_set )
		return NULL;
	gobj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj));
	if( !gobj )
		return NULL;

	atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);
	atk_state_set_add_state (state_set, ATK_STATE_ENABLED);
	atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (state_set, ATK_STATE_SHOWING);

	return state_set;
}

static gint
ea_minicard_get_n_children (AtkObject *accessible)
{
	return 0;
}

static AtkObject *
ea_minicard_ref_child (AtkObject *accessible, gint index)
{
	return NULL;
}

static void atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->do_action = atk_action_interface_do_action;
	iface->get_n_actions = atk_action_interface_get_n_action;
	iface->get_description = atk_action_interface_get_description;
	iface->get_name = atk_action_interface_get_name;
}

static gboolean atk_action_interface_do_action (AtkAction *iface, gint i)
{
	EMinicard *minicard = NULL;

	minicard = E_MINICARD (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (iface)));
	if( minicard == NULL )
		return FALSE;

	if( i >= G_N_ELEMENTS (action_name) || i < 0 )
		return FALSE;

	switch (i) {
		// open card
		case 0:
			e_minicard_activate_editor (minicard);
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

static gint atk_action_interface_get_n_action (AtkAction *iface)
{
	return G_N_ELEMENTS (action_name);
}

static G_CONST_RETURN gchar*
atk_action_interface_get_description (AtkAction *iface, gint i)
{
	return atk_action_interface_get_name (iface, i);
}

static G_CONST_RETURN gchar*
atk_action_interface_get_name (AtkAction *iface, gint i)
{
	if( i >= G_N_ELEMENTS (action_name) || i < 0)
		return NULL;
	
	return action_name[i];
}
		
