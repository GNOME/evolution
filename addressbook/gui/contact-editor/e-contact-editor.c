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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

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
_add_image(GtkTable *table, gchar *image, int left, int right, int top, int bottom)
{
	GdkPixbuf *pixbuf;
	double width, height;
	GtkWidget *canvas, *alignment;

	pixbuf = gdk_pixbuf_new_from_file(image);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	canvas = gnome_canvas_new_aa();
#if 0
	gnome_canvas_item_new(gnome_canvas_root(GNOME_CANVAS(canvas)),
			      gnome_canvas_rect_get_type(),
			      "fill_color_gdk", &(gtk_widget_get_style(GTK_WIDGET(canvas))->bg[GTK_STATE_NORMAL]),
			      "x1", 0.0,
			      "y1", 0.0,
			      "x2", width,
			      "y2", height,
			      NULL);
#endif
	gnome_canvas_item_new(gnome_canvas_root(GNOME_CANVAS(canvas)),
			      gnome_canvas_pixbuf_get_type(),
			      "pixbuf", pixbuf,
			      NULL);
	alignment = gtk_widget_new(gtk_alignment_get_type(),
				   "child", canvas,
				   "xalign", (double) 0,
				   "yalign", (double) 0,
				   "xscale", (double) 0,
				   "yscale", (double) 0,
				   NULL);
	
	gtk_widget_set_usize(canvas, width, height);

	gtk_table_attach(table,
			 alignment,
			 left, right, top, bottom,
			 GTK_FILL, GTK_FILL,
			 0, 0);

	gdk_pixbuf_unref(pixbuf);

	gtk_widget_show(canvas);
	gtk_widget_show(alignment);
}

static void
_add_images(GtkTable *table)
{
	_add_image(table, EVOLUTION_IMAGES "/malehead.png", 0, 1, 0, 4);
	_add_image(table, EVOLUTION_IMAGES "/cellphone.png", 4, 5, 0, 4);
	_add_image(table, EVOLUTION_IMAGES "/envelope.png", 0, 1, 5, 7);
	_add_image(table, EVOLUTION_IMAGES "/globe.png",
		   0, 1, 8, 10);
	_add_image(table, EVOLUTION_IMAGES "/house.png", 4, 5, 5, 10);
}

static void
_add_details_images(GtkTable *table)
{
	_add_image(table, EVOLUTION_IMAGES "/briefcase.png", 0, 1, 0, 2);
	_add_image(table, EVOLUTION_IMAGES "/malehead.png", 0, 1, 4, 6);
	_add_image(table, EVOLUTION_IMAGES "/globe.png", 0, 1, 7, 9);
}

static void
_replace_button(EContactEditor *editor, gchar *button_xml, gchar *image, GtkSignalFunc func)
{
	GladeXML *gui = editor->gui;
	GtkWidget *button = glade_xml_get_widget(gui, button_xml);
	GtkWidget *pixmap;
	gchar *image_temp;
	image_temp = g_strdup_printf("%s%s", DATADIR "/evolution/", image);
	pixmap = gnome_pixmap_new_from_file(image_temp);
	gtk_container_add(GTK_CONTAINER(button),
			  pixmap);
	g_free(image_temp);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(button), "button_press_event", func, editor);
			   
}

static void
_replace_buttons(EContactEditor *editor)
{
	_replace_button(editor, "button-phone1", "arrow.png", _phone_arrow_pressed);
	_replace_button(editor, "button-phone2", "arrow.png", _phone_arrow_pressed);
	_replace_button(editor, "button-phone3", "arrow.png", _phone_arrow_pressed);
	_replace_button(editor, "button-phone4", "arrow.png", _phone_arrow_pressed);
	_replace_button(editor, "button-address1", "arrow.png", _address_arrow_pressed);
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
	GtkCombo *combo = GTK_COMBO(glade_xml_get_widget(editor->gui, "combo-file-as"));
	GtkEntry *file_as = GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-file-as"));

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

	gtk_combo_set_popdown_strings(combo, strings);
	g_list_foreach(strings, (GFunc) g_free, NULL);
	g_list_free(strings);

	if (style != -1) {
		string = name_to_style(editor->name, editor->company, style);
		gtk_entry_set_text(file_as, string);
		g_free(string);
	}
}

