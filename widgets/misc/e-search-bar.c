/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Ettore Perazzoli <ettore@ximian.com>
 *		Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <misc/e-unicode.h>
#include <misc/e-gui-utils.h>


#include <bonobo/bonobo-ui-util.h>

#include <stdlib.h>
#include <string.h>

#include "e-icon-entry.h"
#include "e-search-bar.h"
#include "e-util/e-util.h"


enum {
	QUERY_CHANGED,
	MENU_ACTIVATED,
	SEARCH_ACTIVATED,
	SEARCH_CLEARED,
	LAST_SIGNAL
};

static gint esb_signals [LAST_SIGNAL] = { 0, };

static GtkHBoxClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_ITEM_ID,
	PROP_SUBITEM_ID,
	PROP_TEXT
};


/* Forward decls.  */

static gint find_id (GtkWidget *menu, gint idin, const gchar *type, GtkWidget **widget);

static void emit_search_activated (ESearchBar *esb);
static void emit_query_changed (ESearchBar *esb);


/* Utility functions.  */

static void
esb_paint_label (GtkWidget *label, gboolean active)
{
	static gchar *sens = NULL;
	static gchar *insens = NULL;
	gchar *text;

	if (!label)
		return;

	if (!sens) {
		GtkStyle *default_style = gtk_widget_get_default_style ();
		sens = gdk_color_to_string (&default_style->text[GTK_STATE_SELECTED]);
		insens = gdk_color_to_string (&default_style->text[GTK_STATE_NORMAL]);
	}
	text = g_strdup_printf("<span foreground=\"%s\">%s</span>", active ? sens : insens, _("Search"));
	gtk_label_set_markup ((GtkLabel *)label, text);
	g_free(text);
}

static void
set_find_now_sensitive (ESearchBar *search_bar,
			gboolean sensitive)
{
	if (search_bar->ui_component != NULL)
		bonobo_ui_component_set_prop (search_bar->ui_component,
					      "/commands/ESearchBar:FindNow",
					      "sensitive", sensitive ? "1" : "0", NULL);
}

static void
update_clear_menuitem_sensitive (ESearchBar *search_bar)
{
	if (search_bar->ui_component != NULL) {
		gboolean sensitive = GTK_WIDGET_SENSITIVE (search_bar->clear_button) || search_bar->viewitem_id != 0;

		bonobo_ui_component_set_prop (search_bar->ui_component,
					      "/commands/ESearchBar:Clear",
					      "sensitive", sensitive ? "1" : "0", NULL);
	}
}

static void
clear_button_state_changed (GtkWidget *clear_button, GtkStateType state, ESearchBar *search_bar)
{
	g_return_if_fail (clear_button != NULL && search_bar != NULL);

	if (!search_bar->lite)
		update_clear_menuitem_sensitive (search_bar);
}

static gchar *
verb_name_from_id (gint id)
{
	return g_strdup_printf ("ESearchBar:Activate:%d", id);
}

/* This implements the "clear" action, i.e. clears the text and then emits
 * ::search_activated.  */

static void
clear_search (ESearchBar *esb)
{
	e_search_bar_set_text (esb, "");
	esb->block_search = TRUE;
	if (esb->item_id < 0)
		e_search_bar_set_item_id (esb, esb->last_search_option);
	e_search_bar_set_viewitem_id (esb, 0);
	esb->block_search = FALSE;
	emit_search_activated (esb);
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
	if (!esb->lite)
		update_clear_menuitem_sensitive (esb);
}

static void
emit_search_activated(ESearchBar *esb)
{
	if (esb->pending_activate) {
		g_source_remove (esb->pending_activate);
		esb->pending_activate = 0;
	}

	g_signal_emit (esb, esb_signals [SEARCH_ACTIVATED], 0);

	if (!esb->lite) {
		set_find_now_sensitive (esb, FALSE);
		update_clear_menuitem_sensitive (esb);
	}
}

static void
emit_menu_activated (ESearchBar *esb, gint item)
{
	g_signal_emit (esb,
		       esb_signals [MENU_ACTIVATED], 0,
		       item);
}


/* Callbacks -- Standard verbs.  */

