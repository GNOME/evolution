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
#include "e-address-popup.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-generic-factory.h>
#include <gal/widgets/e-popup-menu.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/gui/contact-editor/e-contact-editor.h>
#include <addressbook/gui/contact-editor/e-contact-quick-add.h>
#include <addressbook/gui/widgets/e-minicard-widget.h>
#include <addressbook/gui/widgets/e-addressbook-util.h>
#include "e-util/e-gui-utils.h"

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
 * card.
 */

#define EMPTY_ENTRY N_("(none)")

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
email_menu_add_option (EMailMenu *menu, const gchar *addr)
{
	GtkWidget *menu_item;
	gchar *addr_cpy;

	g_return_if_fail (menu != NULL);
	if (addr == NULL)
		return;

	addr_cpy = g_strdup (addr);
	menu->options = g_list_append (menu->options, addr_cpy);

	menu_item = gtk_menu_item_new_with_label (addr);
	g_object_set_data (G_OBJECT (menu_item), "addr", addr_cpy);
	gtk_widget_show_all (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (gtk_option_menu_get_menu (GTK_OPTION_MENU (menu->option_menu))), menu_item);

	g_signal_connect (menu_item,
			  "activate",
			  G_CALLBACK (menu_activate_cb),
			  menu);
}

static void
email_menu_add_options_from_card (EMailMenu *menu, ECard *card, const gchar *extra_addr)
{
	ECardSimple *simple;

	g_return_if_fail (card && E_IS_CARD (card));

	simple = e_card_simple_new (card);

	/* If any of these three e-mail fields are NULL, email_menu_add_option will just
	   return without doing anything. */
	email_menu_add_option (menu, e_card_simple_get_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL));
	email_menu_add_option (menu, e_card_simple_get_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL_2));
	email_menu_add_option (menu, e_card_simple_get_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL_3));
	email_menu_add_option (menu, extra_addr);
	email_menu_add_option (menu, EMPTY_ENTRY);

	g_object_unref (simple);
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

#ifdef UNDEFINED_FUNCTIONS_SHOULD_PLEASE_BE_INCLUDED
static void
email_menu_unset_option (EMailMenu *menu, const gchar *addr)
{
	GList *iter;

	g_return_if_fail (menu != NULL);
	g_return_if_fail (addr != NULL);

	if (menu->current_selection == NULL || strcmp (addr, menu->current_selection))
		return;

	iter = menu->options;
	while (iter && strcmp (addr, (gchar *) iter->data)) {
		iter = g_list_next (iter);
	}
	if (iter) {
		iter = g_list_next (iter);
		if (iter) {
			email_menu_set_option (menu, (gchar *) iter->data);
		} else {
			email_menu_set_option (menu, EMPTY_ENTRY);
		}
	}
}
#endif



typedef struct _EMailTable EMailTable;
struct _EMailTable {
	GtkWidget *table;
	ECard *card;
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

	g_object_unref (et->card);
	email_menu_free (et->primary);
	email_menu_free (et->email2);
	email_menu_free (et->email3);

	g_free (et);
}

static void
email_table_from_card (EMailTable *et)
{
	ECardSimple *simple;
	
	g_return_if_fail (et != NULL);

	simple = e_card_simple_new (et->card);
	email_menu_set_option (et->primary, e_card_simple_get_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL));
	email_menu_set_option (et->email2,  e_card_simple_get_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL_2));
	email_menu_set_option (et->email3,  e_card_simple_get_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL_3));
	g_object_unref (simple);
}