static void
name_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	ECardName *name;
	char *string;
	GtkEntry *entry = GTK_ENTRY(widget);
	int style = 0;

	style = file_as_get_style(editor);
	
	name = editor->name;

	if (name)
		e_card_name_free(name);

	string = gtk_entry_get_text(entry);
	
	name = e_card_name_from_string(string);
	
	editor->name = name;
	
	file_as_set_style(editor, style);
}

static void
company_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	int style = 0;

	style = file_as_get_style(editor);
	
	g_free(editor->company);

	editor->company = g_strdup(gtk_entry_get_text(entry));
	
	file_as_set_style(editor, style);
}

static void
set_entry_changed_signal_phone(EContactEditor *editor, char *id)
{
	GtkWidget *widget = glade_xml_get_widget(editor->gui, id);
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
	gtk_signal_connect(GTK_OBJECT(widget), "changed",
			   email_entry_changed, editor);
	widget = glade_xml_get_widget(editor->gui, "text-address");
	gtk_signal_connect(GTK_OBJECT(widget), "changed",
			   address_text_changed, editor);
	widget = glade_xml_get_widget(editor->gui, "entry-fullname");
	gtk_signal_connect(GTK_OBJECT(widget), "changed",
			   name_entry_changed, editor);
	widget = glade_xml_get_widget(editor->gui, "entry-company");
	gtk_signal_connect(GTK_OBJECT(widget), "changed",
			   company_entry_changed, editor);
}

static void
full_name_clicked(GtkWidget *button, EContactEditor *editor)
{
	GnomeDialog *dialog = GNOME_DIALOG(e_contact_editor_fullname_new(editor->name));
	int result;
	gtk_widget_show(GTK_WIDGET(dialog));
	gnome_dialog_close_hides (dialog, TRUE);
	result = gnome_dialog_run_and_close (dialog);
	if (result == 0) {
		ECardName *name;
		char *full_name;
		gtk_object_get(GTK_OBJECT(dialog),
			       "name", &name,
			       NULL);
		full_name = e_card_name_to_string(name);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-fullname")), full_name);
		g_free(full_name);
		e_card_name_free(editor->name);
		editor->name = e_card_name_copy(name);
	}
	gtk_object_unref(GTK_OBJECT(dialog));
}

static void
e_contact_editor_init (EContactEditor *e_contact_editor)
{
	GladeXML *gui;
	GtkAdjustment *adjustment;

	e_contact_editor->card = NULL;
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/contact-editor.glade", NULL);
	e_contact_editor->gui = gui;
	gtk_widget_reparent(glade_xml_get_widget(gui, "notebook-contact-editor"),
			    GTK_WIDGET(e_contact_editor));

	_add_images(GTK_TABLE(glade_xml_get_widget(gui, "table-contact-editor-general")));
	_add_details_images(GTK_TABLE(glade_xml_get_widget(gui, "table-contact-editor-details")));
	_replace_buttons(e_contact_editor);
	set_entry_changed_signals(e_contact_editor);

	gtk_signal_connect(GTK_OBJECT(glade_xml_get_widget(e_contact_editor->gui, "button-fullname")), "clicked",
			   full_name_clicked, e_contact_editor);

	gtk_object_get(GTK_OBJECT(glade_xml_get_widget(gui, "text-comments")),
		       "vadjustment", &adjustment,
		       NULL);
	gtk_range_set_adjustment(GTK_RANGE(glade_xml_get_widget(gui, "vscrollbar-comments")),
				 adjustment);
	
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
			gtk_object_set(GTK_OBJECT(glade_xml_get_widget(editor->gui, label)),
				       "label", g_list_nth_data(*list, menu_item),
				       NULL);
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
#if 0
	GtkWidget *entry = gtk_object_get_data(GTK_OBJECT(dialog),
					       "e_contact_editor_entry");
#endif
	GtkWidget *dialog_entry = gtk_object_get_data(GTK_OBJECT(dialog),
						      "e_contact_editor_dialog_entry");
	GList **list = gtk_object_get_data(GTK_OBJECT(dialog),
					   "e_contact_editor_list");
	GList **info = gtk_object_get_data(GTK_OBJECT(dialog),
					   "e_contact_editor_info");
	switch (button) {
	case 0:
		gtk_object_set(GTK_OBJECT(label),
			       "label", gtk_entry_get_text(GTK_ENTRY(dialog_entry)),
			       NULL);
		*list = g_list_append(*list, g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog_entry))));
		g_free(*info);
		*info = NULL;
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
	
	result = _arrow_pressed (widget, button, editor, editor->address_popup, &editor->address_list, &editor->address_info, "label-address1", "text-address", "Add new Address type");

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
set_phone_field(GtkEntry *entry, const ECardPhone *phone)
{
	set_field(entry, phone ? phone->number : "");
}

