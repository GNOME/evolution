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

static G_CONST_RETURN gchar* ea_minicard_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_minicard_get_description (AtkObject *accessible);

static void ea_minicard_class_init (EaMinicardClass *klass);

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
	GList *list;

	g_return_val_if_fail (EA_IS_MINICARD(accessible), NULL);
	memset (name, '\0', BUFFERSIZE);

	g_string_append (new_str, _("contact's header: "));

	card = E_MINICARD(atk_gobject_accessible_get_object 
			 (ATK_GOBJECT_ACCESSIBLE(accessible)));
	g_object_get (card->header_text, "text", &string, NULL);

	/* get header of current card */
	g_string_append (new_str, string);
	g_free (string);

	/* if there exist no enough space for remain info, return */
	if (new_str->len >= BUFFERSIZE) {
		strncpy (name, new_str->str, BUFFERSIZE);
		return name;
	}

	g_string_append (new_str, " ");

	for ( list = card->fields; list; list = g_list_next( list ) ) {
		gchar *f, *fn;
		EMinicardLabel *label;

		label = E_MINICARD_LABEL (E_MINICARD_FIELD (list->data)->label);

		/* get field name */
		g_object_get (label->fieldname, "text", &fn, NULL);
		g_string_append (new_str, fn);
		g_free (fn);

		if (new_str->len >= BUFFERSIZE) {
			strncpy(name, new_str->str, BUFFERSIZE);
			return name;
		}

		g_string_append (new_str, " ");

		/* get field */
		g_object_get (label->field, "text", &f, NULL);
		g_string_append (new_str, f);
		g_free (f);

		if (new_str->len >= BUFFERSIZE) {
			strncpy (name, new_str->str, BUFFERSIZE);
			return name;
		}

		g_string_append (new_str, " ");
	}

	strcpy (name, new_str->str);
	g_string_free (new_str, TRUE);

	return name;
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

	accessible->role = ATK_ROLE_PANEL;
	return accessible;
}