static void
search_now_verb_cb (BonoboUIComponent *ui_component,
		    gpointer data,
		    const gchar *verb_name)
{
	ESearchBar *esb;
	GtkStyle *style = gtk_widget_get_default_style ();
	gchar *text;

	esb = E_SEARCH_BAR (data);
	text = e_search_bar_get_text (esb);

	if (text && *text) {
		gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
		gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
		gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
		gtk_widget_modify_base (esb->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
	} else {
		gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_set_sensitive (esb->clear_button, FALSE);
	}

	g_free (text);
	emit_search_activated (esb);
}

static void
clear_verb_cb (BonoboUIComponent *ui_component,
	       gpointer data,
	       const gchar *verb_name)
{
	ESearchBar *esb;
	esb = E_SEARCH_BAR (data);

	e_search_bar_clear_search (esb);
	gtk_widget_grab_focus (esb->entry);
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

	/* Make sure the entries are created with the correct sensitivity. */
	set_find_now_sensitive (search_bar, FALSE);
	update_clear_menuitem_sensitive (search_bar);
}

/* Callbacks -- The verbs for all the definable items.  */

static void
search_verb_cb (BonoboUIComponent *ui_component,
		gpointer data,
		const gchar *verb_name)
{
	ESearchBar *esb;
	const gchar *p;
	gint id;

	esb = E_SEARCH_BAR (data);

	p = strrchr (verb_name, ':');
	g_return_if_fail (p != NULL);

	id = atoi (p + 1);

	emit_menu_activated (esb, id);
}

/* Get the selected menu item's label */
static const gchar *
get_selected_item_label (GtkWidget *menu)
{
	GtkWidget *label, *item;
	const gchar *text = NULL;

	item = gtk_menu_get_active ((GtkMenu *)menu);
	label = gtk_bin_get_child ((GtkBin *)item);

	if (GTK_IS_LABEL (label))
		text = gtk_label_get_text ((GtkLabel *)label);

	return text;
}

static gboolean
entry_focus_in_cb (GtkWidget *widget,
		   GdkEventFocus *event,
		   ESearchBar *esb)
{
	GtkStyle *entry_style, *default_style;

	entry_style = gtk_widget_get_style (esb->entry);
	default_style = gtk_widget_get_default_style ();

	if (gdk_color_equal (&(entry_style->text[GTK_STATE_NORMAL]), &(default_style->text[GTK_STATE_INSENSITIVE]))) {
		gtk_entry_set_text (GTK_ENTRY (esb->entry), "");
		gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
	}

	return FALSE;
}

static gboolean
paint_search_text (GtkWidget *widget, ESearchBar *esb)
{
	GtkStyle *style = gtk_widget_get_default_style ();
	const gchar *text = NULL;
	GtkWidget *menu_widget = esb->option_menu;

	text = gtk_entry_get_text (GTK_ENTRY (widget));
	if (text && *text)
		return FALSE;

	if (!GTK_WIDGET_SENSITIVE (esb->option_button)) {
		menu_widget = esb->scopeoption_menu;
		text = g_object_get_data (G_OBJECT(gtk_menu_get_active ( GTK_MENU (esb->scopeoption_menu))),"string");
	} else if (!GTK_IS_RADIO_MENU_ITEM (gtk_menu_get_active ( GTK_MENU (esb->option_menu))))
		return FALSE;
	else /* no query in search entry .. so set the current option */
		text = get_selected_item_label (menu_widget);

	if (text && *text) {
		gchar *t;

		if (!GTK_WIDGET_HAS_FOCUS(esb->entry)) {
			gtk_entry_set_text (GTK_ENTRY (esb->entry), text);
			gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_INSENSITIVE]));
		}

		t = g_strdup_printf ("%s: %s\n%s", _("Search"), text, _("Click here to change the search type"));
		gtk_widget_set_tooltip_text (esb->option_button, t);
		g_free (t);

		gtk_widget_set_sensitive (esb->clear_button, FALSE);
	}

	return FALSE;
}

void
e_search_bar_paint (ESearchBar *search_bar)
{
	paint_search_text (search_bar->entry, search_bar);
}

static gboolean
entry_focus_out_cb (GtkWidget *widget,
		   GdkEventFocus *event,
		   ESearchBar *esb)
{
	return paint_search_text (widget, esb);
}

static void
entry_activated_cb (GtkWidget *widget,
		     ESearchBar *esb)
{
	const gchar *text = gtk_entry_get_text (GTK_ENTRY (esb->entry));
	GtkStyle *style = gtk_widget_get_default_style ();

	if (text && *text) {
		gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
		gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
		gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
		esb_paint_label (esb->label, TRUE);
		if (!esb->lite)
			gtk_widget_modify_base (esb->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
	} else {
		gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, NULL);
		esb_paint_label (esb->label, FALSE);
		if (!esb->lite)
			gtk_widget_set_sensitive (esb->clear_button, FALSE);
	}

	emit_search_activated (esb);
}

static void
search_entry_press_cb (GtkWidget *w, GdkEventButton *event, ESearchBar *esb)
{
	if (event->button == 1)
		entry_activated_cb (w, esb);
}

static void
entry_changed_cb (GtkWidget *widget,
		  ESearchBar *esb)
{
	const gchar *text = gtk_entry_get_text (GTK_ENTRY (esb->entry));
	GtkStyle *entry_style, *default_style;

	entry_style = gtk_widget_get_style (esb->entry);
	default_style = gtk_widget_get_default_style ();

	if (text && *text) {
		if (gdk_color_equal (&(entry_style->text[GTK_STATE_NORMAL]), &(default_style->text[GTK_STATE_INSENSITIVE])))
			gtk_widget_set_sensitive (esb->clear_button, FALSE);
		else
			gtk_widget_set_sensitive (esb->clear_button, TRUE);
	} else {
		/* selected color means some search text is active */
		gtk_widget_set_sensitive (esb->clear_button, gdk_color_equal (&(entry_style->base[GTK_STATE_NORMAL]), &(default_style->base[GTK_STATE_SELECTED])));
	}
}

