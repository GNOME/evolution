/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-list-editor.c
 * Copyright (C) 2001  Ximian, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
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

#include "e-contact-list-editor.h"

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-window-icon.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-ui-util.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/widgets/e-unicode.h>
#include "shell/evolution-shell-component-utils.h"

#include "addressbook/gui/widgets/e-addressbook-util.h"

#include "e-contact-editor.h"
#include "e-contact-save-as.h"
#include "e-contact-list-model.h"

/* Signal IDs */
enum {
	LIST_ADDED,
	LIST_MODIFIED,
	LIST_DELETED,
	EDITOR_CLOSED,
	LAST_SIGNAL
};

static void e_contact_list_editor_init		(EContactListEditor		 *editor);
static void e_contact_list_editor_class_init	(EContactListEditorClass	 *klass);
static void e_contact_list_editor_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_contact_list_editor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_contact_list_editor_destroy (GtkObject *object);

static void create_ui (EContactListEditor *ce);
static void set_editable (EContactListEditor *editor);
static void command_state_changed (EContactListEditor *editor);
static void close_dialog (EContactListEditor *cle);
static void extract_info(EContactListEditor *editor);
static void fill_in_info(EContactListEditor *editor);

static void add_email_cb (GtkWidget *w, EContactListEditor *editor);
static void remove_entry_cb (GtkWidget *w, EContactListEditor *editor);
static void list_name_changed_cb (GtkWidget *w, EContactListEditor *editor);
static void visible_addrs_toggled_cb (GtkWidget *w, EContactListEditor *editor);

static gint app_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data);
static gboolean table_drag_drop_cb (ETable *table, int row, int col, GdkDragContext *context,
				    gint x, gint y, guint time, EContactListEditor *editor);
static gboolean table_drag_motion_cb (ETable *table, int row, int col, GdkDragContext *context,
				      gint x, gint y, guint time, EContactListEditor *editor);
static void table_drag_data_received_cb (ETable *table, int row, int col,
					 GdkDragContext *context,
					 gint x, gint y,
					 GtkSelectionData *selection_data, guint info, guint time,
					 EContactListEditor *editor);

static GtkObjectClass *parent_class = NULL;

static guint contact_list_editor_signals[LAST_SIGNAL];

enum DndTargetType {
	DND_TARGET_TYPE_VCARD,
};
#define VCARD_TYPE "text/x-vcard"
static GtkTargetEntry drag_types[] = {
	{ VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD },
};
static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

/* The arguments we take */
enum {
	ARG_0,
	ARG_BOOK,
	ARG_CARD,
	ARG_IS_NEW_LIST,
	ARG_EDITABLE
};

static GSList *all_contact_list_editors = NULL;

