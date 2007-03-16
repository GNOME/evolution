/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-contact-quick-add.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#ifndef __E_CONTACT_QUICK_ADD_H__
#define __E_CONTACT_QUICK_ADD_H__

#include <libebook/e-contact.h>

typedef void (*EContactQuickAddCallback) (EContact *new_contact, gpointer closure);

void e_contact_quick_add (const gchar *name, const gchar *email, 
			  EContactQuickAddCallback cb, gpointer closure);

void e_contact_quick_add_free_form (const gchar *text, EContactQuickAddCallback cb, gpointer closure);

#endif /* __E_CONTACT_QUICK_ADD_H__ */