static void
viewitem_activated_cb(GtkWidget *widget, ESearchBar *esb)
{
	gint viewid;
	GtkStyle *entry_style, *default_style;

	widget = gtk_menu_get_active (GTK_MENU (esb->viewoption_menu));

	viewid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "EsbItemId"));
	esb->viewitem_id = viewid;

	entry_style = gtk_widget_get_style (esb->entry);
	default_style = gtk_widget_get_default_style ();

	/* If the text is grayed, Its not the query string */
	if (gdk_color_equal (&(entry_style->text[GTK_STATE_NORMAL]), &(default_style->text[GTK_STATE_INSENSITIVE]))) {
		gtk_entry_set_text (GTK_ENTRY (esb->entry), "");
		gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
	}

	esb->block_search = TRUE;
	emit_search_activated (esb);
	esb->block_search = FALSE;
}

static void
scopeitem_activated_cb(GtkWidget *widget, ESearchBar *esb)
{
	gint scopeid;
	GtkStyle *entry_style, *default_style;

	widget = gtk_menu_get_active (GTK_MENU (esb->scopeoption_menu));

	scopeid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "EsbItemId"));
	esb->scopeitem_id = scopeid;

	entry_style = gtk_widget_get_style (esb->entry);
	default_style = gtk_widget_get_default_style ();

	/* If the text is grayed, Its not the query string */
	if (gdk_color_equal (&(entry_style->text[GTK_STATE_NORMAL]), &(default_style->text[GTK_STATE_INSENSITIVE]))) {
		gtk_widget_grab_focus (esb->entry);
		gtk_entry_set_text (GTK_ENTRY (esb->entry), "");
		gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
	}

	esb->block_search = TRUE;
	emit_search_activated (esb);
	esb->block_search = FALSE;
}

static void
option_activated_cb (GtkWidget *widget,
		     ESearchBar *esb)
{
	gint id;
	const gchar *text;

	id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "EsbItemId"));

	e_search_bar_set_item_id (esb, id);

	if (GTK_IS_RADIO_MENU_ITEM (gtk_menu_get_active ( GTK_MENU (esb->option_menu)))) {
		gchar *t;
		text = get_selected_item_label (esb->option_menu);
		if (text && *text)
			t = g_strdup_printf ("%s: %s\n%s", _("Search"), text, _("Click here to change the search type"));
		else
			t = g_strdup_printf ("%s: %s", _("Search"), _("Click here to change the search type"));

		gtk_widget_set_tooltip_text (esb->option_button, t);
		g_free (t);
	}

	if (!esb->block_search) {
		emit_query_changed (esb);
	}
	if (!esb->block_search && id > 0) {
		emit_search_activated (esb);
	}
}

static void
option_button_clicked_cb (GtkWidget *widget, GdkEventButton *event,
			  ESearchBar *esb)
{
	gtk_menu_popup (GTK_MENU (esb->option_menu), NULL, NULL, NULL, NULL,1,gtk_get_current_event_time());

	gtk_widget_grab_focus (esb->entry);
}

static void
clear_button_clicked_cb (GtkWidget *widget, GdkEventButton *event,
			 ESearchBar *esb)
{
	gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, NULL);
	gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
	gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, NULL);
	gtk_widget_set_sensitive (esb->clear_button, FALSE);
	esb_paint_label (esb->label, FALSE);

	clear_search (esb);
	gtk_entry_set_text (GTK_ENTRY (esb->entry), "");
	gtk_widget_grab_focus (esb->entry);
}

static gboolean
entry_key_press_cb (GtkWidget *widget,
		   GdkEventKey *key_event,
		   ESearchBar *esb)
{
	if (((key_event->state & gtk_accelerator_get_default_mod_mask ()) ==
		GDK_MOD1_MASK) && (key_event->keyval == GDK_Down)) {
			option_button_clicked_cb (NULL, NULL, esb);
			return TRUE;
	}

	return FALSE;
}

#if 0
static void
scopeoption_changed_cb (GtkWidget *option_menu, ESearchBar *search_bar)
{
	gchar *text = NULL;

	text = e_search_bar_get_text (search_bar);
	if (!(text && *text))
		gtk_widget_grab_focus (search_bar->entry);

	if (!search_bar->block_search)
		emit_query_changed (search_bar);

	g_free (text);
}
#endif

/* Widgetry creation.  */

#if 0
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
#endif