GtkType
e_contact_list_editor_get_type (void)
{
  static GtkType contact_list_editor_type = 0;

  if (!contact_list_editor_type)
    {
      static const GtkTypeInfo contact_list_editor_info =
      {
        "EContactListEditor",
        sizeof (EContactListEditor),
        sizeof (EContactListEditorClass),
        (GtkClassInitFunc) e_contact_list_editor_class_init,
        (GtkObjectInitFunc) e_contact_list_editor_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      contact_list_editor_type = gtk_type_unique (GTK_TYPE_OBJECT, &contact_list_editor_info);
    }

  return contact_list_editor_type;
}


typedef void (*GtkSignal_NONE__INT_OBJECT) (GtkObject * object,
					    gint arg1,
					    GtkObject *arg2,
					    gpointer user_data);

static void
e_marshal_NONE__INT_OBJECT (GtkObject * object,
			    GtkSignalFunc func,
			    gpointer func_data, GtkArg * args)
{
	GtkSignal_NONE__INT_OBJECT rfunc;
	rfunc = (GtkSignal_NONE__INT_OBJECT) func;
	(*rfunc) (object,
		  GTK_VALUE_INT (args[0]),
		  GTK_VALUE_OBJECT (args[1]),
		  func_data);
}

static void
e_contact_list_editor_class_init (EContactListEditorClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  gtk_object_add_arg_type ("EContactListEditor::book", GTK_TYPE_OBJECT, 
			   GTK_ARG_READWRITE, ARG_BOOK);
  gtk_object_add_arg_type ("EContactListEditor::card", GTK_TYPE_OBJECT, 
			   GTK_ARG_READWRITE, ARG_CARD);
  gtk_object_add_arg_type ("EContactListEditor::is_new_list", GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE, ARG_IS_NEW_LIST);
  gtk_object_add_arg_type ("EContactListEditor::editable", GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE, ARG_EDITABLE);

  contact_list_editor_signals[LIST_ADDED] =
	  gtk_signal_new ("list_added",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactListEditorClass, list_added),
			  e_marshal_NONE__INT_OBJECT,
			  GTK_TYPE_NONE, 2,
			  GTK_TYPE_INT, GTK_TYPE_OBJECT);

  contact_list_editor_signals[LIST_MODIFIED] =
	  gtk_signal_new ("list_modified",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactListEditorClass, list_modified),
			  e_marshal_NONE__INT_OBJECT,
			  GTK_TYPE_NONE, 2,
			  GTK_TYPE_INT, GTK_TYPE_OBJECT);

  contact_list_editor_signals[LIST_DELETED] =
	  gtk_signal_new ("list_deleted",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactListEditorClass, list_deleted),
			  e_marshal_NONE__INT_OBJECT,
			  GTK_TYPE_NONE, 2,
			  GTK_TYPE_INT, GTK_TYPE_OBJECT);

  contact_list_editor_signals[EDITOR_CLOSED] =
	  gtk_signal_new ("editor_closed",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EContactListEditorClass, editor_closed),
			  gtk_marshal_NONE__NONE,
			  GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, contact_list_editor_signals, LAST_SIGNAL);
 
  object_class->set_arg = e_contact_list_editor_set_arg;
  object_class->get_arg = e_contact_list_editor_get_arg;
  object_class->destroy = e_contact_list_editor_destroy;
}

