/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_SECTION_BOX_H
#define E_SECTION_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_SECTION_BOX (e_section_box_get_type ())
G_DECLARE_FINAL_TYPE (ESectionBox, e_section_box, E, SECTION_BOX, GtkBox)

GtkWidget *	e_section_box_new	(void);
void		e_section_box_add	(ESectionBox *self,
					 GtkWidget *widget); /* transfer full */
void		e_section_box_connect_parent_container
					(ESectionBox *self,
					 GtkWidget *container); /* transfer none */
G_END_DECLS

#endif /* E_SECTION_BOX_H */