static void
append_xml_menu_item (GString *xml,
		      const gchar *name,
		      const gchar *label,
		      const gchar *stock,
		      const gchar *verb,
		      const gchar *accelerator)
{
	gchar *encoded_label;

	encoded_label = bonobo_ui_util_encode_str (label);
	g_string_append_printf (xml, "<menuitem name=\"%s\" verb=\"%s\" label=\"%s\"",
				name, verb, encoded_label);
	g_free (encoded_label);

	if (accelerator != NULL)
		g_string_append_printf (xml, " accel=\"%s\"", accelerator);
	if (stock != NULL)
		g_string_append_printf (xml, " pixtype=\"stock\" pixname=\"%s\"", stock);

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
	gchar *verb_name;
	gchar *encoded_title;

	xml = g_string_new ("");

	encoded_title = bonobo_ui_util_encode_str (_("_Search"));
	g_string_append_printf (xml, "<submenu name=\"Search\" label=\"%s\">", encoded_title);
	g_free (encoded_title);

	g_string_append (xml, "<placeholder name=\"SearchBar\">");

	append_xml_menu_item (xml, "FindNow", _("_Find Now"), "gtk-find", "ESearchBar:FindNow", NULL);
	append_xml_menu_item (xml, "Clear", _("_Clear"), "gtk-clear", "ESearchBar:Clear", "*Control**Shift*q");

	for (p = esb->menu_items; p != NULL; p = p->next) {
		const ESearchBarItem *item;

		item = (const ESearchBarItem *) p->data;

		verb_name = verb_name_from_id (item->id);
		bonobo_ui_component_add_verb (esb->ui_component, verb_name, search_verb_cb, esb);

		if (item->text == NULL)
			g_string_append (xml, "<separator/>");
		else
			append_xml_menu_item (xml, verb_name, item->text, NULL, verb_name, NULL);

		g_free (verb_name);
	}

	g_string_append (xml, "</placeholder>");
	g_string_append (xml, "</submenu>");

	remove_bonobo_menus (esb);
	bonobo_ui_component_set (esb->ui_component, "/menu/SearchPlaceholder", xml->str, NULL);

	g_string_free (xml, TRUE);

	if (esb->clear_button) {
		g_signal_connect (esb->clear_button, "state-changed", G_CALLBACK (clear_button_state_changed), esb);
	}
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
	gint i;

	free_menu_items (esb);

	if (items == NULL)
		return;

	for (i = 0; items[i].id != -1; i++) {
		ESearchBarItem *new_item;

		new_item = g_new (ESearchBarItem, 1);
		new_item->text	   = items[i].text ? g_strdup (_(items[i].text)) : NULL;
		new_item->id	   = items[i].id;
		new_item->type     = items[i].type;

		esb->menu_items = g_slist_append (esb->menu_items, new_item);
	}

	if (!esb->lite && esb->ui_component != NULL)
		update_bonobo_menus (esb);
}

/* /\* Callback used when an option item is destroyed.  We have to destroy its */
/*  * suboption items. */
/*  *\/ */
/* static gpointer */
/* option_item_destroy_cb (GtkObject *object, gpointer data) */
/* { */
/* /\*	ESearchBarSubitem *subitems; *\/ */

/* /\*	subitems = data; *\/ */

/* /\*	g_assert (subitems != NULL); *\/ */
/* /\*	free_subitems (subitems); *\/ */
/* /\*	g_object_set_data (G_OBJECT (object), "EsbChoiceSubitems", NULL); *\/ */
/* } */

static void
set_option (ESearchBar *esb, ESearchBarItem *items)
{
	GtkWidget *menu;
	GSList *group = NULL;
	gint i;

	if (esb->option_menu)
		gtk_widget_destroy (esb->option_menu);

	esb->option_menu = menu = gtk_menu_new ();
	for (i = 0; items[i].id != -1; i++) {
		GtkWidget *item;

		/* Create a new group */
		if (items[i].id == 0)
			group = NULL;

		if (items[i].text) {
			gchar *str;
			str = e_str_without_underscores (_(items[i].text));
			switch (items[i].type) {
			    case ESB_ITEMTYPE_NORMAL:
				    item = gtk_menu_item_new_with_label (str);
				    break;
			    case ESB_ITEMTYPE_CHECK:
				    item = gtk_check_menu_item_new_with_label (str);
				    break;
			    case ESB_ITEMTYPE_RADIO:
				    item = gtk_radio_menu_item_new_with_label (group, str);
				    group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM (item));
				    break;
			    default:
				    /* Fixme : this should be a normal item */
				    item = gtk_radio_menu_item_new_with_label (group, str);
				    group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM (item));
				    break;
			}
			g_free (str);
		} else {
			item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (item, FALSE);
		}

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		g_object_set_data (G_OBJECT (item), "EsbItemId", GINT_TO_POINTER(items[i].id));

		g_signal_connect (item, "activate",
				  G_CALLBACK (option_activated_cb),
				  esb);
	}

	gtk_widget_show_all (menu);
	g_object_set_data (G_OBJECT(esb->option_menu), "group", group);
	entry_focus_out_cb (esb->entry, NULL, esb);
}

static gint
find_id (GtkWidget *menu, gint idin, const gchar *type, GtkWidget **widget)
{
	GList *l = GTK_MENU_SHELL (menu)->children;
	gint row = -1, i = 0, id;

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

	case PROP_TEXT:
		g_value_take_string (value, e_search_bar_get_text (esb));
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

	if (!esb->lite && esb->ui_component != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (esb->ui_component));
		esb->ui_component = NULL;
	}
