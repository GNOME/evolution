/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-search-bar.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Authors:
 *  Chris Lahey      <clahey@ximian.com>
 *  Ettore Perazzoli <ettore@ximian.com>
 *  Jon Trowbridge   <trow@ximian.com>

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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmain.h>

#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-gui-utils.h>

#include <libgnome/gnome-i18n.h>

#include <bonobo/bonobo-ui-util.h>

#include <stdlib.h>
#include <string.h>

#include "e-search-bar.h"
#include "e-util-marshal.h"


enum {
	QUERY_CHANGED,
	MENU_ACTIVATED,
	SEARCH_ACTIVATED,

	LAST_SIGNAL
};

static gint esb_signals [LAST_SIGNAL] = { 0, };

static GtkHBoxClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_ITEM_ID,
	PROP_SUBITEM_ID,
	PROP_TEXT,
};


/* Forward decls.  */

static int find_id (GtkWidget *menu, int idin, const char *type, GtkWidget **widget);
static void activate_by_subitems (ESearchBar *esb, gint item_id, ESearchBarSubitem *subitems);

static void emit_search_activated (ESearchBar *esb);
static void emit_query_changed (ESearchBar *esb);


/* Utility functions.  */

static void
set_find_now_sensitive (ESearchBar *search_bar,
			gboolean sensitive)
{
	if (search_bar->ui_component != NULL)
		bonobo_ui_component_set_prop (search_bar->ui_component,
					      "/commands/ESearchBar:FindNow",
					      "sensitive", sensitive ? "1" : "0", NULL);
	
	gtk_widget_set_sensitive (search_bar->activate_button, sensitive);
}

static char *
verb_name_from_id (int id)
{
	return g_strdup_printf ("ESearchBar:Activate:%d", id);
}

/* This implements the "clear" action, i.e. clears the text and then emits
 * ::search_activated.  */

static void
clear_search (ESearchBar *esb)
{
	int item_row;
	GtkWidget *widget;
	ESearchBarSubitem *subitems;

	e_search_bar_set_text (esb, "");
	e_search_bar_set_item_id (esb, 0);

	item_row = find_id (esb->option_menu, 0, "EsbChoiceId", &widget);

	subitems = g_object_get_data (G_OBJECT (widget), "EsbChoiceSubitems");
	activate_by_subitems (esb, 0, subitems);

	emit_search_activated (esb);
}

/* Frees an array of subitem information */
static void
free_subitems (ESearchBarSubitem *subitems)
{
	ESearchBarSubitem *s;

	g_assert (subitems != NULL);

	for (s = subitems; s->id != -1; s++) {
		if (s->text)
			g_free (s->text);
	}

	g_free (subitems);
}

static void
free_menu_items (ESearchBar *esb)
{
	GSList *p;

	if (esb->menu_items == NULL)
		return;

	for (p = esb->menu_items; p != NULL; p = p->next) {
		ESearchBarItem *item;

		item = (ESearchBarItem *) p->data;

		/* (No submitems for the menu_items, so no need to free that
		   member.)  */

		g_free (item->text);
		g_free (item);
	}

	g_slist_free (esb->menu_items);
	esb->menu_items = NULL;
}


/* Signals.  */

static void
emit_query_changed (ESearchBar *esb)
{
	g_signal_emit (esb, esb_signals [QUERY_CHANGED], 0);
}

static void
emit_search_activated (ESearchBar *esb)
{
	if (esb->pending_activate) {
		g_source_remove (esb->pending_activate);
		esb->pending_activate = 0;
	}

	g_signal_emit (esb, esb_signals [SEARCH_ACTIVATED], 0);

	set_find_now_sensitive (esb, FALSE);
}

static void
emit_menu_activated (ESearchBar *esb, int item)
{
	g_signal_emit (esb,
		       esb_signals [MENU_ACTIVATED], 0,
		       item);
}


/* Callbacks -- Standard verbs.  */

static void
search_now_verb_cb (BonoboUIComponent *ui_component,
		    void *data,
		    const char *verb_name)
{
	ESearchBar *esb;

	esb = E_SEARCH_BAR (data);
	emit_search_activated (esb);
}

