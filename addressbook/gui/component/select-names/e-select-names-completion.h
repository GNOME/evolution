/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-select-names-completion.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifndef E_SELECT_NAMES_COMPLETION_H
#define E_SELECT_NAMES_COMPLETION_H

#include <gal/e-text/e-completion.h>
#include <addressbook/backend/ebook/e-book.h>
#include "e-select-names-model.h"

BEGIN_GNOME_DECLS

#define E_SELECT_NAMES_COMPLETION_TYPE        (e_select_names_completion_get_type ())
#define E_SELECT_NAMES_COMPLETION(o)          (GTK_CHECK_CAST ((o), E_SELECT_NAMES_COMPLETION_TYPE, ESelectNamesCompletion))
#define E_SELECT_NAMES_COMPLETION_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), E_SELECT_NAMES_COMPLETION_TYPE, ESelectNamesCompletionClass))
#define E_IS_SELECT_NAMES_COMPLETION(o)       (GTK_CHECK_TYPE ((o), E_SELECT_NAMES_COMPLETION_TYPE))
#define E_IS_SELECT_NAMES_COMPLETION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SELECT_NAMES_COMPLETION_TYPE))

typedef struct _ESelectNamesCompletion ESelectNamesCompletion;
typedef struct _ESelectNamesCompletionClass ESelectNamesCompletionClass;
struct _ESelectNamesCompletionPrivate;

struct _ESelectNamesCompletion {
	ECompletion parent;

	struct _ESelectNamesCompletionPrivate *priv;
};

struct _ESelectNamesCompletionClass {
	ECompletionClass parent_class;

};

GtkType e_select_names_completion_get_type (void);

ECompletion *e_select_names_completion_new (EBook *, ESelectNamesModel *);

END_GNOME_DECLS

#endif /* E_SELECT_NAMES_COMPLETION_H */

