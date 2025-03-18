/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include "e-contact-card.h"
#include "e-contact-card-box.h"

#define MIN_CARD_WIDTH 321
#define CARD_PACKING_MARGIN 2

/* Max gap between two consecutive ranges to merge them into
   a single call when getting selected contacts */
#define MAX_CONTACT_READ_OVERHEAD 5

/* Do not want to track all the selected items, due to memory
   requirments (imagine 1.000.000 contains in the list), but want
   to cover most of the cases, like when just moving between
   the contacts with a single item selected, in which case
   it's not needed to traverse all the 1.000.000 ItemState-s,
   but only those affected when unselecting all. */
#define TRACK_N_SELECTED 5

static GtkTargetEntry dnd_types[] = {
	{ (gchar *) "text/x-source-vcard", 0, E_CONTACT_CARD_BOX_DND_TYPE_SOURCE_VCARD_LIST },
	{ (gchar *) "text/x-vcard", 0, E_CONTACT_CARD_BOX_DND_TYPE_VCARD_LIST }
};

#define E_TYPE_CONTACT_CARD_CONTAINER e_contact_card_container_get_type ()
G_DECLARE_FINAL_TYPE (EContactCardContainer, e_contact_card_container, E, CONTACT_CARD_CONTAINER, GtkLayout)

struct _EContactCardContainer {
	GtkLayout parent;

	EContactCardBoxGetItemsFunc get_items_func;
	EContactCardBoxGetItemsFinishFunc get_items_finish_func;
	gpointer get_items_source_data;
	GDestroyNotify get_items_source_data_destroy;

	GtkCssProvider *css_provider;
	GtkAllocation viewport; /* visible part of the container */
	gint scrollbar_size;
	gint card_width;
	gint card_height;

	GPtrArray *cards; /* EContactCard * */
	GArray *items; /* ItemState */
	guint items_range_start;
	guint items_range_length;
	gint n_cols;

	gpointer ongoing_range_read; /* GetItemsData * */
	GSList *range_read_queue; /* GetItemsData * */

	guint stamp;
	guint focused_index;

	guint tracked_selected[TRACK_N_SELECTED]; /* cyclic buffer, contains indexes to items, G_MAXUINT for unused */
	guint tracked_selected_index; /* index into tracked_selected[], last used item */
	guint n_known_selected; /* when larger than TRACK_N_SELECTED, then tracked_selected[] is useless */
};

G_DEFINE_TYPE (EContactCardContainer, e_contact_card_container, GTK_TYPE_LAYOUT)

enum {
	CONTAINER_SELECTED_CHANGED,
	CONTAINER_CARD_EVENT,
	CONTAINER_CARD_POPUP_MENU,
	CONTAINER_CARD_DRAG_BEGIN,
	CONTAINER_CARD_DRAG_DATA_GET,
	CONTAINER_CARD_DRAG_END,
	CONTAINER_LAST_SIGNAL
};

static guint container_signals[CONTAINER_LAST_SIGNAL];

typedef struct _ItemState {
	EContact *item;
	gboolean selected;
} ItemState;

static void
item_state_clear (gpointer ptr)
{
	ItemState *self = ptr;

	if (self)
		g_clear_object (&self->item);
}

static gboolean
e_contact_card_container_card_event_cb (GtkWidget *widget,
					GdkEvent *event,
					gpointer user_data)
{
	EContactCardContainer *self = E_CONTACT_CARD_CONTAINER (user_data);
	gboolean res = FALSE;

	g_signal_emit (self, container_signals[CONTAINER_CARD_EVENT], 0, widget, event, &res);

	return res;
}

static gboolean
e_contact_card_container_card_popup_menu_cb (GtkWidget *widget,
					     gpointer user_data)
{
	EContactCardContainer *self = E_CONTACT_CARD_CONTAINER (user_data);
	gboolean res = FALSE;

	g_signal_emit (self, container_signals[CONTAINER_CARD_POPUP_MENU], 0, widget, &res);

	return res;
}

static void
e_contact_card_container_card_drag_begin_cb (GtkWidget *widget,
					     GdkDragContext *context,
					     gpointer user_data)
{
	EContactCardContainer *self = E_CONTACT_CARD_CONTAINER (user_data);

	g_signal_emit (self, container_signals[CONTAINER_CARD_DRAG_BEGIN], 0, context);
}

static void
e_contact_card_container_card_drag_data_get_cb (GtkWidget *widget,
						GdkDragContext *context,
						GtkSelectionData *selection_data,
						guint info,
						guint time,
						gpointer user_data)
{
	EContactCardContainer *self = E_CONTACT_CARD_CONTAINER (user_data);

	g_signal_emit (self, container_signals[CONTAINER_CARD_DRAG_DATA_GET], 0, context, selection_data, info, time);
}

static void
e_contact_card_container_card_drag_end_cb (GtkWidget *widget,
					   GdkDragContext *context,
					   gpointer user_data)
{
	EContactCardContainer *self = E_CONTACT_CARD_CONTAINER (user_data);

	g_signal_emit (self, container_signals[CONTAINER_CARD_DRAG_END], 0, context);
}

static GtkWidget *
e_contact_card_container_get_card (EContactCardContainer *self,
				   guint item_index)
{
	GtkWidget *card = NULL;

	if (item_index >= self->items_range_start &&
	    item_index < self->items_range_start + self->items_range_length &&
	    item_index - self->items_range_start < self->cards->len)
		card = g_ptr_array_index (self->cards, item_index - self->items_range_start);

	if (card && !gtk_widget_get_visible (card))
		card = NULL;

	return card;
}

static gboolean
e_contact_card_container_item_grab_focus (EContactCardContainer *self,
					  guint item_index,
					  GtkWidget **out_card)
{
	GtkWidget *card;
	gboolean had_focus = FALSE;

	card = e_contact_card_container_get_card (self, item_index);
	if (card) {
		had_focus = gtk_widget_has_focus (card);
		if (!had_focus)
			gtk_widget_grab_focus (card);
	}

	if (out_card)
		*out_card = card;

	return had_focus;
}

typedef void (* EGetItemsCallback) (EContactCardContainer *self,
				    guint range_start,
				    guint range_length,
				    GPtrArray *contacts, /* EContact * */
				    gpointer user_data,
				    const GError *error);

typedef struct _GetItemsData {
	GWeakRef self_weakref;
	guint stamp;
	guint range_start;
	guint range_length;
	GCancellable *cancellable;
	EGetItemsCallback cb;
	gpointer cb_user_data;
} GetItemsData;

static GetItemsData *
get_items_data_new (EContactCardContainer *self,
		    guint range_start,
		    guint range_length,
		    GCancellable *cancellable,
		    EGetItemsCallback cb,
		    gpointer cb_user_data)
{
	GetItemsData *gid;

	gid = g_new0 (GetItemsData, 1);
	g_weak_ref_init (&gid->self_weakref, self);
	gid->stamp = self->stamp;
	gid->range_start = range_start;
	gid->range_length = range_length;
	gid->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	gid->cb = cb;
	gid->cb_user_data = cb_user_data;

	return gid;
}

static void
get_items_data_free (GetItemsData *gid)
{
	if (!gid)
		return;

	g_weak_ref_clear (&gid->self_weakref);
	g_clear_object (&gid->cancellable);
	g_free (gid);
}

static GPtrArray * /* EContact *; (transfer container) */
e_contact_card_container_get_range_from_cache (EContactCardContainer *self,
					       guint range_start,
					       guint range_length)
{
	GPtrArray *contacts;
	guint ii;

	for (ii = 0; ii < range_length; ii++) {
		ItemState *item_state = &g_array_index (self->items, ItemState, range_start + ii);

		if (!item_state->item)
			break;
	}

	if (ii != range_length)
		return NULL;

	contacts = g_ptr_array_new_full (range_length, g_object_unref);

	for (ii = 0; ii < range_length; ii++) {
		ItemState *item_state = &g_array_index (self->items, ItemState, range_start + ii);

		g_ptr_array_add (contacts, g_object_ref (item_state->item));
	}

	return contacts;
}

static void
e_contact_card_container_got_items_cb (GObject *source_object,
				       GAsyncResult *result,
				       gpointer user_data);

static void
e_contact_card_container_read_next_range (EContactCardContainer *self)
{
	if (self->ongoing_range_read)
		return;

	while (self->range_read_queue) {
		GetItemsData *gid;
		GPtrArray *contacts;

		gid = self->range_read_queue->data;
		self->range_read_queue = g_slist_remove (self->range_read_queue, gid);

		if (gid->stamp != self->stamp) {
			GError local_error;

			local_error.domain = G_IO_ERROR;
			local_error.code = G_IO_ERROR_CANCELLED;
			local_error.message = (gchar *) "Operation cancelled due to internal data invalidated";

			gid->cb (self, gid->range_start, gid->range_length, NULL, gid->cb_user_data, &local_error);

			get_items_data_free (gid);

			continue;
		}

		contacts = e_contact_card_container_get_range_from_cache (self, gid->range_start, gid->range_length);
		if (contacts) {
			gid->cb (self, gid->range_start, gid->range_length, contacts, gid->cb_user_data, NULL);

			g_ptr_array_unref (contacts);
			get_items_data_free (gid);

			continue;
		}

		self->ongoing_range_read = gid;
		self->get_items_func (self->get_items_source_data, gid->range_start, gid->range_length,
			gid->cancellable, e_contact_card_container_got_items_cb, gid);

		break;
	}
}