static void
clear_verb_cb (BonoboUIComponent *ui_component,
	       void *data,
	       const char *verb_name)
{
	ESearchBar *esb;

	esb = E_SEARCH_BAR (data);
	clear_search (esb);
}

static void
setup_standard_verbs (ESearchBar *search_bar)
{
	bonobo_ui_component_add_verb (search_bar->ui_component, "ESearchBar:Clear",
				      clear_verb_cb, search_bar);
	bonobo_ui_component_add_verb (search_bar->ui_component, "ESearchBar:FindNow",
				      search_now_verb_cb, search_bar);

	bonobo_ui_component_set (search_bar->ui_component, "/",
				 ("<commands>"
				  "  <cmd name=\"ESearchBar:Clear\"/>"
				  "  <cmd name=\"ESearchBar:FindNow\"/>"
				  "</commands>"),
				 NULL);

	/* Make sure the entries are created with the correct sensitivity.  */
	set_find_now_sensitive (search_bar, FALSE);
}

/* Callbacks -- The verbs for all the definable items.  */

static void
search_verb_cb (BonoboUIComponent *ui_component,
		void *data,
		const char *verb_name)
{
	ESearchBar *esb;
	const char *p;
	int id;

	esb = E_SEARCH_BAR (data);

	p = strrchr (verb_name, ':');
	g_assert (p != NULL);

	id = atoi (p + 1);
	emit_menu_activated (esb, id);
}

static void
entry_activated_cb (GtkWidget *widget,
		     ESearchBar *esb)
{
	emit_search_activated (esb);
}

static void
entry_changed_cb (GtkWidget *widget,
		  ESearchBar *esb)
{
	set_find_now_sensitive (esb, TRUE);
}

static void
subitem_activated_cb (GtkWidget *widget, ESearchBar *esb)
{
	gint id, subid;

	id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "EsbItemId"));
	subid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "EsbSubitemId"));

	esb->item_id = id;
	esb->subitem_id = subid;

	set_find_now_sensitive (esb, TRUE);
}

static char *
string_without_underscores (const char *s)
{
	char *new_string;
	const char *sp;
	char *dp;

	new_string = g_malloc (strlen (s) + 1);

	dp = new_string;
	for (sp = s; *sp != '\0'; sp ++) {
		if (*sp != '_') {
			*dp = *sp;
			dp ++;
		} else if (sp[1] == '_') {
			/* Translate "__" in "_".  */
			*dp = '_';
			dp ++;
			sp ++;
		}
	}
	*dp = 0;

	return new_string;
}

