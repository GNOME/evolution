/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-editor.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <gnome.h>
#include "e-contact-editor.h"
#include <e-contact-editor-fullname.h>
#include <e-contact-editor-categories.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include <e-util/e-gui-utils.h>

static void e_contact_editor_init		(EContactEditor		 *card);
static void e_contact_editor_class_init	(EContactEditorClass	 *klass);
static void e_contact_editor_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_contact_editor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_contact_editor_destroy (GtkObject *object);

#if 0
static GtkWidget *e_contact_editor_build_dialog(EContactEditor *editor, gchar *entry_id, gchar *label_id, gchar *title, GList **list, GnomeUIInfo **info);
#endif
static void _email_arrow_pressed (GtkWidget *widget, GdkEventButton *button, EContactEditor *editor);
static void _phone_arrow_pressed (GtkWidget *widget, GdkEventButton *button, EContactEditor *editor);
static void _address_arrow_pressed (GtkWidget *widget, GdkEventButton *button, EContactEditor *editor);
static void fill_in_info(EContactEditor *editor);
static void extract_info(EContactEditor *editor);
static void set_fields(EContactEditor *editor);
static void set_address_field(EContactEditor *editor, int result);

static GtkVBoxClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_CARD
};

enum {
	DYNAMIC_LIST_EMAIL,
	DYNAMIC_LIST_PHONE,
	DYNAMIC_LIST_ADDRESS
};

GtkType
e_contact_editor_get_type (void)
{
  static GtkType contact_editor_type = 0;

  if (!contact_editor_type)
    {
      static const GtkTypeInfo contact_editor_info =
      {
        "EContactEditor",
        sizeof (EContactEditor),
        sizeof (EContactEditorClass),
        (GtkClassInitFunc) e_contact_editor_class_init,
        (GtkObjectInitFunc) e_contact_editor_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      contact_editor_type = gtk_type_unique (gtk_vbox_get_type (), &contact_editor_info);
    }

  return contact_editor_type;
}

static void
e_contact_editor_class_init (EContactEditorClass *klass)
{
  GtkObjectClass *object_class;
  GtkVBoxClass *vbox_class;

  object_class = (GtkObjectClass*) klass;
  vbox_class = (GtkVBoxClass *) klass;

  parent_class = gtk_type_class (gtk_vbox_get_type ());

  gtk_object_add_arg_type ("EContactEditor::card", GTK_TYPE_OBJECT, 
			   GTK_ARG_READWRITE, ARG_CARD);
 
  object_class->set_arg = e_contact_editor_set_arg;
  object_class->get_arg = e_contact_editor_get_arg;
  object_class->destroy = e_contact_editor_destroy;
}

static void
_replace_button(EContactEditor *editor, gchar *button_xml, gchar *image, GtkSignalFunc func)
{
	GladeXML *gui = editor->gui;
	GtkWidget *button = glade_xml_get_widget(gui, button_xml);
	GtkWidget *pixmap;
	gchar *image_temp;
	if (button && GTK_IS_BUTTON(button)) {
		image_temp = g_strdup_printf("%s%s", DATADIR "/evolution/", image);
		pixmap = e_create_image_widget(NULL, image_temp, NULL, 0, 0);
		gtk_container_add(GTK_CONTAINER(button),
				  pixmap);
		g_free(image_temp);
		gtk_widget_show(pixmap);
		gtk_signal_connect(GTK_OBJECT(button), "button_press_event", func, editor);
	}
}

static void
_replace_buttons(EContactEditor *editor)
{
	_replace_button(editor, "button-phone1", "arrow.png", _phone_arrow_pressed);
	_replace_button(editor, "button-phone2", "arrow.png", _phone_arrow_pressed);
	_replace_button(editor, "button-phone3", "arrow.png", _phone_arrow_pressed);
	_replace_button(editor, "button-phone4", "arrow.png", _phone_arrow_pressed);
	_replace_button(editor, "button-address", "arrow.png", _address_arrow_pressed);
	_replace_button(editor, "button-email1", "arrow.png", _email_arrow_pressed);
}

static void
phone_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	int which;
	gchar *string;
	GtkEntry *entry = GTK_ENTRY(widget);
	ECardPhone *phone;

	if ( widget == glade_xml_get_widget(editor->gui, "entry-phone1") ) {
		which = 1;
	} else if ( widget == glade_xml_get_widget(editor->gui, "entry-phone2") ) {
		which = 2;
	} else if ( widget == glade_xml_get_widget(editor->gui, "entry-phone3") ) {
		which = 3;
	} else if ( widget == glade_xml_get_widget(editor->gui, "entry-phone4") ) {
		which = 4;
	} else
		return;
	string = gtk_entry_get_text(entry);
	phone = e_card_phone_new();
	phone->number = string;
	e_card_simple_set_phone(editor->simple, editor->phone_choice[which - 1], phone);
	phone->number = NULL;
	e_card_phone_free(phone);
	set_fields(editor);
}

