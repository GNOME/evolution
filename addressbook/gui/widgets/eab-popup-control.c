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
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-generic-factory.h>
#include <gal/widgets/e-popup-menu.h>
#include <addressbook/util/eab-book-util.h>
#include <addressbook/gui/contact-editor/e-contact-editor.h>
#include <addressbook/gui/contact-editor/e-contact-quick-add.h>
#include <addressbook/gui/widgets/eab-contact-display.h>
#include <addressbook/gui/widgets/eab-gui-util.h>
#include "e-util/e-gui-utils.h"

static void eab_popup_control_set_name (EABPopupControl *pop, const gchar *name);
static void eab_popup_control_set_email (EABPopupControl *pop, const gchar *email);

/*
 * Some general scaffolding for our widgets.  Think of this as a really, really
 * lame implementation of a wizard (...which is still somewhat more general that
 * we really need it to be).
 */

typedef struct _MiniWizard MiniWizard;
struct _MiniWizard {
	GtkWidget *body;

	GtkWidget *vbox;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;

	void (*ok_cb) (MiniWizard *, gpointer);
	void (*cleanup_cb) (gpointer);
	gpointer closure;

	void (*destroy_cb) (MiniWizard *, gpointer);
	gpointer destroy_closure;
};

static void
mini_wizard_container_add (MiniWizard *wiz, GtkWidget *w)
{
	GList *iter = gtk_container_get_children (GTK_CONTAINER (wiz->vbox));
	while (iter != NULL) {
		GtkWidget *oldw = (GtkWidget *) iter->data;
		iter = g_list_next (iter);
		gtk_container_remove (GTK_CONTAINER (wiz->vbox), oldw);
	}
	gtk_container_add (GTK_CONTAINER (wiz->vbox), w);
}

static void
mini_wizard_destroy (MiniWizard *wiz)
{
	if (wiz->cleanup_cb)
		wiz->cleanup_cb (wiz->closure);
	wiz->cleanup_cb = NULL;

	if (wiz->destroy_cb)
		wiz->destroy_cb (wiz, wiz->destroy_closure);
}

static void
mini_wizard_ok_cb (GtkWidget *b, gpointer closure)
{
	MiniWizard *wiz = (MiniWizard *) closure;

	gpointer old_closure = wiz->closure;
	void (*old_cleanup) (gpointer) = wiz->cleanup_cb;

	wiz->cleanup_cb = NULL;

	if (wiz->ok_cb)
		wiz->ok_cb (wiz, wiz->closure);

	if (old_cleanup)
		old_cleanup (old_closure);

}

static void
mini_wizard_cancel_cb (GtkWidget *b, gpointer closure)
{
	mini_wizard_destroy ((MiniWizard *) closure);
}

static void
mini_wizard_destroy_cb (gpointer closure, GObject *where_object_was)
{
	MiniWizard *wiz = (MiniWizard *) closure;
	if (wiz->cleanup_cb)
		wiz->cleanup_cb (wiz->closure);
	g_free (wiz);
}

static MiniWizard *
mini_wizard_new (void)
{
	MiniWizard *wiz = g_new (MiniWizard, 1);
	GtkWidget *bbox;

	wiz->body          = gtk_vbox_new (FALSE, 2);
	wiz->vbox          = gtk_vbox_new (FALSE, 2);
	wiz->ok_button     = gtk_button_new_from_stock (GTK_STOCK_OK);
	wiz->cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

	wiz->ok_cb      = NULL;
	wiz->cleanup_cb = NULL;
	wiz->closure    = NULL;

	wiz->destroy_cb      = NULL;
	wiz->destroy_closure = NULL;

	bbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox),
				   GTK_BUTTONBOX_END);

	gtk_box_pack_start (GTK_BOX (bbox), wiz->cancel_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (bbox), wiz->ok_button, FALSE, TRUE, 0);

	gtk_box_set_spacing (GTK_BOX (bbox),
			     10 /* ugh */);

	gtk_box_pack_start (GTK_BOX (wiz->body), wiz->vbox, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (wiz->body), gtk_hseparator_new (), FALSE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (wiz->body), bbox, FALSE, TRUE, 2);

	gtk_widget_show_all (wiz->body);

	g_signal_connect (wiz->ok_button,
			  "clicked",
			  G_CALLBACK (mini_wizard_ok_cb),
			  wiz);
	g_signal_connect (wiz->cancel_button,
			  "clicked",
			  G_CALLBACK (mini_wizard_cancel_cb),
			  wiz);

	g_object_weak_ref (G_OBJECT (wiz->body),
			   mini_wizard_destroy_cb,
			   wiz);

	return wiz;
	
}



