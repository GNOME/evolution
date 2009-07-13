/*
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _EAB_MODEL_H_
#define _EAB_MODEL_H_

#include <glib.h>
#include <glib-object.h>
#include <libebook/e-book.h>
#include <libebook/e-book-view.h>

#define EAB_TYPE_MODEL                  (eab_model_get_type ())
#define EAB_MODEL(o)                    (G_TYPE_CHECK_INSTANCE_CAST ((o), EAB_TYPE_MODEL, EABModel))
#define EAB_MODEL_CLASS(k)              (G_TYPE_CHECK_CLASS_CAST((k), EAB_TYPE_MODEL, EABModelClass))
#define E_IS_ADDRESSBOOK_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAB_TYPE_MODEL))
#define E_IS_ADDRESSBOOK_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EAB_TYPE_MODEL))

typedef struct _EABModel EABModel;
typedef struct _EABModelClass EABModelClass;

struct _EABModel {
	GObject parent;

	/* item specific fields */
	EBook *book;
	EBookQuery *query;
	EBookView *book_view;

	gint book_view_idle_id;

	EContact **data;
	gint data_count;
	gint allocated_count;

	gint create_contact_id, remove_contact_id, modify_contact_id;
	gint status_message_id, writable_status_id, sequence_complete_id;
	gint backend_died_id;

	guint search_in_progress : 1;
	guint editable : 1;
	guint editable_set : 1;
	guint first_get_view : 1;
};

struct _EABModelClass {
	GObjectClass parent_class;

	/*
	 * Signals
	 */
	void (*writable_status)    (EABModel *model, gboolean writable);
	void (*search_started)     (EABModel *model);
	void (*search_result)      (EABModel *model, EBookViewStatus status);
	void (*status_message)     (EABModel *model, const gchar *message);
	void (*folder_bar_message) (EABModel *model, const gchar *message);
	void (*contact_added)      (EABModel *model, gint index, gint count);
	void (*contacts_removed)    (EABModel *model, gpointer id_list);
	void (*contact_changed)    (EABModel *model, gint index);
	void (*model_changed)      (EABModel *model);
	void (*stop_state_changed) (EABModel *model);
	void (*backend_died)       (EABModel *model);
};

GType              eab_model_get_type                  (void);
EABModel          *eab_model_new                       (void);

/* Returns object with ref count of 1. */
EContact          *eab_model_get_contact               (EABModel *model,
							gint                row);
EBook             *eab_model_get_ebook                 (EABModel *model);

void               eab_model_stop                      (EABModel *model);
gboolean           eab_model_can_stop                  (EABModel *model);

void               eab_model_force_folder_bar_message  (EABModel *model);

gint                eab_model_contact_count             (EABModel *model);
const EContact    *eab_model_contact_at                (EABModel *model,
							gint                index);
gboolean           eab_model_editable                  (EABModel *model);

#endif /* _EAB_MODEL_H_ */
