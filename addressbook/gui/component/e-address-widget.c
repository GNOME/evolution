/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-address-widget.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
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
 * USA.
 */

#include <config.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-generic-factory.h>
#include <addressbook/contact-editor/e-contact-quick-add.h>
#include "e-address-widget.h"

static void e_address_widget_class_init (EAddressWidgetClass *klass);
static void e_address_widget_init       (EAddressWidget *obj);
static void e_address_widget_destroy    (GtkObject *obj);

static gint e_address_widget_button_press_handler (GtkWidget *w, GdkEventButton *ev);
static void e_address_widget_popup (EAddressWidget *, GdkEventButton *ev);

static GtkObjectClass *parent_class;

static void
e_address_widget_class_init (EAddressWidgetClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = GTK_OBJECT_CLASS (gtk_type_class (gtk_event_box_get_type ()));

	object_class->destroy = e_address_widget_destroy;

	widget_class->button_press_event = e_address_widget_button_press_handler;
}

static void
e_address_widget_init (EAddressWidget *addr)
{

}

static void
e_address_widget_destroy (GtkObject *obj)
{
	EAddressWidget *addr = E_ADDRESS_WIDGET (obj);

	g_free (addr->name);
	g_free (addr->email);
	if (addr->card)
		gtk_object_unref (GTK_OBJECT (addr->card));
}

static gint
e_address_widget_button_press_handler (GtkWidget *w, GdkEventButton *ev)
{
	EAddressWidget *addr = E_ADDRESS_WIDGET (w);
	if (ev->button == 3 && ev->state == 0) {
		e_address_widget_popup (addr, ev);
		return TRUE;
	}

	return FALSE;
}

GtkType
e_address_widget_get_type (void)
{
	static GtkType aw_type = 0;

	if (!aw_type) {
		GtkTypeInfo aw_info = {
			"EAddressWidget",
			sizeof (EAddressWidget),
			sizeof (EAddressWidgetClass),
			(GtkClassInitFunc) e_address_widget_class_init,
			(GtkObjectInitFunc) e_address_widget_init,
			NULL, NULL, /* reserved... but for what sinister purpose? */
			(GtkClassInitFunc) NULL
		};

		aw_type = gtk_type_unique (gtk_event_box_get_type (), &aw_info);
	}

	return aw_type;
}

static void
gtk_widget_visible (GtkWidget *w, gboolean x)
{
	if (x)
		gtk_widget_show (w);
	else
		gtk_widget_hide (w);
}

static void
e_address_widget_refresh (EAddressWidget *addr)
{
	gchar *str;
	gboolean have_name, have_email;

	g_return_if_fail (addr && E_IS_ADDRESS_WIDGET (addr));

	have_name = addr->name && *addr->name;
	have_email = addr->email && *addr->email;

	gtk_label_set_text (GTK_LABEL (addr->name_widget), have_name ? addr->name : "");
	gtk_widget_visible (addr->name_widget, have_name);

	if (have_email) {
		str = g_strdup_printf (have_name ? "<%s>" : "%s", addr->email);
		gtk_label_set_text (GTK_LABEL (addr->email_widget), str);
		g_free (str);
	} else {
		gtk_label_set_text (GTK_LABEL (addr->email_widget), "");
	}
	gtk_widget_visible (addr->email_widget, have_email);

	gtk_widget_visible (addr->spacer, have_name && have_email);

	/* Launch a query to find the appropriate card, if necessary. */
	addr->querying = TRUE;
}

void
e_address_widget_set_name (EAddressWidget *addr, const gchar *name)
{
	g_return_if_fail (addr && E_IS_ADDRESS_WIDGET (addr));

	g_free (addr->name);
	addr->name = g_strdup (name);

	e_address_widget_refresh (addr);
}

void
e_address_widget_set_email (EAddressWidget *addr, const gchar *email)
{
	g_return_if_fail (addr && E_IS_ADDRESS_WIDGET (addr));

	g_free (addr->email);
	addr->email = g_strdup (email);

	e_address_widget_refresh (addr);
}


void
e_address_widget_set_text (EAddressWidget *addr, const gchar *text)
{
	g_return_if_fail (addr && E_IS_ADDRESS_WIDGET (addr));
	
	e_address_widget_set_email (addr, text); /* CRAP */
}

void
e_address_widget_construct (EAddressWidget *addr)
{
	GtkWidget *box;

	g_return_if_fail (addr && E_IS_ADDRESS_WIDGET (addr));

	box = gtk_hbox_new (FALSE, 2);
	
	addr->name_widget = gtk_label_new ("");
	addr->spacer = gtk_label_new (" ");
	addr->email_widget = gtk_label_new ("");
	
	gtk_box_pack_start (GTK_BOX (box), addr->name_widget, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), addr->spacer, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), addr->email_widget, FALSE, FALSE, 0);


	gtk_container_add (GTK_CONTAINER (addr), box);

	gtk_widget_show (box);
	gtk_widget_show (addr->name_widget);
	gtk_widget_show (addr->email_widget);
}

GtkWidget *
e_address_widget_new (void)
{
	EAddressWidget *addr = gtk_type_new (e_address_widget_get_type ());
	e_address_widget_construct (addr);
	return GTK_WIDGET (addr);
}

/*
 *
 * Popup Menu
 *
 */

#define ARBITRARY_UIINFO_LIMIT 64
static GtkWidget *
popup_menu_card (EAddressWidget *addr)
{
	ECard *card = E_CARD (addr->card);
	g_return_val_if_fail (card != NULL, NULL);

	return NULL;
}