static void
e_contact_card_container_cleanup_get_items_queue (EContactCardContainer *self)
{
	GSList *link;

	for (link = self->range_read_queue; link; link = g_slist_next (link)) {
		GetItemsData *gid = link->data;
		GError local_error;

		local_error.domain = G_IO_ERROR;
		local_error.code = G_IO_ERROR_CANCELLED;
		local_error.message = (gchar *) "Operation cancelled due to internal data invalidated";

		gid->cb (self, gid->range_start, gid->range_length, NULL, gid->cb_user_data, &local_error);

		get_items_data_free (gid);
	}

	g_slist_free (self->range_read_queue);
	self->range_read_queue = NULL;
}

static void
e_contact_card_container_schedule_range_read (EContactCardContainer *self,
					      guint range_start,
					      guint range_length,
					      GCancellable *cancellable,
					      EGetItemsCallback cb,
					      gpointer cb_user_data)
{
	GetItemsData *gid;
	GPtrArray *contacts;

	contacts = e_contact_card_container_get_range_from_cache (self, range_start, range_length);
	if (contacts) {
		cb (self, range_start, range_length, contacts, cb_user_data, NULL);
		g_ptr_array_unref (contacts);
		return;
	}

	gid = get_items_data_new (self, range_start, range_length, cancellable, cb, cb_user_data);

	self->range_read_queue = g_slist_append (self->range_read_queue, gid);

	e_contact_card_container_read_next_range (self);
}

static void
e_contact_card_container_got_items_cb (GObject *source_object,
				       GAsyncResult *result,
				       gpointer user_data)
{
	GetItemsData *gid = user_data;
	EContactCardContainer *self;
	GError *local_error = NULL;
	GPtrArray *items;
	gboolean selected_or_focused_changed = FALSE;

	self = g_weak_ref_get (&gid->self_weakref);
	if (!self) {
		get_items_data_free (gid);
		return;
	}

	items = self->get_items_finish_func (self->get_items_source_data, result, &local_error);

	if (items && gid->stamp == self->stamp) {
		guint ii;

		for (ii = 0; ii < items->len && ii < gid->range_length; ii++) {
			guint item_index = gid->range_start + ii;
			EContact *item = g_ptr_array_index (items, ii);
			ItemState *state = &g_array_index (self->items, ItemState, item_index);

			if (!state->item) {
				GtkWidget *card;

				state->item = g_object_ref (item);

				selected_or_focused_changed = selected_or_focused_changed || state->selected || item_index == self->focused_index;

				card = e_contact_card_container_get_card (self, item_index);
				if (card)
					e_contact_card_set_contact (E_CONTACT_CARD (card), state->item);
			}
		}
	} else if (!items &&
		   !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		   !g_error_matches (local_error, E_CLIENT_ERROR, E_CLIENT_ERROR_OUT_OF_SYNC)) {
		g_message ("%s: Failed to get items: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");
	}

	if (selected_or_focused_changed)
		g_signal_emit (self, container_signals[CONTAINER_SELECTED_CHANGED], 0, NULL);

	gid->cb (self, gid->range_start, gid->range_length, items, gid->cb_user_data, local_error);

	g_warn_if_fail (self->ongoing_range_read == gid);
	self->ongoing_range_read = NULL;

	e_contact_card_container_read_next_range (self);

	g_clear_error (&local_error);
	g_clear_object (&self);
	g_clear_pointer (&items, g_ptr_array_unref);
	get_items_data_free (gid);
}

static void
e_contact_card_container_got_contacts_cb (EContactCardContainer *self,
					  guint range_start,
					  guint range_length,
					  GPtrArray *contacts, /* EContact * */
					  gpointer user_data,
					  const GError *error)
{
	/* Nothing to do here, the e_contact_card_container_got_items_cb() did everything needed */
}

static gboolean
e_contact_card_container_update_card_state (EContactCardContainer *self,
					    GtkWidget *card,
					    guint item_index,
					    ItemState *item_state)
{
	GtkStyleContext *style_context;
	gboolean changed = FALSE;
	gboolean current;

	style_context = gtk_widget_get_style_context (card);

	current = gtk_style_context_has_class (style_context, "selected");

	if ((current ? 1 : 0) != (item_state->selected ? 1 : 0)) {
		changed = TRUE;

		if (item_state->selected)
			gtk_style_context_add_class (style_context, "selected");
		else
			gtk_style_context_remove_class (style_context, "selected");
	}

	current = gtk_style_context_has_class (style_context, "focused");
	if ((current ? 1 : 0) != (item_index == self->focused_index ? 1 : 0)) {
		changed = TRUE;

		if (item_index == self->focused_index)
			gtk_style_context_add_class (style_context, "focused");
		else
			gtk_style_context_remove_class (style_context, "focused");
	}

	return changed;
}

static void
e_contact_card_container_update_item_state (EContactCardContainer *self,
					    guint item_index)
{
	GtkWidget *card;

	card = e_contact_card_container_get_card (self, item_index);

	if (card) {
		ItemState *item_state;

		item_state = &g_array_index (self->items, ItemState, item_index);

		if (e_contact_card_container_update_card_state (self, card, item_index, item_state))
			gtk_widget_queue_draw (card);
	}
}

static void
e_contact_card_container_update (EContactCardContainer *self)
{
	GtkWidget *card, *self_widget = GTK_WIDGET (self);
	GtkLayout *self_layout = GTK_LAYOUT (self);
	guint new_content_width, new_content_height;
	guint old_content_width = 0, old_content_height = 0;
	gint card_width = 0, card_height = 0;
	guint ii;
	gboolean is_new_card;

	if (self->cards->len > 0) {
		card = g_ptr_array_index (self->cards, 0);
		gtk_widget_set_size_request (card, -1, -1);
		is_new_card = FALSE;
	} else {
		card = e_contact_card_new (self->css_provider);
		gtk_layout_put (self_layout, card, 0, 0);
		gtk_widget_set_visible (card, TRUE);
		is_new_card = TRUE;
	}

	gtk_widget_get_preferred_width (card, &card_width, NULL);
	gtk_widget_get_preferred_height (card, &card_height, NULL);

	if (is_new_card)
		gtk_widget_destroy (card);
	else
		gtk_widget_set_size_request (card, self->card_width, self->card_height);

	if (card_width != 0)
		self->card_width = MAX (card_width, MIN_CARD_WIDTH);

	if (card_height > self->card_height)
		self->card_height = card_height;

	if (!self->card_width || !self->card_height || !self->items->len) {
		new_content_width = 0;
		new_content_height = 0;
		self->n_cols = 0;
		for (ii = 0; ii < self->cards->len; ii++) {
			card = g_ptr_array_index (self->cards, ii);
			gtk_widget_set_visible (card, FALSE);
			e_contact_card_set_contact (E_CONTACT_CARD (card), NULL);
		}
	} else {
		guint max_cards;
		gint n_rows, n_cols;
		gint used_space, available_space;

		available_space = self->viewport.width - 2;

		n_cols = available_space / (self->card_width + (2 * CARD_PACKING_MARGIN));
		if (n_cols <= 0)
			n_cols = 1;

		n_rows = self->items->len / n_cols;
		if (n_rows <= 0)
			n_rows = 1;
		if (n_rows * n_cols < self->items->len)
			n_rows++;

		if (self->scrollbar_size > 0) {
			gboolean needs_vscrollbar;

			needs_vscrollbar = (n_rows * (self->card_height + (2 * CARD_PACKING_MARGIN))) + self->scrollbar_size >= self->viewport.height;

			if (needs_vscrollbar) {
				available_space -= self->scrollbar_size;

				n_cols = available_space / (self->card_width + (2 * CARD_PACKING_MARGIN));
				if (n_cols <= 0)
					n_cols = 1;

				n_rows = self->items->len / n_cols;
			}
		}

		if (n_rows <= 0)
			n_rows = 1;
		if (n_rows * n_cols < self->items->len)
			n_rows++;

		used_space = n_cols * (self->card_width + (2 * CARD_PACKING_MARGIN));
		if (used_space < available_space)
			self->card_width += (available_space - used_space) / n_cols;

		new_content_width = n_cols * (self->card_width + (2 * CARD_PACKING_MARGIN));
		new_content_height = n_rows * (self->card_height + (2 * CARD_PACKING_MARGIN));

		self->n_cols = n_cols;

		n_rows = self->viewport.height / (self->card_height + (2 * CARD_PACKING_MARGIN));

		/* two more rows, for partially visible row at the top and at the bottom */
		max_cards = (n_rows + 2) * n_cols;

		/* only add new cards, if needed */
		for (ii = self->cards->len; ii < max_cards; ii++) {
			card = e_contact_card_new (self->css_provider);

			gtk_drag_source_set (card, GDK_BUTTON1_MASK, dnd_types, G_N_ELEMENTS (dnd_types), GDK_ACTION_MOVE | GDK_ACTION_COPY);

			gtk_layout_put (self_layout, card, 0, 0);
			g_ptr_array_add (self->cards, card);

			g_signal_connect_object (card, "event",
				G_CALLBACK (e_contact_card_container_card_event_cb), self, 0);
			g_signal_connect_object (card, "popup-menu",
				G_CALLBACK (e_contact_card_container_card_popup_menu_cb), self, 0);
			g_signal_connect_object (card, "drag-begin",
				G_CALLBACK (e_contact_card_container_card_drag_begin_cb), self, 0);
			g_signal_connect_object (card, "drag-data-get",
				G_CALLBACK (e_contact_card_container_card_drag_data_get_cb), self, 0);
			g_signal_connect_object (card, "drag-end",
				G_CALLBACK (e_contact_card_container_card_drag_end_cb), self, 0);
		}
	}

	gtk_layout_get_size (self_layout, &old_content_width, &old_content_height);
	if (new_content_width != old_content_width ||
	    new_content_height != old_content_height) {
		gtk_layout_set_size (self_layout, new_content_width, new_content_height);
	}

	if (self->n_cols > 0 && self->card_height > 0) {
		GtkWidget *parent;
		guint get_range_start = 0, get_range_length = 0;
		gint xx, yy, first_row, nth_col;
		guint first_item, visible_items;

		visible_items = (2 + (self->viewport.height / (self->card_height + (2 * CARD_PACKING_MARGIN)))) * self->n_cols;

		first_row = self->viewport.y / (self->card_height + (2 * CARD_PACKING_MARGIN));
		first_item = first_row * self->n_cols;

		self->items_range_start = first_item;
		self->items_range_length = visible_items;

		xx = CARD_PACKING_MARGIN;
		yy = CARD_PACKING_MARGIN + (self->card_height  + (2 * CARD_PACKING_MARGIN)) * first_row;

		/* show and position visible cards */
		for (ii = 0, nth_col = 0; ii < self->cards->len && first_item + ii < self->items->len && ii < visible_items; ii++) {
			ItemState *state = &g_array_index (self->items, ItemState, first_item + ii);

			card = g_ptr_array_index (self->cards, ii);

			gtk_widget_set_size_request (card, self->card_width, self->card_height);
			gtk_layout_move (self_layout, card, xx, yy);
			if (!gtk_widget_get_visible (card))
				gtk_widget_set_visible (card, TRUE);

			e_contact_card_container_update_card_state (self, card, first_item + ii, state);
			e_contact_card_set_contact (E_CONTACT_CARD (card), state->item);

			if (!state->item) {
				if (!get_range_length)
					get_range_start = first_item + ii;
				get_range_length = first_item + ii - get_range_start + 1;
			}

			nth_col++;
			if (nth_col == self->n_cols) {
				nth_col = 0;
				xx = CARD_PACKING_MARGIN;
				yy += self->card_height + (2 * CARD_PACKING_MARGIN);
			} else {
				xx += self->card_width + (2 * CARD_PACKING_MARGIN);
			}
		}

		/* hide out of bounds cards */
		while (ii < self->cards->len) {
			card = g_ptr_array_index (self->cards, ii);
			if (!gtk_widget_get_visible (card))
				break;
			gtk_widget_set_visible (card, FALSE);
			ii++;
		}

		if (get_range_length) {
			e_contact_card_container_schedule_range_read (self, get_range_start, get_range_length, NULL,
				e_contact_card_container_got_contacts_cb, NULL);
		}

		parent = gtk_widget_get_parent (self_widget);
		if (GTK_IS_CONTAINER (parent))
			gtk_container_check_resize (GTK_CONTAINER (parent));
	}
}