static void
e_contact_list_editor_init (EContactListEditor *editor)
{
	GladeXML *gui;
	GtkWidget *bonobo_win;
	BonoboUIContainer *container;
	char *icon_path;

	editor->card = NULL;
	editor->changed = FALSE;
	editor->editable = TRUE;
	editor->in_async_call = FALSE;
	editor->is_new_list = FALSE;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/contact-list-editor.glade", NULL);
	editor->gui = gui;

	editor->app = glade_xml_get_widget (gui, "contact list editor");

	editor->table = glade_xml_get_widget (gui, "contact-list-table");
	editor->model = gtk_object_get_data (GTK_OBJECT(editor->table), "model");

	editor->add_button = glade_xml_get_widget (editor->gui, "add-email-button");
	editor->remove_button = glade_xml_get_widget (editor->gui, "remove-button");

	editor->email_entry = glade_xml_get_widget (gui, "email-entry");
	editor->list_name_entry = glade_xml_get_widget (gui, "list-name-entry");

	editor->visible_addrs_checkbutton = glade_xml_get_widget (gui, "visible-addrs-checkbutton");

	/* Construct the app */
	bonobo_win = bonobo_window_new ("contact-list-editor", _("Contact List Editor"));

	/* FIXME: The sucking bit */
	{
		GtkWidget *contents;

		contents = gnome_dock_get_client_area (
			GNOME_DOCK (GNOME_APP (editor->app)->dock));
		if (!contents) {
			g_message ("contact_list_editor_construct(): Could not get contents");
			return;
		}
		gtk_widget_ref (contents);
		gtk_container_remove (GTK_CONTAINER (contents->parent), contents);
		bonobo_window_set_contents (BONOBO_WINDOW (bonobo_win), contents);
		gtk_widget_destroy (editor->app);
		editor->app = bonobo_win;
	}

	/* Build the menu and toolbar */

	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WINDOW (editor->app));

	editor->uic = bonobo_ui_component_new_default ();
	if (!editor->uic) {
		g_message ("e_contact_list_editor_init(): eeeeek, could not create the UI handler!");
		return;
	}
	bonobo_ui_component_set_container (editor->uic,
					   bonobo_object_corba_objref (
								       BONOBO_OBJECT (container)));

	create_ui (editor);

	/* connect signals */
	gtk_signal_connect (GTK_OBJECT(editor->add_button),
			    "clicked", GTK_SIGNAL_FUNC(add_email_cb), editor);
	gtk_signal_connect (GTK_OBJECT(editor->email_entry),
			    "activate", GTK_SIGNAL_FUNC(add_email_cb), editor);
	gtk_signal_connect (GTK_OBJECT(editor->remove_button),
			    "clicked", GTK_SIGNAL_FUNC(remove_entry_cb), editor);
	gtk_signal_connect (GTK_OBJECT(editor->list_name_entry),
			    "changed", GTK_SIGNAL_FUNC(list_name_changed_cb), editor);
	gtk_signal_connect (GTK_OBJECT(editor->visible_addrs_checkbutton),
			    "toggled", GTK_SIGNAL_FUNC(visible_addrs_toggled_cb), editor);

	e_table_drag_dest_set (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			       0, drag_types, num_drag_types, GDK_ACTION_LINK);

	gtk_signal_connect (GTK_OBJECT(e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table))),
			    "table_drag_motion", GTK_SIGNAL_FUNC(table_drag_motion_cb), editor);
	gtk_signal_connect (GTK_OBJECT(e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table))),
			    "table_drag_drop", GTK_SIGNAL_FUNC(table_drag_drop_cb), editor);
	gtk_signal_connect (GTK_OBJECT(e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table))),
			    "table_drag_data_received", GTK_SIGNAL_FUNC(table_drag_data_received_cb), editor);

	command_state_changed (editor);

	/* Connect to the deletion of the dialog */

	gtk_signal_connect (GTK_OBJECT (editor->app), "delete_event",
			    GTK_SIGNAL_FUNC (app_delete_event_cb), editor);

	/* set the icon */
	icon_path = g_concat_dir_and_file (EVOLUTION_ICONSDIR, "contact-list-16.png");
	gnome_window_icon_set_from_file (GTK_WINDOW (editor->app), icon_path);
	g_free (icon_path);
}

static void
e_contact_list_editor_destroy (GtkObject *object)
{
}

typedef struct {
	EContactListEditor *cle;
	gboolean should_close;
} EditorCloseStruct;

static void
list_added_cb (EBook *book, EBookStatus status, const char *id, EditorCloseStruct *ecs)
{
	EContactListEditor *cle = ecs->cle;
	gboolean should_close = ecs->should_close;

	if (cle->app)
		gtk_widget_set_sensitive (cle->app, TRUE);
	cle->in_async_call = FALSE;

	e_card_set_id (cle->card, id);

	gtk_signal_emit (GTK_OBJECT (cle), contact_list_editor_signals[LIST_ADDED],
			 status, cle->card);

	if (status == E_BOOK_STATUS_SUCCESS) {
		cle->is_new_list = FALSE;

		if (should_close)
			close_dialog (cle);
		else
			command_state_changed (cle);
	}

	gtk_object_unref (GTK_OBJECT (cle));
	g_free (ecs);
}

