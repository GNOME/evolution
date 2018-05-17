/*
 * e-mail-junk-filter.c
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

#include "e-mail-junk-filter.h"

#include <libemail-engine/e-mail-session.h>

G_DEFINE_ABSTRACT_TYPE (
	EMailJunkFilter,
	e_mail_junk_filter,
	E_TYPE_EXTENSION)

static void
e_mail_junk_filter_class_init (EMailJunkFilterClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_SESSION;
}

static void
e_mail_junk_filter_init (EMailJunkFilter *junk_filter)
{
}

gboolean
e_mail_junk_filter_available (EMailJunkFilter *junk_filter)
{
	EMailJunkFilterClass *class;

	g_return_val_if_fail (E_IS_MAIL_JUNK_FILTER (junk_filter), FALSE);

	class = E_MAIL_JUNK_FILTER_GET_CLASS (junk_filter);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->available != NULL, FALSE);

	return class->available (junk_filter);
}

GtkWidget *
e_mail_junk_filter_new_config_widget (EMailJunkFilter *junk_filter)
{
	EMailJunkFilterClass *class;
	GtkWidget *widget = NULL;

	g_return_val_if_fail (E_IS_MAIL_JUNK_FILTER (junk_filter), NULL);

	class = E_MAIL_JUNK_FILTER_GET_CLASS (junk_filter);
	g_return_val_if_fail (class != NULL, NULL);

	if (class->new_config_widget != NULL)
		widget = class->new_config_widget (junk_filter);

	return widget;
}

gint
e_mail_junk_filter_compare (EMailJunkFilter *junk_filter_a,
                            EMailJunkFilter *junk_filter_b)
{
	EMailJunkFilterClass *class_a;
	EMailJunkFilterClass *class_b;

	class_a = E_MAIL_JUNK_FILTER_GET_CLASS (junk_filter_a);
	class_b = E_MAIL_JUNK_FILTER_GET_CLASS (junk_filter_b);

	g_return_val_if_fail (class_a != NULL, 0);
	g_return_val_if_fail (class_b != NULL, 0);

	return g_utf8_collate (class_a->display_name, class_b->display_name);
}