static void
email_table_to_card (EMailTable *et)
{
	ECardSimple *simple;
	gchar *curr;

	g_return_if_fail (et != NULL);

	simple = e_card_simple_new (et->card);

	curr = et->primary->current_selection;
	if (curr && !strcmp (curr, _(EMPTY_ENTRY)))
		curr = NULL;
	e_card_simple_set_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL, curr);

	curr = et->email2->current_selection;
	if (curr && !strcmp (curr, _(EMPTY_ENTRY)))
		curr = NULL;
	e_card_simple_set_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL_2, curr);

	curr = et->email3->current_selection;
	if (curr && !strcmp (curr, _(EMPTY_ENTRY)))
		curr = NULL;
	e_card_simple_set_email (simple, E_CARD_SIMPLE_EMAIL_ID_EMAIL_3, curr);

	e_card_simple_sync_card (simple);
	g_object_unref (simple);
}

static void
email_table_save_card_cb (EBook *book, EBookStatus status, gpointer closure)
{
	ECard *card = E_CARD (closure);

	if (status == E_BOOK_STATUS_SUCCESS) {
		e_book_commit_card (book, card, NULL, NULL);
	}
	if (book)
		g_object_unref (book);
	g_object_unref (card);
}

/*
 * We have to do this in an idle function because of what might be a
 * re-entrancy problems with EBook.
 */
static gint
add_card_idle_cb (gpointer closure)
{
	EBook *book;

	book = e_book_new ();
	addressbook_load_default_book (book, email_table_save_card_cb, closure);

	return 0;
}

static void
email_table_ok_cb (MiniWizard *wiz, gpointer closure)
{
	EMailTable *et = (EMailTable *) closure;

	email_table_to_card (et);

	g_object_ref (et->card);
	gtk_idle_add (add_card_idle_cb, et->card);

	mini_wizard_destroy (wiz);
}