static void
list_modified_cb (EBook *book, EBookStatus status, EditorCloseStruct *ecs)
{
	EContactListEditor *cle = ecs->cle;
	gboolean should_close = ecs->should_close;

	if (cle->app)
		gtk_widget_set_sensitive (cle->app, TRUE);
	cle->in_async_call = FALSE;

	gtk_signal_emit (GTK_OBJECT (cle), contact_list_editor_signals[LIST_MODIFIED],
			 status, cle->card);

	if (status == E_BOOK_STATUS_SUCCESS) {
		if (should_close)
			close_dialog (cle);
	}

	gtk_object_unref (GTK_OBJECT (cle)); /* release ref held for ebook callback */
	g_free (ecs);
}

static void
save_card (EContactListEditor *cle, gboolean should_close)
{
	extract_info (cle);

	if (cle->book) {
		EditorCloseStruct *ecs = g_new(EditorCloseStruct, 1);
		
		ecs->cle = cle;
		gtk_object_ref (GTK_OBJECT (cle));
		ecs->should_close = should_close;

		if (cle->app)
			gtk_widget_set_sensitive (cle->app, FALSE);
		cle->in_async_call = TRUE;

		if (cle->is_new_list)
			e_book_add_card (cle->book, cle->card, GTK_SIGNAL_FUNC(list_added_cb), ecs);
		else
			e_book_commit_card (cle->book, cle->card, GTK_SIGNAL_FUNC(list_modified_cb), ecs);

		cle->changed = FALSE;
	}
}

static gboolean
is_named (EContactListEditor *editor)
{
	char *string = e_utf8_gtk_editable_get_chars(GTK_EDITABLE (editor->list_name_entry), 0, -1);
	gboolean named = FALSE;

	if (string && *string) {
		named = TRUE;
	}

	g_free (string);

	return named;
}

static gboolean
prompt_to_save_changes (EContactListEditor *editor)
{
	if (!editor->changed || !is_named (editor))
		return TRUE;

	switch (e_addressbook_prompt_save_dialog (GTK_WINDOW(editor->app))) {
	case 0: /* Save */
		save_card (editor, FALSE);
		return TRUE;
	case 1: /* Discard */
		return TRUE;
	case 2: /* Cancel */
	default:
		return FALSE;
	}
}

static void
file_close_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);

	if (!prompt_to_save_changes (cle))
		return;

	close_dialog (cle);
}

static void
file_save_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);

	save_card (cle, FALSE);
}

static void
file_save_as_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);

	extract_info (cle);

	e_contact_save_as(_("Save List as VCard"), cle->card, GTK_WINDOW (cle->app));
}

static void
file_send_as_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);

	extract_info (cle);

	e_card_send(cle->card, E_CARD_DISPOSITION_AS_ATTACHMENT);
}

static void
file_send_to_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);

	extract_info (cle);

	e_card_send(cle->card, E_CARD_DISPOSITION_AS_TO);
}

static void
tb_save_and_close_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);
	save_card (cle, TRUE);
}

static void
list_deleted_cb (EBook *book, EBookStatus status, EContactListEditor *cle)
{
	if (cle->app)
		gtk_widget_set_sensitive (cle->app, TRUE);
	cle->in_async_call = FALSE;

	gtk_signal_emit (GTK_OBJECT (cle), contact_list_editor_signals[LIST_DELETED],
			 status, cle->card);

	/* always close the dialog after we successfully delete a list */
	if (status == E_BOOK_STATUS_SUCCESS)
		close_dialog (cle);

	gtk_object_unref (GTK_OBJECT (cle)); /* release reference held for callback */
}

static void
delete_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);
	ECard *card = cle->card;

	gtk_object_ref(GTK_OBJECT(card));

	if (e_contact_editor_confirm_delete(GTK_WINDOW(cle->app))) {

		extract_info (cle);
		
		if (!cle->is_new_list) {
			gtk_widget_set_sensitive (cle->app, FALSE);
			cle->in_async_call = TRUE;
			
			gtk_object_ref (GTK_OBJECT (cle)); /* hold reference for callback */
			e_book_remove_card (cle->book, card, GTK_SIGNAL_FUNC(list_deleted_cb), cle);
		}
	}

	gtk_object_unref(GTK_OBJECT(card));
}

