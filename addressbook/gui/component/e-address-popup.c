/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-address-popup.c
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
#include "e-address-popup.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-generic-factory.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-unicode.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/contact-editor/e-contact-editor.h>
#include <addressbook/contact-editor/e-contact-quick-add.h>
#include <addressbook/gui/widgets/e-minicard-widget.h>

static GtkObjectClass *parent_class;
static EBook *common_book = NULL; /* still sort of lame */

static void e_address_popup_destroy             (GtkObject *);
static void e_address_popup_query              (EAddressPopup *);


static void
e_address_popup_class_init (EAddressPopupClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	parent_class = GTK_OBJECT_CLASS (gtk_type_class (gtk_event_box_get_type ()));

	object_class->destroy = e_address_popup_destroy;
}

static void
e_address_popup_init (EAddressPopup *pop)
{
	
}

static void
e_address_popup_destroy (GtkObject *obj)
{
	EAddressPopup *pop = E_ADDRESS_POPUP (obj);

	if (pop->card)
		gtk_object_unref (GTK_OBJECT (pop->card));

	if (pop->leave_timeout_tag)
		gtk_timeout_remove (pop->leave_timeout_tag);

	g_free (pop->name);
	g_free (pop->email);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

GtkType
e_address_popup_get_type (void)
{
	static GtkType pop_type = 0;

	if (!pop_type) {
		GtkTypeInfo pop_info = {
			"EAddressPopup",
			sizeof (EAddressPopup),
			sizeof (EAddressPopupClass),
			(GtkClassInitFunc) e_address_popup_class_init,
			(GtkObjectInitFunc) e_address_popup_init,
			NULL, NULL,
			(GtkClassInitFunc) NULL
		};

		pop_type = gtk_type_unique (gtk_event_box_get_type (), &pop_info);
	}

	return pop_type;
}

static void
e_address_popup_refresh_names (EAddressPopup *pop)
{
	if (pop->name_widget) {
		if (pop->name && *pop->name) {
			gchar *s = e_utf8_to_gtk_string (pop->name_widget, pop->name);
			gtk_label_set_text (GTK_LABEL (pop->name_widget), s);
			g_free (s);
			gtk_widget_show (pop->name_widget);
		} else {
			gtk_widget_hide (pop->name_widget);
		}
	}

	if (pop->email_widget) {
		if (pop->email && *pop->email) {
			gchar *s = e_utf8_to_gtk_string (pop->email_widget, pop->email);
			gtk_label_set_text (GTK_LABEL (pop->email_widget), s);
			g_free (s);
			gtk_widget_show (pop->email_widget);
		} else {
			gtk_widget_hide (pop->email_widget);
		}
	}

	e_address_popup_query (pop);
}

void
e_address_popup_set_name (EAddressPopup *pop, const gchar *name)
{
	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	g_free (pop->name);
	pop->name = g_strdup (name);
	g_strstrip (pop->name);

	if (pop->name && pop->email)
		e_address_popup_refresh_names (pop);
}

void
e_address_popup_set_email (EAddressPopup *pop, const gchar *email)
{
	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	g_free (pop->email);
	pop->email = g_strdup (email);
	g_strstrip (pop->email);

	if (pop->name && pop->email)
		e_address_popup_refresh_names (pop);
}

void
e_address_popup_construct (EAddressPopup *pop)
{
	GtkWidget *vbox, *name_holder;
	GdkColor color = { 0x0, 0xffff, 0xffff, 0xffff };

	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	pop->main_vbox = gtk_vbox_new (FALSE, 0);

	/* Build Generic View */

	name_holder = gtk_event_box_new ();
	vbox = gtk_vbox_new (FALSE, 2);
	pop->name_widget = gtk_label_new ("");
	pop->email_widget = gtk_label_new ("");

	gtk_box_pack_start (GTK_BOX (vbox), pop->name_widget, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (vbox), pop->email_widget, TRUE, TRUE, 2);
	gtk_container_add (GTK_CONTAINER (name_holder), GTK_WIDGET (vbox));

	if (gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (name_holder)), &color, FALSE, TRUE)) {
		GtkStyle *style = gtk_style_copy (gtk_widget_get_style (GTK_WIDGET (name_holder)));
		style->bg[0] = color;
		gtk_widget_set_style (GTK_WIDGET (name_holder), style);
	}

	pop->generic_view = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (pop->generic_view), name_holder);
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), pop->generic_view, TRUE, TRUE, 0);
	gtk_widget_show_all (pop->generic_view);

	pop->query_msg = gtk_label_new ("Querying Addressbook...");
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), pop->query_msg, TRUE, TRUE, 0);
	gtk_widget_show (pop->query_msg);

	/* Build Minicard View */
	pop->minicard_view = e_minicard_widget_new ();
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), pop->minicard_view, TRUE, TRUE, 0);


	/* Final assembly */

	gtk_container_add (GTK_CONTAINER (pop), pop->main_vbox);
	gtk_widget_show (pop->main_vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);
	gtk_container_set_border_width (GTK_CONTAINER (pop), 2);
}

GtkWidget *
e_address_popup_new (void)
{
	EAddressPopup *pop = gtk_type_new (E_ADDRESS_POPUP_TYPE);
	e_address_popup_construct (pop);
	return GTK_WIDGET (pop);
}

static void
found_fields_cb (EBook *book, EBookStatus status, EList *writable_fields, gpointer closure)
{
	EAddressPopup *pop = E_ADDRESS_POPUP (closure);
	EContactEditor *ce = e_contact_editor_new (pop->card, FALSE, writable_fields, FALSE);
	e_contact_editor_raise (ce);
	gtk_widget_destroy (GTK_WIDGET (pop));
}

