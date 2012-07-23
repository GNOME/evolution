<<<<<<< HEAD
/*
 * e-emoticon-chooser.c
 *
 * Copyright (C) 2008 Novell, Inc.
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
=======
/* e-emoticon-chooser.c
>>>>>>> Import GtkhtmlFace* classes as EEmoticon*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-emoticon-chooser.h"

#include <glib/gi18n-lib.h>

/* Constant version of EEMoticon. */
typedef struct {
	const gchar *label;
	const gchar *icon_name;
	const gchar *text_face;
} ConstantEmoticon;

static ConstantEmoticon available_emoticons[] = {
	/* Translators: :-) */
	{ N_("_Smile"),		"face-smile",		":-)"	},
	/* Translators: :-( */
	{ N_("S_ad"),		"face-sad",		":-("	},
	/* Translators: ;-) */
	{ N_("_Wink"),		"face-wink",		";-)"	},
	/* Translators: :-P */
	{ N_("Ton_gue"),	"face-raspberry",	":-P"	},
	/* Translators: :-)) */
	{ N_("Laug_h"),		"face-laugh",		":-))"	},
	/* Translators: :-| */
	{ N_("_Plain"),		"face-plain",		":-|"	},
	/* Translators: :-! */
	{ N_("Smi_rk"),		"face-smirk",		":-!"	},
	/* Translators: :"-) */
	{ N_("_Embarrassed"),	"face-embarrassed",	":\"-)"	},
	/* Translators: :-D */
	{ N_("_Big Smile"),	"face-smile-big",	":-D"	},
	/* Translators: :-/ */
	{ N_("Uncer_tain"),	"face-uncertain",	":-/"	},
	/* Translators: :-O */
	{ N_("S_urprise"),	"face-surprise",	":-O"	},
	/* Translators: :-S */
	{ N_("W_orried"),	"face-worried",		":-S"	},
	/* Translators: :-* */
	{ N_("_Kiss"),		"face-kiss",		":-*"	},
	/* Translators: X-( */
	{ N_("A_ngry"),		"face-angry",		"X-("	},
	/* Translators: B-) */
	{ N_("_Cool"),		"face-cool",		"B-)"	},
	/* Translators: O:-) */
	{ N_("Ange_l"),		"face-angel",		"O:-)"	},
	/* Translators: :'( */
	{ N_("Cr_ying"),	"face-crying",		":'("	},
	/* Translators: :-Q */
	{ N_("S_ick"),		"face-sick",		":-Q"	},
	/* Translators: |-) */
	{ N_("Tire_d"),		"face-tired",		"|-)"	},
	/* Translators: >:-) */
	{ N_("De_vilish"),	"face-devilish",	">:-)"	},
	/* Translators: :-(|) */
	{ N_("_Monkey"),	"face-monkey",		":-(|)"	}
};

enum {
	ITEM_ACTIVATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

<<<<<<< HEAD
G_DEFINE_INTERFACE (
	EEmoticonChooser,
	e_emoticon_chooser,
	G_TYPE_OBJECT)

static void
e_emoticon_chooser_default_init (EEmoticonChooserInterface *interface)
{
	g_object_interface_install_property (
		interface,
=======
static void
emoticon_chooser_class_init (EEmoticonChooserIface *iface)
{
	g_object_interface_install_property (
		iface,
>>>>>>> Import GtkhtmlFace* classes as EEmoticon*
		g_param_spec_boxed (
			"current-emoticon",
			"Current Emoticon",
			"Currently selected emoticon",
			E_TYPE_EMOTICON,
			G_PARAM_READWRITE));

	signals[ITEM_ACTIVATED] = g_signal_new (
		"item-activated",
<<<<<<< HEAD
		G_TYPE_FROM_INTERFACE (interface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EEmoticonChooserInterface, item_activated),
=======
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EEmoticonChooserIface, item_activated),
>>>>>>> Import GtkhtmlFace* classes as EEmoticon*
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

<<<<<<< HEAD
EEmoticon *
e_emoticon_chooser_get_current_emoticon (EEmoticonChooser *chooser)
{
	EEmoticonChooserInterface *interface;

	g_return_val_if_fail (E_IS_EMOTICON_CHOOSER (chooser), NULL);

	interface = E_EMOTICON_CHOOSER_GET_INTERFACE (chooser);
	g_return_val_if_fail (interface->get_current_emoticon != NULL, NULL);

	return interface->get_current_emoticon (chooser);
=======
GType
e_emoticon_chooser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EEmoticonChooserIface),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) emoticon_chooser_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			0,     /* instance_size */
			0,     /* n_preallocs */
			NULL,  /* instance_init */
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_INTERFACE, "EEmoticonChooser", &type_info, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

EEmoticon *
e_emoticon_chooser_get_current_emoticon (EEmoticonChooser *chooser)
{
	EEmoticonChooserIface *iface;

	g_return_val_if_fail (E_IS_EMOTICON_CHOOSER (chooser), NULL);

	iface = E_EMOTICON_CHOOSER_GET_IFACE (chooser);
	g_return_val_if_fail (iface->get_current_emoticon != NULL, NULL);

	return iface->get_current_emoticon (chooser);
>>>>>>> Import GtkhtmlFace* classes as EEmoticon*
}

void
e_emoticon_chooser_set_current_emoticon (EEmoticonChooser *chooser,
<<<<<<< HEAD
                                         EEmoticon *emoticon)
{
	EEmoticonChooserInterface *interface;

	g_return_if_fail (E_IS_EMOTICON_CHOOSER (chooser));

	interface = E_EMOTICON_CHOOSER_GET_INTERFACE (chooser);
	g_return_if_fail (interface->set_current_emoticon != NULL);

	interface->set_current_emoticon (chooser, emoticon);
=======
					 EEmoticon *emoticon)
{
	EEmoticonChooserIface *iface;

	g_return_if_fail (E_IS_EMOTICON_CHOOSER (chooser));

	iface = E_EMOTICON_CHOOSER_GET_IFACE (chooser);
	g_return_if_fail (iface->set_current_emoticon != NULL);

	iface->set_current_emoticon (chooser, emoticon);
>>>>>>> Import GtkhtmlFace* classes as EEmoticon*
}

void
e_emoticon_chooser_item_activated (EEmoticonChooser *chooser)
{
	g_return_if_fail (E_IS_EMOTICON_CHOOSER (chooser));

	g_signal_emit (chooser, signals[ITEM_ACTIVATED], 0);
}

GList *
<<<<<<< HEAD
e_emoticon_chooser_get_items (void)
=======
e_emoticon_chooser_get_items (EEmoticonChooser *chooser)
>>>>>>> Import GtkhtmlFace* classes as EEmoticon*
{
	GList *list = NULL;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (available_emoticons); ii++)
		list = g_list_prepend (list, &available_emoticons[ii]);

	return g_list_reverse (list);
}
<<<<<<< HEAD

const EEmoticon *
e_emoticon_chooser_lookup_emoticon (const gchar *icon_name)
{
	gint ii;

	g_return_val_if_fail (icon_name && *icon_name, NULL);

	for (ii = 0; ii < G_N_ELEMENTS (available_emoticons); ii++) {
		if (strcmp (available_emoticons[ii].icon_name, icon_name) == 0) {
			return (const EEmoticon *) &available_emoticons[ii];
		}
	}

	return NULL;
}

=======
>>>>>>> Import GtkhtmlFace* classes as EEmoticon*
