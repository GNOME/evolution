/*
 * e-spell-entry.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SPELL_ENTRY_H
#define E_SPELL_ENTRY_H

#include <gtk/gtk.h>

#include <e-util/e-spell-checker.h>

/* Standard GObject macros */
#define E_TYPE_SPELL_ENTRY \
	(e_spell_entry_get_type ())
#define E_SPELL_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SPELL_ENTRY, ESpellEntry))
#define E_SPELL_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SPELL_ENTRY, ESpellEntryClass))
#define E_IS_SPELL_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SPELL_ENTRY))
#define E_IS_SPELL_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SPELL_ENTRY))
#define E_SPELL_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SPELL_ENTRY, ESpellEntryClass))

G_BEGIN_DECLS

typedef struct _ESpellEntry ESpellEntry;
typedef struct _ESpellEntryClass ESpellEntryClass;
typedef struct _ESpellEntryPrivate ESpellEntryPrivate;

struct _ESpellEntry {
	GtkEntry parent;
	ESpellEntryPrivate *priv;
};

struct _ESpellEntryClass {
	GtkEntryClass parent_class;
};

GType		e_spell_entry_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_spell_entry_new		(void);
gboolean	e_spell_entry_get_checking_enabled
						(ESpellEntry *spell_entry);
void		e_spell_entry_set_checking_enabled
						(ESpellEntry *spell_entry,
						 gboolean enable_checking);
ESpellChecker *	e_spell_entry_get_spell_checker	(ESpellEntry *spell_entry);
void		e_spell_entry_set_spell_checker	(ESpellEntry *spell_entry,
						 ESpellChecker *spell_checker);

G_END_DECLS

#endif /* E_SPELL_ENTRY_H */