static void
email_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	gchar *string;
	GtkEntry *entry = GTK_ENTRY(widget);

	string = gtk_entry_get_text(entry);

	e_card_simple_set_email(editor->simple, editor->email_choice, string);
}

static void
address_text_changed (GtkWidget *widget, EContactEditor *editor)
{
	GtkEditable *editable = GTK_EDITABLE(widget);
	ECardAddrLabel *address;

	if (editor->address_choice == -1)
		return;

	address = e_card_address_label_new();
	address->data = gtk_editable_get_chars(editable, 0, -1);
	e_card_simple_set_address(editor->simple, editor->address_choice, address);
	e_card_address_label_free(address);
}

/* This function tells you whether name_to_style will make sense.  */
static gboolean
style_makes_sense(const ECardName *name, char *company, int style)
{
	switch (style) {
	case 0: /* Fall Through */
	case 1:
		return TRUE;
	case 2:
		if (company && *company)
			return TRUE;
		else
			return FALSE;
	case 3: /* Fall Through */
	case 4:
		if (company && *company && ((name->given && *name->given) || (name->family && *name->family)))
			return TRUE;
		else
			return FALSE;
	default:
		return FALSE;
	}
}

static char *
name_to_style(const ECardName *name, char *company, int style)
{
	char *string;
	char *strings[4], **stringptr;
	char *substring;
	switch (style) {
	case 0:
		stringptr = strings;
		if (name->family && *name->family)
			*(stringptr++) = name->family;
		if (name->given && *name->given)
			*(stringptr++) = name->given;
		*stringptr = NULL;
		string = g_strjoinv(", ", strings);
		break;
	case 1:
		stringptr = strings;
		if (name->given && *name->given)
			*(stringptr++) = name->given;
		if (name->family && *name->family)
			*(stringptr++) = name->family;
		*stringptr = NULL;
		string = g_strjoinv(" ", strings);
		break;
	case 2:
		string = g_strdup(company);
		break;
	case 3: /* Fall Through */
	case 4:
		stringptr = strings;
		if (name->family && *name->family)
			*(stringptr++) = name->family;
		if (name->given && *name->given)
			*(stringptr++) = name->given;
		*stringptr = NULL;
		substring = g_strjoinv(", ", strings);
		if (!(company && *company))
			company = "";
		if (style == 3)
			string = g_strdup_printf("%s (%s)", substring, company);
		else
			string = g_strdup_printf("%s (%s)", company, substring);
		g_free(substring);
		break;
	default:
		string = g_strdup("");
	}
	return string;
}

static int
file_as_get_style (EContactEditor *editor)
{
	GtkEntry *file_as = GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-file-as"));
	char *filestring = gtk_entry_get_text(file_as);
	char *trystring;
	ECardName *name = editor->name;
	int i;
	int style;

	if (!name) return 0;

	style = -1;
	for (i = 0; i < 5; i++) {
		trystring = name_to_style(name, editor->company, i);
		if (!strcmp(trystring, filestring)) {
			g_free(trystring);
			return i;
		}
		g_free(trystring);
	}
	return -1;
}

static void
file_as_set_style(EContactEditor *editor, int style)
{
	char *string;
	int i;
	GList *strings = NULL;
	GtkEntry *file_as = GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-file-as"));
	GtkWidget *widget;
		

	if (style == -1) {
		string = g_strdup(gtk_entry_get_text(file_as));
		strings = g_list_append(strings, string);
	}

	for (i = 0; i < 5; i++) {
		if (style_makes_sense(editor->name, editor->company, i)) {
			string = name_to_style(editor->name, editor->company, i);
			strings = g_list_append(strings, string);
		}
	}

	widget = glade_xml_get_widget(editor->gui, "combo-file-as");
	if (widget && GTK_IS_COMBO(widget)) {
		GtkCombo *combo = GTK_COMBO(widget);
		gtk_combo_set_popdown_strings(combo, strings);
		g_list_foreach(strings, (GFunc) g_free, NULL);
		g_list_free(strings);
	}

	if (style != -1) {
		string = name_to_style(editor->name, editor->company, style);
		gtk_entry_set_text(file_as, string);
		g_free(string);
	}
}

static void
name_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	GtkWidget *file_as;
	int style = 0;

	file_as = glade_xml_get_widget(editor->gui, "entry-file-as");

	if (file_as && GTK_IS_ENTRY(file_as)) {
		style = file_as_get_style(editor);
	}
	
	e_card_name_free(editor->name);
	editor->name = e_card_name_from_string(gtk_entry_get_text(GTK_ENTRY(widget)));
	
	if (file_as && GTK_IS_ENTRY(file_as)) {
		file_as_set_style(editor, style);
	}
}