static
BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ContactListEditorSave", file_save_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactListEditorSaveClose", tb_save_and_close_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactListEditorDelete", delete_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactListEditorSaveAs", file_save_as_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactListEditorSendAs", file_send_as_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactListEditorSendTo", file_send_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactListEditorClose", file_close_cb),
	BONOBO_UI_VERB_END
};

static EPixmap pixmaps[] = {
	E_PIXMAP ("/commands/ContactListEditorSave", "save-16.png"),
	E_PIXMAP ("/commands/ContactListEditorSaveClose", "save-16.png"),
	E_PIXMAP ("/commands/ContactListEditorSaveAs", "save-as-16.png"),

	E_PIXMAP ("/commands/ContactListEditorDelete", "evolution-trash-mini.png"),
#if 0 /* Envelope printing is disabled for Evolution 1.0. */
	E_PIXMAP ("/commands/ContactListEditorPrint", "print.xpm"),
	E_PIXMAP ("/commands/ContactListEditorPrintEnvelope", "print.xpm"),
#endif
	E_PIXMAP ("/Toolbar/ContactListEditorSaveClose", "buttons/save-24.png"),
	E_PIXMAP ("/Toolbar/ContactListEditorDelete", "buttons/delete-message.png"),
	E_PIXMAP ("/Toolbar/ContactListEditorPrint", "buttons/print.png"),

	E_PIXMAP_END
};

static void
create_ui (EContactListEditor *ce)
{
	bonobo_ui_component_add_verb_list_with_data (
		ce->uic, verbs, ce);

	bonobo_ui_util_set_ui (ce->uic, EVOLUTION_DATADIR,
			       "evolution-contact-list-editor.xml",
			       "evolution-contact-list-editor");

	e_pixmaps_update (ce->uic, pixmaps);
}

static void
contact_list_editor_destroy_notify (void *data)
{
	EContactListEditor *ce = E_CONTACT_LIST_EDITOR (data);

	all_contact_list_editors = g_slist_remove (all_contact_list_editors, ce);
}

EContactListEditor *
e_contact_list_editor_new (EBook *book,
			   ECard *list_card,
			   gboolean is_new_list,
			   gboolean editable)
{
	EContactListEditor *ce;

	ce = E_CONTACT_LIST_EDITOR (gtk_type_new (E_CONTACT_LIST_EDITOR_TYPE));

	all_contact_list_editors = g_slist_prepend (all_contact_list_editors, ce);
	gtk_object_weakref (GTK_OBJECT (ce), contact_list_editor_destroy_notify, ce);

	gtk_object_set (GTK_OBJECT (ce),
			"book", book,
			"card", list_card,
			"is_new_list", is_new_list,
			"editable", editable,
			NULL);

	return ce;
}

static void
e_contact_list_editor_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EContactListEditor *editor;

	editor = E_CONTACT_LIST_EDITOR (o);
	
	switch (arg_id){
	case ARG_BOOK:
		if (editor->book)
			gtk_object_unref(GTK_OBJECT(editor->book));
		editor->book = E_BOOK(GTK_VALUE_OBJECT (*arg));
		gtk_object_ref (GTK_OBJECT (editor->book));
		/* XXX more here about editable/etc. */
		break;
	case ARG_CARD:
		if (editor->card)
			gtk_object_unref(GTK_OBJECT(editor->card));
		editor->card = e_card_duplicate(E_CARD(GTK_VALUE_OBJECT (*arg)));
		fill_in_info(editor);
		editor->changed = FALSE;
		command_state_changed (editor);
		break;

	case ARG_IS_NEW_LIST: {
		gboolean new_value = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		gboolean changed = (editor->is_new_list != new_value);

		editor->is_new_list = new_value;

		if (changed)
			command_state_changed (editor);
		break;
	}

	case ARG_EDITABLE: {
		gboolean new_value = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		gboolean changed = (editor->editable != new_value);

		editor->editable = new_value;

		if (changed) {
			set_editable (editor);
			command_state_changed (editor);
		}
		break;
	}

	}
}