/*	if (esb->entry) { */
/*		g_object_unref (esb->entry); */
/*		esb->entry = NULL; */
/*	} */
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
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	esb_signals [MENU_ACTIVATED] =
		g_signal_new ("menu_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESearchBarClass, menu_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	esb_signals [SEARCH_ACTIVATED] =
		g_signal_new ("search_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESearchBarClass, search_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	esb_signals [SEARCH_CLEARED] =
		g_signal_new ("search_cleared",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESearchBarClass, search_cleared),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
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
	esb->option_button    = NULL;
	esb->clear_button     = NULL;
	esb->entry_box        = NULL;

	esb->scopeoption_menu = NULL;
	esb->scopeoption      = NULL;
	esb->scopeoption_box  = NULL;

	esb->pending_activate = 0;

	esb->item_id          = 0;
	esb->scopeitem_id     = 0;
	esb->last_search_option = 0;
	esb->block_search        = FALSE;
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
	GtkWidget *label, *hbox, *bighbox;

	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	g_return_if_fail (option_items != NULL);

	gtk_box_set_spacing (GTK_BOX (search_bar), 3);

	gtk_box_set_homogeneous (GTK_BOX (search_bar), FALSE);

	bighbox = gtk_hbox_new (FALSE, 0);
	search_bar->entry_box = gtk_hbox_new (0, FALSE);
	search_bar->icon_entry = e_icon_entry_new ();
	search_bar->entry = e_icon_entry_get_entry (E_ICON_ENTRY (search_bar->icon_entry));

	g_signal_connect (search_bar->entry, "changed",
			  G_CALLBACK (entry_changed_cb), search_bar);
	g_signal_connect (search_bar->entry, "activate",
			  G_CALLBACK (entry_activated_cb), search_bar);
	g_signal_connect (search_bar->entry, "focus-in-event",
			  G_CALLBACK (entry_focus_in_cb), search_bar);
	g_signal_connect (search_bar->entry, "focus-out-event",
			  G_CALLBACK (entry_focus_out_cb), search_bar);
	g_signal_connect (search_bar->entry, "key-press-event",
			  G_CALLBACK (entry_key_press_cb), search_bar);

	search_bar->label = NULL;
	if (search_bar->lite) {
		label = e_icon_entry_create_text (_("Search"));
		g_signal_connect (G_OBJECT (label), "button-press-event", G_CALLBACK(search_entry_press_cb), search_bar);
		e_icon_entry_pack_widget (E_ICON_ENTRY (search_bar->icon_entry), label, FALSE);
		search_bar->label = g_object_get_data ((GObject *)label, "lbl");
		esb_paint_label (search_bar->label, FALSE);

		label = e_icon_entry_create_separator ();
		e_icon_entry_pack_widget (E_ICON_ENTRY (search_bar->icon_entry), label, FALSE);
	}

	search_bar->clear_button = e_icon_entry_create_button ("gtk-clear");
	g_signal_connect (G_OBJECT (search_bar->clear_button), "button-press-event", G_CALLBACK(clear_button_clicked_cb), search_bar);
	e_icon_entry_pack_widget (E_ICON_ENTRY (search_bar->icon_entry), search_bar->clear_button, FALSE);

	search_bar->option_button = e_icon_entry_create_button ("gtk-find");
	g_signal_connect (G_OBJECT (search_bar->option_button), "button-press-event", G_CALLBACK(option_button_clicked_cb), search_bar);
	e_icon_entry_pack_widget (E_ICON_ENTRY (search_bar->icon_entry), search_bar->option_button, TRUE);

	if (!search_bar->lite)
		gtk_box_pack_start (GTK_BOX(search_bar->entry_box), search_bar->icon_entry, FALSE, FALSE, 0);
	else
		gtk_box_pack_start (GTK_BOX(search_bar->entry_box), search_bar->icon_entry, TRUE, TRUE, 0);

	gtk_widget_show_all (search_bar->entry_box);
	gtk_widget_set_sensitive (search_bar->clear_button, FALSE);

	if (!search_bar->lite) {
		/* Current View filter */
		search_bar->viewoption_box = gtk_hbox_new (0, FALSE);

		/* To Translators: The "Show: " label is followed by the Quick Search Dropdown Menu where you can choose
		to display "All Messages", "Unread Messages", "Message with 'Important' Label" and so on... */
		label = gtk_label_new_with_mnemonic (_("Sho_w: "));
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX(search_bar->viewoption_box), label, FALSE, FALSE, 0);

		search_bar->viewoption = gtk_option_menu_new ();
		gtk_label_set_mnemonic_widget ((GtkLabel *)label, search_bar->viewoption);
		gtk_box_pack_start (GTK_BOX(search_bar->viewoption_box), search_bar->viewoption, FALSE, TRUE, 0);
		gtk_widget_show_all (search_bar->viewoption_box);
		gtk_box_pack_start (GTK_BOX(search_bar), search_bar->viewoption_box, FALSE, FALSE, 0);

		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX(search_bar), hbox, FALSE, FALSE, 0);
	}

	/* Search entry */
	hbox = gtk_hbox_new (FALSE, 0);
	/* To Translators: The "Show: " label is followed by the Quick Search Text input field where one enters
	the term to search for */
	if (!search_bar->lite) {
		label = gtk_label_new_with_mnemonic (_("Sear_ch: "));
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX(hbox), search_bar->entry_box, FALSE, FALSE, 0);
		gtk_label_set_mnemonic_widget ((GtkLabel *)label, search_bar->entry);
	} else {
		gtk_box_pack_start (GTK_BOX(hbox), search_bar->entry_box, TRUE, TRUE, 0);
	}
	gtk_widget_show (search_bar->entry_box);

	if (!search_bar->lite) {
		/* Search Scope Widgets */
		search_bar->scopeoption_box = gtk_hbox_new (0, FALSE);
		gtk_box_set_spacing (GTK_BOX (search_bar->scopeoption_box), 3);
		/* To Translators: The " in " label is part of the Quick Search Bar, example:
		Search: | <user's_search_term> | in | Current Folder/All Accounts/Current Account */
		label = gtk_label_new_with_mnemonic (_(" i_n "));
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX(search_bar->scopeoption_box), label, FALSE, FALSE, 0);

		search_bar->scopeoption = gtk_option_menu_new ();
	/*	g_signal_connect (GTK_OPTION_MENU (search_bar->scopeoption), "changed", scopeoption_changed_cb, search_bar); */
		gtk_box_pack_start (GTK_BOX(search_bar->scopeoption_box), search_bar->scopeoption, FALSE, FALSE, 0);
		gtk_widget_show_all (search_bar->scopeoption_box);
		gtk_widget_hide (hbox);
		gtk_label_set_mnemonic_widget ((GtkLabel *)label, search_bar->scopeoption);

		gtk_box_pack_end (GTK_BOX(hbox), search_bar->scopeoption_box, FALSE, FALSE, 0);
		gtk_widget_hide (search_bar->scopeoption_box);

	}
	if (!search_bar->lite)
		gtk_box_pack_end (GTK_BOX(search_bar), hbox, FALSE, FALSE, 0);
	else
		gtk_box_pack_end (GTK_BOX(search_bar), hbox, TRUE, TRUE, 0);

	gtk_widget_show (hbox);

	/* Set the menu */
	e_search_bar_set_menu (search_bar, menu_items);
	e_search_bar_set_option (search_bar, option_items);

	/*
	 * If the default choice for the option menu has subitems, then we need to
	 * activate the search immediately.  However, the developer won't have
	 * connected to the activated signal until after the object is constructed,
	 * so we can't emit here.  Thus we launch a one-shot idle function that will
	 * emit the changed signal, so that the proper callback will get invoked.
	 */
	if (!search_bar->lite)
		search_bar->pending_activate = g_idle_add (idle_activate_hack, search_bar);
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