static void
e_contact_card_container_get_item_place (EContactCardContainer *self,
					 guint item_index,
					 GtkAllocation *out_allocation)
{
	out_allocation->x = 0;
	out_allocation->y = 0;
	out_allocation->width = 0;
	out_allocation->height = 0;

	if (item_index < self->items->len && self->n_cols > 0) {
		guint row_index, col_index;

		row_index = item_index / self->n_cols;
		col_index = item_index - (row_index * self->n_cols);

		out_allocation->x = col_index * (self->card_width + (2 * CARD_PACKING_MARGIN));
		out_allocation->y = row_index * (self->card_height + (2 * CARD_PACKING_MARGIN));
		out_allocation->width = self->card_width + (2 * CARD_PACKING_MARGIN);
		out_allocation->height = self->card_height + (2 * CARD_PACKING_MARGIN);

		out_allocation->x = CLAMP (out_allocation->x - CARD_PACKING_MARGIN, 0, out_allocation->x);
		out_allocation->y = CLAMP (out_allocation->y - CARD_PACKING_MARGIN, 0, out_allocation->y);
	}
}

static gboolean
e_contact_card_container_focus (GtkWidget *widget,
				GtkDirectionType direction)
{
	EContactCardContainer *self = E_CONTACT_CARD_CONTAINER (widget);
	gboolean ret = FALSE;

	if (self->focused_index < self->items->len)
		ret = !e_contact_card_container_item_grab_focus (self, self->focused_index, NULL);

	return ret;
}

static void
e_contact_card_container_dispose (GObject *object)
{
	EContactCardContainer *self = E_CONTACT_CARD_CONTAINER (object);

	e_contact_card_container_cleanup_get_items_queue (self);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_contact_card_container_parent_class)->dispose (object);
}

static void
e_contact_card_container_finalize (GObject *object)
{
	EContactCardContainer *self = E_CONTACT_CARD_CONTAINER (object);

	e_contact_card_container_cleanup_get_items_queue (self);

	g_clear_pointer (&self->cards, g_ptr_array_unref);
	g_clear_pointer (&self->items, g_array_unref);
	g_clear_object (&self->css_provider);

	if (self->get_items_source_data_destroy)
		self->get_items_source_data_destroy (self->get_items_source_data);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_contact_card_container_parent_class)->finalize (object);
}

static void
e_contact_card_container_class_init (EContactCardContainerClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->focus = e_contact_card_container_focus;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = e_contact_card_container_dispose;
	object_class->finalize = e_contact_card_container_finalize;

	container_signals[CONTAINER_SELECTED_CHANGED] = g_signal_new (
		"selected-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL, g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	container_signals[CONTAINER_CARD_EVENT] = g_signal_new (
		"card-event",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		g_signal_accumulator_true_handled, NULL, NULL,
		G_TYPE_BOOLEAN, 2,
		E_TYPE_CONTACT_CARD,
		GDK_TYPE_EVENT);

	container_signals[CONTAINER_CARD_POPUP_MENU] = g_signal_new (
		"card-popup-menu",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		g_signal_accumulator_true_handled, NULL, NULL,
		G_TYPE_BOOLEAN, 1,
		E_TYPE_CONTACT_CARD);

	container_signals[CONTAINER_CARD_DRAG_BEGIN] = g_signal_new (
		"card-drag-begin",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		GDK_TYPE_DRAG_CONTEXT);

	container_signals[CONTAINER_CARD_DRAG_DATA_GET] = g_signal_new (
		"card-drag-data-get",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 4,
		GDK_TYPE_DRAG_CONTEXT,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT,
		G_TYPE_UINT);

	container_signals[CONTAINER_CARD_DRAG_END] = g_signal_new (
		"card-drag-end",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		GDK_TYPE_DRAG_CONTEXT);
}

static void
e_contact_card_container_init (EContactCardContainer *self)
{
	guint ii;

	self->cards = g_ptr_array_new ();
	self->items = g_array_new (FALSE, TRUE, sizeof (ItemState));
	self->tracked_selected_index = 0;
	self->n_known_selected = 0;

	for (ii = 0; ii < TRACK_N_SELECTED; ii++) {
		self->tracked_selected[ii] = G_MAXUINT;
	}

	g_array_set_clear_func (self->items, item_state_clear);

	gtk_widget_set_can_focus (GTK_WIDGET (self), FALSE);
}

static EContactCardContainer *
e_contact_card_container_new (void)
{
	return g_object_new (E_TYPE_CONTACT_CARD_CONTAINER, NULL);
}

static void
e_contact_card_container_set_viewport (EContactCardContainer *self,
				       const GtkAllocation *viewport,
				       gint scrollbar_size)
{
	if (viewport->x != self->viewport.x ||
	    viewport->y != self->viewport.y ||
	    viewport->width != self->viewport.width ||
	    viewport->height != self->viewport.height ||
	    scrollbar_size != self->scrollbar_size) {
		self->viewport = *viewport;
		self->scrollbar_size = scrollbar_size;

		e_contact_card_container_update (self);
	}
}

static void
e_contact_card_container_update_tracked_selected (EContactCardContainer *self,
						  guint item_index,
						  gboolean select)
{
	guint ii;

	if (!select && !self->n_known_selected)
		return;

	if (self->n_known_selected > TRACK_N_SELECTED) {
		if (select) {
			self->n_known_selected++;
		} else {
			self->n_known_selected--;

			if (self->n_known_selected <= TRACK_N_SELECTED) {
				guint left_selected = self->n_known_selected;

				for (ii = 0; ii < self->items->len && left_selected; ii++) {
					ItemState *state = &g_array_index (self->items, ItemState, ii);

					if (state->selected) {
						self->tracked_selected[self->tracked_selected_index] = ii;
						self->tracked_selected_index = (self->tracked_selected_index + 1) % TRACK_N_SELECTED;
						left_selected--;
					}
				}
			}
		}
		return;
	}

	if (select) {
		self->n_known_selected++;

		if (self->n_known_selected > TRACK_N_SELECTED)
			return;

		for (ii = 0; ii < TRACK_N_SELECTED; ii++) {
			guint idx = (self->tracked_selected_index + ii) % TRACK_N_SELECTED;
			if (self->tracked_selected[idx] == G_MAXUINT) {
				self->tracked_selected[idx] = item_index;
				self->tracked_selected_index = idx;
				break;
			}
		}

		g_warn_if_fail (ii < TRACK_N_SELECTED);
	} else {
		self->n_known_selected--;

		for (ii = 0; ii < TRACK_N_SELECTED; ii++) {
			guint idx = (self->tracked_selected_index + ii) % TRACK_N_SELECTED;
			if (self->tracked_selected[idx] == item_index) {
				self->tracked_selected[idx] = G_MAXUINT;
				self->tracked_selected_index = idx;
				break;
			}
		}
	}
}