static void
e_contact_list_editor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EContactListEditor *editor;

	editor = E_CONTACT_LIST_EDITOR (object);

	switch (arg_id) {
	case ARG_BOOK:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(editor->book);
		break;

	case ARG_CARD:
		extract_info(editor);
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(editor->card);
		break;

	case ARG_IS_NEW_LIST:
		GTK_VALUE_BOOL (*arg) = editor->is_new_list ? TRUE : FALSE;
		break;

	case ARG_EDITABLE:
		GTK_VALUE_BOOL (*arg) = editor->editable ? TRUE : FALSE;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

void
e_contact_list_editor_show (EContactListEditor *editor)
{
	gtk_widget_show (editor->app);
}

void
e_contact_list_editor_raise (EContactListEditor *editor)
{
	gdk_window_raise (GTK_WIDGET (editor->app)->window);
}

#define SPEC "<ETableSpecification no-headers=\"true\" cursor-mode=\"line\" selection-mode=\"single\"> \
 <ETableColumn model_col= \"0\" _title=\"Contact\" expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\" compare=\"string\" /> \
	<ETableState>                   			       \
		<column source=\"0\"/>     			       \
	        <grouping> </grouping>				       \
	</ETableState>                  			       \
</ETableSpecification>"

GtkWidget *
e_contact_list_editor_create_table(gchar *name,
				   gchar *string1, gchar *string2,
				   gint int1, gint int2);

GtkWidget *
e_contact_list_editor_create_table(gchar *name,
				   gchar *string1, gchar *string2,
				   gint int1, gint int2)
{
	
	ETableModel *model;
	GtkWidget *table;

	model = e_contact_list_model_new ();

	table = e_table_scrolled_new (model, NULL, SPEC, NULL);

	gtk_object_set_data(GTK_OBJECT(table), "model", model);

	return table;
}

static void
add_email_cb (GtkWidget *w, EContactListEditor *editor)
{
	GtkAdjustment *adj = e_scroll_frame_get_vadjustment (E_SCROLL_FRAME (editor->table));
	char *text = gtk_entry_get_text (GTK_ENTRY(editor->email_entry));

	if (text && *text) {
		e_contact_list_model_add_email (E_CONTACT_LIST_MODEL(editor->model), text);

		/* Skip to the end of the list */
		if (adj->upper - adj->lower > adj->page_size)
			gtk_adjustment_set_value (adj, adj->upper);
	}

	gtk_entry_set_text (GTK_ENTRY(editor->email_entry), "");

	editor->changed = TRUE;

	command_state_changed (editor);
}

static void
remove_row (int model_row, EContactListEditor *editor)
{
	e_contact_list_model_remove_row (E_CONTACT_LIST_MODEL (editor->model), model_row);
}

static void
remove_entry_cb (GtkWidget *w, EContactListEditor *editor)
{
	e_table_selected_row_foreach (e_table_scrolled_get_table(E_TABLE_SCROLLED(editor->table)),
				      (EForeachFunc)remove_row, editor);
	editor->changed = TRUE;
	command_state_changed (editor);
}

static void
list_name_changed_cb (GtkWidget *w, EContactListEditor *editor)
{
	editor->changed = TRUE;
	command_state_changed (editor);
}

static void
visible_addrs_toggled_cb (GtkWidget *w, EContactListEditor *editor)
{
	editor->changed = TRUE;
	command_state_changed (editor);
}

static void
set_editable (EContactListEditor *editor)
{
	gtk_widget_set_sensitive (editor->email_entry, editor->editable);
	gtk_widget_set_sensitive (editor->list_name_entry, editor->editable);
	gtk_widget_set_sensitive (editor->add_button, editor->editable);
	gtk_widget_set_sensitive (editor->remove_button, editor->editable);
	gtk_widget_set_sensitive (editor->table, editor->editable);
}

/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (EContactListEditor *cle)
{
	g_assert (cle->app != NULL);

	gtk_widget_destroy (cle->app);
	cle->app = NULL;

	gtk_signal_emit (GTK_OBJECT (cle), contact_list_editor_signals[EDITOR_CLOSED]);
}

/* Callback used when the editor is destroyed */
static gint
app_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	EContactListEditor *ce;

	ce = E_CONTACT_LIST_EDITOR (data);

	/* if we're in an async call, don't allow the dialog to close */
	if (ce->in_async_call)
		return TRUE;

	if (!prompt_to_save_changes (ce))
		return TRUE;

	close_dialog (ce);
	return TRUE;
}

