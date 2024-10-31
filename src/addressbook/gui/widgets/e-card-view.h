/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CARD_VIEW_H
#define E_CARD_VIEW_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include "e-alphabet-box.h"
#include "e-contact-card-box.h"

/* Standard GObject macros */
#define E_TYPE_CARD_VIEW \
	(e_card_view_get_type ())
#define E_CARD_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CARD_VIEW, ECardView))
#define E_CARD_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CARD_VIEW, ECardViewClass))
#define E_IS_CARD_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CARD_VIEW))
#define E_IS_CARD_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CARD_VIEW))
#define E_CARD_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CARD_VIEW, ECardViewClass))

G_BEGIN_DECLS

typedef struct _ECardView ECardView;
typedef struct _ECardViewClass ECardViewClass;
typedef struct _ECardViewPrivate ECardViewPrivate;

struct _ECardView {
	GtkEventBox parent;
	ECardViewPrivate *priv;
};

struct _ECardViewClass {
	GtkEventBoxClass parent_class;

	void		(*status_message)	(ECardView *card_view,
						 const gchar *message,
						 gint percentage);
	void		(*double_click)		(ECardView *card_view);

	/* Padding for future expansion */
	gpointer padding[12];
};

GType		e_card_view_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_card_view_new			(void);
EContactCardBox *
		e_card_view_get_card_box	(ECardView *self);
EAlphabetBox *	e_card_view_get_alphabet_box	(ECardView *self);
EBookClient *	e_card_view_get_book_client	(ECardView *self);
void		e_card_view_set_book_client	(ECardView *self,
						 EBookClient *book_client);
const gchar *	e_card_view_get_query		(ECardView *self);
void		e_card_view_set_query		(ECardView *self,
						 const gchar *query);
void		e_card_view_set_sort_fields	(ECardView *self,
						 const EBookClientViewSortFields *sort_fields);
EBookClientViewSortFields *
		e_card_view_dup_sort_fields	(ECardView *self);

G_END_DECLS

#endif /* E_CARD_VIEW_H */