static void
set_fields(EContactEditor *editor)
{
	GtkEntry *entry;

	entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-phone1"));
	set_phone_field(entry, e_card_simple_get_phone(editor->simple, editor->phone_choice[0]));

	entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-phone2"));
	set_phone_field(entry, e_card_simple_get_phone(editor->simple, editor->phone_choice[1]));

	entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-phone3"));
	set_phone_field(entry, e_card_simple_get_phone(editor->simple, editor->phone_choice[2]));

	entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-phone4"));
	set_phone_field(entry, e_card_simple_get_phone(editor->simple, editor->phone_choice[3]));
	
	entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, "entry-email1"));
	set_field(entry, e_card_simple_get_email(editor->simple, editor->email_choice));
	
	set_address_field(editor, -1);
}

static void
set_address_field(EContactEditor *editor, int result)
{
	GtkEditable *editable;
	int position;
	const ECardAddrLabel *address;
	if (result == -1)
		result = editor->address_choice;
	editor->address_choice = -1;

	position = 0;
	editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "text-address"));
	gtk_editable_delete_text(editable, 0, -1);
	address = e_card_simple_get_address(editor->simple, result);
	if (address && address->data)
		gtk_editable_insert_text(editable, address->data, strlen(address->data), &position);
	editor->address_choice = result;
}

