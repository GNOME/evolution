/* e-editor-spell-dialog.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SPELL_DIALOG_H
#define E_SPELL_DIALOG_H

#include <gtk/gtk.h>
#include <e-util/e-spell-dictionary.h>

/* Standard GObject macros */
#define E_TYPE_SPELL_DIALOG \
	(e_spell_dialog_get_type ())
#define E_SPELL_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SPELL_DIALOG, ESpellDialog))
#define E_SPELL_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SPELL_DIALOG, ESpellDialogClass))
#define E_IS_SPELL_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SPELL_DIALOG))
#define E_IS_SPELL_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SPELL_DIALOG))
#define E_SPELL_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SPELL_DIALOG, ESpellDialogClass))

G_BEGIN_DECLS

typedef struct _ESpellDialog ESpellDialog;
typedef struct _ESpellDialogClass ESpellDialogClass;
typedef struct _ESpellDialogPrivate ESpellDialogPrivate;

struct _ESpellDialog {
	GtkDialog parent;
	ESpellDialogPrivate *priv;
};

struct _ESpellDialogClass {
	GtkDialogClass parent_class;
};

GType		e_spell_dialog_get_type		(void);
GtkWidget *	e_spell_dialog_new		(GtkWindow *parent);
void		e_spell_dialog_close		(ESpellDialog *dialog);
const gchar *	e_spell_dialog_get_word		(ESpellDialog *dialog);
void		e_spell_dialog_set_word		(ESpellDialog *dialog,
						 const gchar *word);
void		e_spell_dialog_next_word	(ESpellDialog *dialog);
void		e_spell_dialog_prev_word	(ESpellDialog *dialog);
GList *		e_spell_dialog_get_dictionaries
						(ESpellDialog *dialog);
void		e_spell_dialog_set_dictionaries
						(ESpellDialog *dialog,
						 GList *dictionaries);
ESpellDictionary *
		e_spell_dialog_get_active_dictionary
						(ESpellDialog *dialog);
gchar *		e_spell_dialog_get_active_suggestion
						(ESpellDialog *dialog);

G_END_DECLS

#endif /* E_SPELL_DIALOG_H */