static void
add_contacts_cb (GtkWidget *w, gpointer user_data)
{
	EAddressWidget *addr = E_ADDRESS_WIDGET (user_data);

	e_contact_quick_add (addr->name, addr->email, NULL, NULL);
}

static GtkWidget *
popup_menu_nocard (EAddressWidget *addr)
{
	GnomeUIInfo uiinfo[ARBITRARY_UIINFO_LIMIT];
	GtkWidget *pop;
	gint i=0;

	memset (uiinfo, 0, sizeof (uiinfo));

	if (addr->name) {
		uiinfo[i].type = GNOME_APP_UI_ITEM;
		uiinfo[i].label = addr->name;
		++i;
	}

	if (addr->email) {
		uiinfo[i].type = GNOME_APP_UI_ITEM;
		uiinfo[i].label = addr->email;
		++i;
	}
	dead = i;

	uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
	++i;

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = N_("Add to Contacts");
	uiinfo[i].moreinfo = add_contacts_cb;
	++i;

	uiinfo[i].type = GNOME_APP_UI_ENDOFINFO;

	pop = gnome_popup_menu_new (uiinfo);

	return pop;
}

static void
e_address_widget_popup (EAddressWidget *addr, GdkEventButton *ev)
{
	GtkWidget *pop;

	g_return_if_fail (addr && E_IS_ADDRESS_WIDGET (addr));

	pop = addr->card ? popup_menu_card (addr) : popup_menu_nocard (addr);

	if (pop)
		gnome_popup_menu_do_popup (pop, NULL, NULL, ev, addr);
}

/*
 *
 * Bonobo Control Magic
 *
 */

enum {
	ADDRESS_PROPERTY_NAME,
	ADDRESS_PROPERTY_EMAIL,
	ADDRESS_PROPERTY_TEXT,
	ADDRESS_PROPERTY_BACKGROUND_RGB
};


static void
get_prop (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id, CORBA_Environment *ev, gpointer user_data)
{
	EAddressWidget *addr = E_ADDRESS_WIDGET (user_data);

	switch (arg_id) {

	case ADDRESS_PROPERTY_NAME:
		BONOBO_ARG_SET_STRING (arg, addr->name ? addr->name :"");
		break;

	case ADDRESS_PROPERTY_EMAIL:
		BONOBO_ARG_SET_STRING (arg, addr->email ? addr->email : "");
		break;

	case ADDRESS_PROPERTY_TEXT:
		BONOBO_ARG_SET_STRING (arg, "?");
		break;
	}
}

static void
set_prop (BonoboPropertyBag *bag, const BonoboArg *arg, guint arg_id, CORBA_Environment *ev, gpointer user_data)
{
	EAddressWidget *addr = E_ADDRESS_WIDGET (user_data);

	switch (arg_id) {
	case ADDRESS_PROPERTY_NAME:
		e_address_widget_set_name (addr, BONOBO_ARG_GET_STRING (arg));
		break;

	case ADDRESS_PROPERTY_EMAIL:
		e_address_widget_set_email (addr, BONOBO_ARG_GET_STRING (arg));
		break;

	case ADDRESS_PROPERTY_TEXT:
		e_address_widget_set_text (addr, BONOBO_ARG_GET_STRING (arg));
		break;

		
	case ADDRESS_PROPERTY_BACKGROUND_RGB:
		{
			gint bg = BONOBO_ARG_GET_INT (arg);
			GdkColor color;

			color.red   = (bg & 0xff0000) >> 8;
			color.green = (bg & 0x00ff00);
			color.blue  = (bg & 0x0000ff) << 8;

			if (gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (addr)), &color, FALSE, TRUE)) {
				GtkStyle *style = gtk_style_copy (gtk_widget_get_style (GTK_WIDGET (addr)));
				style->bg[0] = color;
				gtk_widget_set_style (GTK_WIDGET (addr), style);
			}
		}
		
		break;
	}
}

static BonoboControl *
e_address_widget_factory_new_control (void)
{
	BonoboControl *control;
	BonoboPropertyBag *bag;
	GtkWidget *w;

	w = e_address_widget_new ();
	gtk_widget_show (w);

	control = bonobo_control_new (w);

	bag = bonobo_property_bag_new (get_prop, set_prop, w);
	bonobo_property_bag_add (bag, "name", ADDRESS_PROPERTY_NAME,
				 BONOBO_ARG_STRING, NULL, NULL,
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (bag, "email", ADDRESS_PROPERTY_EMAIL,
				 BONOBO_ARG_STRING, NULL, NULL,
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (bag, "text", ADDRESS_PROPERTY_TEXT,
				 BONOBO_ARG_STRING, NULL, NULL,
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (bag, "background_rgb", ADDRESS_PROPERTY_BACKGROUND_RGB,
				 BONOBO_ARG_INT, NULL, NULL,
				 BONOBO_PROPERTY_WRITEABLE);

	bonobo_control_set_properties (control, bag);
	bonobo_object_unref (BONOBO_OBJECT (bag));

	return control;
}

static BonoboObject *
e_address_widget_factory (BonoboGenericFactory *factory, gpointer user_data)
{
	return BONOBO_OBJECT (e_address_widget_factory_new_control ());
}

void
e_address_widget_factory_init (void)
{
	static BonoboGenericFactory *factory = NULL;

	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new ("OAFIID:GNOME_Evolution_Addressbook_AddressWidgetFactory",
					      e_address_widget_factory, NULL);

	if (factory == NULL)
		g_error ("I could not register an AddressWidget factory.");
}
