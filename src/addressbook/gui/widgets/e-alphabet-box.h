/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_ALPHABET_BOX_H
#define E_ALPHABET_BOX_H

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

G_BEGIN_DECLS

#define E_TYPE_ALPHABET_BOX e_alphabet_box_get_type ()
G_DECLARE_FINAL_TYPE (EAlphabetBox, e_alphabet_box, E, ALPHABET_BOX, GtkListBox)

GtkWidget *	e_alphabet_box_new		(void);
void		e_alphabet_box_take_indices	(EAlphabetBox *self,
						 EBookIndices *indices);
const EBookIndices *
		e_alphabet_box_get_indices	(EAlphabetBox *self);

G_END_DECLS

#endif /* E_ALPHABET_BOX_H */