void
e_search_bar_set_viewoption_menufunc (ESearchBar *search_bar, ESearchBarMenuFunc *menu_gen_func, gpointer data)
{
	g_signal_connect (search_bar->viewoption, "button_press_event", G_CALLBACK (menu_gen_func), data);
}

/**
 * e_search_bar_set_viewoption_menu:
 * @search_bar: A search bar.
 * @option_id: Identifier of the main option menu item under which the subitems
 * are to be set.
 * @subitems: Array of subitem information.
 *
 * Sets the items for the secondary option menu of a search bar.
 **/
void
e_search_bar_set_viewoption_menu (ESearchBar *search_bar, GtkWidget *menu)
{

	if (search_bar->viewoption_menu != NULL)
		gtk_option_menu_remove_menu (GTK_OPTION_MENU (search_bar->viewoption));

	search_bar->viewoption_menu = menu;
	gtk_option_menu_set_menu (GTK_OPTION_MENU (search_bar->viewoption), search_bar->viewoption_menu);

	g_signal_connect (search_bar->viewoption_menu,
			  "selection-done",
			  G_CALLBACK (viewitem_activated_cb),
			  search_bar);
}

GtkWidget *
e_search_bar_get_selected_viewitem (ESearchBar *search_bar)
{
	GtkWidget *widget = NULL;

	widget = gtk_menu_get_active (GTK_MENU (search_bar->viewoption_menu));

	return widget;
}

/**
 * e_search_bar_set_viewoption:
 * @search_bar: A search bar.
 * @option_id: Identifier of the main option menu item under which the subitems
 * are to be set.
 * @subitems: Array of subitem information.
 *
 * Sets the items for the secondary option menu of a search bar.
 **/
void
e_search_bar_set_viewoption (ESearchBar *search_bar, gint option_id, ESearchBarItem *subitems)
{
	GtkWidget *menu;
	GtkWidget *menu_item;
	gint i;

	/* Create the menu if it is not there. right scenario ????*/

	if (search_bar->viewoption_menu == NULL) {
		search_bar->viewoption_menu = menu = gtk_menu_new ();
	} else {
		gtk_option_menu_remove_menu (GTK_OPTION_MENU (search_bar->viewoption));
		search_bar->viewoption_menu = menu = gtk_menu_new ();
	}

	/* Create the items */

	for (i = 0; subitems[i].id != -1; ++i) {
		if (subitems[i].text) {
			gchar *str = NULL;
			str = e_str_without_underscores (subitems[i].text);
			menu_item = gtk_menu_item_new_with_label (str);
			g_free (str);
		} else {
			menu_item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menu_item, FALSE);
		}

		g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
				   GINT_TO_POINTER (subitems[i].id));

		g_signal_connect (menu_item,
				  "activate",
				  G_CALLBACK (viewitem_activated_cb),
				  search_bar);

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}
		gtk_option_menu_set_menu (GTK_OPTION_MENU (search_bar->viewoption), menu);

}

