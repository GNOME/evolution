/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gtk-combo-text.c - A combo box for selecting from a list.
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <ctype.h>
#include <string.h>

#include <gtk/gtk.h>

#include "e-util/e-util.h"

#include "gal-combo-text.h"

#define PARENT_TYPE GAL_COMBO_BOX_TYPE
static GtkObjectClass *gal_combo_text_parent_class;

static gboolean cb_pop_down (GtkWidget *w, GtkWidget *pop_down,
			     gpointer dummy);

static void list_unselect_cb (GtkWidget *list, GtkWidget *child,
			      gpointer data);

static void update_list_selection (GalComboText *ct, const gchar *text);

static void
gal_combo_text_destroy (GtkObject *object)
{
	GalComboText *ct = GAL_COMBO_TEXT (object);

	if (ct->elements != NULL) {
		g_hash_table_destroy (ct->elements);
		ct->elements = NULL;
	}
	if (ct->list != NULL) {
		g_signal_handlers_disconnect_matched (ct,
						      G_SIGNAL_MATCH_FUNC,
						      0, 0, NULL,
						      cb_pop_down, NULL);

		g_signal_handlers_disconnect_matched (ct->list,
						      G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
						      0, 0, NULL,
						      list_unselect_cb, ct);
		ct->list = NULL;
	}

	(*gal_combo_text_parent_class->destroy) (object);
}

static void
gal_combo_text_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = &gal_combo_text_destroy;
	gal_combo_text_parent_class = g_type_class_ref (PARENT_TYPE);
}

static void
gal_combo_text_init (GalComboText *object)
{
}

E_MAKE_TYPE (gal_combo_text,
	     "GalComboText",
	     GalComboText,
	     gal_combo_text_class_init,
	     gal_combo_text_init,
	     PARENT_TYPE)

static gint
strcase_equal (gconstpointer v, gconstpointer v2)
{
	return g_ascii_strcasecmp ((const gchar*) v, (const gchar*)v2) == 0;
}

static guint
strcase_hash (gconstpointer v)
{
	const unsigned char *s = (const unsigned char *)v;
	const unsigned char *p;
	guint h = 0, g;

	for(p = s; *p != '\0'; p += 1) {
		h = ( h << 4 ) + g_ascii_tolower (*p);
		if ( ( g = h & 0xf0000000 ) ) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}

	return h;
}

/**
 * gal_combo_text_set_case_sensitive
 * @combo_text:  ComboText widget
 * @val:         make case sensitive if TRUE
 *
 * Specifies whether the text entered into the GtkEntry field and the text
 * in the list items is case sensitive. Because the values are stored in a
 * hash, it is not legal to change case sensitivity when the list contains
 * elements.
 *
 * Returns: The function returns -1 if request could not be honored. On
 * success, it returns 0.
 */
gint
gal_combo_text_set_case_sensitive (GalComboText *combo, gboolean val)
{
	if (combo->elements
	    && g_hash_table_size (combo->elements) > 0
	    && val != combo->case_sensitive)
		return -1;
	else {
		combo->case_sensitive = val;
		if (val != combo->case_sensitive) {
			GHashFunc hashfunc;
			GCompareFunc comparefunc;

			g_hash_table_destroy (combo->elements);
			if (combo->case_sensitive) {
				hashfunc = g_str_hash;
				comparefunc = g_str_equal;
			} else {
				hashfunc = strcase_hash;
				comparefunc = strcase_equal;
			}
			combo->elements = g_hash_table_new (hashfunc,
							    comparefunc);
		}
		return 0;
	}
}

static void
entry_activate_cb (GtkWidget *entry, gpointer data)
{
	GalComboText *combo = GAL_COMBO_TEXT (data);

	update_list_selection (combo,
			       gtk_entry_get_text (GTK_ENTRY (combo->entry)));
}