static void
activate_by_subitems (ESearchBar *esb, gint item_id, ESearchBarSubitem *subitems)
{
	if (subitems == NULL) {
		/* This item uses the entry. */

		/* Remove the menu */
		if (esb->suboption && esb->subitem_id != -1) {
			g_assert (esb->suboption->parent == esb->entry_box);
			g_assert (!esb->entry || esb->entry->parent == NULL);
			gtk_container_remove (GTK_CONTAINER (esb->entry_box), esb->suboption);
		}

		/* Create and add the entry */

		if (esb->entry == NULL) {
			esb->entry = gtk_entry_new();
			gtk_widget_set_size_request (esb->entry, 4, -1);
			g_object_ref (esb->entry);
			g_signal_connect (esb->entry, "changed",
					  G_CALLBACK (entry_changed_cb), esb);
			g_signal_connect (esb->entry, "activate",
					  G_CALLBACK (entry_activated_cb), esb);
			gtk_container_add (GTK_CONTAINER (esb->entry_box), esb->entry);
			gtk_widget_show(esb->entry);

			esb->subitem_id = -1;
		}

		if (esb->subitem_id == -1) {
			g_assert (esb->entry->parent == esb->entry_box);
			g_assert (!esb->suboption || esb->suboption->parent == NULL);
		} else {
			gtk_container_add (GTK_CONTAINER (esb->entry_box), esb->entry);
			gtk_widget_grab_focus (esb->entry);
			
			esb->subitem_id = -1;

		}
	} else {
		/* This item uses a submenu */
		GtkWidget *menu;
		GtkWidget *menu_item;
		gint i;

		/* Remove the entry */
		if (esb->entry && esb->subitem_id == -1) {
			g_assert (esb->entry->parent == esb->entry_box);
			g_assert (!esb->suboption || esb->suboption->parent == NULL);
			gtk_container_remove (GTK_CONTAINER (esb->entry_box), esb->entry);
		}

		/* Create and add the menu */

		if (esb->suboption == NULL) {
			esb->suboption = gtk_option_menu_new ();
			g_object_ref (esb->suboption);
			gtk_container_add (GTK_CONTAINER (esb->entry_box), esb->suboption);
			gtk_widget_show (esb->suboption);

			esb->subitem_id = subitems[0].id;
		}

		if (esb->subitem_id != -1) {
			g_assert (esb->suboption->parent == esb->entry_box);
			g_assert (!esb->entry || esb->entry->parent == NULL);
		} else {
			gtk_container_add (GTK_CONTAINER (esb->entry_box), esb->suboption);
			esb->subitem_id = subitems[0].id;
		}

		/* Create the items */

		esb->suboption_menu = menu = gtk_menu_new ();
		for (i = 0; subitems[i].id != -1; ++i) {
			if (subitems[i].text) {
				char *str;

				if (subitems[i].translate)
					str = string_without_underscores (_(subitems[i].text));
				else
					str = string_without_underscores (subitems[i].text);

				menu_item = gtk_menu_item_new_with_label (str);

				g_free (str);
			} else {
				menu_item = gtk_menu_item_new ();
				gtk_widget_set_sensitive (menu_item, FALSE);
			}

			g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
					   GINT_TO_POINTER (item_id));
			g_object_set_data (G_OBJECT (menu_item), "EsbSubitemId",
					   GINT_TO_POINTER (subitems[i].id));

			g_signal_connect (menu_item,
					  "activate",
					  G_CALLBACK (subitem_activated_cb),
					  esb);

			gtk_widget_show (menu_item);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
		}

		gtk_option_menu_remove_menu (GTK_OPTION_MENU (esb->suboption));
		gtk_option_menu_set_menu (GTK_OPTION_MENU (esb->suboption), menu);
	}

	if (esb->activate_button)
		gtk_widget_set_sensitive (esb->activate_button, subitems == NULL);
}

static void
option_activated_cb (GtkWidget *widget,
		     ESearchBar *esb)
{
	int id;

	id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "EsbChoiceId"));

	activate_by_subitems (esb, id, g_object_get_data (G_OBJECT (widget), "EsbChoiceSubitems"));

	esb->item_id = id;
	emit_query_changed (esb);
}

static void
activate_button_clicked_cb (GtkWidget *widget,
			    ESearchBar *esb)
{
	emit_search_activated (esb);

	gtk_widget_grab_focus (esb->entry);
}

static void
clear_button_clicked_cb (GtkWidget *widget,
			 ESearchBar *esb)
{
	clear_search (esb);

	gtk_widget_grab_focus (esb->entry);
}


/* Widgetry creation.  */

/* This function exists to fix the irreparable GtkOptionMenu stupidity.  In
   fact, this lame-ass widget adds a 1-pixel-wide empty border around the
   button for no reason.  So we have add a 1-pixel-wide border around the the
   buttons we have in the search bar to make things look right.  This is done
   through an event box.  */
static GtkWidget *
put_in_spacer_widget (GtkWidget *widget)
{
	GtkWidget *holder;

	holder = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (holder), 1);
	gtk_container_add (GTK_CONTAINER (holder), widget);

	return holder;
}