/*
 * This is the code for the UI thingie that lets you manipulate the e-mail
 * addresses (and *only* the e-mail addresses) associated with an existing
 * contact.
 */

#define EMPTY_ENTRY _("(none)")

typedef struct _EMailMenu EMailMenu;
struct _EMailMenu {
	GtkWidget *option_menu;
	GList *options;
	gchar *current_selection;
};

static void
email_menu_free (EMailMenu *menu)
{
	if (menu == NULL)
		return;

	g_list_foreach (menu->options, (GFunc) g_free, NULL);
	g_list_free (menu->options);
	g_free (menu);
}

static EMailMenu *
email_menu_new (void)
{
	EMailMenu *menu = g_new (EMailMenu, 1);

	menu->option_menu = gtk_option_menu_new ();
	menu->options = NULL;
	menu->current_selection = NULL;

	gtk_option_menu_set_menu (GTK_OPTION_MENU (menu->option_menu), gtk_menu_new ());

	return menu;
}

static void
menu_activate_cb (GtkWidget *w, gpointer closure)
{
	EMailMenu *menu = (EMailMenu *) closure;
	gchar *addr = (gchar *) g_object_get_data (G_OBJECT (w), "addr");

	menu->current_selection = addr;
}

static void
email_menu_add_option (EMailMenu *menu, char *addr)
{
	GtkWidget *menu_item;

	g_return_if_fail (menu != NULL);
	if (!addr || !*addr)
		return;

	menu->options = g_list_append (menu->options, addr);

	menu_item = gtk_menu_item_new_with_label (addr);
	g_object_set_data (G_OBJECT (menu_item), "addr", addr);
	gtk_widget_show_all (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (gtk_option_menu_get_menu (GTK_OPTION_MENU (menu->option_menu))), menu_item);

	g_signal_connect (menu_item,
			  "activate",
			  G_CALLBACK (menu_activate_cb),
			  menu);
}

static void
email_menu_add_options_from_contact (EMailMenu *menu, EContact *contact, const gchar *extra_addr)
{
	g_return_if_fail (contact && E_IS_CONTACT (contact));

	/* If any of these three e-mail fields are NULL, email_menu_add_option will just
	   return without doing anything. */
	email_menu_add_option (menu, e_contact_get (contact, E_CONTACT_EMAIL_1));
	email_menu_add_option (menu, e_contact_get (contact, E_CONTACT_EMAIL_2));
	email_menu_add_option (menu, e_contact_get (contact, E_CONTACT_EMAIL_3));
	email_menu_add_option (menu, g_strdup (extra_addr));
	email_menu_add_option (menu, g_strdup (EMPTY_ENTRY));
}

static void
email_menu_set_option (EMailMenu *menu, const gchar *addr)
{
	guint count = 0;
	GList *iter;

	g_return_if_fail (menu != NULL);

	if (addr == NULL) {
		email_menu_set_option (menu, EMPTY_ENTRY);
		return;
	}

	iter = menu->options;
	while (iter && strcmp (addr, (gchar *) iter->data)) {
		++count;
		iter = g_list_next (iter);
	}

	if (iter) {
		gtk_option_menu_set_history (GTK_OPTION_MENU (menu->option_menu), count);
		menu->current_selection = (gchar *) iter->data;
	} 
}

typedef struct _EMailTable EMailTable;
struct _EMailTable {
	GtkWidget *table;
	EContact *contact;
	EMailMenu *primary;
	EMailMenu *email2;
	EMailMenu *email3;
};

