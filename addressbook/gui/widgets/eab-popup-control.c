/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * eab-popup-control.c
 *
 * Copyright (C) 2001-2003, Ximian, Inc.
 *
 * Authors: Jon Trowbridge <trow@ximian.com>
 *          Chris Toshok <toshok@ximian.com>
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

/*
 * This file is too big and this widget is too complicated.  Forgive me.
 */

#include <config.h>
#include <string.h>
#include "addressbook.h"
#include "eab-popup-control.h"
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkvbox.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-generic-factory.h>
#include <addressbook/util/eab-book-util.h>
#include <addressbook/gui/contact-editor/e-contact-editor.h>
#include <addressbook/gui/contact-editor/e-contact-quick-add.h>
#include <addressbook/gui/widgets/eab-contact-display.h>
#include <addressbook/gui/widgets/eab-gui-util.h>
#include "e-util/e-gui-utils.h"

static void eab_popup_control_set_name (EABPopupControl *pop, const gchar *name);
static void eab_popup_control_set_email (EABPopupControl *pop, const gchar *email);

static GtkObjectClass *parent_class;

static void eab_popup_control_dispose (GObject *);
static void eab_popup_control_query   (EABPopupControl *);


static void
eab_popup_control_class_init (EABPopupControlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = eab_popup_control_dispose;
}

static void
eab_popup_control_init (EABPopupControl *pop)
{
	pop->transitory = TRUE;
}

static void
eab_popup_control_cleanup (EABPopupControl *pop)
{
	if (pop->contact) {
		g_object_unref (pop->contact);
		pop->contact = NULL;
	}

	if (pop->scheduled_refresh) {
		g_source_remove (pop->scheduled_refresh);
		pop->scheduled_refresh = 0;
	}

	if (pop->query_tag) {
#if notyet
		e_book_simple_query_cancel (pop->book, pop->query_tag);
#endif
		pop->query_tag = 0;
	}

	if (pop->book) {
		g_object_unref (pop->book);
		pop->book = NULL;
	}

	g_free (pop->name);
	pop->name = NULL;

	g_free (pop->email);
	pop->email = NULL;
}

static void
eab_popup_control_dispose (GObject *obj)
{
	EABPopupControl *pop = EAB_POPUP_CONTROL (obj);

	eab_popup_control_cleanup (pop);

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (obj);
}

GType
eab_popup_control_get_type (void)
{
	static GType pop_type = 0;

	if (!pop_type) {
		static const GTypeInfo pop_info =  {
			sizeof (EABPopupControlClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) eab_popup_control_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EABPopupControl),
			0,             /* n_preallocs */
			(GInstanceInitFunc) eab_popup_control_init,
		};

		pop_type = g_type_register_static (gtk_event_box_get_type (), "EABPopupControl", &pop_info, 0);
	}

	return pop_type;
}

static void
eab_popup_control_refresh_names (EABPopupControl *pop)
{
	if (pop->name_widget) {
		if (pop->name && *pop->name) {
			gtk_label_set_text (GTK_LABEL (pop->name_widget), pop->name);
			gtk_widget_show (pop->name_widget);
		} else {
			gtk_widget_hide (pop->name_widget);
		}
	}

	if (pop->email_widget) {
		if (pop->email && *pop->email) {
			gtk_label_set_text (GTK_LABEL (pop->email_widget), pop->email);
			gtk_widget_show (pop->email_widget);
		} else {
			gtk_widget_hide (pop->email_widget);
		}
	}

	eab_popup_control_query (pop);
}

static gint
refresh_timeout_cb (gpointer ptr)
{
	EABPopupControl *pop = EAB_POPUP_CONTROL (ptr);
	eab_popup_control_refresh_names (pop);
	pop->scheduled_refresh = 0;
	return 0;
}

static void
eab_popup_control_schedule_refresh (EABPopupControl *pop)
{
	if (pop->scheduled_refresh == 0)
		pop->scheduled_refresh = g_timeout_add (20, refresh_timeout_cb, pop);
}

/* If we are handed something of the form "Foo <bar@bar.com>",
   do the right thing. */
static gboolean
eab_popup_control_set_free_form (EABPopupControl *pop, const gchar *txt)
{
	gchar *lt, *gt = NULL;

	g_return_val_if_fail (pop && EAB_IS_POPUP_CONTROL (pop), FALSE);

	if (txt == NULL)
		return FALSE;

	lt = strchr (txt, '<');
	if (lt)
		gt = strchr (txt, '>');

	if (lt && gt && lt+1 < gt) {
		gchar *name  = g_strndup (txt,  lt-txt);
		gchar *email = g_strndup (lt+1, gt-lt-1);
		eab_popup_control_set_name (pop, name);
		eab_popup_control_set_email (pop, email);

		return TRUE;
	}
	
	return FALSE;
}

static void
eab_popup_control_set_name (EABPopupControl *pop, const gchar *name)
{
	g_return_if_fail (pop && EAB_IS_POPUP_CONTROL (pop));

	/* We only allow the name to be set once. */
	if (pop->name)
		return;

	if (!eab_popup_control_set_free_form (pop, name)) {
		pop->name = g_strdup (name);
		if (pop->name)
			g_strstrip (pop->name);
	}

	eab_popup_control_schedule_refresh (pop);
}

static void
eab_popup_control_set_email (EABPopupControl *pop, const gchar *email)
{
	g_return_if_fail (pop && EAB_IS_POPUP_CONTROL (pop));

	/* We only allow the e-mail to be set once. */
	if (pop->email)
		return;

	if (!eab_popup_control_set_free_form (pop, email)) {
		pop->email = g_strdup (email);
		if (pop->email)
			g_strstrip (pop->email);
	}

	eab_popup_control_schedule_refresh (pop);
}