static void
company_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	int style = 0;
	GtkWidget *file_as;

	file_as = glade_xml_get_widget(editor->gui, "entry-file-as");

	if (file_as && GTK_IS_ENTRY(file_as)) {
		style = file_as_get_style(editor);
	}
	
	g_free(editor->company);
	
	editor->company = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
	
	if (file_as && GTK_IS_ENTRY(file_as)) {
		file_as_set_style(editor, style);
	}
}

static void
set_entry_changed_signal_phone(EContactEditor *editor, char *id)
{
	GtkWidget *widget = glade_xml_get_widget(editor->gui, id);
	if (widget && GTK_IS_ENTRY(widget))
		gtk_signal_connect(GTK_OBJECT(widget), "changed",
				   phone_entry_changed, editor);
}

static void
set_entry_changed_signals(EContactEditor *editor)
{
	GtkWidget *widget;
	set_entry_changed_signal_phone(editor, "entry-phone1");
	set_entry_changed_signal_phone(editor, "entry-phone2");
	set_entry_changed_signal_phone(editor, "entry-phone3");
	set_entry_changed_signal_phone(editor, "entry-phone4");
	widget = glade_xml_get_widget(editor->gui, "entry-email1");
	if (widget && GTK_IS_ENTRY(widget)) {
		gtk_signal_connect(GTK_OBJECT(widget), "changed",
				   email_entry_changed, editor);
	}
	widget = glade_xml_get_widget(editor->gui, "text-address");
	if (widget && GTK_IS_ENTRY(widget)) {
		gtk_signal_connect(GTK_OBJECT(widget), "changed",
				   address_text_changed, editor);
	}
	widget = glade_xml_get_widget(editor->gui, "entry-fullname");
	if (widget && GTK_IS_ENTRY(widget)) {
		gtk_signal_connect(GTK_OBJECT(widget), "changed",
				   name_entry_changed, editor);
	}
	widget = glade_xml_get_widget(editor->gui, "entry-company");
	if (widget && GTK_IS_ENTRY(widget)) {
		gtk_signal_connect(GTK_OBJECT(widget), "changed",
				   company_entry_changed, editor);
	}
}

static void
full_name_clicked(GtkWidget *button, EContactEditor *editor)
{
	GnomeDialog *dialog = GNOME_DIALOG(e_contact_editor_fullname_new(editor->name));
	int result;
	gtk_widget_show(GTK_WIDGET(dialog));
	result = gnome_dialog_run_and_close (dialog);
	if (result == 0) {
		ECardName *name;
		GtkWidget *fname_widget;

		gtk_object_get(GTK_OBJECT(dialog),
			       "name", &name,
			       NULL);
		e_card_name_free(editor->name);
		editor->name = e_card_name_copy(name);

		fname_widget = glade_xml_get_widget(editor->gui, "entry-fullname");
		if (fname_widget && GTK_IS_ENTRY(fname_widget)) {
			char *full_name = e_card_name_to_string(name);
			gtk_entry_set_text(GTK_ENTRY(fname_widget), full_name);
			g_free(full_name);
		}
	}
	gtk_object_unref(GTK_OBJECT(dialog));
}

static void
categories_clicked(GtkWidget *button, EContactEditor *editor)
{
	char *categories;
	GnomeDialog *dialog;
	int result;
	GtkWidget *entry = glade_xml_get_widget(editor->gui, "entry-categories");
	if (entry && GTK_IS_ENTRY(entry))
		categories = gtk_entry_get_text(GTK_ENTRY(entry));
	else if (editor->card)
		gtk_object_get(GTK_OBJECT(editor->card),
			       "categories", &categories,
			       NULL);
	dialog = GNOME_DIALOG(e_contact_editor_categories_new(categories));
	gtk_widget_show(GTK_WIDGET(dialog));
	result = gnome_dialog_run (dialog);
	if (result == 0) {
		gtk_object_get(GTK_OBJECT(dialog),
			       "categories", &categories,
			       NULL);
		if (entry && GTK_IS_ENTRY(entry))
			gtk_entry_set_text(GTK_ENTRY(entry), categories);
		else
			gtk_object_set(GTK_OBJECT(editor->card),
				       "categories", categories,
				       NULL);
		g_free(categories);
	}
	gtk_object_destroy(GTK_OBJECT(dialog));
	if (!entry)
		g_free(categories);
}