static ESearchBarSubitem *
copy_subitems (ESearchBarSubitem *subitems)
{
	gint i, N;
	ESearchBarSubitem *copy;

	if (subitems == NULL)
		return NULL;

	for (N=0; subitems[N].id != -1; ++N);
	copy = g_new (ESearchBarSubitem, N+1);

	for (i=0; i<N; ++i) {
		copy[i].text = g_strdup (subitems[i].text);
		copy[i].id = subitems[i].id;
		copy[i].translate = subitems[i].translate;
	}

	copy[N].text = NULL;
	copy[N].id = -1;

	return copy;
}

static void
append_xml_menu_item (GString *xml,
		      const char *name,
		      const char *label,
		      const char *verb,
		      const char *accelerator)
{
	char *encoded_label;
	
	encoded_label = bonobo_ui_util_encode_str (label);
	g_string_append_printf (xml, "<menuitem name=\"%s\" verb=\"%s\" label=\"%s\"",
				name, verb, encoded_label);
	g_free (encoded_label);
	
	if (accelerator != NULL)
		g_string_append_printf (xml, " accel=\"%s\"", accelerator);
	
	g_string_append (xml, "/>");
}

static void
remove_bonobo_menus (ESearchBar *esb)
{
	if (bonobo_ui_component_get_container (esb->ui_component) == CORBA_OBJECT_NIL)
		return;
	
	bonobo_ui_component_rm (esb->ui_component, "/menu/SearchPlaceholder", NULL);
}

static void
setup_bonobo_menus (ESearchBar *esb)
{
	GString *xml;
	GSList *p;
	char *verb_name;
	char *encoded_title;
	
	xml = g_string_new ("");
	
	encoded_title = bonobo_ui_util_encode_str (_("_Search"));
	g_string_append_printf (xml, "<submenu name=\"Search\" label=\"%s\">", encoded_title);
	g_free (encoded_title);
	
	g_string_append (xml, "<placeholder name=\"SearchBar\">");
	
	append_xml_menu_item (xml, "FindNow", _("_Find Now"), "ESearchBar:FindNow", NULL);
	append_xml_menu_item (xml, "Clear", _("_Clear"), "ESearchBar:Clear", "*Control**Shift*q");
	
	for (p = esb->menu_items; p != NULL; p = p->next) {
		const ESearchBarItem *item;
		
		item = (const ESearchBarItem *) p->data;
		
		verb_name = verb_name_from_id (item->id);
		bonobo_ui_component_add_verb (esb->ui_component, verb_name, search_verb_cb, esb);
		
		if (item->text == NULL)
			g_string_append (xml, "<separator/>");
		else
			append_xml_menu_item (xml, verb_name, item->text, verb_name, NULL);
		
		g_free (verb_name);
	}
	
	g_string_append (xml, "</placeholder>");
	g_string_append (xml, "</submenu>");
	
	remove_bonobo_menus (esb);
	bonobo_ui_component_set (esb->ui_component, "/menu/SearchPlaceholder", xml->str, NULL);

	g_string_free (xml, TRUE);
}

static void
update_bonobo_menus (ESearchBar *esb)
{
	setup_bonobo_menus (esb);
}

static void
set_menu (ESearchBar *esb,
	  ESearchBarItem *items)
{
	int i;

	free_menu_items (esb);

	if (items == NULL)
		return;

	for (i = 0; items[i].id != -1; i++) {
		ESearchBarItem *new_item;

		g_assert (items[i].subitems == NULL);

		new_item = g_new (ESearchBarItem, 1);
		new_item->text 	   = g_strdup (_(items[i].text));
		new_item->id   	   = items[i].id;
		new_item->subitems = NULL;

		esb->menu_items = g_slist_append (esb->menu_items, new_item);
	}

	if (esb->ui_component != NULL)
		update_bonobo_menus (esb);
}

/* Callback used when an option item is destroyed.  We have to destroy its
 * suboption items.
 */
static void
option_item_destroy_cb (GtkObject *object, gpointer data)
{
	ESearchBarSubitem *subitems;

	subitems = data;

	g_assert (subitems != NULL);
	free_subitems (subitems);
	g_object_set_data (G_OBJECT (object), "EsbChoiceSubitems", NULL);
}

