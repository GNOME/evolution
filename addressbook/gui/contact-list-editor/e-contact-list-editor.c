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

#include <string.h>
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-window-icon.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-window.h>
#include <gal/e-table/e-table-scrolled.h>
#include "shell/evolution-shell-component-utils.h"

#include "addressbook/gui/widgets/e-addressbook-util.h"

#include "e-contact-editor.h"
#include "e-contact-save-as.h"
#include "e-contact-list-model.h"
#include "e-contact-list-editor-marshal.h"

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
static void e_contact_list_editor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_contact_list_editor_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_contact_list_editor_dispose (GObject *object);

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
	PROP_0,
	PROP_BOOK,
	PROP_CARD,
	PROP_IS_NEW_LIST,
	PROP_EDITABLE
};

static GSList *all_contact_list_editors = NULL;

GType
e_contact_list_editor_get_type (void)
{
	static GType cle_type = 0;

	if (!cle_type) {
		static const GTypeInfo cle_info =  {
			sizeof (EContactListEditorClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_contact_list_editor_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EContactListEditor),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_contact_list_editor_init,
		};

		cle_type = g_type_register_static (GTK_TYPE_OBJECT, "EContactListEditor", &cle_info, 0);
	}

	return cle_type;
}


static void
e_contact_list_editor_class_init (EContactListEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (GTK_TYPE_OBJECT);

	object_class->set_property = e_contact_list_editor_set_property;
	object_class->get_property = e_contact_list_editor_get_property;
	object_class->dispose = e_contact_list_editor_dispose;

	g_object_class_install_property (object_class, PROP_BOOK, 
					 g_param_spec_object ("book",
							      _("Book"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CARD, 
					 g_param_spec_object ("card",
							      _("Card"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_CARD,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_IS_NEW_LIST, 
					 g_param_spec_boolean ("is_new_list",
							       _("Is New List"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE, 
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	contact_list_editor_signals[LIST_ADDED] =
		g_signal_new ("list_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EContactListEditorClass, list_added),
			      NULL, NULL,
			      ecle_marshal_NONE__INT_OBJECT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_OBJECT);

	contact_list_editor_signals[LIST_MODIFIED] =
		g_signal_new ("list_modified",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EContactListEditorClass, list_modified),
			      NULL, NULL,
			      ecle_marshal_NONE__INT_OBJECT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_OBJECT);

	contact_list_editor_signals[LIST_DELETED] =
		g_signal_new ("list_deleted",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EContactListEditorClass, list_deleted),
			      NULL, NULL,
			      ecle_marshal_NONE__INT_OBJECT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_OBJECT);

	contact_list_editor_signals[EDITOR_CLOSED] =
		g_signal_new ("editor_closed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EContactListEditorClass, editor_closed),
			      NULL, NULL,
			      ecle_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
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

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/contact-list-editor.glade", NULL, NULL);
	editor->gui = gui;

	editor->app = glade_xml_get_widget (gui, "contact list editor");

	editor->table = glade_xml_get_widget (gui, "contact-list-table");
	editor->model = g_object_get_data (G_OBJECT(editor->table), "model");

	/* XXX need this for libglade-2 it seems */
	gtk_widget_show (editor->table);

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

		contents = bonobo_dock_get_client_area (gnome_app_get_dock (GNOME_APP (editor->app)));

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

	container = bonobo_window_get_ui_container (BONOBO_WINDOW (editor->app));

	editor->uic = bonobo_ui_component_new_default ();
	if (!editor->uic) {
		g_message ("e_contact_list_editor_init(): eeeeek, could not create the UI handler!");
		return;
	}
	bonobo_ui_component_set_container (editor->uic,
					   bonobo_object_corba_objref (
								       BONOBO_OBJECT (container)), NULL);

	create_ui (editor);

	/* connect signals */
	g_signal_connect (editor->add_button,
			  "clicked", G_CALLBACK(add_email_cb), editor);
	g_signal_connect (editor->email_entry,
			  "activate", G_CALLBACK(add_email_cb), editor);
	g_signal_connect (editor->remove_button,
			  "clicked", G_CALLBACK(remove_entry_cb), editor);
	g_signal_connect (editor->list_name_entry,
			  "changed", G_CALLBACK(list_name_changed_cb), editor);
	g_signal_connect (editor->visible_addrs_checkbutton,
			  "toggled", G_CALLBACK(visible_addrs_toggled_cb), editor);

	e_table_drag_dest_set (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			       0, drag_types, num_drag_types, GDK_ACTION_LINK);

	g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			  "table_drag_motion", G_CALLBACK(table_drag_motion_cb), editor);
	g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			  "table_drag_drop", G_CALLBACK (table_drag_drop_cb), editor);
	g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			  "table_drag_data_received", G_CALLBACK(table_drag_data_received_cb), editor);

	command_state_changed (editor);

	/* Connect to the deletion of the dialog */

	g_signal_connect (editor->app, "delete_event",
			  G_CALLBACK (app_delete_event_cb), editor);

	/* set the icon */
	icon_path = g_build_filename (EVOLUTION_IMAGESDIR, "contact-list-16.png", NULL);
	gnome_window_icon_set_from_file (GTK_WINDOW (editor->app), icon_path);
	g_free (icon_path);
}

static void
e_contact_list_editor_dispose (GObject *object)
{
	/* XXX need to call parent dispose */
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

	g_signal_emit (cle, contact_list_editor_signals[LIST_ADDED], 0,
		       status, cle->card);

	if (status == E_BOOK_STATUS_SUCCESS) {
		cle->is_new_list = FALSE;

		if (should_close)
			close_dialog (cle);
		else
			command_state_changed (cle);
	}

	g_object_unref (cle);
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

	g_signal_emit (cle, contact_list_editor_signals[LIST_MODIFIED], 0,
		       status, cle->card);

	if (status == E_BOOK_STATUS_SUCCESS) {
		if (should_close)
			close_dialog (cle);
	}

	g_object_unref (cle); /* release ref held for ebook callback */
	g_free (ecs);
}

static void
save_card (EContactListEditor *cle, gboolean should_close)
{
	extract_info (cle);

	if (cle->book) {
		EditorCloseStruct *ecs = g_new(EditorCloseStruct, 1);
		
		ecs->cle = cle;
		g_object_ref (cle);
		ecs->should_close = should_close;

		if (cle->app)
			gtk_widget_set_sensitive (cle->app, FALSE);
		cle->in_async_call = TRUE;

		if (cle->is_new_list)
			e_book_add_card (cle->book, cle->card, (EBookIdCallback)list_added_cb, ecs);
		else
			e_book_commit_card (cle->book, cle->card, (EBookCallback)list_modified_cb, ecs);

		cle->changed = FALSE;
	}
}

static gboolean
is_named (EContactListEditor *editor)
{
	char *string = gtk_editable_get_chars(GTK_EDITABLE (editor->list_name_entry), 0, -1);
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
	case GTK_RESPONSE_YES:
		save_card (editor, FALSE);
		return TRUE;
	case GTK_RESPONSE_NO:
		return TRUE;
	case GTK_RESPONSE_CANCEL:
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

	e_addressbook_send_card(cle->card, E_ADDRESSBOOK_DISPOSITION_AS_ATTACHMENT);
}

static void
file_send_to_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);

	extract_info (cle);

	e_addressbook_send_card(cle->card, E_ADDRESSBOOK_DISPOSITION_AS_TO);
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

	g_signal_emit (cle, contact_list_editor_signals[LIST_DELETED], 0,
		       status, cle->card);

	/* always close the dialog after we successfully delete a list */
	if (status == E_BOOK_STATUS_SUCCESS)
		close_dialog (cle);

	g_object_unref (cle); /* release reference held for callback */
}

static void
delete_cb (GtkWidget *widget, gpointer data)
{
	EContactListEditor *cle = E_CONTACT_LIST_EDITOR (data);
	ECard *card = cle->card;

	g_object_ref (card);

	if (e_contact_editor_confirm_delete(GTK_WINDOW(cle->app))) {

		extract_info (cle);
		
		if (!cle->is_new_list) {
			gtk_widget_set_sensitive (cle->app, FALSE);
			cle->in_async_call = TRUE;
			
			g_object_ref (cle); /* hold reference for callback */
			e_book_remove_card (cle->book, card, (EBookCallback)list_deleted_cb, cle);
		}
	}

	g_object_unref (card);
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

	bonobo_ui_util_set_ui (ce->uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-contact-list-editor.xml",
			       "evolution-contact-list-editor", NULL);

	e_pixmaps_update (ce->uic, pixmaps);
}

static void
contact_list_editor_destroy_notify (gpointer data,
				    GObject *where_the_object_was)
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
	EContactListEditor *ce = g_object_new (E_TYPE_CONTACT_LIST_EDITOR, NULL);

	all_contact_list_editors = g_slist_prepend (all_contact_list_editors, ce);
	g_object_weak_ref (G_OBJECT (ce), contact_list_editor_destroy_notify, ce);

	g_object_ref (ce);
	gtk_object_sink (GTK_OBJECT (ce));

	g_object_set (ce,
		      "book", book,
		      "card", list_card,
		      "is_new_list", is_new_list,
		      "editable", editable,
		      NULL);

	return ce;
}

