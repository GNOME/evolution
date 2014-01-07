/*
 * e-autocomplete-selector.h
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

#ifndef E_AUTOCOMPLETE_SELECTOR_H
#define E_AUTOCOMPLETE_SELECTOR_H

#include <e-util/e-source-selector.h>

/* Standard GObject macros */
#define E_TYPE_AUTOCOMPLETE_SELECTOR \
	(e_autocomplete_selector_get_type ())
#define E_AUTOCOMPLETE_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_AUTOCOMPLETE_SELECTOR, EAutocompleteSelector))
#define E_AUTOCOMPLETE_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_AUTOCOMPLETE_SELECTOR, EAutocompleteSelectorClass))
#define E_IS_AUTOCOMPLETE_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_AUTOCOMPLETE_SELECTOR))
#define E_IS_AUTOCOMPLETE_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_AUTOCOMPLETE_SELECTOR))
#define E_AUTOCOMPLETE_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_AUTOCOMPLETE_SELECTOR, EAutocompleteSelectorClass))

G_BEGIN_DECLS

typedef struct _EAutocompleteSelector EAutocompleteSelector;
typedef struct _EAutocompleteSelectorClass EAutocompleteSelectorClass;
typedef struct _EAutocompleteSelectorPrivate EAutocompleteSelectorPrivate;

struct _EAutocompleteSelector {
	ESourceSelector parent;
	EAutocompleteSelectorPrivate *priv;
};

struct _EAutocompleteSelectorClass {
	ESourceSelectorClass parent_class;
};

GType		e_autocomplete_selector_get_type
						(void) G_GNUC_CONST;
GtkWidget *	e_autocomplete_selector_new	(ESourceRegistry *registry);

G_END_DECLS

#endif /* E_AUTOCOMPLETE_SELECTOR_H */