static void
e_contact_card_container_set_n_items (EContactCardContainer *self,
				      guint n_items)
{
	guint ii;

	self->stamp++;
	e_contact_card_container_cleanup_get_items_queue (self);

	if (self->items->len != n_items)
		g_array_set_size (self->items, n_items);

	for (ii = 0; ii < self->items->len; ii++) {
		ItemState *state = &g_array_index (self->items, ItemState, ii);

		g_clear_object (&state->item);
		state->selected = FALSE;
	}

	self->tracked_selected_index = 0;
	self->n_known_selected = 0;

	for (ii = 0; ii < TRACK_N_SELECTED; ii++) {
		self->tracked_selected[ii] = G_MAXUINT;
	}

	e_contact_card_container_update (self);
}

static gboolean
e_contact_card_container_modify_selection_all (EContactCardContainer *self,
					       gboolean select_them)
{
	gboolean any_changed = FALSE;
	guint ii;

	if (!select_them && self->n_known_selected <= TRACK_N_SELECTED) {
		guint tracked_selected_index = self->tracked_selected_index;

		for (ii = 0; ii < TRACK_N_SELECTED && self->n_known_selected; ii++) {
			guint idx = (tracked_selected_index + ii) % TRACK_N_SELECTED;

			if (self->tracked_selected[idx] != G_MAXUINT) {
				guint item_index = self->tracked_selected[idx];
				ItemState *state = &g_array_index (self->items, ItemState, item_index);

				state->selected = FALSE;
				any_changed = TRUE;

				self->tracked_selected[idx] = G_MAXUINT;
				self->tracked_selected_index = idx;
				self->n_known_selected--;

				e_contact_card_container_update_item_state (self, item_index);
			}
		}
	} else {
		for (ii = 0; ii < self->items->len; ii++) {
			ItemState *state = &g_array_index (self->items, ItemState, ii);

			if ((select_them ? 1 : 0) != (state->selected ? 1 : 0)) {
				state->selected = select_them;
				any_changed = TRUE;

				e_contact_card_container_update_tracked_selected (self, ii, select_them);
				e_contact_card_container_update_item_state (self, ii);
			}
		}
	}

	return any_changed;
}

static gboolean
e_contact_card_container_is_selected (EContactCardContainer *self,
				      guint item_index)
{
	if (item_index < self->items->len) {
		ItemState *item_state = &g_array_index (self->items, ItemState, item_index);
		return item_state->selected;
	}

	return FALSE;
}

/* ************************************************************************* */

struct _EContactCardBoxPrivate {
	GtkCssProvider *css_provider;
	EContactCardContainer *container;

	gint last_width;
	gint last_height;
	guint n_columns;
	guint n_rows;
};

enum {
	BOX_CHILD_ACTIVATED,
	BOX_SELECTED_CHILDREN_CHANGED,
	BOX_ACTIVATE_CURSOR_CHILD,
	BOX_TOGGLE_CURSOR_CHILD,
	BOX_MOVE_CURSOR,
	BOX_SELECT_ALL,
	BOX_UNSELECT_ALL,
	BOX_CARD_EVENT,
	BOX_CARD_POPUP_MENU,
	BOX_CARD_DRAG_BEGIN,
	BOX_CARD_DRAG_DATA_GET,
	BOX_CARD_DRAG_END,
	BOX_COUNT_CHANGED,
	BOX_LAST_SIGNAL
};

static guint box_signals[BOX_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EContactCardBox, e_contact_card_box, GTK_TYPE_SCROLLED_WINDOW)

static guint
e_contact_card_box_get_card_index (EContactCardBox *self,
				   EContactCard *card)
{
	guint ii;

	for (ii = 0; ii < self->priv->container->cards->len; ii++) {
		EContactCard *adept = g_ptr_array_index (self->priv->container->cards, ii);

		if (adept == card) {
			return self->priv->container->items_range_start + ii;
		}
	}

	return self->priv->container->items->len;
}

/* mimic what gtk_scrolled_window_update_use_indicators() does */
static gboolean
e_contact_card_box_is_overlay_scrolling (EContactCardBox *self)
{
	GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (self));
	gboolean overlay_scrolling;

	g_object_get (settings, "gtk-overlay-scrolling", &overlay_scrolling, NULL);

	overlay_scrolling = overlay_scrolling && gtk_scrolled_window_get_overlay_scrolling (GTK_SCROLLED_WINDOW (self));

	if (overlay_scrolling) {
		static gchar env_overlay_scrolling = -1;

		if (env_overlay_scrolling == -1)
			env_overlay_scrolling = g_strcmp0 (g_getenv ("GTK_OVERLAY_SCROLLING"), "0") == 0 ? FALSE : TRUE;

		overlay_scrolling = env_overlay_scrolling;
	}

	return overlay_scrolling;
}

static void
e_contact_card_box_update_viewport (EContactCardBox *self)
{
	GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (self);
	GtkAllocation allocation;
	GtkAdjustment *adjustment;
	gint scrollbar_size = 0;

	gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

	adjustment = gtk_scrolled_window_get_hadjustment (scrolled_window);
	allocation.x = (gint) gtk_adjustment_get_value (adjustment);

	adjustment = gtk_scrolled_window_get_vadjustment (scrolled_window);
	allocation.y = (gint) gtk_adjustment_get_value (adjustment);

	if (!e_contact_card_box_is_overlay_scrolling (self)) {
		GtkWidget *scrollbar;

		scrollbar = gtk_scrolled_window_get_vscrollbar (scrolled_window);
		scrollbar_size = gtk_widget_get_allocated_width (scrollbar);
	}

	e_contact_card_container_set_viewport (self->priv->container, &allocation, scrollbar_size);
}

static void
e_contact_card_box_get_current_selection_modifiers (GtkWidget *widget,
						    gboolean *out_modify,
						    gboolean *out_extend)
{
	GdkModifierType state = 0;

	if (gtk_get_current_event_state (&state)) {
		GdkModifierType mask;

		mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_MODIFY_SELECTION);
		*out_modify = (state & mask) == mask;

		mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_EXTEND_SELECTION);
		*out_extend = (state & mask) == mask;
	} else {
		*out_modify = FALSE;
		*out_extend = FALSE;
	}
}

static void
e_contact_card_box_update_cursor (EContactCardBox *self,
				  guint index)
{
	AtkObject *accessible;
	GtkWidget *card = NULL;

	if (self->priv->container->focused_index != index) {
		guint old_focused_index = self->priv->container->focused_index;

		self->priv->container->focused_index = index;

		e_contact_card_container_update_item_state (self->priv->container, old_focused_index);
		e_contact_card_box_scroll_to_index (self, index, TRUE);
		e_contact_card_container_update_item_state (self->priv->container, index);

		g_signal_emit (self, box_signals[BOX_SELECTED_CHILDREN_CHANGED], 0);
	}

	e_contact_card_container_item_grab_focus (self->priv->container, index, &card);

	accessible = gtk_widget_get_accessible (GTK_WIDGET (self));
	if (accessible) {
		AtkObject *descendant;

		descendant = card ? gtk_widget_get_accessible (card) : NULL;
		g_signal_emit_by_name (accessible, "active-descendant-changed", descendant);
	}
}

static gboolean
e_contact_card_box_set_selected_items (EContactCardBox *self,
				       guint from_index, /* inclusive */
				       guint to_index, /* inclusive */
				       gboolean selected)
{
	ItemState *state;
	gboolean any_changed = FALSE;
	guint ii;

	g_return_val_if_fail (from_index < self->priv->container->items->len, any_changed);
	g_return_val_if_fail (to_index < self->priv->container->items->len, any_changed);

	if (from_index > to_index) {
		guint swap;

		swap = from_index;
		from_index = to_index;
		to_index = swap;
	}

	for (ii = from_index; ii <= to_index; ii++) {
		state = &g_array_index (self->priv->container->items, ItemState, ii);

		if ((state->selected ? 1 : 0) == (selected ? 1 : 0))
			continue;

		state->selected = selected;
		any_changed = TRUE;

		e_contact_card_container_update_tracked_selected (self->priv->container, ii, selected);
		e_contact_card_container_update_item_state (self->priv->container, ii);
	}

	return any_changed;
}

static void
e_contact_card_box_select_and_activate (EContactCardBox *self,
					guint index)
{
	if (index < self->priv->container->items->len) {
		gboolean changed;

		changed = e_contact_card_box_set_selected_items (self, index, index, TRUE);
		e_contact_card_box_update_cursor (self, index);

		if (changed)
			g_signal_emit (self, box_signals[BOX_SELECTED_CHILDREN_CHANGED], 0);
		g_signal_emit (self, box_signals[BOX_CHILD_ACTIVATED], 0, index);
    }
}

static void
e_contact_card_box_activate_cursor_child (EContactCardBox *self)
{
	if (self->priv->container->focused_index < self->priv->container->items->len)
		e_contact_card_box_select_and_activate (self, self->priv->container->focused_index);
}

