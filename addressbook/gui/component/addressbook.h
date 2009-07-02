/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libebook/e-book.h>

guint      addressbook_load                 (EBook *book, EBookCallback cb, gpointer closure);
void       addressbook_load_cancel          (guint id);
void       addressbook_load_default_book    (EBookCallback open_response, gpointer closure);

#endif /* __ADDRESSBOOK_H__ */
