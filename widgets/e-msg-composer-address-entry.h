/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-address-entry.h
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

#ifndef __E_MSG_COMPOSER_ADDRESS_ENTRY_H__
#define __E_MSG_COMPOSER_ADDRESS_ENTRY_H__

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_MSG_COMPOSER_ADDRESS_ENTRY			(e_msg_composer_address_entry_get_type ())
#define E_MSG_COMPOSER_ADDRESS_ENTRY(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_MSG_COMPOSER_ADDRESS_ENTRY, EMsgComposerAddressEntry))
#define E_MSG_COMPOSER_ADDRESS_ENTRY_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER_ADDRESS_ENTRY, EMsgComposerAddressEntryClass))
#define E_IS_MSG_COMPOSER_ADDRESS_ENTRY(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_MSG_COMPOSER_ADDRESS_ENTRY))
#define E_IS_MSG_COMPOSER_ADDRESS_ENTRY_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER_ADDRESS_ENTRY))


typedef struct _EMsgComposerAddressEntry       EMsgComposerAddressEntry;
typedef struct _EMsgComposerAddressEntryClass  EMsgComposerAddressEntryClass;

struct _EMsgComposerAddressEntry {
	GtkEntry parent;
};

struct _EMsgComposerAddressEntryClass {
	GtkEntryClass parent_class;
};


GtkType    e_msg_composer_address_entry_get_type      (void);
GtkWidget *e_msg_composer_address_entry_new           (void);
GList     *e_msg_composer_address_entry_get_addresses (EMsgComposerAddressEntry *entry);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_MSG_COMPOSER_ADDRESS_ENTRY_H__ */