static void
e_contact_editor_init (EContactEditor *e_contact_editor)
{
	GladeXML *gui;
	GtkWidget *widget;


	e_contact_editor->email_info = NULL;
	e_contact_editor->phone_info = NULL;
	e_contact_editor->address_info = NULL;
	e_contact_editor->email_popup = NULL;
	e_contact_editor->phone_popup = NULL;
	e_contact_editor->address_popup = NULL;
	e_contact_editor->email_list = NULL;
	e_contact_editor->phone_list = NULL;
	e_contact_editor->address_list = NULL;
	e_contact_editor->name = NULL;
	e_contact_editor->company = g_strdup("");
	
	e_contact_editor->email_choice = 0;
	e_contact_editor->phone_choice[0] = E_CARD_SIMPLE_PHONE_ID_BUSINESS;
	e_contact_editor->phone_choice[1] = E_CARD_SIMPLE_PHONE_ID_HOME;
	e_contact_editor->phone_choice[2] = E_CARD_SIMPLE_PHONE_ID_BUSINESS_FAX;
	e_contact_editor->phone_choice[3] = E_CARD_SIMPLE_PHONE_ID_MOBILE;
	e_contact_editor->address_choice = 0;
	
	e_contact_editor->simple = e_card_simple_new(NULL);

	e_contact_editor->card = NULL;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/contact-editor.glade", NULL);
	e_contact_editor->gui = gui;

	widget = glade_xml_get_widget(gui, "notebook-contact-editor");
	if (!widget) {
		return;
	}
	gtk_widget_reparent(widget,
			    GTK_WIDGET(e_contact_editor));

	_replace_buttons(e_contact_editor);
	set_entry_changed_signals(e_contact_editor);
	
	widget = glade_xml_get_widget(e_contact_editor->gui, "button-fullname");
	if (widget && GTK_IS_BUTTON(widget))
		gtk_signal_connect(GTK_OBJECT(widget), "clicked",
				   full_name_clicked, e_contact_editor);

	widget = glade_xml_get_widget(e_contact_editor->gui, "button-categories");
	if (widget && GTK_IS_BUTTON(widget))
		gtk_signal_connect(GTK_OBJECT(widget), "clicked",
				   categories_clicked, e_contact_editor);
}

void
e_contact_editor_destroy (GtkObject *object) {
	EContactEditor *e_contact_editor = E_CONTACT_EDITOR(object);
	
	if (e_contact_editor->email_list) {
		g_list_foreach(e_contact_editor->email_list, (GFunc) g_free, NULL);
		g_list_free(e_contact_editor->email_list);
	}
	if (e_contact_editor->email_info) {
		g_free(e_contact_editor->email_info);
	}
	if (e_contact_editor->email_popup) {
		gtk_widget_unref(e_contact_editor->email_popup);
	}
	
	if (e_contact_editor->phone_list) {
		g_list_foreach(e_contact_editor->phone_list, (GFunc) g_free, NULL);
		g_list_free(e_contact_editor->phone_list);
	}
	if (e_contact_editor->phone_info) {
		g_free(e_contact_editor->phone_info);
	}
	if (e_contact_editor->phone_popup) {
		gtk_widget_unref(e_contact_editor->phone_popup);
	}
	
	if (e_contact_editor->address_list) {
		g_list_foreach(e_contact_editor->address_list, (GFunc) g_free, NULL);
		g_list_free(e_contact_editor->address_list);
	}
	if (e_contact_editor->address_info) {
		g_free(e_contact_editor->address_info);
	}
	if (e_contact_editor->address_popup) {
		gtk_widget_unref(e_contact_editor->address_popup);
	}
	
	if (e_contact_editor->simple)
		gtk_object_unref(GTK_OBJECT(e_contact_editor->simple));

	g_free (e_contact_editor->company);

	gtk_object_unref(GTK_OBJECT(e_contact_editor->gui));
}

GtkWidget*
e_contact_editor_new (ECard *card)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_contact_editor_get_type ()));
	gtk_object_set (GTK_OBJECT(widget),
			"card", card,
			NULL);
	return widget;
}

static void
e_contact_editor_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EContactEditor *editor;

	editor = E_CONTACT_EDITOR (o);
	
	switch (arg_id){
	case ARG_CARD:
		if (editor->card)
			gtk_object_unref(GTK_OBJECT(editor->card));
		editor->card = e_card_duplicate(E_CARD(GTK_VALUE_OBJECT (*arg)));
		gtk_object_set(GTK_OBJECT(editor->simple),
			       "card", editor->card,
			       NULL);
		fill_in_info(editor);
		break;
	}
}

