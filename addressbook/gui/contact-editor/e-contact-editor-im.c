/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-editor-im.c
 * Copyright (C) 2003  Ximian, Inc.
 * Author: Christian Hammond <chipx86@gnupdate.org>
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
 */

#include <config.h>
#include "e-contact-editor-im.h"
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtksizegroup.h>
#include <gtk/gtkstock.h>
#include <string.h>
#include <e-util/e-icon-factory.h>

static void e_contact_editor_im_init		(EContactEditorIm		 *card);
static void e_contact_editor_im_class_init	(EContactEditorImClass	 *klass);
static void e_contact_editor_im_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_contact_editor_im_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_contact_editor_im_dispose (GObject *object);

static void fill_in_info(EContactEditorIm *editor);
static void extract_info(EContactEditorIm *editor);

static GtkDialogClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_SERVICE,
	PROP_LOCATION,
	PROP_USERNAME,
	PROP_EDITABLE
};

#define FIRST_IM_TYPE E_CONTACT_IM_AIM
#define LAST_IM_TYPE  E_CONTACT_IM_ICQ

static const char *im_labels[] = {
	N_("AOL Instant Messenger"),
        N_("Novell Groupwise"),
	N_("Jabber"),
	N_("Yahoo Messenger"),
	N_("MSN Messenger"),
	N_("ICQ")
};

static const char *im_images[] = {
	"im-aim",
	"im-nov",
	"im-jabber",
	"im-yahoo",
	"im-msn",
	"im-icq"
};

GType
e_contact_editor_im_get_type (void)
{
	static GType contact_editor_im_type = 0;

	if (!contact_editor_im_type) {
		static const GTypeInfo contact_editor_im_info =  {
			sizeof (EContactEditorImClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_contact_editor_im_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EContactEditorIm),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_contact_editor_im_init,
		};

		contact_editor_im_type = g_type_register_static (GTK_TYPE_DIALOG, "EContactEditorIm", &contact_editor_im_info, 0);
	}

	return contact_editor_im_type;
}

