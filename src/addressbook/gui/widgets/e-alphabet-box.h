/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_ALPHABET_BOX_H
#define E_ALPHABET_BOX_H

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

/* Standard GObject macros */
#define E_TYPE_ALPHABET_BOX \
	(e_alphabet_box_get_type ())
#define E_ALPHABET_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ALPHABET_BOX, EAlphabetBox))
#define E_ALPHABET_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ALPHABET_BOX, EAlphabetBoxClass))
#define E_IS_ALPHABET_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ALPHABET_BOX))
#define E_IS_ALPHABET_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ALPHABET_BOX))
#define E_ALPHABET_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ALPHABET_BOX, EAlphabetBoxClass))

G_BEGIN_DECLS

typedef struct _EAlphabetBox EAlphabetBox;
typedef struct _EAlphabetBoxClass EAlphabetBoxClass;
typedef struct _EAlphabetBoxPrivate EAlphabetBoxPrivate;

struct _EAlphabetBox {
	GtkBin parent;
	EAlphabetBoxPrivate *priv;
};

struct _EAlphabetBoxClass {
	GtkBinClass parent_class;

	void		(* clicked)	(EAlphabetBox *box,
					 guint index);

	/* Padding for future expansion */
	gpointer padding[12];
};

GType		e_alphabet_box_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_alphabet_box_new		(void);
void		e_alphabet_box_take_indices	(EAlphabetBox *self,
						 EBookIndices *indices);
const EBookIndices *
		e_alphabet_box_get_indices	(EAlphabetBox *self);

G_END_DECLS

#endif /* E_ALPHABET_BOX_H */