static void
e_contact_card_box_toggle_child (EContactCardBox *self,
				 guint item_index)
{
	if (item_index < self->priv->container->items->len) {
		ItemState *state;

		state = &g_array_index (self->priv->container->items, ItemState, item_index);

		e_contact_card_box_set_selected_items (self, item_index, item_index, !state->selected);
	}
}

static void
e_contact_card_box_toggle_cursor_child (EContactCardBox *self)
{
	e_contact_card_box_toggle_child (self, self->priv->container->focused_index);
	g_signal_emit (self, box_signals[BOX_SELECTED_CHILDREN_CHANGED], 0);
}

static void
e_contact_card_box_update_selection (EContactCardBox *self,
				     guint item_index,
				     gboolean modify,
				     gboolean extend)
{
	guint focused_index = self->priv->container->focused_index;
	gboolean changed = FALSE;

	if (extend) {
		e_contact_card_container_modify_selection_all (self->priv->container, FALSE);
		changed = e_contact_card_box_set_selected_items (self, focused_index, item_index, TRUE);
	} else if (modify) {
		e_contact_card_box_toggle_child (self, item_index);
		changed = TRUE;
	} else if (/*self->priv->container->n_known_selected != 1 ||*/ /* this may clear multi-selection before drag begin */
		   !e_contact_card_container_is_selected (self->priv->container, item_index)) {
		e_contact_card_container_modify_selection_all (self->priv->container, FALSE);
		changed = e_contact_card_box_set_selected_items (self, item_index, item_index, TRUE);
	}

	e_contact_card_box_update_cursor (self, item_index);

	if (changed)
		g_signal_emit (self, box_signals[BOX_SELECTED_CHILDREN_CHANGED], 0);
}

static gboolean
e_contact_card_box_move_cursor (EContactCardBox *self,
				GtkMovementStep step,
				gint count)
{
	guint page_size;
	guint new_focus_index;
	guint last_item_index;

	if (!self->priv->container->items->len)
		return TRUE;

	last_item_index = self->priv->container->items->len - 1;
	new_focus_index = self->priv->container->focused_index;

	switch (step) {
	case GTK_MOVEMENT_VISUAL_POSITIONS:
		if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
			count = -count;

		if (count < 0) {
			if (new_focus_index >= -count)
				new_focus_index += count;
			else
				new_focus_index = 0;
		} else {
			if (new_focus_index + count <= last_item_index)
				new_focus_index += count;
			else
				new_focus_index = last_item_index;
		}
		break;

	case GTK_MOVEMENT_BUFFER_ENDS:
		if (count < 0)
			new_focus_index = 0;
		else
			new_focus_index = last_item_index;
		break;

	case GTK_MOVEMENT_DISPLAY_LINES:
		if (count < 0) {
			if (new_focus_index >= -count * self->priv->container->n_cols)
				new_focus_index += count * self->priv->container->n_cols;
			else
				new_focus_index = 0;
		} else {
			if (new_focus_index + (count * self->priv->container->n_cols) <= last_item_index)
				new_focus_index += count * self->priv->container->n_cols;
			else
				new_focus_index = last_item_index;
		}
		break;

	case GTK_MOVEMENT_PAGES:
		page_size = self->priv->container->items_range_length - (2 * self->priv->container->n_cols);
		if (!page_size || page_size >= self->priv->container->items->len)
			page_size = self->priv->container->n_cols;

		if (count < 0) {
			if (new_focus_index >= -count * page_size)
				new_focus_index += count * page_size;
			else
				new_focus_index = 0;
		} else {
			if (new_focus_index + (count * page_size) <= last_item_index)
				new_focus_index += count * page_size;
			else
				new_focus_index = last_item_index;
		}
		break;

	default:
		g_warn_if_reached ();
		break;
	}

	if (new_focus_index != self->priv->container->focused_index) {
		gboolean modify;
		gboolean extend;

		e_contact_card_box_get_current_selection_modifiers (GTK_WIDGET (self), &modify, &extend);

		if (!modify)
			e_contact_card_box_update_selection (self, new_focus_index, FALSE, extend);
		e_contact_card_box_update_cursor (self, new_focus_index);
	}

	return TRUE;
}

static void
e_contact_card_box_select_all (EContactCardBox *self)
{
	if (e_contact_card_container_modify_selection_all (self->priv->container, TRUE)) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		g_signal_emit (self, box_signals[BOX_SELECTED_CHILDREN_CHANGED], 0);
	}
}

static void
e_contact_card_box_unselect_all (EContactCardBox *self)
{
	if (e_contact_card_container_modify_selection_all (self->priv->container, FALSE)) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		g_signal_emit (self, box_signals[BOX_SELECTED_CHILDREN_CHANGED], 0);
	}
}

static void
e_contact_card_box_selected_children_changed (EContactCardBox *self)
{
	/*AtkObject *accessible;

	accessible = gtk_widget_get_accessible (self->priv->container);
	if (accessible)
		g_signal_emit_by_name (accessible, "selection-changed");*/
}

static void
e_contact_card_box_size_allocate (GtkWidget *widget,
				  GtkAllocation *allocation)
{
	EContactCardBox *self = E_CONTACT_CARD_BOX (widget);

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_contact_card_box_parent_class)->size_allocate (widget, allocation);

	e_contact_card_box_update_viewport (self);
}

static gboolean
e_contact_card_box_card_event_cb (GtkWidget *container,
				  EContactCard *card,
				  GdkEvent *event,
				  gpointer user_data)
{
	EContactCardBox *self = E_CONTACT_CARD_BOX (user_data);
	guint item_index = G_MAXUINT;
	gboolean ret = FALSE;

	if (card)
		item_index = e_contact_card_box_get_card_index (self, card);

	if (event->type == GDK_BUTTON_PRESS && event->button.button == GDK_BUTTON_PRIMARY) {
		if (card) {
			gboolean modify;
			gboolean extend;

			e_contact_card_box_get_current_selection_modifiers (GTK_WIDGET (self), &modify, &extend);

			e_contact_card_box_update_selection (self, item_index, modify, extend);

			if (item_index != self->priv->container->focused_index)
				e_contact_card_box_update_cursor (self, item_index);
		} else if (!gtk_widget_has_focus (GTK_WIDGET (self->priv->container))) {
			gtk_widget_grab_focus (GTK_WIDGET (self->priv->container));
		}
	}

	g_signal_emit (self, box_signals[BOX_CARD_EVENT], 0, item_index, event, &ret);

	return ret;
}

static gboolean
e_contact_card_box_card_popup_menu_cb (GtkWidget *container,
				       EContactCard *card,
				       gpointer user_data)
{
	gboolean ret = FALSE;

	if (card) {
		EContactCardBox *self = E_CONTACT_CARD_BOX (user_data);
		guint item_index;

		item_index = e_contact_card_box_get_card_index (self, card);

		g_signal_emit (self, box_signals[BOX_CARD_POPUP_MENU], 0, item_index, &ret);
	}

	return ret;
}

static void
e_contact_card_box_card_drag_begin_cb (GtkWidget *widget,
				       GdkDragContext *context,
				       gpointer user_data)
{
	EContactCardBox *self = E_CONTACT_CARD_BOX (user_data);

	g_signal_emit (self, box_signals[BOX_CARD_DRAG_BEGIN], 0, context);
}

static void
e_contact_card_box_card_drag_data_get_cb (GtkWidget *widget,
					  GdkDragContext *context,
					  GtkSelectionData *selection_data,
					  guint info,
					  guint time,
					  gpointer user_data)
{
	EContactCardBox *self = E_CONTACT_CARD_BOX (user_data);

	g_signal_emit (self, box_signals[BOX_CARD_DRAG_DATA_GET], 0, context, selection_data, info, time);
}

static void
e_contact_card_box_card_drag_end_cb (GtkWidget *widget,
				     GdkDragContext *context,
				     gpointer user_data)
{
	EContactCardBox *self = E_CONTACT_CARD_BOX (user_data);

	g_signal_emit (self, box_signals[BOX_CARD_DRAG_END], 0, context);
}

static gboolean
e_contact_card_box_container_popup_menu_cb (GtkWidget *container,
					    gpointer user_data)
{
	EContactCardBox *self = E_CONTACT_CARD_BOX (user_data);
	gboolean ret = FALSE;

	g_signal_emit (self, box_signals[BOX_CARD_POPUP_MENU], 0, G_MAXUINT, &ret);

	return ret;
}

static void
e_contact_card_box_container_selected_changed_cb (GtkWidget *container,
						  gpointer user_data)
{
	EContactCardBox *self = user_data;

	g_signal_emit (self, box_signals[BOX_SELECTED_CHILDREN_CHANGED], 0);
}

