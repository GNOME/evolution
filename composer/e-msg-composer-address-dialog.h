/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-address-dialog.h
 *
 * Copyright (C) 1999  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */
#ifndef __E_MSG_COMPOSER_ADDRESS_DIALOG_H__
#define __E_MSG_COMPOSER_ADDRESS_DIALOG_H__

#include <gnome.h>
#include <glade/glade-xml.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define E_TYPE_MSG_COMPOSER_ADDRESS_DIALOG			(e_msg_composer_address_dialog_get_type ())
#define E_MSG_COMPOSER_ADDRESS_DIALOG(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_MSG_COMPOSER_ADDRESS_DIALOG, EMsgComposerAddressDialog))
#define E_MSG_COMPOSER_ADDRESS_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER_ADDRESS_DIALOG, EMsgComposerAddressDialogClass))
#define E_IS_MSG_COMPOSER_ADDRESS_DIALOG(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_MSG_COMPOSER_ADDRESS_DIALOG))
#define E_IS_MSG_COMPOSER_ADDRESS_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER_ADDRESS_DIALOG))


typedef struct _EMsgComposerAddressDialog       EMsgComposerAddressDialog;
typedef struct _EMsgComposerAddressDialogClass  EMsgComposerAddressDialogClass;

struct _EMsgComposerAddressDialog {
	GnomeDialog parent;

	GladeXML *gui;

	gchar *cut_buffer;
};

struct _EMsgComposerAddressDialogClass {
	GnomeDialogClass parent_class;

	void (* apply) (EMsgComposerAddressDialog *dialog);
};


GtkType e_msg_composer_address_dialog_get_type (void);
GtkWidget *e_msg_composer_address_dialog_new (void);
void e_msg_composer_address_dialog_construct (EMsgComposerAddressDialog *dialog);
void e_msg_composer_address_dialog_set_to_list (EMsgComposerAddressDialog *dialog, GList *to_list);
void e_msg_composer_address_dialog_set_cc_list (EMsgComposerAddressDialog *dialog, GList *cc_list);
void e_msg_composer_address_dialog_set_bcc_list (EMsgComposerAddressDialog *dialog, GList *bcc_list);
GList *e_msg_composer_address_dialog_get_to_list (EMsgComposerAddressDialog *dialog);
GList *e_msg_composer_address_dialog_get_cc_list (EMsgComposerAddressDialog *dialog);
GList *e_msg_composer_address_dialog_get_bcc_list (EMsgComposerAddressDialog *dialog);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_MSG_COMPOSER_ADDRESS_DIALOG_H__ */