static void
list_select_cb (GtkWidget *list, GtkWidget *child, gpointer data)
{
	GalComboText *combo = GAL_COMBO_TEXT (data);
	GtkEntry *entry = GTK_ENTRY (combo->entry);
	gchar *value = (gchar*) g_object_get_data (G_OBJECT (child), "value");

	g_return_if_fail (entry && value);

	if (combo->cached_entry == child)
		combo->cached_entry = NULL;

	gtk_entry_set_text (entry, value);
	gtk_signal_handler_block_by_func (GTK_OBJECT (entry), 
					  GTK_SIGNAL_FUNC (entry_activate_cb),
					  (gpointer) combo);
	g_signal_emit_by_name (entry, "activate");
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (entry), 
					  GTK_SIGNAL_FUNC (entry_activate_cb),
					  (gpointer) combo);

	gal_combo_box_popup_hide (GAL_COMBO_BOX (data));
}

static void
list_unselect_cb (GtkWidget *list, GtkWidget *child, gpointer data)
{
	if (GTK_WIDGET_VISIBLE (list)) /* Undo interactive unselect */
		gtk_list_select_child (GTK_LIST (list), child);
}

static void
cb_toggle (GtkWidget *child, gpointer data)
{
	GalComboText *ct = GAL_COMBO_TEXT (data);

	gtk_list_select_child (GTK_LIST (ct->list), child);
}

void
gal_combo_text_select_item (GalComboText *ct, int elem)
{
	gtk_list_select_item (GTK_LIST(ct->list), elem);
}

static void
update_list_selection (GalComboText *ct, const gchar *text)
{
	gpointer candidate;
	GtkWidget *child;

	gtk_signal_handler_block_by_func (GTK_OBJECT (ct->list), 
					  GTK_SIGNAL_FUNC (list_select_cb),
					  (gpointer) ct);
	gtk_signal_handler_block_by_func (GTK_OBJECT (ct->list), 
					  GTK_SIGNAL_FUNC (list_unselect_cb),
					  (gpointer) ct);
	
	gtk_list_unselect_all (GTK_LIST (ct->list));
	candidate = g_hash_table_lookup (ct->elements, (gconstpointer) text);
	if (candidate && GTK_IS_WIDGET (candidate)) {
		child = GTK_WIDGET (candidate);
		gtk_list_select_child (GTK_LIST (ct->list), child);
		gtk_widget_grab_focus (child);
	}
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (ct->list), 
					    GTK_SIGNAL_FUNC (list_select_cb),
					    (gpointer) ct);
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (ct->list), 
					    GTK_SIGNAL_FUNC (list_unselect_cb),
					    (gpointer) ct);
}

void
gal_combo_text_set_text (GalComboText *ct, const gchar *text)
{
	gtk_entry_set_text (GTK_ENTRY (ct->entry), text);
	update_list_selection (ct, text);
}

/*
 * We can't just cache the old widget state on entry: If the pointer is
 * dragged, we receive two enter-notify-events, and the original cached
 * value would be overwritten with the GTK_STATE_ACTIVE we just set.
 *
 * However, we know that the gtklist only uses GTK_STATE_SELECTED and
 * GTK_STATE_NORMAL. We're OK if we only cache those two.
 */
static gboolean
cb_enter (GtkWidget *w, GdkEventCrossing *event,
	  gpointer user)
{
	GalComboText *ct = user;
	GtkStateType state = GTK_WIDGET_STATE (w);

	if (state == GTK_STATE_NORMAL || state == GTK_STATE_SELECTED) {
		ct->cached_entry = w;
		ct->cache_mouse_state = state;
	}
	if (state != GTK_STATE_SELECTED)
		gtk_widget_set_state (w, GTK_STATE_ACTIVE);

	return TRUE;
}
static gboolean
cb_exit (GtkWidget *w, GdkEventCrossing *event,
	  gpointer user)
{
	GalComboText *ct = user;

	if (ct->cached_entry == w)
		gtk_widget_set_state (w, ct->cache_mouse_state);

	return TRUE;
}

static gboolean
cb_pop_down (GtkWidget *w, GtkWidget *pop_down, gpointer dummy)
{
	GalComboText *ct = GAL_COMBO_TEXT (w);

	if (ct->cached_entry)
		gtk_widget_set_state (ct->cached_entry, ct->cache_mouse_state);
	ct->cached_entry = NULL;

	return FALSE;
}

typedef struct {
	GalComboText *ct;
	gchar *value;
} WeakRefClosure;

