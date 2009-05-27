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

#include "e-search-bar.h"

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <e-util/e-util.h>
#include <e-util/e-unicode.h>

#include <e-action-combo-box.h>
#include <e-gui-utils.h>
#include <e-icon-entry.h>

#define E_SEARCH_BAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SEARCH_BAR, ESearchBarPrivate))

struct _ESearchBarPrivate {
	RuleContext *context;
	FilterRule *current_query;

	GtkWidget *filter_label;
	GtkWidget *filter_combo_box;
	GtkWidget *search_label;
	GtkWidget *search_entry;
	GtkWidget *scope_label;
	GtkWidget *scope_combo_box;

	GtkRadioAction *search_action;
	GtkWidget *search_popup_menu;

	GtkActionGroup *action_group;
};

enum {
	PROP_0,
	PROP_CONTEXT,
	PROP_FILTER_ACTION,
	PROP_FILTER_VALUE,
	PROP_FILTER_VISIBLE,
	PROP_SEARCH_ACTION,
	PROP_SEARCH_TEXT,
	PROP_SEARCH_VALUE,
	PROP_SEARCH_VISIBLE,
	PROP_SCOPE_ACTION,
	PROP_SCOPE_VALUE,
	PROP_SCOPE_VISIBLE
};

static gpointer parent_class;

static void
action_search_clear_cb (GtkAction *action,
                        ESearchBar *search_bar)
{
	e_search_bar_set_search_text (search_bar, "");
	gtk_action_set_sensitive (action, FALSE);
}

