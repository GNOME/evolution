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
#include <e-util/e-unicode.h>
#include <e-contact-save-as.h>
#include "addressbook/printing/e-contact-print.h"

/* Signal IDs */
enum {
	ADD_CARD,
	COMMIT_CARD,
	DELETE_CARD,
	EDITOR_CLOSED,
	LAST_SIGNAL
};

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
static void add_field_callback(GtkWidget *widget, EContactEditor *editor);

static GtkObjectClass *parent_class = NULL;

static guint contact_editor_signals[LAST_SIGNAL];

/* The arguments we take */
enum {
	ARG_0,
	ARG_CARD,
	ARG_IS_NEW_CARD
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

      contact_editor_type = gtk_type_unique (GTK_TYPE_OBJECT, &contact_editor_info);
    }

  return contact_editor_type;
}

static void
e_contact_editor_class_init (EContactEditorClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  gtk_object_add_arg_type ("EContactEditor::card", GTK_TYPE_OBJECT, 
			   GTK_ARG_READWRITE, ARG_CARD);
  gtk_object_add_arg_type ("EContactEditor::is_new_card", GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE, ARG_IS_NEW_CARD);

  contact_editor_signals[ADD_CARD] =
	  gtk_signal_new ("add_card",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactEditorClass, add_card),
			  gtk_marshal_NONE__OBJECT,
			  GTK_TYPE_NONE, 1,
			  GTK_TYPE_OBJECT);

  contact_editor_signals[COMMIT_CARD] =
	  gtk_signal_new ("commit_card",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactEditorClass, commit_card),
			  gtk_marshal_NONE__OBJECT,
			  GTK_TYPE_NONE, 1,
			  GTK_TYPE_OBJECT);

  contact_editor_signals[DELETE_CARD] =
	  gtk_signal_new ("delete_card",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactEditorClass, delete_card),
			  gtk_marshal_NONE__OBJECT,
			  GTK_TYPE_NONE, 1,
			  GTK_TYPE_OBJECT);

  contact_editor_signals[EDITOR_CLOSED] =
	  gtk_signal_new ("editor_closed",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactEditorClass, editor_closed),
			  gtk_marshal_NONE__NONE,
			  GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, contact_editor_signals, LAST_SIGNAL);
 
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
		image_temp = g_strdup_printf("%s/%s", EVOLUTIONDIR, image);
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
	string = e_utf8_gtk_entry_get_text(entry);
	phone = e_card_phone_new();
	phone->number = string;
	e_card_simple_set_phone(editor->simple, editor->phone_choice[which - 1], phone);
#if 0
	phone->number = NULL;
#endif
	e_card_phone_free(phone);
	set_fields(editor);
}

static void
email_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	gchar *string;
	GtkEntry *entry = GTK_ENTRY(widget);

	string = e_utf8_gtk_entry_get_text(entry);

	e_card_simple_set_email(editor->simple, editor->email_choice, string);

	g_free (string);
}

static void
address_text_changed (GtkWidget *widget, EContactEditor *editor)
{
	GtkEditable *editable = GTK_EDITABLE(widget);
	ECardAddrLabel *address;

	if (editor->address_choice == -1)
		return;

	address = e_card_address_label_new();

	address->data = e_utf8_gtk_editable_get_chars(editable, 0, -1);

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
	char *filestring;
	char *trystring;
	ECardName *name = editor->name;
	int i;
	int style;

	if (!name) return 0;

	filestring = e_utf8_gtk_entry_get_text(file_as);

	style = -1;
	for (i = 0; i < 5; i++) {
		trystring = name_to_style(name, editor->company, i);
		if (!strcmp(trystring, filestring)) {
			g_free(trystring);
			g_free(filestring);
			return i;
		}
		g_free(trystring);
	}
	g_free (filestring);
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
		string = e_utf8_gtk_entry_get_text(file_as);
		strings = g_list_append(strings, string);
	}

	widget = glade_xml_get_widget(editor->gui, "combo-file-as");

	for (i = 0; i < 5; i++) {
		if (style_makes_sense(editor->name, editor->company, i)) {
			char *u;
			u = name_to_style(editor->name, editor->company, i);
			string = e_utf8_to_gtk_string (widget, u);
			g_free (u);
			if (string) strings = g_list_append(strings, string);
		}
	}

	if (widget && GTK_IS_COMBO(widget)) {
		GtkCombo *combo = GTK_COMBO(widget);
		gtk_combo_set_popdown_strings(combo, strings);
		g_list_foreach(strings, (GFunc) g_free, NULL);
		g_list_free(strings);
	}

	if (style != -1) {
		string = name_to_style(editor->name, editor->company, style);
		e_utf8_gtk_entry_set_text(file_as, string);
		g_free(string);
	}
}