/**
 * e_search_bar_set_scopeoption:
 * @search_bar: A search bar.
 * are to be set.
 * @scopeitems: Array of scope information.
 *
 * Sets the items for the search scope option menu of a search bar.
 **/
void
e_search_bar_set_scopeoption (ESearchBar *search_bar, ESearchBarItem *scopeitems)
{
	GtkWidget *menu;
	GtkWidget *menu_item;
	gint i;

	gtk_widget_show (search_bar->scopeoption_box);
	if (search_bar->scopeoption_menu != NULL) {
		gtk_option_menu_remove_menu (GTK_OPTION_MENU (search_bar->scopeoption));
	}

	search_bar->scopeoption_menu = menu = gtk_menu_new ();

	/* Generate items */
	for (i = 0; scopeitems[i].id != -1; ++i) {
		if (scopeitems[i].text) {
			gchar *str;
			str = e_str_without_underscores (_(scopeitems[i].text));
			menu_item = gtk_menu_item_new_with_label (str);
			g_object_set_data_full (G_OBJECT (menu_item), "string",str, g_free);
		} else {
			menu_item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menu_item, FALSE);
		}

		g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
				   GINT_TO_POINTER (scopeitems[i].id));

		g_signal_connect (menu_item,
				  "activate",
				  G_CALLBACK (scopeitem_activated_cb),
				  search_bar);

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}
		gtk_option_menu_set_menu (GTK_OPTION_MENU (search_bar->scopeoption), menu);
}

/**
 * e_search_bar_set_scopeoption_menu:
 * @search_bar: A search bar.
 * @menu: the scope option menu
 *
 * Sets the items for the secondary option menu of a search bar.
 **/
void
e_search_bar_set_scopeoption_menu (ESearchBar *search_bar, GtkMenu *menu)
{

	if (search_bar->scopeoption_menu != NULL)
		gtk_option_menu_remove_menu (GTK_OPTION_MENU (search_bar->scopeoption));

	search_bar->scopeoption_menu = GTK_WIDGET (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (search_bar->scopeoption), search_bar->scopeoption_menu);

	g_signal_connect (search_bar->scopeoption_menu,
			  "selection-done",
			  G_CALLBACK (scopeitem_activated_cb),
			  search_bar);
}

GtkWidget *
e_search_bar_new (ESearchBarItem *menu_items,
		  ESearchBarItem *option_items)
{
	GtkWidget *widget;

	g_return_val_if_fail (option_items != NULL, NULL);

	widget = g_object_new (e_search_bar_get_type (), NULL);

	e_search_bar_construct (E_SEARCH_BAR (widget), menu_items, option_items);

	return widget;
}

GtkWidget *
e_search_bar_lite_new (ESearchBarItem *menu_items,
		  ESearchBarItem *option_items)
{
	GtkWidget *widget;

	g_return_val_if_fail (option_items != NULL, NULL);

	widget = g_object_new (e_search_bar_get_type (), NULL);
	E_SEARCH_BAR(widget)->lite = TRUE;

	e_search_bar_construct (E_SEARCH_BAR (widget), menu_items, option_items);

	return widget;
}

void
e_search_bar_set_ui_component (ESearchBar *search_bar,
			       BonoboUIComponent *ui_component)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (search_bar->lite)
		return;

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
e_search_bar_set_menu_sensitive (ESearchBar *search_bar, gint id, gboolean state)
{
	gchar *verb_name;
	gchar *path;

	if (search_bar->lite)
		return;

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

void
e_search_bar_set_viewitem_id (ESearchBar *search_bar, gint id)
{
	gint row;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	if (!search_bar->viewoption_menu)
		return;

	row = find_id (search_bar->viewoption_menu, id, "EsbItemId", NULL);
	if (row == -1)
		return;
	search_bar->viewitem_id = id;
	gtk_option_menu_set_history (GTK_OPTION_MENU (search_bar->viewoption), row);

	emit_query_changed (search_bar);
}

/**
 * e_search_bar_set_item_id:
 * @search_bar: A search bar.
 * @id: Identifier of the item to set.
 *
 * Sets the active item in the options menu of a search bar.
 **/
void
e_search_bar_set_item_id (ESearchBar *search_bar, gint id)
{
	gint row;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (!search_bar->option_menu)
		return;
	row = find_id (search_bar->option_menu, id, "EsbItemId", NULL);
	if (row == -1)
		return;

	if (id>=0)
		search_bar->last_search_option = id;
	search_bar->item_id = id;
	gtk_menu_set_active ((GtkMenu *)search_bar->option_menu, row);

	if (!search_bar->block_search)
		emit_query_changed (search_bar);

	if (!search_bar->lite)
		update_clear_menuitem_sensitive (search_bar);
}

void
e_search_bar_set_item_menu (ESearchBar *search_bar, gint id)
{
	gint row;
	GtkWidget *item;
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	row = find_id (search_bar->option_menu, id, "EsbItemId", &item);
	if (row == -1)
		return;

	gtk_menu_set_active ((GtkMenu *)search_bar->option_menu, row);
	if (id>=0)
		gtk_check_menu_item_set_active ((GtkCheckMenuItem *)item, TRUE);
}

/**
 * e_search_bar_set_search_scope:
 * @search_bar: A search bar.
 * @id: Identifier of the item to set.
 *
 * Sets the active item in the options menu of a search bar.
 **/
void
e_search_bar_set_search_scope (ESearchBar *search_bar, gint id)
{
	gint row;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (!search_bar->scopeoption_menu)
		return;
	row = find_id (search_bar->scopeoption_menu, id, "EsbItemId", NULL);
	if (row == -1)
		return;

	search_bar->scopeitem_id = id;
	gtk_option_menu_set_history (GTK_OPTION_MENU (search_bar->scopeoption), row);

	if (!search_bar->block_search)
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
gint
e_search_bar_get_item_id (ESearchBar *search_bar)
{
	GtkWidget *menu_item;
	gint item_id;

	g_return_val_if_fail (search_bar != NULL, -1);
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), -1);

	menu_item = gtk_menu_get_active (GTK_MENU (search_bar->option_menu));
	item_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "EsbItemId"));
	search_bar->item_id = item_id;

	return search_bar->item_id;
}

