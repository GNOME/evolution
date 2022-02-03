/*
 * e-spell-text-view.h
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

#ifndef E_SPELL_TEXT_VIEW_H
#define E_SPELL_TEXT_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean	e_spell_text_view_is_supported	(void);
void		e_spell_text_view_attach	(GtkTextView *text_view);
gboolean	e_spell_text_view_get_enabled	(GtkTextView *text_view);
void		e_spell_text_view_set_enabled	(GtkTextView *text_view,
						 gboolean enabled);
void		e_spell_text_view_set_languages	(GtkTextView *text_view,
						 const gchar **languages);

G_END_DECLS

#endif /* E_SPELL_TEXT_VIEW_H */