static void
action_search_find_cb (GtkAction *action,
                       ESearchBar *search_bar)
{
	gtk_action_set_sensitive (action, FALSE);
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
search_bar_rule_changed (FilterRule *rule,
                         GtkDialog *dialog)
{
	gboolean sensitive;

	sensitive = (rule != NULL && rule->parts != NULL);

	if (!esb->lite) {
		set_find_now_sensitive (esb, FALSE);
		update_clear_menuitem_sensitive (esb);
	}
}

static void
search_bar_update_search_popup (ESearchBar *search_bar)
{
	GtkAction *action;
	GtkMenuShell *menu_shell;
	GSList *list, *iter;

	action = gtk_action_group_get_action (
		search_bar->priv->action_group, "search-type");

	if (search_bar->priv->search_popup_menu != NULL) {
		g_object_unref (search_bar->priv->search_popup_menu);
		search_bar->priv->search_popup_menu = NULL;
	}

	if (search_bar->priv->search_action == NULL) {
		gtk_action_set_sensitive (action, FALSE);
		return;
	}

	search_bar->priv->search_popup_menu = gtk_menu_new ();
	menu_shell = GTK_MENU_SHELL (search_bar->priv->search_popup_menu);
	list = gtk_radio_action_get_group (search_bar->priv->search_action);

	for (iter = list; iter != NULL; iter = iter->next) {
		GtkAction *action = iter->data;
		GtkWidget *widget;

		widget = gtk_action_create_menu_item (action);
		gtk_menu_shell_append (menu_shell, widget);
		gtk_widget_show (widget);
	}

	gtk_action_set_sensitive (action, TRUE);
}

static gboolean
paint_search_text (GtkWidget *widget,
                   ESearchBar *search_bar)
{
#if 0
	GtkStyle *style = gtk_widget_get_default_style ();
	const gchar *text;
	GtkWidget *menu_widget = search_bar->option_menu;

	text = gtk_entry_get_text (GTK_ENTRY (widget));
	if (text && *text)
		return FALSE;

	style = gtk_widget_get_default_style ();

	if (!GTK_WIDGET_SENSITIVE (search_bar->option_button)) {
		menu_widget = search_bar->scopeoption_menu;
		text = g_object_get_data (G_OBJECT(gtk_menu_get_active ( GTK_MENU (search_bar->scopeoption_menu))),"string");
	} else if (!GTK_IS_RADIO_MENU_ITEM (gtk_menu_get_active ( GTK_MENU (search_bar->option_menu))))
		return FALSE;
	else /* no query in search entry .. so set the current option */
		text = get_selected_item_label (menu_widget);


	if (text && *text) {
		gchar *t;

		if (!GTK_WIDGET_HAS_FOCUS(search_bar->entry)) {
			gtk_entry_set_text (GTK_ENTRY (search_bar->entry), text);
			gtk_widget_modify_text (search_bar->entry, GTK_STATE_NORMAL, &(style->text[GTK_STATE_INSENSITIVE]));
		}

		t = g_strdup_printf ("%s: %s\n%s", _("Search"), text, _("Click here to change the search type"));
		gtk_widget_set_tooltip_text (search_bar->option_button, t);
		g_free (t);

		gtk_widget_set_sensitive (search_bar->clear_button, FALSE);
	}

	return FALSE;
#endif

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
		if (!esb->lite)
			gtk_widget_modify_base (esb->viewoption, GTK_STATE_NORMAL, &(style->base[GTK_STATE_SELECTED]));
	} else {
		gtk_widget_modify_base (esb->entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_text (esb->entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_base (esb->icon_entry, GTK_STATE_NORMAL, NULL);
		if (!esb->lite)
			gtk_widget_set_sensitive (esb->clear_button, FALSE);
	}

	icon_entry = E_ICON_ENTRY (search_bar->priv->search_entry);
	entry = e_icon_entry_get_entry (icon_entry);
	paint_search_text (entry, search_bar);
}

static void
search_bar_entry_activated_cb (ESearchBar *search_bar,
                               GtkWidget *entry)
{
	GtkStyle *style;
	GtkAction *action;
	GtkActionGroup *action_group;
	gboolean sensitive;
	const gchar *text;

	style = gtk_widget_get_default_style ();
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	action_group = e_search_bar_get_action_group (search_bar);

	if (text && *text) {
		gtk_widget_modify_base (
			entry, GTK_STATE_NORMAL,
			&(style->base[GTK_STATE_SELECTED]));
		gtk_widget_modify_text (
			entry, GTK_STATE_NORMAL,
			&(style->text[GTK_STATE_SELECTED]));
		gtk_widget_modify_base (
			search_bar->priv->search_entry,
			GTK_STATE_NORMAL,
			&(style->base[GTK_STATE_SELECTED]));
		gtk_widget_modify_base (
			search_bar->priv->filter_combo_box,
			GTK_STATE_NORMAL,
			&(style->base[GTK_STATE_SELECTED]));
		sensitive = TRUE;
	} else {
		gtk_widget_modify_base (
			entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_text (
			entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_base (
			search_bar->priv->search_entry,
			GTK_STATE_NORMAL, NULL);
		sensitive = FALSE;
	}

	action = gtk_action_group_get_action (
		action_group, E_SEARCH_BAR_ACTION_CLEAR);
	gtk_action_set_sensitive (action, sensitive);

	action = gtk_action_group_get_action (
		action_group, E_SEARCH_BAR_ACTION_FIND);
	gtk_action_activate (action);
}

static void
search_bar_entry_changed_cb (ESearchBar *search_bar,
                             GtkWidget *entry)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkStyle *style1;
	GtkStyle *style2;
	GdkColor *color1;
	GdkColor *color2;
	gboolean sensitive;
	const gchar *text;

	style1 = gtk_widget_get_style (entry);
	style2 = gtk_widget_get_default_style ();

	text = gtk_entry_get_text (GTK_ENTRY (entry));

	if (text != NULL && *text != '\0') {
		color1 = &style1->text[GTK_STATE_NORMAL];
		color2 = &style2->text[GTK_STATE_INSENSITIVE];
		sensitive = !gdk_color_equal (color1, color2);
	} else {
		color1 = &style1->text[GTK_STATE_NORMAL];
		color2 = &style2->text[GTK_STATE_SELECTED];
		sensitive = gdk_color_equal (color1, color2);
	}

	action_group = search_bar->priv->action_group;
	action = gtk_action_group_get_action (
		action_group, E_SEARCH_BAR_ACTION_CLEAR);
	gtk_action_set_sensitive (action, sensitive);
}

static gboolean
search_bar_entry_focus_in_cb (ESearchBar *search_bar,
                              GdkEventFocus *event,
                              GtkWidget *entry)
{
	GtkStyle *style1;
	GtkStyle *style2;
	GdkColor *color1;
	GdkColor *color2;

	style1 = gtk_widget_get_style (entry);
	style2 = gtk_widget_get_default_style ();

	color1 = &style1->text[GTK_STATE_NORMAL];
	color2 = &style2->text[GTK_STATE_INSENSITIVE];

	if (gdk_color_equal (color1, color2)) {
		gtk_entry_set_text (GTK_ENTRY (entry), "");
		gtk_widget_modify_text (entry, GTK_STATE_NORMAL, NULL);
	}

	return FALSE;
}

static gboolean
search_bar_entry_focus_out_cb (ESearchBar *search_bar,
                               GdkEventFocus *event,
                               GtkWidget *entry)
{
	return paint_search_text (entry, search_bar);
}

static gboolean
search_bar_entry_key_press_cb (ESearchBar *search_bar,
                               GdkEventKey *key_event,
                               GtkWidget *entry)
{
	guint state;

#if 0  /* FIXME */
	state = key_event->state & gtk_accelerator_get_default_mod_mask ();
	if (state == GDK_MOD1_MASK && key_event->keyval == GDK_Down) {
		search_bar_option_clicked_cb (search_bar, NULL, NULL);
		return TRUE;
	}
#endif

	return FALSE;
}

static void
search_bar_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONTEXT:
			e_search_bar_set_context (
				E_SEARCH_BAR (object),
				g_value_get_object (value));
			return;

		case PROP_FILTER_ACTION:
			e_search_bar_set_filter_action (
				E_SEARCH_BAR (object),
				g_value_get_object (value));
			return;

		case PROP_FILTER_VALUE:
			e_search_bar_set_filter_value (
				E_SEARCH_BAR (object),
				g_value_get_int (value));
			return;

		case PROP_FILTER_VISIBLE:
			e_search_bar_set_filter_visible (
				E_SEARCH_BAR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SEARCH_ACTION:
			e_search_bar_set_search_action (
				E_SEARCH_BAR (object),
				g_value_get_object (value));
			return;

		case PROP_SEARCH_TEXT:
			e_search_bar_set_search_text (
				E_SEARCH_BAR (object),
				g_value_get_string (value));
			return;

		case PROP_SEARCH_VALUE:
			e_search_bar_set_search_value (
				E_SEARCH_BAR (object),
				g_value_get_int (value));
			return;

		case PROP_SEARCH_VISIBLE:
			e_search_bar_set_search_visible (
				E_SEARCH_BAR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SCOPE_ACTION:
			e_search_bar_set_scope_action (
				E_SEARCH_BAR (object),
				g_value_get_object (value));
			return;

		case PROP_SCOPE_VALUE:
			e_search_bar_set_scope_value (
				E_SEARCH_BAR (object),
				g_value_get_int (value));
			return;

		case PROP_SCOPE_VISIBLE:
			e_search_bar_set_scope_visible (
				E_SEARCH_BAR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
search_bar_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONTEXT:
			g_value_set_object (
				value, e_search_bar_get_context (
				E_SEARCH_BAR (object)));
			return;

		case PROP_FILTER_ACTION:
			g_value_set_object (
				value, e_search_bar_get_filter_action (
				E_SEARCH_BAR (object)));
			return;

		case PROP_FILTER_VALUE:
			g_value_set_int (
				value, e_search_bar_get_filter_value (
				E_SEARCH_BAR (object)));
			return;

		case PROP_FILTER_VISIBLE:
			g_value_set_boolean (
				value, e_search_bar_get_filter_visible (
				E_SEARCH_BAR (object)));
			return;

		case PROP_SEARCH_ACTION:
			g_value_set_object (
				value, e_search_bar_get_search_action (
				E_SEARCH_BAR (object)));
			return;

		case PROP_SEARCH_TEXT:
			g_value_set_string (
				value, e_search_bar_get_search_text (
				E_SEARCH_BAR (object)));
			return;

		case PROP_SEARCH_VALUE:
			g_value_set_int (
				value, e_search_bar_get_search_value (
				E_SEARCH_BAR (object)));
			return;

		case PROP_SEARCH_VISIBLE:
			g_value_set_boolean (
				value, e_search_bar_get_search_visible (
				E_SEARCH_BAR (object)));
			return;

		case PROP_SCOPE_ACTION:
			g_value_set_object (
				value, e_search_bar_get_scope_action (
				E_SEARCH_BAR (object)));
			return;

		case PROP_SCOPE_VALUE:
			g_value_set_int (
				value, e_search_bar_get_scope_value (
				E_SEARCH_BAR (object)));
			return;

		case PROP_SCOPE_VISIBLE:
			g_value_set_boolean (
				value, e_search_bar_get_scope_visible (
				E_SEARCH_BAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
search_bar_dispose (GObject *object)
{
	ESearchBarPrivate *priv;

	priv = E_SEARCH_BAR_GET_PRIVATE (object);

	if (priv->context != NULL) {
		g_object_unref (priv->context);
		priv->context = NULL;
	}

	if (priv->filter_label != NULL) {
		g_object_unref (priv->filter_label);
		priv->filter_label = NULL;
	}

	if (priv->filter_combo_box != NULL) {
		g_object_unref (priv->filter_combo_box);
		priv->filter_combo_box = NULL;
	}

	if (!esb->lite && esb->ui_component != NULL)
		update_bonobo_menus (esb);
}

/* /\* Callback used when an option item is destroyed.  We have to destroy its */
/*  * suboption items. */
/*  *\/ */
/* static gpointer /
/* option_item_destroy_cb (GtkObject *object, gpointer data) */
/* { */
/* /\* 	ESearchBarSubitem *subitems; *\/ */

/* /\* 	subitems = data; *\/ */

/* /\* 	g_assert (subitems != NULL); *\/ */
/* /\* 	free_subitems (subitems); *\/ */
/* /\* 	g_object_set_data (G_OBJECT (object), "EsbChoiceSubitems", NULL); *\/ */
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

	if (priv->search_entry != NULL) {
		g_object_unref (priv->search_entry);
		priv->search_entry = NULL;
	}

	if (priv->scope_label != NULL) {
		g_object_unref (priv->scope_label);
		priv->scope_label = NULL;
	}

	if (priv->scope_combo_box != NULL) {
		g_object_unref (priv->scope_combo_box);
		priv->scope_combo_box = NULL;
	}

	if (priv->search_action != NULL) {
		g_object_unref (priv->search_action);
		priv->search_action = NULL;
	}

	if (priv->action_group != NULL) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
search_bar_class_init (ESearchBarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ESearchBarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = search_bar_set_property;
	object_class->get_property = search_bar_get_property;
	object_class->dispose = search_bar_dispose;

	g_object_class_install_property (
		object_class,
		PROP_CONTEXT,
		g_param_spec_object (
			"context",
			NULL,
			NULL,
			RULE_TYPE_CONTEXT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FILTER_ACTION,
		g_param_spec_object (
			"filter-action",
			NULL,
			NULL,
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FILTER_VALUE,
		g_param_spec_int (
			"filter-value",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FILTER_VISIBLE,
		g_param_spec_boolean (
			"filter-visible",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_ACTION,
		g_param_spec_object (
			"search-action",
			NULL,
			NULL,
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_TEXT,
		g_param_spec_string (
			"search-text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_VALUE,
		g_param_spec_int (
			"search-value",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_VISIBLE,
		g_param_spec_boolean (
			"search-visible",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SCOPE_ACTION,
		g_param_spec_object (
			"scope-action",
			NULL,
			NULL,
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SCOPE_VALUE,
		g_param_spec_int (
			"scope-value",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SCOPE_VISIBLE,
		g_param_spec_boolean (
			"scope-visible",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
search_bar_init (ESearchBar *search_bar)
{
	EIconEntry *icon_entry;
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkLabel *label;
	GtkWidget *mnemonic;
	GtkWidget *widget;

	search_bar->priv = E_SEARCH_BAR_GET_PRIVATE (search_bar);

	gtk_box_set_spacing (GTK_BOX (search_bar), 3);
	gtk_box_set_homogeneous (GTK_BOX (search_bar), FALSE);

	/*** Filter Widgets ***/

	/* Translators: The "Show: " label is followed by the Quick Search
	 * Dropdown Menu where you can choose to display "All Messages",
	 * "Unread Messages", "Message with 'Important' Label" and so on... */
	widget = gtk_label_new_with_mnemonic (_("Sho_w: "));
	gtk_box_pack_start (GTK_BOX (search_bar), widget, FALSE, FALSE, 0);
	search_bar->priv->filter_label = g_object_ref (widget);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_action_combo_box_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (GTK_BOX (search_bar), widget, FALSE, TRUE, 0);
	search_bar->priv->filter_combo_box = g_object_ref (widget);
	gtk_widget_show (widget);

	/*** Scope Widgets ***/

	widget = e_action_combo_box_new ();
	gtk_box_pack_end (GTK_BOX (search_bar), widget, FALSE, FALSE, 0);
	search_bar->priv->scope_combo_box = g_object_ref (widget);
	gtk_widget_show (widget);

	mnemonic = widget;

	/* Translators: The " in " label is part of the Quick Search Bar,
	 * example: Search: [_________________] in [ Current Folder ] */
	widget = gtk_label_new_with_mnemonic (_(" i_n "));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), mnemonic);
	gtk_box_pack_end (GTK_BOX (search_bar), widget, FALSE, FALSE, 0);
	search_bar->priv->scope_label = g_object_ref (widget);
	gtk_widget_show (widget);

	/*** Search Widgets ***/

	widget = e_icon_entry_new ();
	gtk_box_pack_end (GTK_BOX (search_bar), widget, FALSE, FALSE, 0);
	search_bar->priv->search_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	icon_entry = E_ICON_ENTRY (widget);

	/* Translators: The "Search: " label is followed by the Quick Search
	 * Text input field where one enters the term to search for. */
	widget = gtk_label_new_with_mnemonic (_("Sear_ch: "));
	gtk_box_pack_end (GTK_BOX (search_bar), widget, FALSE, FALSE, 0);
	search_bar->priv->search_label = g_object_ref (widget);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_icon_entry_get_entry (icon_entry);
	gtk_label_set_mnemonic_widget (label, widget);
	g_signal_connect_swapped (
		widget, "activate",
		G_CALLBACK (search_bar_entry_activated_cb), search_bar);
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (search_bar_entry_changed_cb), search_bar);
	g_signal_connect_swapped (
		widget, "focus-in-event",
		G_CALLBACK (search_bar_entry_focus_in_cb), search_bar);
	g_signal_connect_swapped (
		widget, "focus-out-event",
		G_CALLBACK (search_bar_entry_focus_out_cb), search_bar);
	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (search_bar_entry_key_press_cb), search_bar);

	action_group = gtk_action_group_new ("search");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, search_entries,
		G_N_ELEMENTS (search_entries), search_bar);
	search_bar->priv->action_group = action_group;

	action = gtk_action_group_get_action (
		action_group, E_SEARCH_BAR_ACTION_TYPE);
	e_icon_entry_add_action_start (icon_entry, action);
	gtk_action_set_sensitive (action, FALSE);

	action = gtk_action_group_get_action (
		action_group, E_SEARCH_BAR_ACTION_CLEAR);
	e_icon_entry_add_action_end (icon_entry, action);
	gtk_action_set_sensitive (action, FALSE);
}

GType
e_search_bar_get_type (void)
{
	static GType type = 0;

	if (!esb->lite && esb->ui_component != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (esb->ui_component));
		esb->ui_component = NULL;
	}
/* 	if (esb->entry) { */
/* 		g_object_unref (esb->entry); */
/* 		esb->entry = NULL; */
/* 	} */
	if (esb->suboption) {
		g_object_unref (esb->suboption);
		esb->suboption = NULL;
	}

		type = g_type_register_static (
			GTK_TYPE_HBOX, "ESearchBar", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_search_bar_new (void)
{
	return g_object_new (E_TYPE_SEARCH_BAR, NULL);
}

GtkActionGroup *
e_search_bar_get_action_group (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	return search_bar->priv->action_group;
}

RuleContext *
e_search_bar_get_context (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	return search_bar->priv->context;
}

void
e_search_bar_set_context (ESearchBar *search_bar,
                          RuleContext *context)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	g_return_if_fail (IS_RULE_CONTEXT (context));

	if (search_bar->priv->context != NULL)
		g_object_unref (search_bar->priv->context);

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
	/* 	g_signal_connect (GTK_OPTION_MENU (search_bar->scopeoption), "changed", scopeoption_changed_cb, search_bar); */
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

GtkRadioAction *
e_search_bar_get_filter_action (ESearchBar *search_bar)
{
	EActionComboBox *combo_box;

	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	combo_box = E_ACTION_COMBO_BOX (search_bar->priv->filter_combo_box);

	return e_action_combo_box_get_action (combo_box);
}

void
e_search_bar_set_filter_action (ESearchBar *search_bar,
                                GtkRadioAction *action)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	combo_box = E_ACTION_COMBO_BOX (search_bar->priv->filter_combo_box);

	e_action_combo_box_set_action (combo_box, action);
	g_object_notify (G_OBJECT (search_bar), "filter-action");
}

gint
e_search_bar_get_filter_value (ESearchBar *search_bar)
{
	EActionComboBox *combo_box;

	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), 0);

	combo_box = E_ACTION_COMBO_BOX (search_bar->priv->filter_combo_box);

	return e_action_combo_box_get_current_value (combo_box);
}

void
e_search_bar_set_filter_value (ESearchBar *search_bar,
                               gint value)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	combo_box = E_ACTION_COMBO_BOX (search_bar->priv->filter_combo_box);

	e_action_combo_box_set_current_value (combo_box, value);
	g_object_notify (G_OBJECT (search_bar), "filter-value");
}

gboolean
e_search_bar_get_filter_visible (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), FALSE);

	return GTK_WIDGET_VISIBLE (search_bar->priv->filter_combo_box);
}

void
e_search_bar_set_filter_visible (ESearchBar *search_bar,
                                 gboolean visible)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (visible) {
		gtk_widget_show (search_bar->priv->filter_label);
		gtk_widget_show (search_bar->priv->filter_combo_box);
	} else {
		gtk_widget_hide (search_bar->priv->filter_label);
		gtk_widget_hide (search_bar->priv->filter_combo_box);
	}

	g_object_notify (G_OBJECT (search_bar), "filter-visible");
}

GtkRadioAction *
e_search_bar_get_search_action (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	return search_bar->priv->search_action;
}

void
e_search_bar_set_search_action (ESearchBar *search_bar,
                                GtkRadioAction *action)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (action != NULL) {
		g_return_if_fail (GTK_IS_RADIO_ACTION (action));
		g_object_ref (action);
	}

	search_bar->priv->search_action = action;
	search_bar_update_search_popup (search_bar);

	g_object_notify (G_OBJECT (search_bar), "search-action");
}

const gchar *
e_search_bar_get_search_text (ESearchBar *search_bar)
{
	EIconEntry *icon_entry;
	GtkWidget *entry;
	GtkStyle *style1;
	GtkStyle *style2;
	GdkColor *color1;
	GdkColor *color2;

	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	icon_entry = E_ICON_ENTRY (search_bar->priv->search_entry);
	entry = e_icon_entry_get_entry (icon_entry);

	style1 = gtk_widget_get_style (entry);
	style2 = gtk_widget_get_default_style ();

	color1 = &style1->text[GTK_STATE_NORMAL];
	color2 = &style2->text[GTK_STATE_INSENSITIVE];

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
e_search_bar_set_search_text (ESearchBar *search_bar,
                              const gchar *text)
{
	EIconEntry *icon_entry;
	GtkWidget *entry;

	if (search_bar->lite)
		return;

	if (search_bar->lite)
		return;

	verb_name = verb_name_from_id (id);
	path = g_strconcat ("/commands/", verb_name, NULL);
	g_free (verb_name);

	icon_entry = E_ICON_ENTRY (search_bar->priv->search_entry);
	entry = e_icon_entry_get_entry (icon_entry);

	text = (text != NULL) ? text : "";
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	g_object_notify (G_OBJECT (search_bar), "search-text");
}

gint
e_search_bar_get_search_value (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), 0);

	/* FIXME */
	return 0;
}

void
e_search_bar_set_search_value (ESearchBar *search_bar,
                               gint value)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	if (!search_bar->viewoption_menu)
		return;

	/* FIXME */

	g_object_notify (G_OBJECT (search_bar), "search-value");
}

gboolean
e_search_bar_get_search_visible (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), FALSE);

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
e_search_bar_set_search_visible (ESearchBar *search_bar,
                                 gboolean visible)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (visible) {
		gtk_widget_show (search_bar->priv->search_label);
		gtk_widget_show (search_bar->priv->search_entry);
	} else {
		gtk_widget_hide (search_bar->priv->search_label);
		gtk_widget_hide (search_bar->priv->search_entry);
	}

	g_object_notify (G_OBJECT (search_bar), "search-visible");
}

GtkRadioAction *
e_search_bar_get_scope_action (ESearchBar *search_bar)
{
	EActionComboBox *combo_box;

	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	combo_box = E_ACTION_COMBO_BOX (search_bar->priv->scope_combo_box);

	return e_action_combo_box_get_action (combo_box);
}

void
e_search_bar_set_scope_action (ESearchBar *search_bar,
                               GtkRadioAction *action)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	g_return_if_fail (GTK_IS_RADIO_ACTION (action));

	combo_box = E_ACTION_COMBO_BOX (search_bar->priv->scope_combo_box);

	e_action_combo_box_set_action (combo_box, action);
	g_object_notify (G_OBJECT (search_bar), "scope-action");
}

gint
e_search_bar_get_scope_value (ESearchBar *search_bar)
{
	EActionComboBox *combo_box;

	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), 0);

	combo_box = E_ACTION_COMBO_BOX (search_bar->priv->scope_combo_box);

	return e_action_combo_box_get_current_value (combo_box);
}

void
e_search_bar_set_scope_value (ESearchBar *search_bar,
                              gint value)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	combo_box = E_ACTION_COMBO_BOX (search_bar->priv->scope_combo_box);

	e_action_combo_box_set_current_value (combo_box, value);
	g_object_notify (G_OBJECT (search_bar), "scope-value");
}

gboolean
e_search_bar_get_scope_visible (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), FALSE);

	return GTK_WIDGET_VISIBLE (search_bar->priv->scope_combo_box);
}