static void
e_contact_list_editor_set_property (GObject *object, guint prop_id,
				    const GValue *value, GParamSpec *pspec)
{
	EContactListEditor *editor;

	editor = E_CONTACT_LIST_EDITOR (object);
	
	switch (prop_id){
	case PROP_BOOK:
		if (editor->book)
			g_object_unref (editor->book);
		editor->book = E_BOOK(g_value_get_object (value));
		g_object_ref (editor->book);
		/* XXX more here about editable/etc. */
		break;
	case PROP_CARD:
		if (editor->card)
			g_object_unref (editor->card);
		editor->card = e_card_duplicate(E_CARD(g_value_get_object (value)));
		fill_in_info(editor);
		editor->changed = FALSE;
		command_state_changed (editor);
		break;
	case PROP_IS_NEW_LIST: {
		gboolean new_value = g_value_get_boolean (value);
		gboolean changed = (editor->is_new_list != new_value);

		editor->is_new_list = new_value;
		
		if (changed)
			command_state_changed (editor);
		break;
	}
	case PROP_EDITABLE: {
		gboolean new_value = g_value_get_boolean (value);
		gboolean changed = (editor->editable != new_value);

		editor->editable = new_value;

		if (changed) {
			set_editable (editor);
			command_state_changed (editor);
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_contact_list_editor_get_property (GObject *object, guint prop_id,
				    GValue *value, GParamSpec *pspec)
{
	EContactListEditor *editor;

	editor = E_CONTACT_LIST_EDITOR (object);

	switch (prop_id) {
	case PROP_BOOK:
		g_value_set_object (value, editor->book);
		break;

	case PROP_CARD:
		extract_info(editor);
		g_value_set_object (value, editor->card);
		break;

	case PROP_IS_NEW_LIST:
		g_value_set_boolean (value, editor->is_new_list);
		break;

	case PROP_EDITABLE:
		g_value_set_boolean (value, editor->editable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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

	table = e_table_scrolled_new_from_spec_file (model,
				      NULL,
				      EVOLUTION_ETSPECDIR "/e-contact-list-editor.etspec",
				      NULL);

	g_object_set_data(G_OBJECT(table), "model", model);

	return table;
}

static void
add_email_cb (GtkWidget *w, EContactListEditor *editor)
{
	GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (editor->table));
	const char *text = gtk_entry_get_text (GTK_ENTRY(editor->email_entry));

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

	g_signal_emit (cle, contact_list_editor_signals[EDITOR_CLOSED], 0);
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
	if (context->targets == NULL)
		return FALSE;


	gtk_drag_get_data (GTK_WIDGET (table), context,
			   GDK_POINTER_TO_ATOM (context->targets->data),
			   time);
	return TRUE;
}

static void
table_drag_data_received_cb (ETable *table, int row, int col,
			     GdkDragContext *context,
			     gint x, gint y,
			     GtkSelectionData *selection_data,
			     guint info, guint time, EContactListEditor *editor)
{
	GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (editor->table));
	char *target_type;
	gboolean changed = FALSE;
	gboolean handled = FALSE;

	target_type = gdk_atom_name (selection_data->target);

	if (!strcmp (target_type, VCARD_TYPE)) {

		GList *card_list = e_card_load_cards_from_string_with_default_charset (selection_data->data, "ISO-8859-1");
		GList *c;

		if (card_list)
			handled = TRUE;

		for (c = card_list; c; c = c->next) {
			ECard *ecard = c->data;

			if (!e_card_evolution_list (ecard)) {
				ECardSimple *simple = e_card_simple_new (ecard);

				e_contact_list_model_add_card (E_CONTACT_LIST_MODEL (editor->model),
							       simple);

				g_object_unref (simple);

				changed = TRUE;
			}
		}
		g_list_foreach (card_list, (GFunc)g_object_unref, NULL);
		g_list_free (card_list);

		/* Skip to the end of the list */
		if (adj->upper - adj->lower > adj->page_size)
			gtk_adjustment_set_value (adj, adj->upper);

		if (changed) {
			editor->changed = TRUE;
			command_state_changed (editor);
		}
	}

	gtk_drag_finish (context, handled, FALSE, time);
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
		char *string = gtk_editable_get_chars(GTK_EDITABLE (editor->list_name_entry), 0, -1);

		if (string && *string)
			g_object_set (card,
				       "file_as", string,
				       "full_name", string,
				       NULL);

		g_free (string);

		
		g_object_set (card,
				"list", GINT_TO_POINTER (TRUE),
				"list_show_addresses",
				GINT_TO_POINTER (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(editor->visible_addrs_checkbutton))),
				NULL);

		g_object_get (card,
				"email", &email_list,
				NULL);

		/* clear the email list */
		email_iter = e_list_get_iterator (email_list);
		e_iterator_last (email_iter);
		while (e_iterator_is_valid (E_ITERATOR (email_iter))) {
			e_iterator_delete (E_ITERATOR (email_iter));
		}
		g_object_unref (email_iter);

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

		g_object_get (editor->card,
				"file_as", &file_as,
				"email", &email_list,
				"list", &is_evolution_list,
				"list_show_addresses", &show_addresses,
				NULL);

		gtk_editable_delete_text (GTK_EDITABLE (editor->list_name_entry), 0, -1);
		if (file_as) {
			int position = 0;
			gtk_editable_insert_text (GTK_EDITABLE (editor->list_name_entry), file_as, strlen (file_as), &position);
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