void
eab_popup_control_construct (EABPopupControl *pop)
{
	GtkWidget *vbox, *name_holder;
	GdkColor color = { 0x0, 0xffff, 0xffff, 0xffff };

	g_return_if_fail (pop && EAB_IS_POPUP_CONTROL (pop));

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
		g_object_unref (style);
	}

	pop->generic_view = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (pop->generic_view), name_holder);
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), pop->generic_view, TRUE, TRUE, 0);
	gtk_widget_show_all (pop->generic_view);

	pop->query_msg = gtk_label_new (_("Querying Address Book..."));
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), pop->query_msg, TRUE, TRUE, 0);
	gtk_widget_show (pop->query_msg);

	/* Build ContactDisplay */
	pop->contact_display = eab_contact_display_new ();
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), pop->contact_display, TRUE, TRUE, 0);


	/* Final assembly */

	gtk_container_add (GTK_CONTAINER (pop), pop->main_vbox);
	gtk_widget_show (pop->main_vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);
	gtk_container_set_border_width (GTK_CONTAINER (pop), 2);
}

static GtkWidget *
eab_popup_new (void)
{
	EABPopupControl *pop = g_object_new (EAB_TYPE_POPUP_CONTROL, NULL);
	eab_popup_control_construct (pop);
	return GTK_WIDGET (pop);
}

static void
emit_event (EABPopupControl *pop, const char *event)
{
	if (pop->es) {
		BonoboArg *arg;

		arg = bonobo_arg_new (BONOBO_ARG_BOOLEAN);
		BONOBO_ARG_SET_BOOLEAN (arg, TRUE);
		bonobo_event_source_notify_listeners_full (pop->es,
							   "GNOME/Evolution/Addressbook/AddressPopup",
							   "Event",
							   event,
							   arg, NULL);
		bonobo_arg_release (arg);
	}	
}

static void
eab_popup_control_no_matches (EABPopupControl *pop)
{
	if (pop->email && *pop->email) {
		if (pop->name && *pop->name)
			e_contact_quick_add (pop->name, pop->email, NULL, NULL);
		else
			e_contact_quick_add_free_form (pop->email, NULL, NULL);

	}
	eab_popup_control_cleanup (pop);
	emit_event (pop, "Destroy");
}

static void
eab_popup_control_query (EABPopupControl *pop)
{
	g_return_if_fail (pop && EAB_IS_POPUP_CONTROL (pop));

	g_object_ref (pop);

	eab_popup_control_no_matches (pop) ;

	g_object_unref (pop);

}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

enum {
	PROPERTY_NAME,
	PROPERTY_EMAIL,
	PROPERTY_TRANSITORY
};

static void
set_prop (BonoboPropertyBag *bag, const BonoboArg *arg, guint arg_id, CORBA_Environment *ev, gpointer user_data)
{
	EABPopupControl *pop = EAB_POPUP_CONTROL (user_data);

	switch (arg_id) {

	case PROPERTY_NAME:
		eab_popup_control_set_name (pop, BONOBO_ARG_GET_STRING (arg));
		break;

	case PROPERTY_EMAIL:
		eab_popup_control_set_email (pop, BONOBO_ARG_GET_STRING (arg));
		break;

	default:
		g_assert_not_reached ();
	}
}

static void
get_prop (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id, CORBA_Environment *ev, gpointer user_data)
{
	EABPopupControl *pop = EAB_POPUP_CONTROL (user_data);

	switch (arg_id) {

	case PROPERTY_NAME:
		BONOBO_ARG_SET_STRING (arg, pop->name);
		break;

	case PROPERTY_EMAIL:
		BONOBO_ARG_SET_STRING (arg, pop->email);
		break;

	case PROPERTY_TRANSITORY:
		BONOBO_ARG_SET_BOOLEAN (arg, pop->transitory);
		break;

	default:
		g_assert_not_reached ();
	}
}

BonoboControl *
eab_popup_control_new (void)
{
        BonoboControl *control;
        BonoboPropertyBag *bag;
	EABPopupControl *addy;
	GtkWidget *w;

	w = eab_popup_new ();
	addy = EAB_POPUP_CONTROL (w);

	control = bonobo_control_new (w);
	gtk_widget_show (w);

        bag = bonobo_property_bag_new (get_prop, set_prop, w);
        bonobo_property_bag_add (bag, "name", PROPERTY_NAME,
                                 BONOBO_ARG_STRING, NULL, NULL,
                                 BONOBO_PROPERTY_WRITEABLE | BONOBO_PROPERTY_READABLE);

        bonobo_property_bag_add (bag, "email", PROPERTY_EMAIL,
                                 BONOBO_ARG_STRING, NULL, NULL,
                                 BONOBO_PROPERTY_WRITEABLE | BONOBO_PROPERTY_READABLE);

	bonobo_property_bag_add (bag, "transitory", PROPERTY_TRANSITORY,
				 BONOBO_ARG_BOOLEAN, NULL, NULL,
				 BONOBO_PROPERTY_READABLE);

        bonobo_control_set_properties (control, bonobo_object_corba_objref (BONOBO_OBJECT (bag)), NULL);
        bonobo_object_unref (BONOBO_OBJECT (bag));

	addy->es = bonobo_event_source_new ();
	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (addy->es));

        return control;
}