static void
email_table_cleanup_cb (gpointer closure)
{
	EMailTable *et = (EMailTable *) closure;

	if (et == NULL)
		return;

	g_object_unref (et->contact);
	email_menu_free (et->primary);
	email_menu_free (et->email2);
	email_menu_free (et->email3);

	g_free (et);
}

static void
email_table_from_contact (EMailTable *et)
{
	g_return_if_fail (et != NULL);

	email_menu_set_option (et->primary, e_contact_get_const (et->contact, E_CONTACT_EMAIL_1));
	email_menu_set_option (et->email2,  e_contact_get_const (et->contact, E_CONTACT_EMAIL_2));
	email_menu_set_option (et->email3,  e_contact_get_const (et->contact, E_CONTACT_EMAIL_3));
}

static void
email_table_to_contact (EMailTable *et)
{
	gchar *curr;

	g_return_if_fail (et != NULL);

	curr = et->primary->current_selection;
	if (curr && !strcmp (curr, _(EMPTY_ENTRY)))
		curr = NULL;
	e_contact_set (et->contact, E_CONTACT_EMAIL_1, curr);

	curr = et->email2->current_selection;
	if (curr && !strcmp (curr, _(EMPTY_ENTRY)))
		curr = NULL;
	e_contact_set (et->contact, E_CONTACT_EMAIL_2, curr);

	curr = et->email3->current_selection;
	if (curr && !strcmp (curr, _(EMPTY_ENTRY)))
		curr = NULL;
	e_contact_set (et->contact, E_CONTACT_EMAIL_3, curr);
}

static void
email_table_save_contact_cb (EBook *book, EBookStatus status, gpointer closure)
{
	EContact *contact = E_CONTACT (closure);

	if (status == E_BOOK_ERROR_OK) {
		e_book_async_commit_contact (book, contact, NULL, NULL);
	}
	if (book)
		g_object_unref (book);
	g_object_unref (contact);
}

static void
email_table_ok_cb (MiniWizard *wiz, gpointer closure)
{
	EMailTable *et = (EMailTable *) closure;

	email_table_to_contact (et);

	g_object_ref (et->contact);

	addressbook_load_default_book (email_table_save_contact_cb, et->contact);

	mini_wizard_destroy (wiz);
}

static void
email_table_init (MiniWizard *wiz, EContact *contact, const gchar *extra_address)
{
	EMailTable *et;

	gchar *name_str;
	gint xpad, ypad;
	GtkAttachOptions label_x_opts, label_y_opts;
	GtkAttachOptions menu_x_opts, menu_y_opts;

	g_return_if_fail (contact && E_IS_CONTACT (contact));

	et = g_new (EMailTable, 1);

	et->contact = contact;
	g_object_ref (et->contact);

	et->table = gtk_table_new (4, 2, FALSE);

	et->primary = email_menu_new ();
	et->email2  = email_menu_new ();
	et->email3  = email_menu_new ();

	email_menu_add_options_from_contact (et->primary, et->contact, extra_address);
	email_menu_add_options_from_contact (et->email2,  et->contact, extra_address);
	email_menu_add_options_from_contact (et->email3,  et->contact, extra_address);

	email_table_from_contact (et);

	label_x_opts = GTK_FILL;
	label_y_opts = GTK_FILL;
	menu_x_opts = GTK_EXPAND | GTK_FILL;
	menu_y_opts = GTK_EXPAND | GTK_FILL;
	xpad = 3;
	ypad = 3;

	name_str = e_contact_get (et->contact, E_CONTACT_FULL_NAME);
	gtk_table_attach (GTK_TABLE (et->table),
			  gtk_label_new (name_str),
			  0, 2, 0, 1, 
			  label_x_opts, label_y_opts, xpad, ypad);
	g_free (name_str);

	gtk_table_attach (GTK_TABLE (et->table),
			  gtk_label_new (_("Primary Email")),
			  0, 1, 1, 2, 
			  label_x_opts, label_y_opts, xpad, ypad);

	gtk_table_attach (GTK_TABLE (et->table),
			  et->primary->option_menu,
			  1, 2, 1, 2, 
			  menu_x_opts, menu_y_opts, xpad, ypad);

	gtk_table_attach (GTK_TABLE (et->table),
			  gtk_label_new (_("Email 2")),
			  0, 1, 2, 3,
			  label_x_opts, label_y_opts, xpad, ypad);

	gtk_table_attach (GTK_TABLE (et->table),
			  et->email2->option_menu,
			  1, 2, 2, 3,
			  menu_x_opts, menu_y_opts, xpad, ypad);

	gtk_table_attach (GTK_TABLE (et->table),
			  gtk_label_new (_("Email 3")),
			  0, 1, 3, 4,
			  label_x_opts, label_y_opts, xpad, ypad);

	gtk_table_attach (GTK_TABLE (et->table),
			  et->email3->option_menu,
			  1, 2, 3, 4,
			  menu_x_opts, menu_y_opts, xpad, ypad);

	gtk_widget_show_all (et->primary->option_menu);
	gtk_widget_show_all (et->email2->option_menu);
	gtk_widget_show_all (et->email3->option_menu);

	gtk_widget_show_all (et->table);
	mini_wizard_container_add (wiz, et->table);
	wiz->ok_cb      = email_table_ok_cb;
	wiz->cleanup_cb = email_table_cleanup_cb;
	wiz->closure    = et;
}