static void
e_contact_editor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EContactEditor *e_contact_editor;

	e_contact_editor = E_CONTACT_EDITOR (object);

	switch (arg_id) {
	case ARG_CARD:
		e_card_simple_sync_card(e_contact_editor->simple);
		extract_info(e_contact_editor);
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(e_contact_editor->card);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
_popup_position(GtkMenu *menu,
		gint *x,
		gint *y,
		gpointer data)
{
	GtkWidget *button = GTK_WIDGET(data);
	GtkRequisition request;
	int mh, mw;
	gdk_window_get_origin (button->window, x, y);
	*x += button->allocation.width;
	*y += button->allocation.height;

	gtk_widget_size_request(GTK_WIDGET(menu), &request);

	mh = request.height;
	mw = request.width;

	*x -= mw;
	if (*x < 0)
		*x = 0;
	
	if (*y < 0)
		*y = 0;
	
	if ((*x + mw) > gdk_screen_width ())
		*x = gdk_screen_width () - mw;
	
	if ((*y + mh) > gdk_screen_height ())
		*y = gdk_screen_height () - mh;
}

static gint
_arrow_pressed (GtkWidget *widget, GdkEventButton *button, EContactEditor *editor, GtkWidget *popup, GList **list, GnomeUIInfo **info, gchar *label, gchar *entry, gchar *dialog_title)
{
	gint menu_item;
	gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "button_press_event");
	gtk_widget_realize(popup);
	menu_item = gnome_popup_menu_do_popup_modal(popup, _popup_position, widget, button, editor);
	if ( menu_item != -1 ) {
#if 0
		if (menu_item == g_list_length (*list)) {
			e_contact_editor_build_dialog(editor, entry, label, dialog_title, list, info);
		} else {
#endif
			GtkWidget *entry_widget = glade_xml_get_widget(editor->gui, label);
			if (entry_widget && GTK_IS_ENTRY(entry_widget)) {
				gtk_object_set(GTK_OBJECT(entry_widget),
					       "label", g_list_nth_data(*list, menu_item),
					       NULL);
			}
#if 0
		}
#endif
	}
	return menu_item;
}

static void
e_contact_editor_build_ui_info(GList *list, GnomeUIInfo **infop)
{
	GnomeUIInfo *info;
	GnomeUIInfo singleton = { GNOME_APP_UI_TOGGLEITEM, NULL, NULL, NULL, NULL, NULL, GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL };
	GnomeUIInfo end = GNOMEUIINFO_END;
	int length;
	int i;

	info = *infop;

	if ( info )
		g_free(info);
	length = g_list_length( list );
	info = g_new(GnomeUIInfo, length + 2);
	for (i = 0; i < length; i++) {
		info[i] = singleton;
		info[i].label = _(list->data);
		list = list->next;
	}
	info[i] = end;

	*infop = info;
}

#if 0
static void
_dialog_clicked(GtkWidget *dialog, gint button, EContactEditor *editor)
{
	GtkWidget *label = gtk_object_get_data(GTK_OBJECT(dialog),
					       "e_contact_editor_label");

	GtkWidget *dialog_entry = gtk_object_get_data(GTK_OBJECT(dialog),
						      "e_contact_editor_dialog_entry");
	
	GList **list = gtk_object_get_data(GTK_OBJECT(dialog),
					   "e_contact_editor_list");
	GList **info = gtk_object_get_data(GTK_OBJECT(dialog),
					   "e_contact_editor_info");
	switch (button) {
	case 0:
		if (label && GTK_IS_LABEL(label)) {
			gtk_object_set(GTK_OBJECT(label),
				       "label", gtk_entry_get_text(GTK_ENTRY(dialog_entry)),
				       NULL);
			*list = g_list_append(*list, g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog_entry))));
			g_free(*info);
			*info = NULL;
		}
		break;
	}
	gnome_dialog_close(GNOME_DIALOG(dialog));
}

static void
_dialog_destroy(EContactEditor *editor, GtkWidget *dialog)
{
	gnome_dialog_close(GNOME_DIALOG(dialog));
}

static GtkWidget *
e_contact_editor_build_dialog(EContactEditor *editor, gchar *entry_id, gchar *label_id, gchar *title, GList **list, GnomeUIInfo **info)
{
	GtkWidget *dialog_entry = gtk_entry_new();
	GtkWidget *entry = glade_xml_get_widget(editor->gui, entry_id);
	GtkWidget *label = glade_xml_get_widget(editor->gui, label_id);
	
	GtkWidget *dialog = gnome_dialog_new(title,
					     NULL);
	
	gtk_container_add(GTK_CONTAINER(GNOME_DIALOG(dialog)->vbox),
			  gtk_widget_new (gtk_frame_get_type(),
					  "border_width", 4,
					  "label", title,
					  "child", gtk_widget_new(gtk_alignment_get_type(),
								  "child", dialog_entry,
								  "xalign", .5,
								  "yalign", .5,
								  "xscale", 1.0,
								  "yscale", 1.0,
								  "border_width", 9,
								  NULL),
					  NULL));

	gnome_dialog_append_button_with_pixmap(GNOME_DIALOG(dialog),
					       "Add",
					       GNOME_STOCK_PIXMAP_ADD);
	gnome_dialog_append_button(GNOME_DIALOG(dialog), GNOME_STOCK_BUTTON_CANCEL);
	gnome_dialog_set_default(GNOME_DIALOG(dialog), 0);
	
	gtk_signal_connect(GTK_OBJECT(dialog), "clicked",
			   _dialog_clicked, editor);
	gtk_signal_connect_while_alive(GTK_OBJECT(editor), "destroy",
				       _dialog_destroy, GTK_OBJECT(dialog), GTK_OBJECT(dialog));
	
	gtk_object_set_data(GTK_OBJECT(dialog),
			    "e_contact_editor_entry", entry);
	gtk_object_set_data(GTK_OBJECT(dialog),
			    "e_contact_editor_label", label);
	gtk_object_set_data(GTK_OBJECT(dialog),
			    "e_contact_editor_dialog_entry", dialog_entry);
	gtk_object_set_data(GTK_OBJECT(dialog),
			    "e_contact_editor_list", list);
	gtk_object_set_data(GTK_OBJECT(dialog),
			    "e_contact_editor_info", info);

	gtk_widget_show_all(dialog);
	return dialog;
}
#endif

