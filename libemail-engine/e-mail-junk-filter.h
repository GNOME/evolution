/*
 * e-mail-junk-filter.h
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

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef E_MAIL_JUNK_FILTER_H
#define E_MAIL_JUNK_FILTER_H

#include <gtk/gtk.h>
#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_JUNK_FILTER \
	(e_mail_junk_filter_get_type ())
#define E_MAIL_JUNK_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_JUNK_FILTER, EMailJunkFilter))
#define E_MAIL_JUNK_FILTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_JUNK_FILTER, EMailJunkFilterClass))
#define E_IS_MAIL_JUNK_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_JUNK_FILTER))
#define E_IS_MAIL_JUNK_FILTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_JUNK_FILTER))
#define E_MAIL_JUNK_FILTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_JUNK_FILTER, EMailJunkFilterClass))

G_BEGIN_DECLS

typedef struct _EMailJunkFilter EMailJunkFilter;
typedef struct _EMailJunkFilterClass EMailJunkFilterClass;
typedef struct _EMailJunkFilterPrivate EMailJunkFilterPrivate;

struct _EMailJunkFilter {
	EExtension parent;
	EMailJunkFilterPrivate *priv;
};

struct _EMailJunkFilterClass {
	EExtensionClass parent_class;

	const gchar *filter_name;
	const gchar *display_name;

	gboolean	(*available)		(EMailJunkFilter *junk_filter);
	GtkWidget *	(*new_config_widget)	(EMailJunkFilter *junk_filter);
};

GType		e_mail_junk_filter_get_type	(void) G_GNUC_CONST;
gboolean	e_mail_junk_filter_available	(EMailJunkFilter *junk_filter);
GtkWidget *	e_mail_junk_filter_new_config_widget
						(EMailJunkFilter *junk_filter);
gint		e_mail_junk_filter_compare	(EMailJunkFilter *junk_filter_a,
						 EMailJunkFilter *junk_filter_b);

G_END_DECLS

#endif /* E_MAIL_JUNK_FILTER_H */