static void
e_contact_card_box_add_move_binding (GtkBindingSet *binding_set,
				     guint keyval,
				     GdkModifierType modmask,
				     GtkMovementStep step,
				     gint count)
{
	GdkDisplay *display;
	GdkModifierType extend_mod_mask = GDK_SHIFT_MASK;
	GdkModifierType modify_mod_mask = GDK_CONTROL_MASK;

	display = gdk_display_get_default ();
	if (display) {
		extend_mod_mask = gdk_keymap_get_modifier_mask (gdk_keymap_get_for_display (display), GDK_MODIFIER_INTENT_EXTEND_SELECTION);
		modify_mod_mask = gdk_keymap_get_modifier_mask (gdk_keymap_get_for_display (display), GDK_MODIFIER_INTENT_MODIFY_SELECTION);
	}

	gtk_binding_entry_add_signal (binding_set, keyval, modmask,
		"move-cursor", 2,
		GTK_TYPE_MOVEMENT_STEP, step,
		G_TYPE_INT, count,
		NULL);

	gtk_binding_entry_add_signal (binding_set, keyval, modmask | extend_mod_mask,
		"move-cursor", 2,
		GTK_TYPE_MOVEMENT_STEP, step,
		G_TYPE_INT, count,
		NULL);

	gtk_binding_entry_add_signal (binding_set, keyval, modmask | modify_mod_mask,
		"move-cursor", 2,
		GTK_TYPE_MOVEMENT_STEP, step,
		G_TYPE_INT, count,
		NULL);

	gtk_binding_entry_add_signal (binding_set, keyval, modmask | extend_mod_mask | modify_mod_mask,
		"move-cursor", 2,
		GTK_TYPE_MOVEMENT_STEP, step,
		G_TYPE_INT, count,
		NULL);
}

static void
e_contact_card_box_constructed (GObject *object)
{
	EContactCardBox *self = E_CONTACT_CARD_BOX (object);
	GError *local_error = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_contact_card_box_parent_class)->constructed (object);

	g_object_set (self,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"min-content-width", 150,
		"min-content-height", 150,
		"can-focus", FALSE,
		NULL);

	g_signal_connect_swapped (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (self)), "notify::value",
		G_CALLBACK (e_contact_card_box_update_viewport), self);

	g_signal_connect_swapped (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self)), "notify::value",
		G_CALLBACK (e_contact_card_box_update_viewport), self);

	self->priv->css_provider = gtk_css_provider_new ();

	if (!gtk_css_provider_load_from_data (self->priv->css_provider,
		"EContactCard .econtent {"
		"   border-width:1px;"
		"   border-color:darker(@theme_fg_color);"
		"   border-style:solid;"
		"   min-width:210px;"
		"   min-height:80px;"
		"   padding:0px 0px 12px 0px;"
		"   margin: 2px;"
		"}"
		"EContactCard .eheader {"
		"   background:@theme_unfocused_bg_color;"
		"   padding:6px 12px 6px 12px;"
		"   margin-bottom:6px;"
		"   border-width:0px 0px 1px 0px;"
		"   border-color:darker(@theme_fg_color);"
		"   border-style:solid;"
		"}"
		"EContactCard.focused:focus {"
		"   border-color:@theme_selected_bg_color;"
		"   border-width:1px;"
		"   border-style:dashed;"
		"}"
		"EContactCard.focused .econtent:focus {"
		"   margin:2px;"
		"}"
		"EContactCard.selected .econtent {"
		"   border-color:@theme_selected_bg_color;"
		"   border-width:2px;"
		"   border-style:solid;"
		"   margin:1px;"
		"}"
		"EContactCard.selected .econtent .eheader {"
		"   background:@theme_selected_bg_color;"
		"   border-color:@theme_selected_bg_color;"
		"   color:@theme_selected_fg_color;"
		"}"
		"EContactCard .eheaderimage {"
		"   margin:-3px 0px -4px 0px;"
		"}"
		"EContactCard .erowlabel {"
		"   padding:0px 0px 0px 12px;"
		"}"
		"EContactCard .erowvalue {"
		"   padding:0px 12px 0px 0px;"
		"}",
		-1, &local_error)) {
		g_warning ("%s: Failed to parse CSS: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");
		g_clear_error (&local_error);
	}

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), GTK_STYLE_CLASS_VIEW);
	gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self)),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	self->priv->container = e_contact_card_container_new ();
	self->priv->container->css_provider = g_object_ref (self->priv->css_provider);
	g_object_set (self->priv->container,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->priv->container));

	g_signal_connect_object (self->priv->container, "card-event",
		G_CALLBACK (e_contact_card_box_card_event_cb), self, 0);
	g_signal_connect_object (self->priv->container, "card-popup-menu",
		G_CALLBACK (e_contact_card_box_card_popup_menu_cb), self, 0);
	g_signal_connect_object (self->priv->container, "card-drag-begin",
		G_CALLBACK (e_contact_card_box_card_drag_begin_cb), self, 0);
	g_signal_connect_object (self->priv->container, "card-drag-data-get",
		G_CALLBACK (e_contact_card_box_card_drag_data_get_cb), self, 0);
	g_signal_connect_object (self->priv->container, "card-drag-end",
		G_CALLBACK (e_contact_card_box_card_drag_end_cb), self, 0);
	g_signal_connect_object (self->priv->container, "popup-menu",
		G_CALLBACK (e_contact_card_box_container_popup_menu_cb), self, 0);
	g_signal_connect_object (self->priv->container, "selected-changed",
		G_CALLBACK (e_contact_card_box_container_selected_changed_cb), self, 0);
}

static void
e_contact_card_box_finalize (GObject *object)
{
	EContactCardBox *self = E_CONTACT_CARD_BOX (object);

	g_clear_object (&self->priv->css_provider);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_contact_card_box_parent_class)->finalize (object);
}

static void
e_contact_card_box_class_init (EContactCardBoxClass *klass)
{
	GtkBindingSet *binding_set;
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	klass->activate_cursor_child = e_contact_card_box_activate_cursor_child;
	klass->toggle_cursor_child = e_contact_card_box_toggle_cursor_child;
	klass->move_cursor = e_contact_card_box_move_cursor;
	klass->select_all = e_contact_card_box_select_all;
	klass->unselect_all = e_contact_card_box_unselect_all;
	klass->selected_children_changed = e_contact_card_box_selected_children_changed;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->size_allocate = e_contact_card_box_size_allocate;

	gtk_widget_class_set_css_name (widget_class, "EContactCardBox");

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_contact_card_box_constructed;
	object_class->finalize = e_contact_card_box_finalize;

	box_signals[BOX_CHILD_ACTIVATED] = g_signal_new ("child-activated",
		E_TYPE_CONTACT_CARD_BOX,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContactCardBoxClass, child_activated),
		NULL, NULL, g_cclosure_marshal_VOID__UINT,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);

	box_signals[BOX_SELECTED_CHILDREN_CHANGED] = g_signal_new ("selected-children-changed",
		E_TYPE_CONTACT_CARD_BOX,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EContactCardBoxClass, selected_children_changed),
		NULL, NULL, g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	box_signals[BOX_ACTIVATE_CURSOR_CHILD] = g_signal_new ("activate-cursor-child",
		E_TYPE_CONTACT_CARD_BOX,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, activate_cursor_child),
		NULL, NULL, g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	box_signals[BOX_TOGGLE_CURSOR_CHILD] = g_signal_new ("toggle-cursor-child",
		E_TYPE_CONTACT_CARD_BOX,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, toggle_cursor_child),
		NULL, NULL, g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	box_signals[BOX_MOVE_CURSOR] = g_signal_new ("move-cursor",
		E_TYPE_CONTACT_CARD_BOX,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, move_cursor),
		NULL, NULL, NULL,
		G_TYPE_BOOLEAN, 2,
		GTK_TYPE_MOVEMENT_STEP, G_TYPE_INT);

	box_signals[BOX_SELECT_ALL] = g_signal_new ("select-all",
		E_TYPE_CONTACT_CARD_BOX,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, select_all),
		NULL, NULL, g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	box_signals[BOX_UNSELECT_ALL] = g_signal_new ("unselect-all",
		E_TYPE_CONTACT_CARD_BOX,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, unselect_all),
		NULL, NULL, g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	box_signals[BOX_CARD_EVENT] = g_signal_new ("card-event",
		E_TYPE_CONTACT_CARD_BOX,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContactCardBoxClass, card_event),
		g_signal_accumulator_true_handled, NULL, NULL,
		G_TYPE_BOOLEAN, 2,
		G_TYPE_UINT,
		GDK_TYPE_EVENT);

	box_signals[BOX_CARD_POPUP_MENU] = g_signal_new (
		"card-popup-menu",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, card_popup_menu),
		g_signal_accumulator_true_handled, NULL, NULL,
		G_TYPE_BOOLEAN, 1,
		G_TYPE_UINT);

	box_signals[BOX_CARD_DRAG_BEGIN] = g_signal_new (
		"card-drag-begin",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, card_drag_begin),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		GDK_TYPE_DRAG_CONTEXT);

	box_signals[BOX_CARD_DRAG_DATA_GET] = g_signal_new (
		"card-drag-data-get",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, card_drag_data_get),
		NULL, NULL, NULL,
		G_TYPE_NONE, 4,
		GDK_TYPE_DRAG_CONTEXT,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT,
		G_TYPE_UINT);

	box_signals[BOX_CARD_DRAG_END] = g_signal_new (
		"card-drag-end",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, card_drag_end),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		GDK_TYPE_DRAG_CONTEXT);

	box_signals[BOX_COUNT_CHANGED] = g_signal_new (
		"count-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EContactCardBoxClass, count_changed),
		NULL, NULL, g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	widget_class->activate_signal = box_signals[BOX_ACTIVATE_CURSOR_CHILD];

	binding_set = gtk_binding_set_by_class (klass);

	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_Home, 0, GTK_MOVEMENT_BUFFER_ENDS, -1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_KP_Home, 0, GTK_MOVEMENT_BUFFER_ENDS, -1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_End, 0, GTK_MOVEMENT_BUFFER_ENDS, 1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_KP_End, 0, GTK_MOVEMENT_BUFFER_ENDS, 1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_Up, 0, GTK_MOVEMENT_DISPLAY_LINES, -1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_KP_Up, 0, GTK_MOVEMENT_DISPLAY_LINES, -1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_Down, 0, GTK_MOVEMENT_DISPLAY_LINES, 1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_KP_Down, 0, GTK_MOVEMENT_DISPLAY_LINES, 1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_Page_Up, 0, GTK_MOVEMENT_PAGES, -1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_KP_Page_Up, 0, GTK_MOVEMENT_PAGES, -1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_Page_Down, 0, GTK_MOVEMENT_PAGES, 1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_KP_Page_Down, 0, GTK_MOVEMENT_PAGES, 1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_Right, 0, GTK_MOVEMENT_VISUAL_POSITIONS, 1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_KP_Right, 0, GTK_MOVEMENT_VISUAL_POSITIONS, 1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_Left, 0, GTK_MOVEMENT_VISUAL_POSITIONS, -1);
	e_contact_card_box_add_move_binding (binding_set, GDK_KEY_KP_Left, 0, GTK_MOVEMENT_VISUAL_POSITIONS, -1);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, GDK_CONTROL_MASK, "toggle-cursor-child", 0, NULL);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK, "toggle-cursor-child", 0, NULL);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK, "select-all", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "unselect-all", 0);
}