static void
email_table_init (MiniWizard *wiz, ECard *card, const gchar *extra_address)
{
	EMailTable *et;

	gchar *name_str;
	gint xpad, ypad;
	GtkAttachOptions label_x_opts, label_y_opts;
	GtkAttachOptions menu_x_opts, menu_y_opts;

	g_return_if_fail (card && E_IS_CARD (card));

	et = g_new (EMailTable, 1);

	et->card = card;
	g_object_ref (et->card);

	et->table = gtk_table_new (4, 2, FALSE);

	et->primary = email_menu_new ();
	et->email2  = email_menu_new ();
	et->email3  = email_menu_new ();

	email_menu_add_options_from_card (et->primary, et->card, extra_address);
	email_menu_add_options_from_card (et->email2,  et->card, extra_address);
	email_menu_add_options_from_card (et->email3,  et->card, extra_address);

	email_table_from_card (et);

	label_x_opts = GTK_FILL;
	label_y_opts = GTK_FILL;
	menu_x_opts = GTK_EXPAND | GTK_FILL;
	menu_y_opts = GTK_EXPAND | GTK_FILL;
	xpad = 3;
	ypad = 3;

	name_str = e_card_name_to_string (et->card->name);
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
 * This code is for the little UI thing that lets you pick from a set of cards
 * and decide which one you want to add the e-mail address to.
 */

typedef struct _CardPicker CardPicker;
struct _CardPicker {
	GtkWidget *body;
	GtkWidget *list;
	GtkListStore *model;
	GList *cards;
	gchar *new_name;
	gchar *new_email;

	ECard *current_card;
};

enum {
	COLUMN_ACTION,
	COLUMN_CARD
};

static void
card_picker_selection_changed (GtkTreeSelection *selection, gpointer closure)
{
	MiniWizard *wiz = (MiniWizard *) closure;
	CardPicker *pick = (CardPicker *) wiz->closure;
	gboolean selected;
	GtkTreeIter iter;

	selected = gtk_tree_selection_get_selected (selection, NULL, &iter);

	gtk_widget_set_sensitive (wiz->ok_button, selected);

	if (selected) {
		gtk_tree_model_get (GTK_TREE_MODEL (pick->model), &iter,
				    COLUMN_CARD, &pick->current_card,
				    -1);
	}
	else {
		pick->current_card = NULL;
	}
}

static void
card_picker_ok_cb (MiniWizard *wiz, gpointer closure)
{
	CardPicker *pick = (CardPicker *) closure;

	if (pick->current_card == NULL) {
		e_contact_quick_add (pick->new_name, pick->new_email, NULL, NULL);
		mini_wizard_destroy (wiz);
	} else {
		email_table_init (wiz, pick->current_card, pick->new_email);
	}
}

static void
card_picker_cleanup_cb (gpointer closure)
{
	CardPicker *pick = (CardPicker *) closure;

	g_list_foreach (pick->cards, (GFunc) g_object_unref, NULL);
	g_list_free (pick->cards);

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
card_picker_init (MiniWizard *wiz, const GList *cards, const gchar *new_name, const gchar *new_email)
{
	CardPicker *pick;
	gchar *str;
	GtkWidget *w;
	GtkTreeIter iter;

	pick = g_new (CardPicker, 1);

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
			    COLUMN_CARD, NULL,
			    -1);
	g_object_weak_ref (G_OBJECT (pick->model), free_str, str);

	pick->cards = NULL;
	while (cards) {
		ECard *card = (ECard *) cards->data;
		gchar *name_str = e_card_name_to_string (card->name);

		pick->cards = g_list_append (pick->cards, card);
		g_object_ref (card);

		str = g_strdup_printf (_("Add address to existing contact \"%s\""), name_str);
		gtk_list_store_append (pick->model, &iter);
		gtk_list_store_set (pick->model, &iter,
				    COLUMN_ACTION, str,
				    COLUMN_CARD, card,
				    -1);
		g_free (name_str);

		g_object_weak_ref (G_OBJECT (pick->model), free_str, str);

		cards = g_list_next (cards);
	}

	pick->new_name  = g_strdup (new_name);
	pick->new_email = g_strdup (new_email);

	pick->current_card = NULL;
	gtk_widget_set_sensitive (wiz->ok_button, FALSE);

	/* Connect some signals & callbacks */

	wiz->ok_cb      = card_picker_ok_cb;
	wiz->cleanup_cb = card_picker_cleanup_cb;

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (pick->list)),
			  "changed", G_CALLBACK (card_picker_selection_changed),
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
 * The code for the actual EAddressPopup widget begins here.
 */

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */


static GtkObjectClass *parent_class;

static void e_address_popup_dispose (GObject *);
static void e_address_popup_query   (EAddressPopup *);


static void
e_address_popup_class_init (EAddressPopupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = e_address_popup_dispose;
}

static void
e_address_popup_init (EAddressPopup *pop)
{
	pop->transitory = TRUE;
}

static void
e_address_popup_cleanup (EAddressPopup *pop)
{
	if (pop->card) {
		g_object_unref (pop->card);
		pop->card = NULL;
	}

	if (pop->scheduled_refresh) {
		gtk_timeout_remove (pop->scheduled_refresh);
		pop->scheduled_refresh = 0;
	}

	if (pop->query_tag) {
		e_book_simple_query_cancel (pop->book, pop->query_tag);
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
e_address_popup_dispose (GObject *obj)
{
	EAddressPopup *pop = E_ADDRESS_POPUP (obj);

	e_address_popup_cleanup (pop);

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (obj);
}

GType
e_address_popup_get_type (void)
{
	static GType pop_type = 0;

	if (!pop_type) {
		static const GTypeInfo pop_info =  {
			sizeof (EAddressPopupClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_address_popup_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressPopup),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_address_popup_init,
		};

		pop_type = g_type_register_static (gtk_event_box_get_type (), "EAddressPopup", &pop_info, 0);
	}

	return pop_type;
}

static void
e_address_popup_refresh_names (EAddressPopup *pop)
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

	e_address_popup_query (pop);
}

static gint
refresh_timeout_cb (gpointer ptr)
{
	EAddressPopup *pop = E_ADDRESS_POPUP (ptr);
	e_address_popup_refresh_names (pop);
	pop->scheduled_refresh = 0;
	return 0;
}

static void
e_address_popup_schedule_refresh (EAddressPopup *pop)
{
	if (pop->scheduled_refresh == 0)
		pop->scheduled_refresh = gtk_timeout_add (20, refresh_timeout_cb, pop);
}

/* If we are handed something of the form "Foo <bar@bar.com>",
   do the right thing. */
static gboolean
e_address_popup_set_free_form (EAddressPopup *pop, const gchar *txt)
{
	gchar *lt, *gt = NULL;

	g_return_val_if_fail (pop && E_IS_ADDRESS_POPUP (pop), FALSE);

	if (txt == NULL)
		return FALSE;

	lt = strchr (txt, '<');
	if (lt)
		gt = strchr (txt, '>');

	if (lt && gt && lt+1 < gt) {
		gchar *name  = g_strndup (txt,  lt-txt);
		gchar *email = g_strndup (lt+1, gt-lt-1);
		e_address_popup_set_name (pop, name);
		e_address_popup_set_email (pop, email);

		return TRUE;
	}
	
	return FALSE;
}

void
e_address_popup_set_name (EAddressPopup *pop, const gchar *name)
{
	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	/* We only allow the name to be set once. */
	if (pop->name)
		return;

	if (!e_address_popup_set_free_form (pop, name)) {
		pop->name = g_strdup (name);
		if (pop->name)
			g_strstrip (pop->name);
	}

	e_address_popup_schedule_refresh (pop);
}

void
e_address_popup_set_email (EAddressPopup *pop, const gchar *email)
{
	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	/* We only allow the e-mail to be set once. */
	if (pop->email)
		return;

	if (!e_address_popup_set_free_form (pop, email)) {
		pop->email = g_strdup (email);
		if (pop->email)
			g_strstrip (pop->email);
	}

	e_address_popup_schedule_refresh (pop);
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
		g_object_unref (style);
	}

	pop->generic_view = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (pop->generic_view), name_holder);
	gtk_box_pack_start (GTK_BOX (pop->main_vbox), pop->generic_view, TRUE, TRUE, 0);
	gtk_widget_show_all (pop->generic_view);

	pop->query_msg = gtk_label_new (_("Querying Addressbook..."));
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
	EAddressPopup *pop = g_object_new (E_TYPE_ADDRESS_POPUP, NULL);
	e_address_popup_construct (pop);
	return GTK_WIDGET (pop);
}