static void
fill_in_info(EContactEditor *editor)
{
	ECard *card = editor->card;
	if (card) {
		char *file_as;
		char *fname;
		ECardName *name;
		char *title;
		char *org;
		char *org_unit;
		char *office;
		char *url;
		char *role;
		char *manager;
		char *assistant;
		char *nickname;
		char *spouse;
		const ECardDate *anniversary;
		char *fburl;
		char *note;
		const ECardDate *bday;
		GtkEditable *editable;
		int position = 0;

		gtk_object_get(GTK_OBJECT(card),
			       "file_as",       &file_as,
			       "name",          &name,
			       "full_name",     &fname,
			       "url",           &url,
			       "org",           &org,
			       "org_unit",      &org_unit,
			       "office",        &office,
			       "title",         &title,
			       "role",          &role,
			       "manager",       &manager,
			       "assistant",     &assistant,
			       "nickname",      &nickname,
			       "spouse",        &spouse,
			       "anniversary",   &anniversary,
			       "fburl",         &fburl,
			       "note",          &note,
			       "birth_date",    &bday,
			       NULL);
		
		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-fullname"));
		gtk_editable_delete_text(editable, 0, -1);
		if (fname)
			gtk_editable_insert_text(editable, fname, strlen(fname), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-web"));
		gtk_editable_delete_text(editable, 0, -1);
		if (url)
			gtk_editable_insert_text(editable, url, strlen(url), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-company"));
		gtk_editable_delete_text(editable, 0, -1);
		if (org)
			gtk_editable_insert_text(editable, org, strlen(org), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-department"));
		gtk_editable_delete_text(editable, 0, -1);
		if (org_unit)
			gtk_editable_insert_text(editable, org_unit, strlen(org_unit), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-office"));
		gtk_editable_delete_text(editable, 0, -1);
		if (office)
			gtk_editable_insert_text(editable, office, strlen(office), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-jobtitle"));
		gtk_editable_delete_text(editable, 0, -1);
		if (title)
			gtk_editable_insert_text(editable, title, strlen(title), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-manager"));
		gtk_editable_delete_text(editable, 0, -1);
		if (manager)
			gtk_editable_insert_text(editable, manager, strlen(manager), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-assistant"));
		gtk_editable_delete_text(editable, 0, -1);
		if (assistant)
			gtk_editable_insert_text(editable, assistant, strlen(assistant), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-nickname"));
		gtk_editable_delete_text(editable, 0, -1);
		if (nickname)
			gtk_editable_insert_text(editable, nickname, strlen(nickname), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-spouse"));
		gtk_editable_delete_text(editable, 0, -1);
		if (spouse)
			gtk_editable_insert_text(editable, spouse, strlen(spouse), &position);

		if (anniversary) {
			struct tm time_struct = {0,0,0,0,0,0,0,0,0};
			time_t time_val;
			GnomeDateEdit *dateedit;
			time_struct.tm_mday = anniversary->day;
			time_struct.tm_mon = anniversary->month - 1;
			time_struct.tm_year = anniversary->year - 1900;
			time_val = mktime(&time_struct);
			dateedit = GNOME_DATE_EDIT(glade_xml_get_widget(editor->gui, "dateedit-anniversary"));
			gnome_date_edit_set_time(dateedit, time_val);
		}

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-fburl"));
		gtk_editable_delete_text(editable, 0, -1);
		if (fburl)
			gtk_editable_insert_text(editable, fburl, strlen(fburl), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-profession"));
		gtk_editable_delete_text(editable, 0, -1);
		if (role)
			gtk_editable_insert_text(editable, role, strlen(role), &position);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "text-comments"));
		gtk_editable_delete_text(editable, 0, -1);
		if (note)
			gtk_editable_insert_text(editable, note, strlen(note), &position);

		if (bday) {
			struct tm time_struct = {0,0,0,0,0,0,0,0,0};
			time_t time_val;
			GnomeDateEdit *dateedit;
			time_struct.tm_mday = bday->day;
			time_struct.tm_mon = bday->month - 1;
			time_struct.tm_year = bday->year - 1900;
			time_val = mktime(&time_struct);
			dateedit = GNOME_DATE_EDIT(glade_xml_get_widget(editor->gui, "dateedit-birthday"));
			gnome_date_edit_set_time(dateedit, time_val);
		}
		
		/* File as has to come after company and name or else it'll get messed up when setting them. */
		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-file-as"));
		gtk_editable_delete_text(editable, 0, -1);
		if (file_as)
			gtk_editable_insert_text(editable, file_as, strlen(file_as), &position);

		set_fields(editor);
	}
}

static void
extract_info(EContactEditor *editor)
{
	ECard *card = editor->card;
	if (card) {
		char *file_as;
		char *fname;
		char *url;
		char *org;
		char *org_unit;
		char *office;
		char *title;
		char *role;
		char *manager;
		char *assistant;
		char *nickname;
		char *spouse;
		ECardDate *anniversary;
		char *fburl;
		char *note;
		ECardDate *bday;
		GtkEditable *editable;
		GnomeDateEdit *dateedit;
		int position = 0;
		struct tm time_struct;
		time_t time_val;

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-file-as"));
		file_as = gtk_editable_get_chars(editable, 0, -1);
		gtk_object_set(GTK_OBJECT(card),
			       "file_as", file_as,
			       NULL);
		g_free(file_as);
		
		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-fullname"));
		fname = gtk_editable_get_chars(editable, 0, -1);
		if (fname && *fname)
			gtk_object_set(GTK_OBJECT(card),
				       "full_name", fname,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "full_name", NULL,
				       NULL);
		g_free(fname);

		if (editor->name)
			gtk_object_set(GTK_OBJECT(card),
				       "name", editor->name,
				       NULL);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-web"));
		url = gtk_editable_get_chars(editable, 0, -1);
		if (url && *url)
			gtk_object_set(GTK_OBJECT(card),
				       "url", url,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "url", NULL,
				       NULL);
		g_free(url);

		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-company"));
		org = gtk_editable_get_chars(editable, 0, -1);
		if (org && *org)
			gtk_object_set(GTK_OBJECT(card),
				       "org", org,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "org", NULL,
				       NULL);
		g_free(org);

		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-department"));
		org_unit = gtk_editable_get_chars(editable, 0, -1);
		if (org_unit && *org_unit)
			gtk_object_set(GTK_OBJECT(card),
				       "org_unit", org_unit,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "org_unit", NULL,
				       NULL);
		g_free(org_unit);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-office"));
		office = gtk_editable_get_chars(editable, 0, -1);
		if (office && *office)
			gtk_object_set(GTK_OBJECT(card),
				       "office", office,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "office", NULL,
				       NULL);
		g_free(office);

		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-jobtitle"));
		title = gtk_editable_get_chars(editable, 0, -1);
		if (title && *title)
			gtk_object_set(GTK_OBJECT(card),
				       "title", title,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "title", NULL,
				       NULL);
		g_free(title);

		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-profession"));
		role = gtk_editable_get_chars(editable, 0, -1);
		if (role && *role)
			gtk_object_set(GTK_OBJECT(card),
				       "role", role,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "role", NULL,
				       NULL);
		g_free(role);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-manager"));
		manager = gtk_editable_get_chars(editable, 0, -1);
		if (manager && *manager)
			gtk_object_set(GTK_OBJECT(card),
				       "manager", manager,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "manager", NULL,
				       NULL);
		g_free(manager);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-assistant"));
		assistant = gtk_editable_get_chars(editable, 0, -1);
		if (assistant && *assistant)
			gtk_object_set(GTK_OBJECT(card),
				       "assistant", assistant,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "assistant", NULL,
				       NULL);
		g_free(assistant);

		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-nickname"));
		nickname = gtk_editable_get_chars(editable, 0, -1);
		if (nickname && *nickname)
			gtk_object_set(GTK_OBJECT(card),
				       "nickname", nickname,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "nickname", NULL,
				       NULL);
		g_free(nickname);

		position = 0;
		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-spouse"));
		spouse = gtk_editable_get_chars(editable, 0, -1);
		if (spouse && *spouse)
			gtk_object_set(GTK_OBJECT(card),
				       "spouse", spouse,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "spouse", NULL,
				       NULL);
		g_free(spouse);

		dateedit = GNOME_DATE_EDIT(glade_xml_get_widget(editor->gui, "dateedit-anniversary"));
		time_val = gnome_date_edit_get_date(dateedit);
		gmtime_r(&time_val,
			 &time_struct);
		anniversary = g_new(ECardDate, 1);
		anniversary->day   = time_struct.tm_mday;
		anniversary->month = time_struct.tm_mon + 1;
		anniversary->year  = time_struct.tm_year + 1900;
		gtk_object_set(GTK_OBJECT(card),
			       "anniversary", anniversary,
			       NULL);

		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "entry-fburl"));
		fburl = gtk_editable_get_chars(editable, 0, -1);
		if (fburl && *fburl)
			gtk_object_set(GTK_OBJECT(card),
				       "fburl", fburl,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "fburl", NULL,
				       NULL);
		g_free(fburl);

		editable = GTK_EDITABLE(glade_xml_get_widget(editor->gui, "text-comments"));
		note = gtk_editable_get_chars(editable, 0, -1);
		if (note && *note)
			gtk_object_set(GTK_OBJECT(card),
				       "note", note,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       "note", NULL,
				       NULL);
		g_free(note);

		dateedit = GNOME_DATE_EDIT(glade_xml_get_widget(editor->gui, "dateedit-birthday"));
		time_val = gnome_date_edit_get_date(dateedit);
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