static void
e_contact_card_box_init (EContactCardBox *self)
{
	self->priv = e_contact_card_box_get_instance_private (self);
}

/**
 * e_contact_card_box_new:
 * @get_items_func: (not nullable) (scope notified): a function used to get items to show asynchronously
 * @get_items_finish_func: (not nullable) (scope notified): a function used to finish @get_items_func call
 * @get_items_source_data: (optional): user data passed to the both @get_items_func and @get_items_finish_func
 * @get_items_source_data_destroy: (optional): optional destroy callback for the @get_items_source_data
 *
 * Creates a new #EContactCardBox. The provided @get_items_func and @get_items_finish_func
 * are used to retrieve items to be shown in the current view asynchronously. See the information
 * on the #EContactCardBoxGetItemsFunc and #EContactCardBoxGetItemsFinishFunc for their
 * prototypes and what they should do.
 *
 * Returns: (transfer full): a new #EContactCardBox
 *
 * Since: 3.50
 **/
GtkWidget *
e_contact_card_box_new (EContactCardBoxGetItemsFunc get_items_func,
			EContactCardBoxGetItemsFinishFunc get_items_finish_func,
			gpointer get_items_source_data,
			GDestroyNotify get_items_source_data_destroy)
{
	EContactCardBox *self;

	g_return_val_if_fail (get_items_func != NULL, NULL);
	g_return_val_if_fail (get_items_finish_func != NULL, NULL);

	self = g_object_new (E_TYPE_CONTACT_CARD_BOX, NULL);
	self->priv->container->get_items_func = get_items_func;
	self->priv->container->get_items_finish_func = get_items_finish_func;
	self->priv->container->get_items_source_data = get_items_source_data;
	self->priv->container->get_items_source_data_destroy = get_items_source_data_destroy;

	return GTK_WIDGET (self);
}

guint
e_contact_card_box_get_n_items (EContactCardBox *self)
{
	g_return_val_if_fail (E_IS_CONTACT_CARD_BOX (self), 0);

	return self->priv->container->items->len;
}

void
e_contact_card_box_set_n_items (EContactCardBox *self,
				guint n_items)
{
	g_return_if_fail (E_IS_CONTACT_CARD_BOX (self));

	if (self->priv->container->items->len == n_items)
		return;

	e_contact_card_container_set_n_items (self->priv->container, n_items);

	g_signal_emit (self, box_signals[BOX_COUNT_CHANGED], 0, NULL);
}

guint
e_contact_card_box_get_focused_index (EContactCardBox *self)
{
	g_return_val_if_fail (E_IS_CONTACT_CARD_BOX (self), G_MAXUINT);

	return self->priv->container->focused_index;
}

void
e_contact_card_box_set_focused_index (EContactCardBox *self,
				      guint item_index)
{
	g_return_if_fail (E_IS_CONTACT_CARD_BOX (self));

	if (item_index < self->priv->container->items->len)
		e_contact_card_box_update_cursor (self, item_index);
}

void
e_contact_card_box_scroll_to_index (EContactCardBox *self,
				    guint item_index,
				    gboolean can_in_middle)
{
	GtkAllocation allocation = { 0, 0, 0, 0 };
	GtkAdjustment *adjustment;
	gdouble value;

	g_return_if_fail (E_IS_CONTACT_CARD_BOX (self));

	e_contact_card_container_get_item_place (self->priv->container, item_index, &allocation);

	if (allocation.width <= 0 || allocation.height <= 0)
		return;

	adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self));
	value = gtk_adjustment_get_value (adjustment);

	if (allocation.y >= value && allocation.y + allocation.height <= value + self->priv->container->viewport.height)
		return;

	if (can_in_middle && allocation.height <= self->priv->container->viewport.height) {
		gint mid = (self->priv->container->viewport.height - allocation.height) / 2;
		gint upper = gtk_adjustment_get_upper (adjustment);

		value = allocation.y - mid;

		if (value < 1e-9)
			value = 0;

		if (value + allocation.height > upper)
			value = upper;
	} else {
		value = allocation.y;
	}

	if (((gint) gtk_adjustment_get_value (adjustment)) != ((gint) (value))) {
		gtk_adjustment_set_value (adjustment, value);
		e_contact_card_container_update (self->priv->container);
	}
}

gboolean
e_contact_card_box_get_selected (EContactCardBox *self,
				 guint item_index)
{
	g_return_val_if_fail (E_IS_CONTACT_CARD_BOX (self), FALSE);

	if (item_index < self->priv->container->items->len) {
		ItemState *item_state = &g_array_index (self->priv->container->items, ItemState, item_index);

		return item_state->selected;
	}

	return FALSE;
}

void
e_contact_card_box_set_selected (EContactCardBox *self,
				 guint item_index,
				 gboolean selected)
{
	g_return_if_fail (E_IS_CONTACT_CARD_BOX (self));

	if (item_index < self->priv->container->items->len &&
	    e_contact_card_box_set_selected_items (self, item_index, item_index, selected))
		g_signal_emit (self, box_signals[BOX_SELECTED_CHILDREN_CHANGED], 0);
}

void
e_contact_card_box_set_selected_all (EContactCardBox *self,
				     gboolean selected)
{
	g_return_if_fail (E_IS_CONTACT_CARD_BOX (self));

	if (selected)
		g_signal_emit (self, box_signals[BOX_SELECT_ALL], 0, NULL);
	else
		g_signal_emit (self, box_signals[BOX_UNSELECT_ALL], 0, NULL);
}

guint
e_contact_card_box_get_n_selected (EContactCardBox *self)
{
	g_return_val_if_fail (E_IS_CONTACT_CARD_BOX (self), 0);

	return self->priv->container->n_known_selected;
}

GPtrArray *
e_contact_card_box_dup_selected_indexes	(EContactCardBox *self)
{
	GPtrArray *indexes;
	guint left_selected, ii;

	g_return_val_if_fail (E_IS_CONTACT_CARD_BOX (self), NULL);

	left_selected = self->priv->container->n_known_selected;
	indexes = g_ptr_array_sized_new (MAX (1, left_selected));

	if (self->priv->container->n_known_selected <= TRACK_N_SELECTED) {
		for (ii = 0; ii < TRACK_N_SELECTED && left_selected > 0; ii++) {
			guint idx = (ii + self->priv->container->tracked_selected_index) % TRACK_N_SELECTED;

			if (self->priv->container->tracked_selected[idx] != G_MAXUINT) {
				g_ptr_array_add (indexes, GUINT_TO_POINTER (self->priv->container->tracked_selected[idx]));
				left_selected--;
			}
		}
	} else {
		for (ii = 0; ii < self->priv->container->items->len && left_selected > 0; ii++) {
			ItemState *item_state = &g_array_index (self->priv->container->items, ItemState, ii);
			if (item_state->selected) {
				g_ptr_array_add (indexes, GUINT_TO_POINTER (ii));
				left_selected--;
			}
		}
	}

	if (indexes->len == 0 && self->priv->container->focused_index < self->priv->container->items->len)
		g_ptr_array_add (indexes, GUINT_TO_POINTER (self->priv->container->focused_index));

	return indexes;
}

static gint
e_contact_card_box_sort_indexes_cb (gconstpointer aptr,
				    gconstpointer bptr)
{
	guint aa = GPOINTER_TO_UINT (*((gpointer *) aptr));
	guint bb = GPOINTER_TO_UINT (*((gpointer *) bptr));

	if (aa < bb)
		return -1;

	if (aa > bb)
		return 1;

	return 0;
}

typedef struct _Range {
	guint start; /* inclusive */
	guint end; /* inclusive */
} Range;

typedef struct _DupContactsData {
	GArray *todo_ranges; /* Range */
	GArray *skip_ranges; /* Range */
	GPtrArray *contacts; /* EContact * */
	EContactCardBox *self;
	GTask *task;
	GError *error;
} DupContactsData;

static void
dup_contacts_data_free (gpointer ptr)
{
	DupContactsData *dcd = ptr;

	if (dcd) {
		g_clear_pointer (&dcd->todo_ranges, g_array_unref);
		g_clear_pointer (&dcd->skip_ranges, g_array_unref);
		g_clear_pointer (&dcd->contacts, g_ptr_array_unref);
		g_clear_object (&dcd->self);
		g_clear_object (&dcd->task);
		g_clear_error (&dcd->error);
		g_free (dcd);
	}
}