/*
 * This code is for the little UI thing that lets you pick from a set of contacts
 * and decide which one you want to add the e-mail address to.
 */

typedef struct _ContactPicker ContactPicker;
struct _ContactPicker {
	GtkWidget *body;
	GtkWidget *list;
	GtkListStore *model;
	GList *contacts;
	gchar *new_name;
	gchar *new_email;

	EContact *current_contact;
};

enum {
	COLUMN_ACTION,
	COLUMN_CONTACT
};

static void
contact_picker_selection_changed (GtkTreeSelection *selection, gpointer closure)
{
	MiniWizard *wiz = (MiniWizard *) closure;
	ContactPicker *pick = (ContactPicker *) wiz->closure;
	gboolean selected;
	GtkTreeIter iter;

	selected = gtk_tree_selection_get_selected (selection, NULL, &iter);

	gtk_widget_set_sensitive (wiz->ok_button, selected);

	if (selected) {
		gtk_tree_model_get (GTK_TREE_MODEL (pick->model), &iter,
				    COLUMN_CONTACT, &pick->current_contact,
				    -1);
	}
	else {
		pick->current_contact = NULL;
	}
}

static void
contact_picker_ok_cb (MiniWizard *wiz, gpointer closure)
{
	ContactPicker *pick = (ContactPicker *) closure;

	if (pick->current_contact == NULL) {
		e_contact_quick_add (pick->new_name, pick->new_email, NULL, NULL);
		mini_wizard_destroy (wiz);
	} else {
		email_table_init (wiz, pick->current_contact, pick->new_email);
	}
}

static void
contact_picker_cleanup_cb (gpointer closure)
{
	ContactPicker *pick = (ContactPicker *) closure;

	g_list_foreach (pick->contacts, (GFunc) g_object_unref, NULL);
	g_list_free (pick->contacts);

	g_free (pick->new_name);
	g_free (pick->new_email);
}

static void
free_str (gpointer      data,
	  GObject      *where_the_object_was)
{
	g_free (data);
}