static void
set_option (ESearchBar *esb, ESearchBarItem *items)
{
	GtkWidget *menu;
	int i;

	if (esb->option) {
		gtk_widget_destroy (esb->option_menu);
	} else {
		esb->option = gtk_option_menu_new ();
		gtk_widget_show (esb->option);
		gtk_box_pack_start (GTK_BOX (esb), esb->option, FALSE, FALSE, 0);
	}

	esb->option_menu = menu = gtk_menu_new ();
	for (i = 0; items[i].id != -1; i++) {
		GtkWidget *item;
		ESearchBarSubitem *subitems = NULL;

		if (items[i].text) {
			char *str;

			str = string_without_underscores (_(items[i].text));
			item = gtk_menu_item_new_with_label (str);
			g_free (str);
		} else {
			item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (item, FALSE);
		}

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		g_object_set_data (G_OBJECT (item), "EsbChoiceId", GINT_TO_POINTER(items[i].id));

		if (items[i].subitems != NULL) {
			subitems = copy_subitems (items[i].subitems);
			g_object_set_data (G_OBJECT (item), "EsbChoiceSubitems", subitems);
			g_signal_connect (item, "destroy",
					  G_CALLBACK (option_item_destroy_cb), subitems);
		}

		if (i == 0)
			activate_by_subitems (esb, items[i].id, subitems);

		g_signal_connect (item, "activate",
				  G_CALLBACK (option_activated_cb),
				  esb);
	}

	gtk_widget_show_all (menu);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (esb->option), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (esb->option), 0);

	gtk_widget_set_sensitive (esb->option, TRUE);
}

static GtkWidget *
add_button (ESearchBar *esb,
	    const char *text,
	    GCallback callback)
{
	GtkWidget *label;
	GtkWidget *holder;
	GtkWidget *button;

	label = gtk_label_new_with_mnemonic (text);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_widget_show (label);
	
	/* See the comment in `put_in_spacer_widget()' to understand
	   why we have to do this.  */
	
	button = gtk_button_new ();
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (button), label);
	
	holder = put_in_spacer_widget (button);
	gtk_widget_show (holder);
	
	g_signal_connect (G_OBJECT (button), "clicked", callback, esb);
	
	gtk_box_pack_end (GTK_BOX (esb), holder, FALSE, FALSE, 1);

	return button;
}

static int
find_id (GtkWidget *menu, int idin, const char *type, GtkWidget **widget)
{
	GList *l = GTK_MENU_SHELL (menu)->children;
	int row = -1, i = 0, id;

	if (widget)
		*widget = NULL;
	while (l) {
		id = GPOINTER_TO_INT (g_object_get_data (l->data, type));
		if (id == idin) {
			row = i;
			if (widget)
				*widget = l->data;
			break;
		}
		i++;
		l = l->next;
	}
	return row;
}


/* GtkObject methods.  */