static gboolean
table_drag_motion_cb (ETable *table, int row, int col,
		      GdkDragContext *context,
		      gint x, gint y, guint time, EContactListEditor *editor)
{
	GList *p;

	for (p = context->targets; p != NULL; p = p->next) {
		char *possible_type;

		possible_type = gdk_atom_name ((GdkAtom) p->data);
		if (!strcmp (possible_type, VCARD_TYPE)) {
			g_free (possible_type);
			gdk_drag_status (context, GDK_ACTION_LINK, time);
			return TRUE;
		}

		g_free (possible_type);
	}

	return FALSE;
}

static gboolean
table_drag_drop_cb (ETable *table, int row, int col,
		    GdkDragContext *context,
		    gint x, gint y, guint time, EContactListEditor *editor)
{
	if (context->targets != NULL) {
		gtk_drag_get_data (GTK_WIDGET (table), context,
				   GPOINTER_TO_INT (context->targets->data),
				   time);
		return TRUE;
	}

	return FALSE;
}

static void
table_drag_data_received_cb (ETable *table, int row, int col,
			     GdkDragContext *context,
			     gint x, gint y,
			     GtkSelectionData *selection_data,
			     guint info, guint time, EContactListEditor *editor)
{
	GtkAdjustment *adj = e_scroll_frame_get_vadjustment (E_SCROLL_FRAME (editor->table));
	char *target_type;
	gboolean changed = FALSE;

	target_type = gdk_atom_name (selection_data->target);

	if (!strcmp (target_type, VCARD_TYPE)) {

		GList *card_list = e_card_load_cards_from_string_with_default_charset (selection_data->data, "ISO-8859-1");
		GList *c;

		for (c = card_list; c; c = c->next) {
			ECard *ecard = c->data;

			if (!e_card_evolution_list (ecard)) {
				ECardSimple *simple = e_card_simple_new (ecard);

				e_contact_list_model_add_card (E_CONTACT_LIST_MODEL (editor->model),
							       simple);

				gtk_object_unref (GTK_OBJECT (simple));

				changed = TRUE;
			}
		}
		g_list_foreach (card_list, (GFunc)gtk_object_unref, NULL);
		g_list_free (card_list);

		/* Skip to the end of the list */
		if (adj->upper - adj->lower > adj->page_size)
			gtk_adjustment_set_value (adj, adj->upper);
	}

	if (changed) {
		editor->changed = TRUE;
		command_state_changed (editor);
	}
}

static void
command_state_changed (EContactListEditor *editor)
{
	gboolean named = is_named (editor);

	bonobo_ui_component_set_prop (editor->uic,
				      "/commands/ContactListEditorSaveClose",
				      "sensitive",
				      editor->changed && named && editor->editable ? "1" : "0", NULL);

	bonobo_ui_component_set_prop (editor->uic,
				      "/commands/ContactListEditorSave",
				      "sensitive",
				      editor->changed && named && editor->editable ? "1" : "0", NULL);

	bonobo_ui_component_set_prop (editor->uic,
				      "/commands/ContactListEditorDelete",
				      "sensitive",
				      editor->editable && !editor->is_new_list ? "1" : "0", NULL);
}