static void
emit_event (EAddressPopup *pop, const char *event)
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
	if (status == E_BOOK_STATUS_SUCCESS) {
		EAddressPopup *pop = E_ADDRESS_POPUP (closure);
		EContactEditor *ce = e_addressbook_show_contact_editor (book, pop->card, FALSE, TRUE);
		e_address_popup_cleanup (pop);
		emit_event (pop, "Destroy");
		e_contact_editor_raise (ce);
	}

	if (book)
		g_object_unref (book);
}

static void
edit_contact_info_cb (GtkWidget *button, EAddressPopup *pop)
{
	EBook *book;
	emit_event (pop, "Hide");

	book = e_book_new ();
	addressbook_load_default_book (book, contact_editor_cb, pop);
}

static void
e_address_popup_cardify (EAddressPopup *pop, ECard *card)
{
	GtkWidget *b;

	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));
	g_return_if_fail (card && E_IS_CARD (card));
	g_return_if_fail (pop->card == NULL);

	pop->card = card;
	g_object_ref (pop->card);

	e_minicard_widget_set_card (E_MINICARD_WIDGET (pop->minicard_view), card);
	gtk_widget_show (pop->minicard_view);
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
add_contacts_cb (GtkWidget *button, EAddressPopup *pop)
{
	if (pop->email && *pop->email) {
		if (pop->name && *pop->name)
			e_contact_quick_add (pop->name, pop->email, NULL, NULL);
		else
			e_contact_quick_add_free_form (pop->email, NULL, NULL);

	}
	e_address_popup_cleanup (pop);
	emit_event (pop, "Destroy");
}