static void
name_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	GtkWidget *file_as;
	int style = 0;
	char *string;

	file_as = glade_xml_get_widget(editor->gui, "entry-file-as");

	if (file_as && GTK_IS_ENTRY(file_as)) {
		style = file_as_get_style(editor);
	}
	
	e_card_name_free(editor->name);

	string = e_utf8_gtk_entry_get_text (GTK_ENTRY(widget));
	editor->name = e_card_name_from_string(string);
	g_free (string);
	
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
	
	editor->company = e_utf8_gtk_entry_get_text(GTK_ENTRY(widget));
	
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
	if (widget && GTK_IS_TEXT(widget)) {
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
	result = gnome_dialog_run (dialog);
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
			e_utf8_gtk_entry_set_text(GTK_ENTRY(fname_widget), full_name);
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
		categories = e_utf8_gtk_entry_get_text(GTK_ENTRY(entry));
	else if (editor->card)
		gtk_object_get(GTK_OBJECT(editor->card),
			       "categories", &categories,
			       NULL);
	dialog = GNOME_DIALOG(e_contact_editor_categories_new(categories));
	gtk_widget_show(GTK_WIDGET(dialog));
	result = gnome_dialog_run (dialog);
	g_free (categories);
	if (result == 0) {
		gtk_object_get(GTK_OBJECT(dialog),
			       "categories", &categories,
			       NULL);
		if (entry && GTK_IS_ENTRY(entry))
			e_utf8_gtk_entry_set_text(GTK_ENTRY(entry), categories);
		else
			gtk_object_set(GTK_OBJECT(editor->card),
				       "categories", categories,
				       NULL);
		g_free(categories);
	}
	gtk_object_destroy(GTK_OBJECT(dialog));
#if 0
	if (!entry)
		g_free(categories);
#endif
}

/* Emits the signal to request saving a card */
static void
save_card (EContactEditor *ce)
{
	extract_info (ce);
	e_card_simple_sync_card (ce->simple);

	if (ce->is_new_card)
		gtk_signal_emit (GTK_OBJECT (ce), contact_editor_signals[ADD_CARD],
				 ce->card);
	else
		gtk_signal_emit (GTK_OBJECT (ce), contact_editor_signals[COMMIT_CARD],
				 ce->card);

	/* FIXME: should we set the ce->is_new_card here or have the client code
	 * set the "is_new_card" argument on the contact editor object?
	 */

	ce->is_new_card = FALSE;
}

/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (EContactEditor *ce)
{
	g_assert (ce->app != NULL);

	gtk_widget_destroy (ce->app);
	ce->app = NULL;

	gtk_signal_emit (GTK_OBJECT (ce), contact_editor_signals[EDITOR_CLOSED]);
}

/* Menu callbacks */

/* File/Save callback */
static void
file_save_cb (GtkWidget *widget, gpointer data)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (data);
	save_card (ce);
}

/* File/Close callback */
static void
file_close_cb (GtkWidget *widget, gpointer data)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (data);
	close_dialog (ce);
}

static void
file_save_as_cb (GtkWidget *widget, gpointer data)
{
	EContactEditor *ce;
	ECard *card;

	ce = E_CONTACT_EDITOR (data);

	extract_info (ce);
	e_card_simple_sync_card (ce->simple);

	card = ce->card;
	e_contact_save_as("Save as VCard", card);
}