static void
_phone_arrow_pressed (GtkWidget *widget, GdkEventButton *button, EContactEditor *editor)
{
	int which;
	int i;
	gchar *label;
	gchar *entry;
	int result;
	if ( widget == glade_xml_get_widget(editor->gui, "button-phone1") ) {
		which = 1;
	} else if ( widget == glade_xml_get_widget(editor->gui, "button-phone2") ) {
		which = 2;
	} else if ( widget == glade_xml_get_widget(editor->gui, "button-phone3") ) {
		which = 3;
	} else if ( widget == glade_xml_get_widget(editor->gui, "button-phone4") ) {
		which = 4;
	} else
		return;
	
	label = g_strdup_printf("label-phone%d", which);
	entry = g_strdup_printf("entry-phone%d", which);

	if (editor->phone_list == NULL) {
		static char *info[] = {
			N_("Assistant"),
			N_("Business"),
			N_("Business 2"),
			N_("Business Fax"),
			N_("Callback"),
			N_("Car"),
			N_("Company"),
			N_("Home"),
			N_("Home 2"),
			N_("Home Fax"),
			N_("ISDN"),
			N_("Mobile"),
			N_("Other"),
			N_("Other Fax"),
			N_("Pager"),
			N_("Primary"),
			N_("Radio"),
			N_("Telex"),
			N_("TTY/TDD")
		};
		
		for (i = 0; i < sizeof(info) / sizeof(info[0]); i++) {
			editor->phone_list = g_list_append(editor->phone_list, g_strdup(info[i]));
		}
	}
	if (editor->phone_info == NULL) {
		e_contact_editor_build_ui_info(editor->phone_list, &editor->phone_info);
		
		if ( editor->phone_popup )
			gtk_widget_unref(editor->phone_popup);
		
		editor->phone_popup = gnome_popup_menu_new(editor->phone_info);
	}
	
	for(i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i++) {
		const ECardPhone *phone = e_card_simple_get_phone(editor->simple, i);
		gboolean checked;
		checked = phone && phone->number && *phone->number;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(editor->phone_info[i].widget),
					       checked);
		gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(editor->phone_info[i].widget),
						    TRUE);
	}
	
	result = _arrow_pressed (widget, button, editor, editor->phone_popup, &editor->phone_list, &editor->phone_info, label, entry, "Add new phone number type");
	
	if (result != -1) {
		editor->phone_choice[which - 1] = result;
		set_fields(editor);
	}

	g_free(label);
	g_free(entry);
}

static void
_email_arrow_pressed (GtkWidget *widget, GdkEventButton *button, EContactEditor *editor)
{
	int i;
	int result;
	if (editor->email_list == NULL) {
		static char *info[] = {
			N_("Primary Email"),
			N_("Email 2"),
			N_("Email 3")
		};
		
		for (i = 0; i < sizeof(info) / sizeof(info[0]); i++) {
			editor->email_list = g_list_append(editor->email_list, g_strdup(info[i]));
		}
	}
	if (editor->email_info == NULL) {
		e_contact_editor_build_ui_info(editor->email_list, &editor->email_info);

		if ( editor->email_popup )
			gtk_widget_unref(editor->email_popup);
		
		editor->email_popup = gnome_popup_menu_new(editor->email_info);
	}
	
	for(i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i++) {
		const char *string = e_card_simple_get_email(editor->simple, i);
		gboolean checked;
		checked = string && *string;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(editor->email_info[i].widget),
					       checked);
		gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(editor->email_info[i].widget),
						    TRUE);
	}
	
	result = _arrow_pressed (widget, button, editor, editor->email_popup, &editor->email_list, &editor->email_info, "label-email1", "entry-email1", "Add new Email type");
	
	if (result != -1) {
		editor->email_choice = result;
		set_fields(editor);
	}
}