static void
edit_contact_info_cb (EAddressPopup *pop)
{
	e_book_get_supported_fields (common_book, found_fields_cb, pop);
}

static void
e_address_popup_cardify (EAddressPopup *pop, ECard *card)
{
	GtkWidget *b;

	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));
	g_return_if_fail (card && E_IS_CARD (card));
	g_return_if_fail (pop->card == NULL);

	pop->card = card;
	gtk_object_ref (GTK_OBJECT (pop->card));

	e_minicard_widget_set_card (E_MINICARD_WIDGET (pop->minicard_view), card);
	gtk_widget_show (pop->minicard_view);
	gtk_widget_hide (pop->generic_view);

	b = gtk_button_new_with_label ("Edit Contact Info");
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), b, TRUE, TRUE, 0);
	gtk_signal_connect_object (GTK_OBJECT (b),
				   "clicked",
				   GTK_SIGNAL_FUNC (edit_contact_info_cb),
				   GTK_OBJECT (pop));
	gtk_widget_show (b);
}

static void
add_contacts_cb (EAddressPopup *pop)
{
	if (pop->email && *pop->email) {

		if (pop->name && *pop->name)
			e_contact_quick_add (pop->name, pop->email, NULL, NULL);
		else
			e_contact_quick_add_free_form (pop->email, NULL, NULL);

	}

	gtk_widget_destroy (GTK_WIDGET (pop));
}

static void
e_address_popup_no_matches (EAddressPopup *pop)
{
	GtkWidget *b;

	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	b = gtk_button_new_with_label ("Add to Contacts");
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), b, TRUE, TRUE, 0);
	gtk_signal_connect_object (GTK_OBJECT (b),
				   "clicked",
				   GTK_SIGNAL_FUNC (add_contacts_cb),
				   GTK_OBJECT (pop));
	gtk_widget_show (b);
}

static void
e_address_popup_multiple_matches (EAddressPopup *pop)
{
	
}


/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

/*
 *  Addressbook Query Fun
 */

static void
query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	EAddressPopup *pop;

	if (status != E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS)
		return;

	pop = E_ADDRESS_POPUP (closure);

	pop->have_queried = TRUE;
	gtk_widget_hide (pop->query_msg);

	if (cards == NULL) {

		e_address_popup_no_matches (pop);

	} else {
		if (g_list_length ((GList *) cards) == 1)
			e_address_popup_cardify (pop, E_CARD (cards->data));
		else
			e_address_popup_multiple_matches (pop);
	}

	pop->query_tag = 0;
}

static void
start_query (EAddressPopup *pop)
{
	g_assert (common_book != NULL);
	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	if (pop->query_tag)
		e_book_simple_query_cancel (common_book, pop->query_tag);

	pop->query_tag = e_book_name_and_email_query (common_book, pop->name, pop->email, query_cb, pop);
}

static void
loaded_book_cb (EBook *book, EBookStatus status, gpointer closure)
{
	g_return_if_fail (status == E_BOOK_STATUS_SUCCESS);
	g_return_if_fail (book != NULL);

	if (common_book == NULL) {
		common_book = book;
		gtk_object_ref (GTK_OBJECT (common_book));
	}
	
	start_query (E_ADDRESS_POPUP (closure));
}

static void
e_address_popup_query (EAddressPopup *pop)
{
	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	if (common_book == NULL) {
		EBook *book = e_book_new ();
		e_book_load_local_address_book (book, loaded_book_cb, pop);
	} else {
		start_query (pop);
	}
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

enum {
	PROPERTY_NAME,
	PROPERTY_EMAIL
};

static void
set_prop (BonoboPropertyBag *bag, const BonoboArg *arg, guint arg_id, CORBA_Environment *ev, gpointer user_data)
{
	EAddressPopup *pop = E_ADDRESS_POPUP (user_data);

	switch (arg_id) {

	case PROPERTY_NAME:
		e_address_popup_set_name (pop, BONOBO_ARG_GET_STRING (arg));
		break;

	case PROPERTY_EMAIL:
		e_address_popup_set_email (pop, BONOBO_ARG_GET_STRING (arg));
		break;

	default:
		g_assert_not_reached ();
	}
			
}

static BonoboControl *
e_address_popup_factory_new_control (void)
{
        BonoboControl *control;
        BonoboPropertyBag *bag;
	GtkWidget *w;

	w = e_address_popup_new ();
	control = bonobo_control_new (w);
	gtk_widget_show (w);

	gtk_signal_connect_object (GTK_OBJECT (w),
				   "destroy",
				   GTK_SIGNAL_FUNC (bonobo_object_unref),
				   GTK_OBJECT (control));

        bag = bonobo_property_bag_new (NULL, set_prop, w);
        bonobo_property_bag_add (bag, "name", PROPERTY_NAME,
                                 BONOBO_ARG_STRING, NULL, NULL,
                                 BONOBO_PROPERTY_WRITEABLE);

        bonobo_property_bag_add (bag, "email", PROPERTY_EMAIL,
                                 BONOBO_ARG_STRING, NULL, NULL,
                                 BONOBO_PROPERTY_WRITEABLE);

        bonobo_control_set_properties (control, bag);
        bonobo_object_unref (BONOBO_OBJECT (bag));

        return control;
}

static BonoboObject *
e_address_popup_factory (BonoboGenericFactory *factory, gpointer user_data)
{
	return BONOBO_OBJECT (e_address_popup_factory_new_control ());
}

void
e_address_popup_factory_init (void)
{
	static BonoboGenericFactory *factory = NULL;

	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new ("OAFIID:GNOME_Evolution_Addressbook_AddressPopupFactory",
					      e_address_popup_factory, NULL);
	
	if (factory == NULL)
		g_error ("I could not register an AddressPopup factory.");
}
