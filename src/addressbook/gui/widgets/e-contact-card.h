/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CONTACT_CARD_H
#define E_CONTACT_CARD_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

/* Standard GObject macros */
#define E_TYPE_CONTACT_CARD \
	(e_contact_card_get_type ())
#define E_CONTACT_CARD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_CARD, EContactCard))
#define E_CONTACT_CARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_CARD, EContactCardClass))
#define E_IS_CONTACT_CARD(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_CARD))
#define E_IS_CONTACT_CARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACT_CARD))
#define E_CONTACT_CARD_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_CARD, EContactCardClass))

G_BEGIN_DECLS

typedef struct _EContactCard EContactCard;
typedef struct _EContactCardClass EContactCardClass;
typedef struct _EContactCardPrivate EContactCardPrivate;

struct _EContactCard {
	GtkEventBox parent;
	EContactCardPrivate *priv;
};

struct _EContactCardClass {
	GtkEventBoxClass parent_class;

	/* Padding for future expansion */
	gpointer padding[12];
};

GType		e_contact_card_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_contact_card_new		(GtkCssProvider *css_provider);
EContact *	e_contact_card_get_contact	(EContactCard *self);
void		e_contact_card_set_contact	(EContactCard *self,
						 EContact *contact);
void		e_contact_card_update		(EContactCard *self);

G_END_DECLS

#endif /* E_CONTACT_CARD_H */