static void
e_contact_card_box_finish_range_read (DupContactsData *dcd);

static void
e_contact_card_box_got_items_cb (EContactCardContainer *self,
				 guint range_start,
				 guint range_length,
				 GPtrArray *contacts, /* EContact * */
				 gpointer user_data,
				 const GError *error)
{
	DupContactsData *dcd = user_data;

	if (contacts) {
		Range *range = &g_array_index (dcd->todo_ranges, Range, 0);
		Range *skip_range = NULL;
		guint my_range_length = range->end - range->start + 1;
		guint ii;

		if (dcd->skip_ranges && dcd->skip_ranges->len)
			skip_range = &g_array_index (dcd->skip_ranges, Range, 0);

		for (ii = 0; ii < contacts->len && ii < my_range_length && dcd->self->priv->container->items->len; ii++) {
			guint item_index = range->start + ii;
			EContact *contact = g_ptr_array_index (contacts, ii);

			if (skip_range && item_index >= skip_range->start && item_index <= skip_range->end) {
				if (item_index == skip_range->end) {
					g_array_remove_index (dcd->skip_ranges, 0);
					if (dcd->skip_ranges->len > 0)
						skip_range = &g_array_index (dcd->skip_ranges, Range, 0);
					else
						skip_range = NULL;
				}
			} else {
				g_ptr_array_add (dcd->contacts, g_object_ref (contact));
			}
		}

		g_array_remove_index (dcd->todo_ranges, 0);
	} else if (error) {
		g_warn_if_fail (dcd->error == NULL);

		dcd->error = g_error_copy (error);
	}

	e_contact_card_box_finish_range_read (dcd);
}

static void
e_contact_card_box_finish_range_read (DupContactsData *dcd)
{
	GTask *task = dcd->task;

	if (dcd->todo_ranges->len > 0 && !dcd->error) {
		Range *range = &g_array_index (dcd->todo_ranges, Range, 0);

		e_contact_card_container_schedule_range_read (dcd->self->priv->container, range->start, range->end - range->start + 1,
			g_task_get_cancellable (dcd->task), e_contact_card_box_got_items_cb, dcd);
	} else {
		dcd->task = NULL;

		if (dcd->error) {
			GError *error = dcd->error;

			dcd->error = NULL;

			g_task_return_error (task, error);
		} else {
			g_task_return_pointer (task, g_ptr_array_ref (dcd->contacts), (GDestroyNotify) g_ptr_array_unref);
		}
	}
}

/**
 * e_contact_card_box_peek_contact:
 * @self: an #EContactCardBox
 * @item_index: an item index to return
 *
 * Returns cached contact for the item @item_index. It returns %NULL
 * when the index is out of range or when the item is not cached,
 * aka when it was not needed yet.
 *
 * Use e_contact_card_box_dup_contacts() to get the contact asynchrounously.
 *
 * Free the returned non-NULL contact with g_object_unref(),
 * when no longer needed.
 *
 * Returns: (transfer full) (nullable): a cached #EContact at index @item_index,
 *    or %NULL, when not being cached yet.
 *
 * Since: 3.50
 **/
EContact *
e_contact_card_box_peek_contact (EContactCardBox *self,
				 guint item_index)
{
	EContact *contact;
	ItemState *item_state;

	g_return_val_if_fail (E_IS_CONTACT_CARD_BOX (self), NULL);

	if (item_index >= self->priv->container->items->len)
		return NULL;

	item_state = &g_array_index (self->priv->container->items, ItemState, item_index);

	contact = item_state->item;
	if (contact)
		contact = g_object_ref (contact);

	return contact;
}

/**
 * e_contact_card_box_peek_contacts:
 * @self: an #EContactCardBox
 * @indexes: (element-type guint): an array of item indexes to return
 *
 * Returns cached contacts for the items @indexes. It returns %NULL
 * when any of the indexes is out of range or when any of the items
 * is not cached, aka when it was not needed yet.
 *
 * Use e_contact_card_box_dup_contacts() to get the contacts asynchrounously.
 *
 * Free the returned array with g_ptr_array_unref(),
 * when no longer needed.
 *
 * Returns: (transfer container) (nullable) (element-type: EContact): an array
 *    of cached #EContact-s at provided @indexes, or %NULL, when any of them
 *    not being cached yet.
 *
 * Since: 3.50
 **/
GPtrArray *
e_contact_card_box_peek_contacts (EContactCardBox *self,
				  GPtrArray *indexes)
{
	GPtrArray *contacts = NULL;
	guint ii;

	g_return_val_if_fail (E_IS_CONTACT_CARD_BOX (self), NULL);
	g_return_val_if_fail (indexes, NULL);

	/* First verify it makes sense to allocate the `contacts` array */
	for (ii = 0; ii < indexes->len; ii++) {
		guint item_index = GPOINTER_TO_UINT (g_ptr_array_index (indexes, ii));
		ItemState *item_state;

		if (item_index >= self->priv->container->items->len)
			return NULL;

		item_state = &g_array_index (self->priv->container->items, ItemState, item_index);

		if (!item_state->item)
			return NULL;
	}

	contacts = g_ptr_array_new_full (indexes->len, g_object_unref);

	for (ii = 0; ii < indexes->len; ii++) {
		guint item_index = GPOINTER_TO_UINT (g_ptr_array_index (indexes, ii));
		ItemState *item_state;

		if (item_index >= self->priv->container->items->len)
			continue;

		item_state = &g_array_index (self->priv->container->items, ItemState, item_index);

		if (item_state->item)
			g_ptr_array_add (contacts, g_object_ref (item_state->item));
	}

	return contacts;
}

void
e_contact_card_box_dup_contacts (EContactCardBox *self,
				 GPtrArray *indexes,
				 GCancellable *cancellable,
				 GAsyncReadyCallback cb,
				 gpointer user_data)
{
	DupContactsData *dcd;
	Range range = { G_MAXUINT, G_MAXUINT };
	gboolean range_set = FALSE;
	guint ii;

	g_return_if_fail (E_IS_CONTACT_CARD_BOX (self));
	g_return_if_fail (indexes != NULL);

	g_ptr_array_sort (indexes, e_contact_card_box_sort_indexes_cb);

	dcd = g_new0 (DupContactsData, 1);
	dcd->todo_ranges = g_array_new (FALSE, TRUE, sizeof (Range));
	dcd->contacts = g_ptr_array_new_full (indexes->len, g_object_unref);
	dcd->self = g_object_ref (self);
	dcd->task = g_task_new (self, cancellable, cb, user_data);
	g_task_set_task_data (dcd->task, dcd, dup_contacts_data_free);
	g_task_set_source_tag (dcd->task, e_contact_card_box_dup_contacts);

	for (ii = 0; ii < indexes->len; ii++) {
		guint item_index = GPOINTER_TO_UINT (g_ptr_array_index (indexes, ii));
		ItemState *item_state;

		if (item_index >= self->priv->container->items->len)
			continue;

		item_state = &g_array_index (self->priv->container->items, ItemState, item_index);

		if (item_state->item) {
			g_ptr_array_add (dcd->contacts, g_object_ref (item_state->item));
			continue;
		}

		if (range_set) {
			if (item_index == range.end + 1) {
				range.end = item_index;
			} else {
				g_array_append_val (dcd->todo_ranges, range);
				range.start = item_index;
				range.end = item_index;
			}
		} else {
			range_set = TRUE;
			range.start = item_index;
			range.end = item_index;
		}
	}

	if (range_set)
		g_array_append_val (dcd->todo_ranges, range);

	/* merge close-enough consecutive ranges into one get_times() call */
	for (ii = 1; ii < dcd->todo_ranges->len; ii++) {
		Range *range1 = &g_array_index (dcd->todo_ranges, Range, ii - 1);
		Range *range2 = &g_array_index (dcd->todo_ranges, Range, ii);

		if (range1->end + MAX_CONTACT_READ_OVERHEAD >= range2->start) {
			/* remember which items are not asked by the caller */
			range.start = range1->end + 1;
			range.end = range2->start - 1;

			if (!dcd->skip_ranges)
				dcd->skip_ranges = g_array_new (FALSE, TRUE, sizeof (Range));
			g_array_append_val (dcd->skip_ranges, range);

			range1->end = range2->end;
			g_array_remove_index (dcd->todo_ranges, ii);
			ii--;
		}
	}

	e_contact_card_box_finish_range_read (dcd);
}

/* The order of the contacts may not match the order in the `indexes` array. */
GPtrArray * /* (transfer container) (element-type EContact) */
e_contact_card_box_dup_contacts_finish (EContactCardBox *self,
					GAsyncResult *result,
					GError **error)
{
	g_return_val_if_fail (E_IS_CONTACT_CARD_BOX (self), NULL);
	g_return_val_if_fail (g_task_is_valid (result, self), NULL);
	g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == e_contact_card_box_dup_contacts, NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

void
e_contact_card_box_refresh (EContactCardBox *self)
{
	guint ii;

	g_return_if_fail (E_IS_CONTACT_CARD_BOX (self));

	self->priv->container->stamp++;

	for (ii = 0; ii < self->priv->container->items->len; ii++) {
		ItemState *item_state = &g_array_index (self->priv->container->items, ItemState, ii);

		g_clear_object (&item_state->item);
	}

	e_contact_card_container_update (self->priv->container);
}