static void
_address_arrow_pressed (GtkWidget *widget, GdkEventButton *button, EContactEditor *editor)
{
	int i;
	int result;
	if (editor->address_list == NULL) {
		static char *info[] = {
			N_("Business"),
			N_("Home"),
			N_("Other")
		};
		
		for (i = 0; i < sizeof(info) / sizeof(info[0]); i++) {
			editor->address_list = g_list_append(editor->address_list, g_strdup(info[i]));
		}
	}
	if (editor->address_info == NULL) {
		e_contact_editor_build_ui_info(editor->address_list, &editor->address_info);

		if ( editor->address_popup )
			gtk_widget_unref(editor->address_popup);
		
		editor->address_popup = gnome_popup_menu_new(editor->address_info);
	}
	
	for(i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++) {
		const ECardAddrLabel *address = e_card_simple_get_address(editor->simple, i);
		gboolean checked;
		checked = address && address->data && *address->data;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(editor->address_info[i].widget),
					       checked);
		gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(editor->address_info[i].widget),
						    TRUE);
	}
	
	result = _arrow_pressed (widget, button, editor, editor->address_popup, &editor->address_list, &editor->address_info, "label-address", "text-address", "Add new Address type");

	if (result != -1) {
		set_address_field(editor, result);
	}
}

static void
set_field(GtkEntry *entry, const char *string)
{
	char *oldstring = gtk_entry_get_text(entry);
	if (!string)
		string = "";
	if (strcmp(string, oldstring))
		gtk_entry_set_text(entry, string);
}

static void
set_phone_field(GtkWidget *entry, const ECardPhone *phone)
{
	set_field(GTK_ENTRY(entry), phone ? phone->number : "");
}

static void
set_fields(EContactEditor *editor)
{
	GtkWidget *entry;

	entry = glade_xml_get_widget(editor->gui, "entry-phone1");
	if (entry && GTK_IS_ENTRY(entry))
		set_phone_field(entry, e_card_simple_get_phone(editor->simple, editor->phone_choice[0]));

	entry = glade_xml_get_widget(editor->gui, "entry-phone2");
	if (entry && GTK_IS_ENTRY(entry))
		set_phone_field(entry, e_card_simple_get_phone(editor->simple, editor->phone_choice[1]));

	entry = glade_xml_get_widget(editor->gui, "entry-phone3");
	if (entry && GTK_IS_ENTRY(entry))
		set_phone_field(entry, e_card_simple_get_phone(editor->simple, editor->phone_choice[2]));

	entry = glade_xml_get_widget(editor->gui, "entry-phone4");
	if (entry && GTK_IS_ENTRY(entry))
		set_phone_field(entry, e_card_simple_get_phone(editor->simple, editor->phone_choice[3]));
	
	entry = glade_xml_get_widget(editor->gui, "entry-email1");
	if (entry && GTK_IS_ENTRY(entry))
		set_field(GTK_ENTRY(entry), e_card_simple_get_email(editor->simple, editor->email_choice));
	
	set_address_field(editor, -1);
}

static void
set_address_field(EContactEditor *editor, int result)
{
	GtkWidget *widget;
	
	widget = glade_xml_get_widget(editor->gui, "text-address");

	if (widget && GTK_IS_TEXT(widget)) {
		int position;
		GtkEditable *editable;
		const ECardAddrLabel *address;

		if (result == -1)
			result = editor->address_choice;
		editor->address_choice = -1;

		position = 0;
		editable = GTK_EDITABLE(widget);
		gtk_editable_delete_text(editable, 0, -1);
		address = e_card_simple_get_address(editor->simple, result);
		if (address && address->data)
			gtk_editable_insert_text(editable, address->data, strlen(address->data), &position);

		editor->address_choice = result;
	}
}

struct {
	char *id;
	char *key;
} field_mapping [] = {
	{ "entry-fullname", "full_name" },
	{ "entry-web", "url" },
	{ "entry-company", "org" },
	{ "entry-department", "org_unit" },
	{ "entry-office", "office" },
	{ "entry-jobtitle", "title" },
	{ "entry-profession", "role" },
	{ "entry-manager", "manager" },
	{ "entry-assistant", "assistant" },
	{ "entry-nickname", "nickname" },
	{ "entry-spouse", "spouse" },
	{ "text-comments", "note" },
	{ "entry-categories", "categories" },
};

static void
fill_in_field(EContactEditor *editor, char *id, char *value)
{
	GtkWidget *widget = glade_xml_get_widget(editor->gui, id);
	if (widget && GTK_IS_EDITABLE(widget)) {
		int position = 0;
		GtkEditable *editable = GTK_EDITABLE(widget);
		gtk_editable_delete_text(editable, 0, -1);
		if (value)
			gtk_editable_insert_text(editable, value, strlen(value), &position);
	}
}

static void
fill_in_card_field(EContactEditor *editor, ECard *card, char *id, char *key)
{
	char *string;
	gtk_object_get(GTK_OBJECT(card),
		       key, &string,
		       NULL);
	fill_in_field(editor, id, string);
}