static void
e_contact_editor_im_class_init (EContactEditorImClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (GTK_TYPE_DIALOG);

	object_class->set_property = e_contact_editor_im_set_property;
	object_class->get_property = e_contact_editor_im_get_property;
	object_class->dispose = e_contact_editor_im_dispose;

	g_object_class_install_property (object_class, PROP_SERVICE, 
					 g_param_spec_int ("service",
							   _("Service"),
							   /*_( */"XXX blurb" /*)*/,
							   FIRST_IM_TYPE,
							   LAST_IM_TYPE,
							   FIRST_IM_TYPE,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_LOCATION, 
					 g_param_spec_string ("location",
							      _("Location"),
							      /*_( */"XXX blurb" /*)*/,
							      "HOME",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_USERNAME, 
					 g_param_spec_string ("username",
							      _("Username"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE, 
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));
}

static void
service_changed_cb(GtkWidget *optmenu, EContactEditorIm *editor)
{
	editor->service = gtk_option_menu_get_history(GTK_OPTION_MENU(optmenu)) + FIRST_IM_TYPE;
}

static void
location_changed_cb(GtkWidget *optmenu, EContactEditorIm *editor)
{
	int i = gtk_option_menu_get_history(GTK_OPTION_MENU(optmenu));

	if (editor->location != NULL)
		g_free(editor->location);

	if (i == 0)
		editor->location = g_strdup("HOME");
	else if (i == 1)
		editor->location = g_strdup("WORK");
	else
		editor->location = NULL;
}

static void
setup_service_optmenu(EContactEditorIm *editor)
{
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *hbox;
	GtkWidget *item;
	GtkWidget *label;
	GtkWidget *image;
	GtkSizeGroup *sg;
	int i;

	optmenu = glade_xml_get_widget(editor->gui, "optmenu-service");
	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(service_changed_cb), editor);

	menu = gtk_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_widget_show(menu);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (i = 0; i < G_N_ELEMENTS(im_labels); i++) {
		item = gtk_menu_item_new();

		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(item), hbox);
		gtk_widget_show(hbox);

		image = e_icon_factory_get_image (im_images[i], E_ICON_SIZE_MENU);

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		label = gtk_label_new(im_labels[i]);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}
}

static void
setup_location_optmenu(EContactEditorIm *editor)
{
	GtkWidget *item;
	GtkWidget *optmenu;
	GtkWidget *menu;

	optmenu = glade_xml_get_widget(editor->gui, "optmenu-location");

	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(location_changed_cb), editor);

	menu = gtk_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_widget_show(menu);

	item = gtk_menu_item_new_with_label(_("Home"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_menu_item_new_with_label(_("Work"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_menu_item_new_with_label(_("Other"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);
}

static void
e_contact_editor_im_init (EContactEditorIm *e_contact_editor_im)
{
	GladeXML *gui;
	GtkWidget *widget;
	GList *icon_list;

	gtk_dialog_add_buttons (GTK_DIALOG (e_contact_editor_im),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (e_contact_editor_im), FALSE);

	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (e_contact_editor_im)->action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (e_contact_editor_im)->vbox), 0);

	gtk_window_set_resizable(GTK_WINDOW(e_contact_editor_im), TRUE);

	e_contact_editor_im->service = FIRST_IM_TYPE;
	e_contact_editor_im->location = g_strdup("HOME");
	e_contact_editor_im->username = NULL;
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/im.glade", NULL, NULL);
	e_contact_editor_im->gui = gui;

	widget = glade_xml_get_widget(gui, "dialog-im");
	gtk_window_set_title (GTK_WINDOW (e_contact_editor_im),
			      GTK_WINDOW (widget)->title);

	widget = glade_xml_get_widget(gui, "table-im");
	g_object_ref(widget);
	gtk_container_remove(GTK_CONTAINER(widget->parent), widget);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (e_contact_editor_im)->vbox), widget, TRUE, TRUE, 0);
	g_object_unref(widget);

	setup_service_optmenu(e_contact_editor_im);
	setup_location_optmenu(e_contact_editor_im);

	gtk_widget_grab_focus(glade_xml_get_widget(gui, "entry-username"));

	/* set the icon */
	icon_list = e_icon_factory_get_icon_list ("stock_contact");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (e_contact_editor_im), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
}

void
e_contact_editor_im_dispose (GObject *object)
{
	EContactEditorIm *e_contact_editor_im = E_CONTACT_EDITOR_IM(object);

	if (e_contact_editor_im->gui) {
		g_object_unref(e_contact_editor_im->gui);
		e_contact_editor_im->gui = NULL;
	}

	if (e_contact_editor_im->location) {
		g_free(e_contact_editor_im->location);
		e_contact_editor_im->location = NULL;
	}

	if (e_contact_editor_im->username) {
		g_free(e_contact_editor_im->username);
		e_contact_editor_im->username = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

GtkWidget*
e_contact_editor_im_new (EContactField service, const char *location, const char *username)
{
	GtkWidget *widget = g_object_new (E_TYPE_CONTACT_EDITOR_IM, NULL);
	g_object_set (widget,
		      "service", GINT_TO_POINTER(service),
		      "location", location,
		      "username", username,
		      NULL);
	return widget;
}

static void
e_contact_editor_im_set_property (GObject *object, guint prop_id,
					const GValue *value, GParamSpec *pspec)
{
	EContactEditorIm *e_contact_editor_im;
	const char *str;

	e_contact_editor_im = E_CONTACT_EDITOR_IM (object);

	switch (prop_id){
	case PROP_SERVICE:
		e_contact_editor_im->service = g_value_get_int(value);
		fill_in_info(e_contact_editor_im);
		break;

	case PROP_LOCATION:
		if (e_contact_editor_im->location != NULL)
			g_free(e_contact_editor_im->location);

		str = g_value_get_string(value);

		if (str == NULL)
			e_contact_editor_im->location = NULL;
		else if (!g_ascii_strcasecmp(str, "HOME"))
			e_contact_editor_im->location = g_strdup("HOME");
		else if (!g_ascii_strcasecmp(str, "WORK"))
			e_contact_editor_im->location = g_strdup("WORK");
		else
			e_contact_editor_im->location = NULL;

		fill_in_info(e_contact_editor_im);
		break;

	case PROP_USERNAME:
		if (e_contact_editor_im->username != NULL)
			g_free(e_contact_editor_im->username);

		e_contact_editor_im->username = g_strdup(g_value_get_string(value));
		fill_in_info(e_contact_editor_im);
		break;

	case PROP_EDITABLE: {
		int i;
		char *widget_names[] = {
			"optmenu-service",
			"optmenu-location",
			"entry-username",
			"label-service",
			"label-location",
			"label-username",
			NULL
		};
		e_contact_editor_im->editable = g_value_get_boolean (value) ? TRUE : FALSE;
		for (i = 0; widget_names[i] != NULL; i ++) {
			GtkWidget *w = glade_xml_get_widget(e_contact_editor_im->gui, widget_names[i]);
			if (GTK_IS_ENTRY (w)) {
				gtk_editable_set_editable (GTK_EDITABLE (w),
							   e_contact_editor_im->editable);
			}
			else {
				gtk_widget_set_sensitive (w, e_contact_editor_im->editable);
			}
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_contact_editor_im_get_property (GObject *object, guint prop_id,
					GValue *value, GParamSpec *pspec)
{
	EContactEditorIm *e_contact_editor_im;

	e_contact_editor_im = E_CONTACT_EDITOR_IM (object);

	switch (prop_id) {
	case PROP_SERVICE:
		g_value_set_int (value, e_contact_editor_im->service);
		break;
	case PROP_LOCATION:
		g_value_set_string (value, e_contact_editor_im->location);
		break;
	case PROP_USERNAME:
		extract_info(e_contact_editor_im);
		g_value_set_string (value, e_contact_editor_im->username);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, e_contact_editor_im->editable ? TRUE : FALSE);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
fill_in_field(EContactEditorIm *editor, char *field, char *string)
{
	GtkEntry *entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, field));
	if (entry) {
		if (string)
			gtk_entry_set_text(entry, string);
		else
			gtk_entry_set_text(entry, "");
	}
}

static void
fill_in_info(EContactEditorIm *editor)
{
	GtkWidget *optmenu;

	fill_in_field(editor, "entry-username",  editor->username);

	optmenu = glade_xml_get_widget(editor->gui, "optmenu-service");

	if (optmenu != NULL)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), editor->service - FIRST_IM_TYPE);

	optmenu = glade_xml_get_widget(editor->gui, "optmenu-location");

	if (optmenu != NULL)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu),
					    (editor->location == NULL ? 2 :
					     !strcmp(editor->location, "WORK") ? 1 : 0));
}

static char *
extract_field(EContactEditorIm *editor, char *field)
{
	GtkEntry *entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, field));
	if (entry)
		return g_strdup (gtk_entry_get_text(entry));
	else
		return NULL;
}

static void
extract_info(EContactEditorIm *editor)
{
	if (editor->username != NULL)
		g_free(editor->username);

	editor->username = extract_field(editor, "entry-username");

	/*
	 * NOTE: We don't need to handle the option menus.
	 *       These are set by the callbacks.
	 */
}