/**
 * e_search_bar_get_search_scope:
 * @search_bar: A search bar.
 *
 * Queries the currently selected search type in the options menu of a search bar.
 *
 * Return value: Identifier of the selected item in the options menu.
 **/
gint
e_search_bar_get_search_scope (ESearchBar *search_bar)
{
	GtkWidget *menu_item;
	gint scopeitem_id;

	g_return_val_if_fail (search_bar != NULL, -1);
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), -1);

	if (!search_bar->scopeoption_menu)
		return -7 /*Current folder hack */;

	menu_item = gtk_menu_get_active (GTK_MENU (search_bar->scopeoption_menu));
	scopeitem_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "EsbItemId"));

	search_bar->scopeitem_id = scopeitem_id;

	return search_bar->scopeitem_id;
}

/**
 * e_search_bar_get_viewitem_id:
 * @search_bar: A search bar.
 *
 * Queries the currently selected item in the viewoptions menu of a search bar.
 *
 * Return value: Identifier of the selected item in the viewoptions menu.
 * If the search bar currently contains an entry rather than a a viewoption menu,
 * a value less than zero is returned.
 **/
gint
e_search_bar_get_viewitem_id (ESearchBar *search_bar)
{
	GtkWidget *menu_item;
	gint viewitem_id;

	g_return_val_if_fail (search_bar != NULL, -1);
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), -1);

	if (!search_bar->viewoption_menu)
		return -1;

	menu_item = gtk_menu_get_active (GTK_MENU (search_bar->viewoption_menu));
	viewitem_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "EsbItemId"));

	search_bar->viewitem_id = viewitem_id;

	return search_bar->viewitem_id;
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
e_search_bar_set_ids (ESearchBar *search_bar, gint item_id, gint subitem_id)
{
	gint item_row;
	GtkWidget *item_widget;

	g_return_if_fail (search_bar != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	item_row = find_id (search_bar->option_menu, item_id, "EsbChoiceId", &item_widget);
	if (item_row == -1 || !item_widget)
		return;

	search_bar->item_id = item_id;
	gtk_option_menu_set_history (GTK_OPTION_MENU (search_bar->option), item_row);

}

void
e_search_bar_clear_search (ESearchBar *esb)
{
	g_return_if_fail (E_IS_SEARCH_BAR (esb));

	gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, NULL);
	gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
	gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, NULL);
	gtk_widget_set_sensitive (esb->clear_button, FALSE);

	clear_search (esb);
	gtk_entry_set_text (GTK_ENTRY (esb->entry), "");
}

/**
 * e_search_bar_set_text:
 * @search_bar: A search bar.
 * @text: Text to set in the search bar's entry line.
 *
 * Sets the text string inside the entry line of a search bar.
 **/
void
e_search_bar_set_text (ESearchBar *search_bar, const gchar *text)
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
gchar *
e_search_bar_get_text (ESearchBar *search_bar)
{
	GtkStyle *entry_style, *default_style;

	g_return_val_if_fail (search_bar != NULL, NULL);
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	entry_style = gtk_widget_get_style (search_bar->entry);
	default_style = gtk_widget_get_default_style ();

	if (gdk_color_equal (&(entry_style->text[GTK_STATE_NORMAL]), &(default_style->text[GTK_STATE_INSENSITIVE])))
		return g_strdup ("");

	return g_strdup (gtk_entry_get_text (GTK_ENTRY (search_bar->entry)));
}

void e_search_bar_scope_enable (ESearchBar *esb, gint did, gboolean state)
{
	GtkWidget *widget=NULL;
	GList *l;
	gint id;
	gpointer *pointer;

	g_return_if_fail (esb != NULL);
	g_return_if_fail (E_IS_SEARCH_BAR (esb));

	l = GTK_MENU_SHELL (esb->scopeoption_menu)->children;
	while (l) {
		pointer = g_object_get_data (l->data, "EsbItemId");
		if (pointer) {
			id = GPOINTER_TO_INT (pointer);
			if (id == did) {
				widget = l->data;
				break;
			}
		}
		l = l->next;
	}

	if (widget)
		gtk_widget_set_sensitive (widget, state);
}
