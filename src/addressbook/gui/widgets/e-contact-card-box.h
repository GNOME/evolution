/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CONTACT_CARD_BOX_H
#define E_CONTACT_CARD_BOX_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

/* Standard GObject macros */
#define E_TYPE_CONTACT_CARD_BOX \
	(e_contact_card_box_get_type ())
#define E_CONTACT_CARD_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_CARD_BOX, EContactCardBox))
#define E_CONTACT_CARD_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_CARD_BOX, EContactCardBoxClass))
#define E_IS_CONTACT_CARD_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_CARD_BOX))
#define E_IS_CONTACT_CARD_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACT_CARD_BOX))
#define E_CONTACT_CARD_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_CARD_BOX, EContactCardBoxClass))

G_BEGIN_DECLS

enum EContactCardBoxDndType {
	E_CONTACT_CARD_BOX_DND_TYPE_SOURCE_VCARD_LIST,
	E_CONTACT_CARD_BOX_DND_TYPE_VCARD_LIST
};

/**
 * EContactCardBoxGetItemsFunc:
 * @source_data: source data passed to e_contact_card_box_new()
 * @range_start: index of the item to look for, counting from zero
 * @range_length: how many items to retrieve
 * @cancellable: a #GCancellable, or %NULL, for the asynchronous call
 * @callback: (scope async): a callback to call, when the call is finished
 * @callback_data: (closure callback) user data for the @callback
 *
 * A function prototype to get range of items asynchronously. The pair
 * function declared as #EContactCardBoxGetItemsFinishFunc will be called
 * from the @callback.
 *
 * Since: 3.50
 **/
typedef void (* EContactCardBoxGetItemsFunc)	(gpointer source_data,
						 guint range_start,
						 guint range_length,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer callback_data);
/**
 * EContactCardBoxGetItemsFinishFunc:
 * @source_data: source data passed to e_contact_card_box_new()
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * A function prototype to finish an #EContactCardBoxGetItemsFunc call.
 *
 * Returns: (transfer container) (element-type EContact): retrieved items, or %NULL on error
 *
 * Since: 3.50
 **/
typedef GPtrArray * (* EContactCardBoxGetItemsFinishFunc) /* EContact */
						(gpointer source_data,
						 GAsyncResult *result,
						 GError **error);

typedef struct _EContactCardBox EContactCardBox;
typedef struct _EContactCardBoxClass EContactCardBoxClass;
typedef struct _EContactCardBoxPrivate EContactCardBoxPrivate;

struct _EContactCardBox {
	GtkScrolledWindow parent;
	EContactCardBoxPrivate *priv;
};

struct _EContactCardBoxClass {
	GtkScrolledWindowClass parent_class;

	void	(* child_activated)		(EContactCardBox *box,
						 guint child_index);
	void	(* selected_children_changed)	(EContactCardBox *box);
	void	(* activate_cursor_child)	(EContactCardBox *box);
	void	(* toggle_cursor_child)		(EContactCardBox *box);
	gboolean(* move_cursor)			(EContactCardBox *box,
						 GtkMovementStep step,
						 gint count);
	void	(* select_all)			(EContactCardBox *box);
	void	(* unselect_all)		(EContactCardBox *box);
	gboolean(* card_event)			(EContactCardBox *box,
						 guint child_index,
						 GdkEvent *event);
	gboolean(* card_popup_menu)		(EContactCardBox *box,
						 guint child_index);
	void	(* card_drag_begin)		(EContactCardBox *box,
						 GdkDragContext *context);
	void	(* card_drag_data_get)		(EContactCardBox *box,
						 GdkDragContext *context,
						 GtkSelectionData *selection_data,
						 guint info,
						 guint time);
	void	(* card_drag_end)		(EContactCardBox *box,
						 GdkDragContext *context);
	void	(* count_changed)		(EContactCardBox *box);

	/* Padding for future expansion */
	gpointer padding[12];
};

GType		e_contact_card_box_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_contact_card_box_new		(EContactCardBoxGetItemsFunc get_items_func,
						 EContactCardBoxGetItemsFinishFunc get_items_finish_func,
						 gpointer get_items_source_data,
						 GDestroyNotify get_items_source_data_destroy);
guint		e_contact_card_box_get_n_items	(EContactCardBox *self);
void		e_contact_card_box_set_n_items	(EContactCardBox *self,
						 guint n_items);
guint		e_contact_card_box_get_focused_index
						(EContactCardBox *self);
void		e_contact_card_box_set_focused_index
						(EContactCardBox *self,
						 guint item_index);
void		e_contact_card_box_scroll_to_index
						(EContactCardBox *self,
						 guint item_index,
						 gboolean can_in_middle);
gboolean	e_contact_card_box_get_selected	(EContactCardBox *self,
						 guint item_index);
void		e_contact_card_box_set_selected	(EContactCardBox *self,
						 guint item_index,
						 gboolean selected);
void		e_contact_card_box_set_selected_all
						(EContactCardBox *self,
						 gboolean selected);
guint		e_contact_card_box_get_n_selected
						(EContactCardBox *self);
GPtrArray *	e_contact_card_box_dup_selected_indexes
						(EContactCardBox *self); /* guint */
EContact *	e_contact_card_box_peek_contact	(EContactCardBox *self,
						 guint item_index);
GPtrArray *	e_contact_card_box_peek_contacts(EContactCardBox *self, /* EContact * */
						 GPtrArray *indexes);
void		e_contact_card_box_dup_contacts	(EContactCardBox *self,
						 GPtrArray *indexes, /* guint */
						 GCancellable *cancellable,
						 GAsyncReadyCallback cb,
						 gpointer user_data);
GPtrArray *	e_contact_card_box_dup_contacts_finish /* EContact * */
						(EContactCardBox *self,
						 GAsyncResult *result,
						 GError **error);
void		e_contact_card_box_refresh	(EContactCardBox *self);

G_END_DECLS

#endif /* E_CONTACT_CARD_BOX_H */