static void
impl_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ESearchBar *esb = E_SEARCH_BAR (object);

	switch (prop_id) {
	case PROP_ITEM_ID:
		g_value_set_int (value, e_search_bar_get_item_id (esb));
		break;

	case PROP_SUBITEM_ID:
		g_value_set_int (value, e_search_bar_get_subitem_id (esb));
		break;

	case PROP_TEXT:
		g_value_set_string_take_ownership (value, e_search_bar_get_text (esb));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
impl_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ESearchBar *esb = E_SEARCH_BAR(object);
	
	switch (prop_id) {
	case PROP_ITEM_ID:
		e_search_bar_set_item_id (esb, g_value_get_int (value));
		break;

	case PROP_SUBITEM_ID:
		e_search_bar_set_subitem_id (esb, g_value_get_int (value));
		break;
		
	case PROP_TEXT:
		e_search_bar_set_text (esb, g_value_get_string (value));
		break;
		
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
impl_dispose (GObject *object)
{
	ESearchBar *esb = E_SEARCH_BAR (object);
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (object));

	/* These three we do need to unref, because we explicitly hold
	   references to them. */

	if (esb->ui_component != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (esb->ui_component));
		esb->ui_component = NULL;
	}
	if (esb->entry) {
		g_object_unref (esb->entry);
		esb->entry = NULL;
	}
	if (esb->suboption) {
		g_object_unref (esb->suboption);
		esb->suboption = NULL;
	}
	
	if (esb->pending_activate) {
		g_source_remove (esb->pending_activate);
		esb->pending_activate = 0;
	}

	free_menu_items (esb);
	
	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
class_init (ESearchBarClass *klass)
{
	GObjectClass *object_class;
	
	object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (gtk_hbox_get_type ());
	
	object_class->set_property = impl_set_property;
	object_class->get_property = impl_get_property;
	object_class->dispose = impl_dispose;
	
	klass->set_menu = set_menu;
	klass->set_option = set_option;

	g_object_class_install_property (object_class, PROP_ITEM_ID,
					 g_param_spec_int ("item_id",
							   _("Item ID"),
							   /*_( */"XXX blurb" /*)*/,
							   0, 0, 0,
							   G_PARAM_READWRITE | G_PARAM_LAX_VALIDATION));

	g_object_class_install_property (object_class, PROP_SUBITEM_ID,
					 g_param_spec_int ("subitem_id",
							   _("Subitem ID"),
							   /*_( */"XXX blurb" /*)*/,
							   0, 0, 0,
							   G_PARAM_READWRITE | G_PARAM_LAX_VALIDATION));

	g_object_class_install_property (object_class, PROP_TEXT, 
					 g_param_spec_string ("text",
							      _("Text"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));
	
	esb_signals [QUERY_CHANGED] =
		g_signal_new ("query_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESearchBarClass, query_changed),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
	
	esb_signals [MENU_ACTIVATED] =
		g_signal_new ("menu_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESearchBarClass, menu_activated),
			      NULL, NULL,
			      e_util_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	esb_signals [SEARCH_ACTIVATED] =
		g_signal_new ("search_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESearchBarClass, search_activated),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

static void
init (ESearchBar *esb)
{
	esb->ui_component     = NULL;
	esb->menu_items       = NULL;

	esb->option           = NULL;
	esb->entry            = NULL;
	esb->suboption        = NULL;
	
	esb->option_menu      = NULL;
	esb->suboption_menu   = NULL;
	esb->activate_button  = NULL;
	esb->clear_button     = NULL;
	esb->entry_box        = NULL;

	esb->pending_activate = 0;

	esb->item_id          = 0;
	esb->subitem_id       = 0;
}


/* Object construction.  */

static gint
idle_activate_hack (gpointer ptr)
{
	ESearchBar *esb = E_SEARCH_BAR (ptr);
	esb->pending_activate = 0;
	emit_search_activated (esb);
	return FALSE;
}

void
e_search_bar_construct (ESearchBar *search_bar,
			ESearchBarItem *menu_items,
			ESearchBarItem *option_items)
{
	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	g_return_if_fail (option_items != NULL);

	gtk_box_set_spacing (GTK_BOX (search_bar), 1);

	search_bar->clear_button    = add_button (search_bar, _("_Clear"),
						  G_CALLBACK (clear_button_clicked_cb));
	search_bar->activate_button = add_button (search_bar, _("Find _Now"),
						  G_CALLBACK (activate_button_clicked_cb));

	e_search_bar_set_menu (search_bar, menu_items);

	search_bar->entry_box = gtk_hbox_new (0, FALSE);

	e_search_bar_set_option (search_bar, option_items);

	gtk_widget_show (search_bar->entry_box);
	gtk_box_pack_start (GTK_BOX(search_bar), search_bar->entry_box, TRUE, TRUE, 0);

	/* 
	 * If the default choice for the option menu has subitems, then we need to
	 * activate the search immediately.  However, the developer won't have
	 * connected to the activated signal until after the object is constructed,
	 * so we can't emit here.  Thus we launch a one-shot idle function that will
	 * emit the changed signal, so that the proper callback will get invoked.
	 */
	if (search_bar->subitem_id >= 0) {
		gtk_widget_set_sensitive (search_bar->activate_button, FALSE);

		search_bar->pending_activate = g_idle_add (idle_activate_hack, search_bar);
	}
}

void
e_search_bar_set_menu (ESearchBar *search_bar, ESearchBarItem *menu_items)
{
	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	
	((ESearchBarClass *) GTK_OBJECT_GET_CLASS (search_bar))->set_menu (search_bar, menu_items);
}

void
e_search_bar_add_menu (ESearchBar *search_bar, ESearchBarItem *menu_item)
{
	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	
	set_menu (search_bar, menu_item);
}

void
e_search_bar_set_option (ESearchBar *search_bar, ESearchBarItem *option_items)
{
	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	g_return_if_fail (option_items != NULL);
	
	((ESearchBarClass *) GTK_OBJECT_GET_CLASS (search_bar))->set_option (search_bar, option_items);
}

/**
 * e_search_bar_set_suboption:
 * @search_bar: A search bar.
 * @option_id: Identifier of the main option menu item under which the subitems
 * are to be set.
 * @subitems: Array of subitem information.
 * 
 * Sets the items for the secondary option menu of a search bar.
 **/
void
e_search_bar_set_suboption (ESearchBar *search_bar, int option_id, ESearchBarSubitem *subitems)
{
	int row;
	GtkWidget *item;
	ESearchBarSubitem *old_subitems;
	ESearchBarSubitem *new_subitems;
	
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	
	row = find_id (search_bar->option_menu, option_id, "EsbChoiceId", &item);
	g_return_if_fail (row != -1);
	g_assert (item != NULL);

	old_subitems = g_object_get_data (G_OBJECT (item), "EsbChoiceSubitems");
	if (old_subitems) {
		/* This was connected in set_option() */
		g_signal_handlers_disconnect_matched (item,
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      old_subitems);
		free_subitems (old_subitems);
		g_object_set_data (G_OBJECT (item), "EsbChoiceSubitems", NULL);
	}

	if (subitems) {
		new_subitems = copy_subitems (subitems);
		g_object_set_data (G_OBJECT (item), "EsbChoiceSubitems", new_subitems);
		g_signal_connect (item, "destroy",
				  G_CALLBACK (option_item_destroy_cb), new_subitems);
	} else
		new_subitems = NULL;

	if (search_bar->item_id == option_id)
		activate_by_subitems (search_bar, option_id, new_subitems);
}

GtkWidget *
e_search_bar_new (ESearchBarItem *menu_items,
		  ESearchBarItem *option_items)
{
	GtkWidget *widget;

	g_return_val_if_fail (option_items != NULL, NULL);
	
	widget = GTK_WIDGET (gtk_type_new (e_search_bar_get_type ()));
	
	e_search_bar_construct (E_SEARCH_BAR (widget), menu_items, option_items);
	
	return widget;
}

void
e_search_bar_set_ui_component (ESearchBar *search_bar,
			       BonoboUIComponent *ui_component)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (search_bar->ui_component != NULL) {
		remove_bonobo_menus (search_bar);
		bonobo_object_unref (BONOBO_OBJECT (search_bar->ui_component));
	}

	search_bar->ui_component = ui_component;
	if (ui_component != NULL) {
		bonobo_object_ref (BONOBO_OBJECT (ui_component));
		setup_standard_verbs (search_bar);
		setup_bonobo_menus (search_bar);
	}
}

void
e_search_bar_set_menu_sensitive (ESearchBar *search_bar, int id, gboolean state)
{
	char *verb_name;
	char *path;

	verb_name = verb_name_from_id (id);
	path = g_strconcat ("/commands/", verb_name, NULL);
	g_free (verb_name);

	bonobo_ui_component_set_prop (search_bar->ui_component, path,
				      "sensitive", state ? "1" : "0",
				      NULL);

	g_free (path);
}

GType
e_search_bar_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (ESearchBarClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ESearchBar),
			0,             /* n_preallocs */
			(GInstanceInitFunc) init,
		};

		type = g_type_register_static (gtk_hbox_get_type (), "ESearchBar", &info, 0);
	}

	return type;
}

/**
 * e_search_bar_set_item_id:
 * @search_bar: A search bar.
 * @id: Identifier of the item to set.
 * 
 * Sets the active item in the options menu of a search bar.
 **/
void
e_search_bar_set_item_id (ESearchBar *search_bar, int id)
{
	int row;
	
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	
	row = find_id (search_bar->option_menu, id, "EsbChoiceId", NULL);
	g_return_if_fail (row != -1);
	
	search_bar->item_id = id;
	gtk_option_menu_set_history (GTK_OPTION_MENU (search_bar->option), row);
	emit_query_changed (search_bar);
}

/**
 * e_search_bar_get_item_id:
 * @search_bar: A search bar.
 * 
 * Queries the currently selected item in the options menu of a search bar.
 * 
 * Return value: Identifier of the selected item in the options menu.
 **/
int
e_search_bar_get_item_id (ESearchBar *search_bar)
{
	g_return_val_if_fail (search_bar != NULL, -1);
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), -1);
	
	return search_bar->item_id;
}