static void
cb_remove_from_hash (gpointer data, GObject *where_object_was)
{
	WeakRefClosure *closure = data;
	
	if (closure->ct->elements)
		g_hash_table_remove (closure->ct->elements, closure->value);

	g_free (closure->value);
	g_free (closure);
}

void
gal_combo_text_add_item (GalComboText *ct,
			 const gchar *item,
			 const gchar *value)
{
	WeakRefClosure *weak_ref_closure;
	GtkWidget *listitem;
	gchar *value_copy;

	g_return_if_fail (item);

	if (!value)
		value = item;

	value_copy = g_strdup (value);

	listitem = gtk_list_item_new_with_label (item);
	gtk_widget_show (listitem);

	g_object_set_data_full (G_OBJECT (listitem), "value",
				value_copy, g_free);
	g_signal_connect (listitem, "enter-notify-event",
			  G_CALLBACK (cb_enter),
			  (gpointer) ct);
	g_signal_connect (listitem, "leave-notify-event",
			  G_CALLBACK (cb_exit),
			  (gpointer) ct);
	g_signal_connect (listitem, "toggle",
			  G_CALLBACK (cb_toggle),
			  (gpointer) ct);

	gtk_container_add (GTK_CONTAINER (ct->list),
			   listitem);

	g_hash_table_insert (ct->elements, (gpointer)value_copy,
			     (gpointer) listitem);

	weak_ref_closure = g_new (WeakRefClosure, 1);
	weak_ref_closure->ct = ct;
	weak_ref_closure->value = g_strdup (value_copy);
	
	g_object_weak_ref (G_OBJECT (listitem),
			   cb_remove_from_hash,
			   weak_ref_closure);
}

static void
cb_list_mapped (GtkWidget *widget, gpointer user_data)
{
	GtkList *list = GTK_LIST (widget);

	if (g_list_length (list->selection) > 0)
		gtk_widget_grab_focus (GTK_WIDGET ((list->selection->data)));
}

void
gal_combo_text_construct (GalComboText *ct, gboolean const is_scrolled)
{
	GtkWidget *entry, *list, *scroll, *display_widget;

	ct->case_sensitive = FALSE;
	ct->elements = g_hash_table_new (&strcase_hash,
					 &strcase_equal);

	/* Probably irrelevant, but lets be careful */
	ct->cache_mouse_state = GTK_STATE_NORMAL;
	ct->cached_entry = NULL;

	entry = ct->entry = gtk_entry_new ();
	list = ct->list = gtk_list_new ();
	if (is_scrolled) {
		display_widget = scroll = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
						GTK_POLICY_NEVER,
						GTK_POLICY_AUTOMATIC);

		gtk_scrolled_window_add_with_viewport (
			GTK_SCROLLED_WINDOW(scroll), list);
		gtk_container_set_focus_hadjustment (
			GTK_CONTAINER (list),
			gtk_scrolled_window_get_hadjustment (
				GTK_SCROLLED_WINDOW (scroll)));
		gtk_container_set_focus_vadjustment (
			GTK_CONTAINER (list),
			gtk_scrolled_window_get_vadjustment (
				GTK_SCROLLED_WINDOW (scroll)));
		gtk_widget_set_usize (scroll, 0, 200); /* MAGIC NUMBER */
	} else
		display_widget = list;

	g_signal_connect (entry, "activate",
			  G_CALLBACK (entry_activate_cb),
			  ct);
	g_signal_connect (list, "select-child",
			  G_CALLBACK (list_select_cb),
			  ct);
	g_signal_connect (list, "unselect-child",
			  G_CALLBACK (list_unselect_cb),
			  ct);
	g_signal_connect (list, "map",
			  G_CALLBACK (cb_list_mapped), NULL);
	
	gtk_widget_show (display_widget);
	gtk_widget_show (entry);
	gal_combo_box_construct (GAL_COMBO_BOX (ct), entry, display_widget);
	g_signal_connect (ct, "pop_down_done",
			  G_CALLBACK (cb_pop_down), NULL);
}

GtkWidget*
gal_combo_text_new (gboolean const is_scrolled)
{
	GalComboText *ct;

	ct = g_object_new (GAL_COMBO_TEXT_TYPE, NULL);
	gal_combo_text_construct (ct, is_scrolled);
	return GTK_WIDGET (ct);
}

