/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memos.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 * Authors: Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 */

#ifndef _E_MEMOS_H_
#define _E_MEMOS_H_

#include <bonobo/bonobo-ui-component.h>
#include <gtk/gtktable.h>
#include <libedataserver/e-source.h>
#include <libecal/e-cal.h>
#include "e-memo-table.h"

#define E_TYPE_MEMOS            (e_memos_get_type ())
#define E_MEMOS(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_MEMOS, EMemos))
#define E_MEMOS_CLASS(klass)    (GTK_CHECK_CAST_CLASS ((klass), E_TYPE_MEMOS, \
				 EMemosClass))
#define E_IS_MEMOS(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_MEMOS))
#define E_IS_MEMOS_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_MEMOS))

typedef struct _EMemos EMemos;
typedef struct _EMemosClass EMemosClass;
typedef struct _EMemosPrivate EMemosPrivate;

struct _EMemos {
	GtkTable table;

	/* Private data */
	EMemosPrivate *priv;
};

struct _EMemosClass {
	GtkTableClass parent_class;

	/* Notification signals */
	void (* selection_changed) (EMemos *memos, int n_selected);
        void (* source_added)      (EMemos *memos, ESource *source);
        void (* source_removed)    (EMemos *memos, ESource *source);
};


GtkType    e_memos_get_type        (void);
GtkWidget *e_memos_construct       (EMemos *memos);

GtkWidget *e_memos_new             (void);

void       e_memos_set_ui_component  (EMemos            *memos,
				      BonoboUIComponent *ui_component);

gboolean   e_memos_add_memo_source (EMemos *memos, ESource *source);
gboolean   e_memos_remove_memo_source (EMemos *memos, ESource *source);
gboolean   e_memos_set_default_source (EMemos *memos, ESource *source);
ECal      *e_memos_get_default_client    (EMemos *memos);

void       e_memos_open_memo         (EMemos		*memos);
void       e_memos_new_memo          (EMemos            *memos);
void       e_memos_complete_selected (EMemos            *memos);
void       e_memos_delete_selected   (EMemos            *memos);


void e_memos_setup_view_menus (EMemos *memos, BonoboUIComponent *uic);
void e_memos_discard_view_menus (EMemos *memos);

EMemoTable *e_memos_get_calendar_table (EMemos *memos);

#endif /* _E_MEMOS_H_ */