void
e_search_bar_set_scope_visible (ESearchBar *search_bar,
                                gboolean visible)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (visible) {
		gtk_widget_show (search_bar->priv->scope_label);
		gtk_widget_show (search_bar->priv->scope_combo_box);
	} else {
		gtk_widget_hide (search_bar->priv->scope_label);
		gtk_widget_hide (search_bar->priv->scope_combo_box);
	}

	g_object_notify (G_OBJECT (search_bar), "scope-visible");
}

static void
search_bar_rule_changed_cb (FilterRule *rule,
                            GtkDialog *dialog)
{
	/* FIXME Think this does something with sensitivity. */
}

void
e_search_bar_save_search_dialog (ESearchBar *search_bar,
                                 const gchar *filename)
{
	RuleContext *context;
	FilterRule *rule;
	GtkWidget *dialog;
	GtkWidget *parent;
	GtkWidget *widget;
	const gchar *search_text;
	gchar *rule_name;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));
	g_return_if_fail (filename != NULL);

	g_return_if_fail (search_bar->priv->current_query != NULL);

	rule = filter_rule_clone (search_bar->priv->current_query);
	search_text = e_search_bar_get_search_text (search_bar);
	if (search_text == NULL || *search_text == '\0')
		search_text = "''";

	rule_name = g_strdup_printf ("%s %s", rule->name, search_text);
	filter_rule_set_name (rule, rule_name);
	g_free (rule_name);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (search_bar));

	dialog = gtk_dialog_new_with_buttons (
		_("Save Search"), GTK_WINDOW (parent),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 300);
	gtk_container_set_border_width (
		GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 0);
	gtk_container_set_border_width (
		GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 12);

	context = search_bar->priv->context;
	widget = filter_rule_get_widget (rule, context);
	filter_rule_set_source (rule, FILTER_SOURCE_INCOMING);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	gtk_box_pack_start (
		GTK_BOX (GTK_DIALOG (dialog)->vbox),
		widget, TRUE, TRUE, 0);

	g_signal_connect (
		rule, "changed",
		G_CALLBACK (search_bar_rule_changed_cb),
		dialog);

	search_bar_rule_changed_cb (rule, GTK_DIALOG (dialog));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		if (filter_rule_validate (rule)) {
			rule_context_add_rule (context, rule);
			rule_context_save (context, filename);
		}
	}

	gtk_widget_destroy (dialog);
}