static void
contact_picker_init (MiniWizard *wiz, const GList *contacts, const gchar *new_name, const gchar *new_email)
{
	ContactPicker *pick;
	gchar *str;
	GtkWidget *w;
	GtkTreeIter iter;

	pick = g_new (ContactPicker, 1);

	pick->body  = gtk_vbox_new (FALSE, 2);

	pick->model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

	pick->list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (pick->model));

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pick->list), TRUE);

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pick->list),
						     COLUMN_ACTION,
						     _("Select an Action"),
						     gtk_cell_renderer_text_new (),
						     "text", COLUMN_ACTION,
						     NULL);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (pick->list)),
				     GTK_SELECTION_SINGLE);

	str = g_strdup_printf (_("Create a new contact \"%s\""), new_name);
	gtk_list_store_append (pick->model, &iter);
	gtk_list_store_set (pick->model, &iter,
			    COLUMN_ACTION, str,
			    COLUMN_CONTACT, NULL,
			    -1);
	g_object_weak_ref (G_OBJECT (pick->model), free_str, str);

	pick->contacts = NULL;
	while (contacts) {
		EContact *contact = (EContact *) contacts->data;
		gchar *name_str = e_contact_get (contact, E_CONTACT_FULL_NAME);

		pick->contacts = g_list_append (pick->contacts, contact);
		g_object_ref (contact);

		str = g_strdup_printf (_("Add address to existing contact \"%s\""), name_str);
		gtk_list_store_append (pick->model, &iter);
		gtk_list_store_set (pick->model, &iter,
				    COLUMN_ACTION, str,
				    COLUMN_CONTACT, contact,
				    -1);
		g_free (name_str);

		g_object_weak_ref (G_OBJECT (pick->model), free_str, str);

		contacts = g_list_next (contacts);
	}

	pick->new_name  = g_strdup (new_name);
	pick->new_email = g_strdup (new_email);

	pick->current_contact = NULL;
	gtk_widget_set_sensitive (wiz->ok_button, FALSE);

	/* Connect some signals & callbacks */

	wiz->ok_cb      = contact_picker_ok_cb;
	wiz->cleanup_cb = contact_picker_cleanup_cb;

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (pick->list)),
			  "changed", G_CALLBACK (contact_picker_selection_changed),
			  wiz);

	/* Build our widget */

	w = gtk_label_new (new_email);
	gtk_box_pack_start (GTK_BOX (pick->body), w, FALSE, TRUE, 3);

	gtk_box_pack_start (GTK_BOX (pick->body), pick->list, TRUE, TRUE, 2);
	gtk_widget_show_all (pick->body);


	/* Put it in our mini-wizard */

	wiz->closure = pick;
	mini_wizard_container_add (wiz, pick->body);
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

/*
 * The code for the actual EABPopupControl widget begins here.
 */

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */


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
contact_editor_cb (EBook *book, EBookStatus status, gpointer closure)
{
	if (status == E_BOOK_ERROR_OK) {
		EABPopupControl *pop = EAB_POPUP_CONTROL (closure);
		eab_show_contact_editor (book, pop->contact, FALSE, TRUE);
		eab_popup_control_cleanup (pop);
		emit_event (pop, "Destroy");
	}

	if (book)
		g_object_unref (book);
}

static void
edit_contact_info_cb (GtkWidget *button, EABPopupControl *pop)
{
	emit_event (pop, "Hide");

	addressbook_load_default_book (contact_editor_cb, pop);
}

static void
eab_popup_control_display_contact (EABPopupControl *pop, EContact *contact)
{
	GtkWidget *b;

	g_return_if_fail (pop && EAB_IS_POPUP_CONTROL (pop));
	g_return_if_fail (contact && E_IS_CONTACT (contact));
	g_return_if_fail (pop->contact == NULL);

	pop->contact = contact;
	g_object_ref (pop->contact);

	eab_contact_display_render (EAB_CONTACT_DISPLAY (pop->contact_display),
				    contact,
				    EAB_CONTACT_DISPLAY_RENDER_COMPACT);
	gtk_widget_show (pop->contact_display);
	gtk_widget_hide (pop->generic_view);

	b = gtk_button_new_with_label (_("Edit Contact Info"));
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), b, TRUE, TRUE, 0);
	g_signal_connect (b,
			  "clicked",
			  G_CALLBACK (edit_contact_info_cb),
			  pop);
	gtk_widget_show (b);
}