void
e_search_bar_set_subitem_id (ESearchBar *search_bar, int id)
{
	int row;

	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	row = find_id (search_bar->suboption_menu, id, "EsbSubitemId", NULL);
	g_return_if_fail (row != -1);

	search_bar->subitem_id = id;
	gtk_option_menu_set_history (GTK_OPTION_MENU (search_bar->suboption), row);
}

/**
 * e_search_bar_get_subitem_id:
 * @search_bar: A search bar.
 * 
 * Queries the currently selected item in the suboptions menu of a search bar.
 * 
 * Return value: Identifier of the selected item in the suboptions menu.
 * If the search bar currently contains an entry rather than a a suboption menu,
 * a value less than zero is returned.
 **/
int
e_search_bar_get_subitem_id (ESearchBar *search_bar)
{
	g_return_val_if_fail (search_bar != NULL, -1);
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), -1);
	
	return search_bar->subitem_id;
}

/**
 * e_search_bar_set_ids:
 * @search_bar: A search bar.
 * @item_id: Identifier of the item to set.
 * @subitem_id: Identifier of the subitem to set.
 * 
 * Sets the item and subitem ids for a search bar.  This is intended to switch
 * to an item that has subitems.
 **/
void
e_search_bar_set_ids (ESearchBar *search_bar, int item_id, int subitem_id)
{
	int item_row;
	GtkWidget *item_widget;
	ESearchBarSubitem *subitems;

	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	item_row = find_id (search_bar->option_menu, item_id, "EsbChoiceId", &item_widget);
	g_return_if_fail (item_row != -1);
	g_assert (item_widget != NULL);

	subitems = g_object_get_data (G_OBJECT (item_widget), "EsbChoiceSubitems");
	g_return_if_fail (subitems != NULL);

	search_bar->item_id = item_id;
	gtk_option_menu_set_history (GTK_OPTION_MENU (search_bar->option), item_row);

	activate_by_subitems (search_bar, item_id, subitems);
	e_search_bar_set_subitem_id (search_bar, subitem_id);
}

/**
 * e_search_bar_set_text:
 * @search_bar: A search bar.
 * @text: Text to set in the search bar's entry line.
 *
 * Sets the text string inside the entry line of a search bar.
 **/
void
e_search_bar_set_text (ESearchBar *search_bar, const char *text)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	gtk_entry_set_text (GTK_ENTRY (search_bar->entry), text);
}

/**
 * e_search_bar_get_text:
 * @search_bar: A search bar.
 * 
 * Queries the text of the entry line in a search bar.
 * 
 * Return value: The text string that is in the entry line of the search bar.
 * This must be freed using g_free().  If a suboption menu is active instead
 * of an entry, NULL is returned.
 **/
char *
e_search_bar_get_text (ESearchBar *search_bar)
{
	g_return_val_if_fail (search_bar != NULL, NULL);
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);
	
	return search_bar->subitem_id < 0 ? g_strdup (gtk_entry_get_text (GTK_ENTRY (search_bar->entry))) : NULL;
}
