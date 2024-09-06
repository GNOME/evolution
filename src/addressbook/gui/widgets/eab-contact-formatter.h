/*
 * eab-contact-formatter.h
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

#ifndef EAB_CONTACT_FORMATTER_H
#define EAB_CONTACT_FORMATTER_H

#include <camel/camel.h>
#include <libebook/libebook.h>

#include <addressbook/gui/widgets/eab-contact-display.h>

/* Standard GObject macros */
#define EAB_TYPE_CONTACT_FORMATTER \
	(eab_contact_formatter_get_type ())
#define EAB_CONTACT_FORMATTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EAB_TYPE_CONTACT_FORMATTER, EABContactFormatter))
#define EAB_CONTACT_FORMATTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EAB_TYPE_CONTACT_FORMATTER, EABContactFormatterClass))
#define EAB_IS_CONTACT_FORMATTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EAB_TYPE_CONTACT_FORMATTER))
#define EAB_IS_CONTACT_FORMATTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EAB_TYPE_CONTACT_FORMATTER))
#define EAB_CONTACT_FORMATTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EAB_TYPE_CONTACT_FORMATTER, EABContactFormatterClass))

G_BEGIN_DECLS

typedef struct _EABContactFormatter EABContactFormatter;
typedef struct _EABContactFormatterClass EABContactFormatterClass;
typedef struct _EABContactFormatterPrivate EABContactFormatterPrivate;

struct _EABContactFormatter {
	GObject parent;
	EABContactFormatterPrivate *priv;
};

struct _EABContactFormatterClass {
	GObjectClass parent_class;
};

GType		eab_contact_formatter_get_type	(void) G_GNUC_CONST;
EABContactFormatter *
		eab_contact_formatter_new	(void);
gboolean	eab_contact_formatter_get_render_maps
						(EABContactFormatter *formatter);
void		eab_contact_formatter_set_render_maps
						(EABContactFormatter *formatter,
						 gboolean render_maps);
EABContactDisplayMode
		eab_contact_formatter_get_display_mode
						(EABContactFormatter *formatter);
void		eab_contact_formatter_set_display_mode
						(EABContactFormatter *formatter,
						 EABContactDisplayMode mode);
void		eab_contact_formatter_format_contact
						(EABContactFormatter *formatter,
						 EContact *contact,
						 GString *output_buffer);

G_END_DECLS

#endif /* EAB_CONTACT_FORMATTER_H */