gboolean
e_contact_editor_confirm_delete(void)
{
	GnomeDialog *dialog;
	GladeXML *gui;
	int result;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/e-contact-editor-confirm-delete.glade", NULL);

	dialog = GNOME_DIALOG(glade_xml_get_widget(gui, "confirm-dialog"));
	
	result = gnome_dialog_run_and_close(dialog);

	gtk_object_unref(GTK_OBJECT(gui));

	return !result;
}

static void
delete_cb (GtkWidget *widget, gpointer data)
{
	EContactEditor *ce;

	if (e_contact_editor_confirm_delete()) {

		ce = E_CONTACT_EDITOR (data);

		extract_info (ce);
		e_card_simple_sync_card (ce->simple);
		
		if (!ce->is_new_card)
			gtk_signal_emit (GTK_OBJECT (ce), contact_editor_signals[DELETE_CARD],
					 ce->card);
		
		file_close_cb(widget, data);
	}
}

/* Emits the signal to request printing a card */
static void
print_cb (GtkWidget *widget, gpointer data)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (data);

	extract_info (ce);
	e_card_simple_sync_card (ce->simple);

	gtk_widget_show(e_contact_print_card_dialog_new(ce->card));
}


/* Menu bar */

static GnomeUIInfo file_new_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Appointment"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Meeting Re_quest"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Mail Message"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Contact"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Task"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Task _Request"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Journal Entry"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Note"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Ch_oose Form..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo file_page_setup_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Memo Style"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Define Print _Styles..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_NEW_SUBTREE (file_new_menu),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: S_end"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_SAVE_ITEM (file_save_cb, NULL),
	GNOMEUIINFO_MENU_SAVE_AS_ITEM (file_save_as_cb, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Save Attac_hments..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("_Delete"), NULL, delete_cb),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Move to Folder..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Cop_y to Folder..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("Page Set_up"), file_page_setup_menu),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Print Pre_view"), NULL, NULL),
	GNOMEUIINFO_MENU_PRINT_ITEM (print_cb, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_PROPERTIES_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM (file_close_cb, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo edit_object_menu[] = {
	GNOMEUIINFO_ITEM_NONE ("FIXME: what goes here?", NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo edit_menu[] = {
	GNOMEUIINFO_MENU_UNDO_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_REDO_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CUT_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_COPY_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_PASTE_ITEM (NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Paste _Special..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLEAR_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_SELECT_ALL_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Mark as U_nread"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_FIND_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_FIND_AGAIN_ITEM (NULL, NULL),
	GNOMEUIINFO_SUBTREE (N_("_Object"), edit_object_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_previous_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Unread Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Fi_rst Item in Folder"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_next_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Unread Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Last Item in Folder"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_toolbars_menu[] = {
	{ GNOME_APP_UI_TOGGLEITEM, N_("FIXME: _Standard"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_TOGGLEITEM, N_("FIXME: __Formatting"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Customize..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_menu[] = {
	GNOMEUIINFO_SUBTREE (N_("Pre_vious"), view_previous_menu),
	GNOMEUIINFO_SUBTREE (N_("Ne_xt"), view_next_menu),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("_Toolbars"), view_toolbars_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo insert_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _File..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: It_em..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Object..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo format_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Font..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Paragraph..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo tools_forms_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Ch_oose Form..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Desi_gn This Form"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: D_esign a Form..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Publish _Form..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Pu_blish Form As..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Script _Debugger"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo tools_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Spelling..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("_Forms"), tools_forms_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo actions_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _New Contact"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: New _Contact from Same Company"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: New _Letter to Contact"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: New _Message to Contact"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: New Meetin_g with Contact"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Plan a Meeting..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: New _Task for Contact"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: New _Journal Entry for Contact"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Flag for Follow Up..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Display Map of Address"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Open Web Page"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Forward as _vCard"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: For_ward"), NULL, NULL)
};

static GnomeUIInfo help_menu[] = {
	GNOMEUIINFO_ITEM_NONE ("FIXME: fix Bonobo so it supports help items!", NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE (file_menu),
	GNOMEUIINFO_MENU_EDIT_TREE (edit_menu),
	GNOMEUIINFO_MENU_VIEW_TREE (view_menu),
	GNOMEUIINFO_SUBTREE (N_("_Insert"), insert_menu),
	GNOMEUIINFO_SUBTREE (N_("F_ormat"), format_menu),
	GNOMEUIINFO_SUBTREE (N_("_Tools"), tools_menu),
	GNOMEUIINFO_SUBTREE (N_("Actio_ns"), actions_menu),
	GNOMEUIINFO_MENU_HELP_TREE (help_menu),
	GNOMEUIINFO_END
};

/* Creates the menu bar for the contact editor */
static void
create_menu (EContactEditor *ce)
{
	BonoboUIHandlerMenuItem *list;

	bonobo_ui_handler_create_menubar (ce->uih);

	list = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (main_menu, ce);
	bonobo_ui_handler_menu_add_list (ce->uih, "/", list);
}

/* Toolbar/Save and Close callback */
static void
tb_save_and_close_cb (GtkWidget *widget, gpointer data)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (data);
	save_card (ce);
	close_dialog (ce);
}

/* Toolbar */

static GnomeUIInfo toolbar[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Save and Close"),
				N_("Save the appointment and close the dialog box"),
				tb_save_and_close_cb,
				GNOME_STOCK_PIXMAP_SAVE),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Print..."),
				N_("Print this item"), print_cb, 
				GNOME_STOCK_PIXMAP_PRINT),
#if 0
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Insert File..."),
			       N_("Insert a file as an attachment"), NULL),
#endif
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Delete"),
				N_("Delete this item"), delete_cb,
				GNOME_STOCK_PIXMAP_TRASH),
	GNOMEUIINFO_SEPARATOR,
#if 0
	GNOMEUIINFO_ITEM_STOCK (N_("FIXME: Previous"),
				N_("Go to the previous item"), NULL,
				GNOME_STOCK_PIXMAP_BACK),
	GNOMEUIINFO_ITEM_STOCK (N_("FIXME: Next"),
				N_("Go to the next item"), NULL,
				GNOME_STOCK_PIXMAP_FORWARD),
#endif
	GNOMEUIINFO_ITEM_STOCK (N_("FIXME: Help"),
				N_("See online help"), NULL, GNOME_STOCK_PIXMAP_HELP),
	GNOMEUIINFO_END
};

/* Creates the toolbar for the contact editor */
static void
create_toolbar (EContactEditor *ce)
{
	BonoboUIHandlerToolbarItem *list;
	GnomeDockItem *dock_item;
	GtkWidget *toolbar_child;

	bonobo_ui_handler_create_toolbar (ce->uih, "Toolbar");

	/* Fetch the toolbar.  What a pain in the ass. */

	dock_item = gnome_app_get_dock_item_by_name (GNOME_APP (ce->app), GNOME_APP_TOOLBAR_NAME);
	g_assert (dock_item != NULL);

	toolbar_child = gnome_dock_item_get_child (dock_item);
	g_assert (toolbar_child != NULL && GTK_IS_TOOLBAR (toolbar_child));

	/* Turn off labels as GtkToolbar sucks */
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar_child), GTK_TOOLBAR_ICONS);

	list = bonobo_ui_handler_toolbar_parse_uiinfo_list_with_data (toolbar, ce);
	bonobo_ui_handler_toolbar_add_list (ce->uih, "/Toolbar", list);
}

/* Callback used when the dialog box is destroyed */
static gint
app_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (data);

	close_dialog (ce);
	return TRUE;
}

static GList *
add_to_tab_order(GList *list, GladeXML *gui, char *name)
{
	GtkWidget *widget = glade_xml_get_widget(gui, name);
	return g_list_prepend(list, widget);
}

static void
setup_tab_order(GladeXML *gui)
{
	GtkWidget *container;
	GList *list = NULL;

	container = glade_xml_get_widget(gui, "table-contact-editor-general");

	if (container) {
		list = add_to_tab_order(list, gui, "entry-fullname");
		list = add_to_tab_order(list, gui, "entry-jobtitle");
		list = add_to_tab_order(list, gui, "entry-company");
		list = add_to_tab_order(list, gui, "combo-file-as");
		list = add_to_tab_order(list, gui, "entry-phone1");
		list = add_to_tab_order(list, gui, "entry-phone2");
		list = add_to_tab_order(list, gui, "entry-phone3");
		list = add_to_tab_order(list, gui, "entry-phone4");
		list = g_list_reverse(list);
		e_container_change_tab_order(GTK_CONTAINER(container), list);
		g_list_free(list);

		list = NULL;
		list = add_to_tab_order(list, gui, "entry-email1");
		list = add_to_tab_order(list, gui, "entry-web");
		list = add_to_tab_order(list, gui, "text-address");
		list = add_to_tab_order(list, gui, "alignment-contacts");
		list = g_list_reverse(list);
		e_container_change_tab_order(GTK_CONTAINER(container), list);
		g_list_free(list);
	}
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

	e_contact_editor->arbitrary_fields = NULL;
	
	e_contact_editor->simple = e_card_simple_new(NULL);

	e_contact_editor->card = NULL;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/contact-editor.glade", NULL);
	e_contact_editor->gui = gui;

	setup_tab_order(gui);

	e_contact_editor->app = glade_xml_get_widget (gui, "contact editor");

	e_container_foreach_leaf (GTK_CONTAINER (e_contact_editor->app),
				  (GtkCallback) add_field_callback,
				  e_contact_editor);

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

	/* Build the menu and toolbar */

	e_contact_editor->uih = bonobo_ui_handler_new ();
	if (!e_contact_editor->uih) {
		g_message ("e_contact_editor_init(): eeeeek, could not create the UI handler!");
		return;
	}

	bonobo_ui_handler_set_app (e_contact_editor->uih, GNOME_APP (e_contact_editor->app));

	create_menu (e_contact_editor);
	create_toolbar (e_contact_editor);

	/* Connect to the deletion of the dialog */

	gtk_signal_connect (GTK_OBJECT (e_contact_editor->app), "delete_event",
			    GTK_SIGNAL_FUNC (app_delete_event_cb), e_contact_editor);
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

EContactEditor *
e_contact_editor_new (ECard *card, gboolean is_new_card)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (gtk_type_new (E_CONTACT_EDITOR_TYPE));

	gtk_object_set (GTK_OBJECT (ce),
			"card", card,
			"is_new_card", is_new_card,
			NULL);

	gtk_widget_show (ce->app);
	return ce;
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

	case ARG_IS_NEW_CARD:
		editor->is_new_card = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
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

	case ARG_IS_NEW_CARD:
		GTK_VALUE_BOOL (*arg) = e_contact_editor->is_new_card ? TRUE : FALSE;
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
			GtkWidget *label_widget = glade_xml_get_widget(editor->gui, label);
			if (label_widget && GTK_IS_LABEL(label_widget)) {
				gtk_object_set(GTK_OBJECT(label_widget),
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
			*list = g_list_append(*list, e_utf8_gtk_entry_get_text(GTK_ENTRY(dialog_entry)));
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
	char *oldstring = e_utf8_gtk_entry_get_text(entry);
	if (!string)
		string = "";
	if (strcmp(string, oldstring))
		e_utf8_gtk_entry_set_text(entry, string);
	g_free (oldstring);
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
		if (address && address->data) {
			gchar *u = e_utf8_to_gtk_string ((GtkWidget *) editable, address->data);
			gtk_editable_insert_text(editable, u, strlen(u), &position);
			g_free (u);
		}

		editor->address_choice = result;
	}
}

static void
add_field_callback(GtkWidget *widget, EContactEditor *editor)
{
	const char *name;
	int i;
	static const char *builtins[] = {
		"entry-fullname",
		"entry-web",
		"entry-company",
		"entry-department",
		"entry-office",
		"entry-jobtitle",
		"entry-profession",
		"entry-manager",
		"entry-assistant",
		"entry-nickname",
		"entry-spouse",
		"text-comments",
		"entry-categories",
		"entry-contacts",
		"entry-file-as",
		"dateedit-anniversary",
		"dateedit-birthday",
		"entry-phone1",
		"entry-phone2",
		"entry-phone3",
		"entry-phone4",
		"entry-email1",
		"text-address",
		"checkbutton-mailingaddress",
		"checkbutton-htmlmail",
		NULL
	};
	name = glade_get_widget_name(widget);
	if (name) {
		for (i = 0; builtins[i]; i++) {
			if (!strcmp(name, builtins[i]))
				return;
		}
		if (GTK_IS_ENTRY(widget) || GTK_IS_TEXT(widget)) {
			editor->arbitrary_fields = g_list_prepend(editor->arbitrary_fields, g_strdup(name));
		}
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
		if (value) {
			gchar *u = e_utf8_to_gtk_string ((GtkWidget *) editable, value);
			gtk_editable_insert_text(editable, u, strlen(u), &position);
			g_free (u);
		}
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
fill_in_single_field(EContactEditor *editor, char *name)
{
	ECardSimple *simple = editor->simple;
	GtkWidget *widget = glade_xml_get_widget(editor->gui, name);
	if (widget && GTK_IS_EDITABLE(widget)) {
		int position = 0;
		GtkEditable *editable = GTK_EDITABLE(widget);
		const ECardArbitrary *arbitrary;

		gtk_editable_delete_text(editable, 0, -1);
		arbitrary = e_card_simple_get_arbitrary(simple,
							name);
		if (arbitrary && arbitrary->value) {
			gchar *u = e_utf8_to_gtk_string ((GtkWidget *) editable, arbitrary->value);
			gtk_editable_insert_text(editable, u, strlen(u), &position);
			g_free (u);
		}
	}
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
		GList *list;

		gtk_object_get(GTK_OBJECT(card),
			       "file_as",       &file_as,
			       "name",          &name,
			       "anniversary",   &anniversary,
			       "birth_date",    &bday,
			       NULL);
	
		for (i = 0; i < sizeof(field_mapping) / sizeof(field_mapping[0]); i++) {
			fill_in_card_field(editor, card, field_mapping[i].id, field_mapping[i].key);
		}

		for (list = editor->arbitrary_fields; list; list = list->next) {
			fill_in_single_field(editor, list->data);
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
		char *string = e_utf8_gtk_editable_get_chars(editable, 0, -1);

		if (string && *string)
			gtk_object_set(GTK_OBJECT(card),
				       key, string,
				       NULL);
		else
			gtk_object_set(GTK_OBJECT(card),
				       key, NULL,
				       NULL);

		if (string) g_free(string);
	}
}

static void
extract_single_field(EContactEditor *editor, char *name)
{
	GtkWidget *widget = glade_xml_get_widget(editor->gui, name);
	ECardSimple *simple = editor->simple;
	if (widget && GTK_IS_EDITABLE(widget)) {
		GtkEditable *editable = GTK_EDITABLE(widget);
		char *string = e_utf8_gtk_editable_get_chars(editable, 0, -1);

		if (string && *string)
			e_card_simple_set_arbitrary(simple,
						    name,
						    NULL,
						    string);
		else
			e_card_simple_set_arbitrary(simple,
						    name,
						    NULL,
						    NULL);
		if (string) g_free(string);
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
		GList *list;

		widget = glade_xml_get_widget(editor->gui, "entry-file-as");
		if (widget && GTK_IS_EDITABLE(widget)) {
			GtkEditable *editable = GTK_EDITABLE(widget);
			char *string = e_utf8_gtk_editable_get_chars(editable, 0, -1);

			if (string && *string)
				gtk_object_set(GTK_OBJECT(card),
					       "file_as", string,
					       NULL);

			if (string) g_free(string);
		}

		for (i = 0; i < sizeof(field_mapping) / sizeof(field_mapping[0]); i++) {
			extract_field(editor, card, field_mapping[i].id, field_mapping[i].key);
		}

		for (list = editor->arbitrary_fields; list; list = list->next) {
			extract_single_field(editor, list->data);
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
