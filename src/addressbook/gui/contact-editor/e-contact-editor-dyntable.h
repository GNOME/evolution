/*
 * e-contact-editor-dyntable.h
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_CONTACT_EDITOR_DYNTABLE_H_
#define E_CONTACT_EDITOR_DYNTABLE_H_

#include <gtk/gtk.h>
#include <string.h>

#define E_TYPE_CONTACT_EDITOR_DYNTABLE \
	(e_contact_editor_dyntable_get_type ())
#define E_CONTACT_EDITOR_DYNTABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_EDITOR_DYNTABLE, EContactEditorDynTable))
#define E_CONTACT_EDITOR_DYNTABLE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST \
	((klass), E_TYPE_CONTACT_EDITOR_DYNTABLE, EContactEditorDynTableClass))
#define E_IS_CONTACT_EDITOR_DYNTABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_EDITOR_DYNTABLE))
#define E_IS_CONTACT_EDITOR_DYNTABLE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_CONTACT_EDITOR_DYNTABLE))
#define E_CONTACT_EDITOR_DYNTABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	(obj,E_TYPE_CONTACT_EDITOR_DYNTABLE, EContactEditorDynTableClass))

G_BEGIN_DECLS

typedef enum {
	DYNTABLE_STORE_COLUMN_SORTORDER,
	DYNTABLE_STORE_COLUMN_SELECTED_ITEM,
	DYNTABLE_STORE_COLUMN_SELECTED_TEXT, /* set only if the dyntable has combo with entry */
	DYNTABLE_STORE_COLUMN_ENTRY_STRING,
	DYNTABLE_STORE_COLUMN_NUM_COLUMNS
} EContactEditorDynTableStoreColumns;

typedef enum {
	DYNTABLE_COMBO_COLUMN_TEXT,
	DYNTABLE_COMBO_COLUMN_SENSITIVE,
	DYNTABLE_COBMO_COLUMN_NUM_COLUMNS
} EContactEditorDynTableComboColumns;

typedef struct _EContactEditorDynTable EContactEditorDynTable;
typedef struct _EContactEditorDynTableClass EContactEditorDynTableClass;
typedef struct _EContactEditorDynTablePrivate EContactEditorDynTablePrivate;

struct _EContactEditorDynTableClass {
	GtkGridClass parent_class;

	/* Signals */
	void 		(*changed)		(EContactEditorDynTable* dyntable);
	void 		(*activate)		(EContactEditorDynTable* dyntable);
	void		(*row_added)		(EContactEditorDynTable* dyntable);

	/* virtual */

	/* defaults to GtkEntiy */
	GtkWidget*	(*widget_create)	(EContactEditorDynTable *dyntable);

	/* defaults to e_str_is_empty(txt) */
	gboolean	(*widget_is_empty)	(EContactEditorDynTable *dyntable,
						 GtkWidget *w);

	/* defaults to entry_set_text("") */
	void		(*widget_clear)		(EContactEditorDynTable *dyntable,
						 GtkWidget *w);

	/* default impl gtk_entry_set_text
	 * other widgets may need to "parse" value before usage.
	 */
	void 		(*widget_fill)		(EContactEditorDynTable *dyntable,
						 GtkWidget *w,
						 const gchar *value);

	/* default impl returns gtk_entry_get_text
	 * other widget may require some kind of "encoding"
	 */
	const gchar* 	(*widget_extract)	(EContactEditorDynTable *dyntable,
						 GtkWidget *w);

};

struct _EContactEditorDynTable {
	GtkGrid parent;

	EContactEditorDynTablePrivate* priv;
};

GtkWidget* 	e_contact_editor_dyntable_new		(void);
GType 		e_contact_editor_dyntable_get_type	(void) G_GNUC_CONST;
void 		e_contact_editor_dyntable_set_show_min	(EContactEditorDynTable *dyntable,
     		                                      	 guint number_of_entries);
void 		e_contact_editor_dyntable_set_show_max	(EContactEditorDynTable *dyntable,
     		                                      	 guint number_of_entries);
void		e_contact_editor_dyntable_set_num_columns (EContactEditorDynTable *dyntable,
    		                                           guint number_of_columns,
    		                                           gboolean justified);
void		e_contact_editor_dyntable_set_max_entries (EContactEditorDynTable *dyntable,
    		                                           guint max);
void		e_contact_editor_dyntable_set_combo_with_entry
							(EContactEditorDynTable *self,
							 gboolean value);
gboolean	e_contact_editor_dyntable_get_combo_with_entry
							(EContactEditorDynTable *self);

GtkListStore* 	e_contact_editor_dyntable_get_combo_store (EContactEditorDynTable* dyntable);
void		e_contact_editor_dyntable_set_combo_defaults (EContactEditorDynTable* dyntable,
    		                                              const gint *defaults,
    		                                              size_t defaults_n);
void 		e_contact_editor_dyntable_clear_data	(EContactEditorDynTable *dyntable);
void 		e_contact_editor_dyntable_fill_in_data	(EContactEditorDynTable *dyntable);
GtkListStore* 	e_contact_editor_dyntable_extract_data	(EContactEditorDynTable *dyntable);

G_END_DECLS

#endif /* E_CONTACT_EDITOR_DYNTABLE_H_ */
