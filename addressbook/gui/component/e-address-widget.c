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
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
#include <ctype.h>
#include <string.h>
#include <gtk/gtklabel.h>
#include <libgnomeui/gnome-popup-menu.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-generic-factory.h>
#include <addressbook/gui/contact-editor/e-contact-quick-add.h>
#include "e-address-widget.h"

static void e_address_widget_class_init (EAddressWidgetClass *klass);
static void e_address_widget_init       (EAddressWidget *obj);
static void e_address_widget_destroy    (GtkObject *obj);

static gint e_address_widget_button_press_handler (GtkWidget *w, GdkEventButton *ev);
static void e_address_widget_popup (EAddressWidget *, GdkEventButton *ev);
static void e_address_widget_schedule_query (EAddressWidget *);

static GtkObjectClass *parent_class;

static EBook *common_book = NULL; /* sort of lame */

static gboolean doing_queries = FALSE;

static void
e_address_widget_class_init (EAddressWidgetClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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
	addr->name = NULL;

	g_free (addr->email);
	addr->email = NULL;

	if (addr->query_tag) {
		e_book_simple_query_cancel (common_book, addr->query_tag);
		addr->query_tag = 0;
	}

	if (addr->query_idle_tag) {
		g_source_remove (addr->query_idle_tag);
		addr->query_idle_tag = 0;
	}

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (obj);
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

GType
e_address_widget_get_type (void)
{
	static GType aw_type = 0;

	if (!aw_type) {
		static const GTypeInfo aw_info =  {
			sizeof (EAddressWidgetClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_address_widget_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressWidget),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_address_widget_init,
		};

		aw_type = g_type_register_static (gtk_event_box_get_type (), "EAddressWidget", &aw_info, 0);
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
	have_email = addr->email && *addr->email && (addr->card == NULL || !addr->known_email);

	gtk_label_set_text (GTK_LABEL (addr->name_widget), have_name ? addr->name : "");
	gtk_widget_visible (addr->name_widget, have_name);
	if (addr->card) {
		gint i, N = strlen (addr->name);
		gchar *pattern = g_malloc (N+1);
		for (i=0; i<N; ++i)
			pattern[i] = '_';
		pattern[i] = '\0';
		gtk_label_set_pattern (GTK_LABEL (addr->name_widget), pattern);
		g_free (pattern);
	} else {
		gtk_label_set_pattern (GTK_LABEL (addr->name_widget), "");
	}

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
	if (addr->card == NULL) 
		e_address_widget_schedule_query (addr);
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
	EAddressWidget *addr = g_object_new (E_TYPE_ADDRESS_WIDGET, NULL);
	e_address_widget_construct (addr);
	return GTK_WIDGET (addr);
}

/*
 *
 * Cardification
 *
 */

static void
e_address_widget_cardify (EAddressWidget *addr, ECard *card, gboolean known_email)
{
	if (addr->card != card || addr->known_email != known_email) {

		if (addr->card != card) {
			if (addr->card)
				g_object_unref (addr->card);
			addr->card = card;
			g_object_ref (addr->card);
		}

		addr->known_email = known_email;

		if (!(addr->name && *addr->name)) {
			gchar *s = e_card_name_to_string (card->name);
			e_address_widget_set_name (addr, s);
			g_free (s);
		}

		e_address_widget_refresh (addr);
	}
}

static void
query_results_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer user_data)
{
	EAddressWidget *addr = user_data;

	if (g_list_length ((GList *) cards) == 1) {
		ECard *card = E_CARD (cards->data);
		e_address_widget_cardify (addr, card, TRUE);
	}

	addr->query_tag = 0;
}

static void
e_address_widget_do_query (EAddressWidget *addr)
{
	e_book_name_and_email_query (common_book, addr->name, addr->email, query_results_cb, addr);
}

static void
book_ready_cb (EBook *book, EBookStatus status, gpointer user_data)
{
	EAddressWidget *addr = E_ADDRESS_WIDGET (user_data);

	if (common_book == NULL) {
		common_book = book;
		g_object_ref (common_book);
	} else
		g_object_unref (book);

	e_address_widget_do_query (addr);
}

static gint
query_idle_fn (gpointer ptr)
{
	EAddressWidget *addr = E_ADDRESS_WIDGET (ptr);

	if (common_book) {
		e_address_widget_do_query (addr);
	} else {
		e_book_load_default_book (e_book_new (), book_ready_cb, addr);
	}

	addr->query_idle_tag = 0;
	return FALSE;
}

static void
e_address_widget_schedule_query (EAddressWidget *addr)
{
	if (addr->query_idle_tag || !doing_queries)
		return;
	addr->query_idle_tag = g_idle_add (query_idle_fn, addr);
}

/*
 *
 * Popup Menu
 *
 */

#define ARBITRARY_UIINFO_LIMIT 64

static gint
popup_add_name_and_address (EAddressWidget *addr, GnomeUIInfo *uiinfo, gint i)
{
	gboolean flag = FALSE;

	if (addr->name && *addr->name) {
		uiinfo[i].type = GNOME_APP_UI_ITEM;
		uiinfo[i].label = addr->name;
		++i;
		flag = TRUE;
	}

	if (addr->email && *addr->email) {
		uiinfo[i].type = GNOME_APP_UI_ITEM;
		uiinfo[i].label = addr->email;
		++i;
		flag = TRUE;
	}

	if (flag) {
		uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
		++i;
	}
	
	return i;
}

static void
flip_queries_flag_cb (GtkWidget *w, gpointer user_data)
{
	doing_queries = !doing_queries;
}

static gint
popup_add_query_change (EAddressWidget *addr, GnomeUIInfo *uiinfo, gint i)
{
	uiinfo[i].type = GNOME_APP_UI_SEPARATOR;
	++i;

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = doing_queries ? _("Disable Queries") : _("Enable Queries (Dangerous!)");
	uiinfo[i].moreinfo = flip_queries_flag_cb;
	++i;

	return i;
}


static GtkWidget *
popup_menu_card (EAddressWidget *addr)
{
	GnomeUIInfo uiinfo[ARBITRARY_UIINFO_LIMIT];
	GtkWidget *pop;
	gint i=0;
	ECard *card = E_CARD (addr->card);

	g_return_val_if_fail (card != NULL, NULL);

	memset (uiinfo, 0, sizeof (uiinfo));

	i = popup_add_name_and_address (addr, uiinfo, i);

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = _("Edit Contact Info");
	++i;

	i = popup_add_query_change (addr, uiinfo, i);

	uiinfo[i].type = GNOME_APP_UI_ENDOFINFO;
	pop = gnome_popup_menu_new (uiinfo);
	return pop;
}

static void
post_quick_add_cb (ECard *card, gpointer user_data)
{
	e_address_widget_cardify (E_ADDRESS_WIDGET (user_data), card, TRUE);
}

static void
add_contacts_cb (GtkWidget *w, gpointer user_data)
{
	EAddressWidget *addr = E_ADDRESS_WIDGET (user_data);

	e_contact_quick_add (addr->name, addr->email, post_quick_add_cb, addr);
}

static GtkWidget *
popup_menu_nocard (EAddressWidget *addr)
{
	GnomeUIInfo uiinfo[ARBITRARY_UIINFO_LIMIT];
	GtkWidget *pop;
	gint i=0;

	memset (uiinfo, 0, sizeof (uiinfo));

	i = popup_add_name_and_address (addr, uiinfo, i);

	uiinfo[i].type = GNOME_APP_UI_ITEM;
	uiinfo[i].label = _("Add to Contacts");
	uiinfo[i].moreinfo = add_contacts_cb;
	++i;

	i = popup_add_query_change (addr, uiinfo, i);

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
		gnome_popup_menu_do_popup (pop, NULL, NULL, ev, addr, GTK_WIDGET (addr));
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

BonoboControl *
e_address_widget_new_control (void)
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

	bonobo_control_set_properties (control, bonobo_object_corba_objref (BONOBO_OBJECT (bag)), NULL);
	bonobo_object_unref (BONOBO_OBJECT (bag));

	return control;
}