static void
e_address_popup_no_matches (EAddressPopup *pop)
{
	GtkWidget *b;

	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	b = gtk_button_new_with_label (_("Add to Contacts"));
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
e_address_popup_ambiguous_email_add (EAddressPopup *pop, const GList *cards)
{
	MiniWizard *wiz = mini_wizard_new ();
	GtkWidget *win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	wiz->destroy_cb      = wizard_destroy_cb;
	wiz->destroy_closure = win;

	gtk_window_set_title (GTK_WINDOW (win),  _("Merge E-Mail Address"));
	gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_MOUSE);

	card_picker_init (wiz, cards, pop->name, pop->email);

	e_address_popup_cleanup (pop);
	emit_event (pop, "Destroy");

	gtk_container_add (GTK_CONTAINER (win), wiz->body);
	gtk_widget_show_all (win);
}

static void
e_address_popup_multiple_matches (EAddressPopup *pop, const GList *cards)
{
	pop->multiple_matches = TRUE;

	e_address_popup_ambiguous_email_add (pop, cards);
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

/*
 *  Addressbook Query Fun
 */

static void
name_only_query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	EAddressPopup *pop;

	if (status != E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS)
		return;

	pop = E_ADDRESS_POPUP (closure);

	pop->query_tag = 0;

	if (cards == NULL) {
		e_address_popup_no_matches (pop);
	} else {
		e_address_popup_ambiguous_email_add (pop, cards);
	}
}

static void
query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	EAddressPopup *pop;

	if (status != E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS)
		return;

	pop = E_ADDRESS_POPUP (closure);

	pop->query_tag = 0;
	gtk_widget_hide (pop->query_msg);

	if (cards == NULL) {
		
		/* Do a name-only query if:
		   (1) The name is non-empty.
		   (2) The e-mail is also non-empty (so that the query we just did wasn't actually a name-only query.
		*/
		if (pop->name && *pop->name && pop->email && *pop->email) {
			pop->query_tag = e_book_name_and_email_query (book, pop->name, NULL, name_only_query_cb, pop);
		} else {
			e_address_popup_no_matches (pop);
		}
		
	} else {
		if (g_list_length ((GList *) cards) == 1)
			e_address_popup_cardify (pop, E_CARD (cards->data));
		else
			e_address_popup_multiple_matches (pop, cards);
	}
}

static void
start_query (EBook *book, EBookStatus status, gpointer closure)
{
	EAddressPopup *pop = E_ADDRESS_POPUP (closure);

	if (status != E_BOOK_STATUS_SUCCESS) {
		e_address_popup_no_matches (pop);
		if (book)
			g_object_unref (book);
		return;
	}
	
	if (pop->query_tag)
		e_book_simple_query_cancel (book, pop->query_tag);

	if (pop->book != book) {
		g_object_ref (book);
		if (pop->book)
			g_object_unref (pop->book);
		pop->book = book;
	}
		
	pop->query_tag = e_book_name_and_email_query (book, pop->name, pop->email, query_cb, pop);

	g_object_unref (pop);
}

static void
e_address_popup_query (EAddressPopup *pop)
{
	EBook *book;

	g_return_if_fail (pop && E_IS_ADDRESS_POPUP (pop));

	book = e_book_new ();
	g_object_ref (pop);

	addressbook_load_default_book (book, start_query, pop);
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

static void
get_prop (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id, CORBA_Environment *ev, gpointer user_data)
{
	EAddressPopup *pop = E_ADDRESS_POPUP (user_data);

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
e_address_popup_new_control (void)
{
        BonoboControl *control;
        BonoboPropertyBag *bag;
	EAddressPopup *addy;
	GtkWidget *w;

	w = e_address_popup_new ();
	addy = E_ADDRESS_POPUP (w);

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