static void
fill_in_info(EContactEditor *editor)
{
	ECard *card = editor->card;
	if (card) {
		char *file_as;
		ECardName *name;
		const ECardDate *anniversary;
		const ECardDate *bday;
		int i;
		GtkWidget *widget;

		gtk_object_get(GTK_OBJECT(card),
			       "file_as",       &file_as,
			       "name",          &name,
			       "anniversary",   &anniversary,
			       "birth_date",    &bday,
			       NULL);
	
		for (i = 0; i < sizeof(field_mapping) / sizeof(field_mapping[0]); i++) {
			fill_in_card_field(editor, card, field_mapping[i].id, field_mapping[i].key);
		}

		/* File as has to come after company and name or else it'll get messed up when setting them. */
		fill_in_field(editor, "entry-file-as", file_as);
		
		e_card_name_free(editor->name);
		editor->name = e_card_name_copy(name);

		widget = glade_xml_get_widget(editor->gui, "dateedit-anniversary");
		if (anniversary && widget && GNOME_IS_DATE_EDIT(widget)) {
			struct tm time_struct = {0,0,0,0,0,0,0,0,0};
			time_t time_val;
			GnomeDateEdit *dateedit;

			time_struct.tm_mday = anniversary->day;
			time_struct.tm_mon = anniversary->month - 1;
			time_struct.tm_year = anniversary->year - 1900;
			time_val = mktime(&time_struct);
			dateedit = GNOME_DATE_EDIT(widget);
			gnome_date_edit_set_time(dateedit, time_val);
		}

		widget = glade_xml_get_widget(editor->gui, "dateedit-birthday");
		if (bday && widget && GNOME_IS_DATE_EDIT(widget)) {
			struct tm time_struct = {0,0,0,0,0,0,0,0,0};
			time_t time_val;
			GnomeDateEdit *dateedit;
			time_struct.tm_mday = bday->day;
			time_struct.tm_mon = bday->month - 1;
			time_struct.tm_year = bday->year - 1900;
			time_val = mktime(&time_struct);
			dateedit = GNOME_DATE_EDIT(widget);
			gnome_date_edit_set_time(dateedit, time_val);
		}

		set_fields(editor);
	}
}

static void
extract_field(EContactEditor *editor, ECard *card, char *editable_id, char *key)
{
	GtkWidget *widget = glade_xml_get_widget(editor->gui, editable_id);
	if (widget && GTK_IS_EDITABLE(widget)) {
		GtkEditable *editable = GTK_EDITABLE(widget);
		char *string = gtk_editable_get_chars(editable, 0, -1);
		if (string && *string)
			gtk_object_set(GTK_OBJECT(card),
				       key, string,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       key, NULL,
				       NULL);
		g_free(string);
	}
}

static void
extract_info(EContactEditor *editor)
{
	ECard *card = editor->card;
	if (card) {
		ECardDate *anniversary;
		ECardDate *bday;
		struct tm time_struct;
		time_t time_val;
		int i;
		GtkWidget *widget;

		widget = glade_xml_get_widget(editor->gui, "entry-file-as");
		if (widget && GTK_IS_EDITABLE(widget)) {
			GtkEditable *editable = GTK_EDITABLE(widget);
			char *string = gtk_editable_get_chars(editable, 0, -1);
			if (string && *string)
				gtk_object_set(GTK_OBJECT(card),
					       "file_as", string,
					       NULL);
			g_free(string);
		}

		for (i = 0; i < sizeof(field_mapping) / sizeof(field_mapping[0]); i++) {
			extract_field(editor, card, field_mapping[i].id, field_mapping[i].key);
		}

		if (editor->name)
			gtk_object_set(GTK_OBJECT(card),
				       "name", editor->name,
				       NULL);

		widget = glade_xml_get_widget(editor->gui, "dateedit-anniversary");
		if (widget && GNOME_IS_DATE_EDIT(widget)) {
			time_val = gnome_date_edit_get_date(GNOME_DATE_EDIT(widget));
			gmtime_r(&time_val,
				 &time_struct);
			anniversary = g_new(ECardDate, 1);
			anniversary->day   = time_struct.tm_mday;
			anniversary->month = time_struct.tm_mon + 1;
			anniversary->year  = time_struct.tm_year + 1900;
			gtk_object_set(GTK_OBJECT(card),
				       "anniversary", anniversary,
				       NULL);
		}

		widget = glade_xml_get_widget(editor->gui, "dateedit-birthday");
		if (widget && GNOME_IS_DATE_EDIT(widget)) {
			time_val = gnome_date_edit_get_date(GNOME_DATE_EDIT(widget));
			gmtime_r(&time_val,
				 &time_struct);
			bday = g_new(ECardDate, 1);
			bday->day   = time_struct.tm_mday;
			bday->month = time_struct.tm_mon + 1;
			bday->year  = time_struct.tm_year + 1900;
			gtk_object_set(GTK_OBJECT(card),
				       "birth_date", bday,
				       NULL);
		}
	}
}