static void
extract_info(EContactListEditor *editor)
{
	ECard *card = editor->card;
	if (card) {
		int i;
		EList *email_list;
		EIterator *email_iter;
		char *string = e_utf8_gtk_editable_get_chars(GTK_EDITABLE (editor->list_name_entry), 0, -1);

		if (string && *string)
			gtk_object_set(GTK_OBJECT(card),
				       "file_as", string,
				       "full_name", string,
				       NULL);

		g_free (string);

		
		gtk_object_set (GTK_OBJECT(card),
				"list", GINT_TO_POINTER (TRUE),
				"list_show_addresses",
				GINT_TO_POINTER (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(editor->visible_addrs_checkbutton))),
				NULL);

		gtk_object_get (GTK_OBJECT(card),
				"email", &email_list,
				NULL);

		/* clear the email list */
		email_iter = e_list_get_iterator (email_list);
		e_iterator_last (email_iter);
		while (e_iterator_is_valid (E_ITERATOR (email_iter))) {
			e_iterator_delete (E_ITERATOR (email_iter));
		}
		gtk_object_unref (GTK_OBJECT (email_iter));

		/* then refill it from the contact list model */
		for (i = 0; i < e_table_model_row_count (editor->model); i ++) {
			const EDestination *dest = e_contact_list_model_get_destination (E_CONTACT_LIST_MODEL (editor->model), i);
			gchar *dest_xml = e_destination_export (dest);
			if (dest_xml) {
				e_list_append (email_list, dest_xml);
			}
			g_free (dest_xml);
		}
	}
}

static void
fill_in_info(EContactListEditor *editor)
{
	if (editor->card) {
		char *file_as;
		gboolean show_addresses = FALSE;
		gboolean is_evolution_list = FALSE;
		EList *email_list;
		EIterator *email_iter;

		gtk_object_get (GTK_OBJECT (editor->card),
				"file_as", &file_as,
				"email", &email_list,
				"list", &is_evolution_list,
				"list_show_addresses", &show_addresses,
				NULL);

		gtk_editable_delete_text (GTK_EDITABLE (editor->list_name_entry), 0, -1);
		if (file_as) {
			int position = 0;
			gchar *u = e_utf8_to_gtk_string (editor->list_name_entry, file_as);
			gtk_editable_insert_text (GTK_EDITABLE (editor->list_name_entry), u, strlen (u), &position);
			g_free (u);
		}

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(editor->visible_addrs_checkbutton), !show_addresses);

		e_contact_list_model_remove_all (E_CONTACT_LIST_MODEL (editor->model));

		email_iter = e_list_get_iterator (email_list);
		
		while (e_iterator_is_valid (email_iter)) {
			const char *dest_xml = e_iterator_get (email_iter);
			EDestination *dest;

			/* g_message ("incoming xml: [%s]", dest_xml); */
			dest = e_destination_import (dest_xml);

			if (dest != NULL) {
				e_contact_list_model_add_destination (E_CONTACT_LIST_MODEL (editor->model), dest);
			}

			e_iterator_next (email_iter);
		}
	}
}


gboolean
e_contact_list_editor_request_close_all (void)
{
	GSList *p;
	GSList *pnext;
	gboolean retval;

	retval = TRUE;
	for (p = all_contact_list_editors; p != NULL; p = pnext) {
		pnext = p->next;

		e_contact_list_editor_raise (E_CONTACT_LIST_EDITOR (p->data));
		if (! prompt_to_save_changes (E_CONTACT_LIST_EDITOR (p->data))) {
			retval = FALSE;
			break;
		}

		close_dialog (E_CONTACT_LIST_EDITOR (p->data));
	}

	return retval;
}