static void
add_contacts_cb (GtkWidget *button, EABPopupControl *pop)
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
eab_popup_control_no_matches (EABPopupControl *pop)
{
	GtkWidget *b;

	g_return_if_fail (pop && EAB_IS_POPUP_CONTROL (pop));

	b = e_button_new_with_stock_icon (_("Add to Contacts"), "gtk-add");

	gtk_box_pack_start (GTK_BOX (pop->main_vbox), b, TRUE, TRUE, 0);
	g_signal_connect (b,
			  "clicked",
			  G_CALLBACK (add_contacts_cb),
			  pop);
	gtk_widget_show (b);
}

static void
wizard_destroy_cb (MiniWizard *wiz, gpointer closure)
{
	gtk_widget_destroy (GTK_WIDGET (closure));
}

static void
eab_popup_control_ambiguous_email_add (EABPopupControl *pop, const GList *contacts)
{
	MiniWizard *wiz = mini_wizard_new ();
	GtkWidget *win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	wiz->destroy_cb      = wizard_destroy_cb;
	wiz->destroy_closure = win;

	gtk_window_set_title (GTK_WINDOW (win),  _("Merge E-Mail Address"));
	gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_MOUSE);

	contact_picker_init (wiz, contacts, pop->name, pop->email);

	eab_popup_control_cleanup (pop);
	emit_event (pop, "Destroy");

	gtk_container_add (GTK_CONTAINER (win), wiz->body);
	gtk_widget_show_all (win);
}

static void
eab_popup_control_multiple_matches (EABPopupControl *pop, const GList *contacts)
{
	pop->multiple_matches = TRUE;

	eab_popup_control_ambiguous_email_add (pop, contacts);
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

/*
 *  Addressbook Query Fun
 */

static void
name_only_query_cb (EBook *book, EBookStatus status, GList *contacts, gpointer closure)
{
	EABPopupControl *pop;

	if (status != E_BOOK_ERROR_OK)
		return;

	pop = EAB_POPUP_CONTROL (closure);

	pop->query_tag = 0;

	if (contacts == NULL) {
		eab_popup_control_no_matches (pop);
	} else {
		eab_popup_control_ambiguous_email_add (pop, contacts);
	}
}

static void
query_cb (EBook *book, EBookStatus status, GList *contacts, gpointer closure)
{
	EABPopupControl *pop;

	if (status != E_BOOK_ERROR_OK)
		return;

	pop = EAB_POPUP_CONTROL (closure);

	pop->query_tag = 0;
	gtk_widget_hide (pop->query_msg);

	if (contacts == NULL) {
		
		/* Do a name-only query if:
		   (1) The name is non-empty.
		   (2) The e-mail is also non-empty (so that the query we just did wasn't actually a name-only query.
		*/
		if (pop->name && *pop->name && pop->email && *pop->email) {
			pop->query_tag = eab_name_and_email_query (book, pop->name, NULL, name_only_query_cb, pop);
		} else {
			eab_popup_control_no_matches (pop);
		}
		
	} else {
		if (g_list_length ((GList *) contacts) == 1)
			eab_popup_control_display_contact (pop, E_CONTACT (contacts->data));
		else
			eab_popup_control_multiple_matches (pop, contacts);
	}
}

static void
start_query (EBook *book, EBookStatus status, gpointer closure)
{
	EABPopupControl *pop = EAB_POPUP_CONTROL (closure);

	if (status != E_BOOK_ERROR_OK) {
		eab_popup_control_no_matches (pop);
		if (book)
			g_object_unref (book);
		return;
	}
	
#if notyet
	if (pop->query_tag)
		e_book_simple_query_cancel (book, pop->query_tag);
#endif

	if (pop->book != book) {
		g_object_ref (book);
		if (pop->book)
			g_object_unref (pop->book);
		pop->book = book;
	}
		
	pop->query_tag = eab_name_and_email_query (book, pop->name, pop->email, query_cb, pop);

	g_object_unref (pop);
}

static void
eab_popup_control_query (EABPopupControl *pop)
{
	g_return_if_fail (pop && EAB_IS_POPUP_CONTROL (pop));

	g_object_ref (pop);

	addressbook_load_default_book (start_query, pop);
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
