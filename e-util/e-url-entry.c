/*
 * e-url-entry.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-url-entry.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-misc-utils.h"

#define E_URL_ENTRY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_URL_ENTRY, EUrlEntryPrivate))

#define ICON_POSITION GTK_ENTRY_ICON_SECONDARY

G_DEFINE_TYPE (
	EUrlEntry,
	e_url_entry,
	GTK_TYPE_ENTRY)

static gboolean
url_entry_text_to_sensitive (GBinding *binding,
                             const GValue *source_value,
                             GValue *target_value,
                             gpointer user_data)
{
	const gchar *text;
	gboolean sensitive = FALSE;

	text = g_value_get_string (source_value);

	if (text != NULL) {
		gchar *scheme;

		/* Skip leading whitespace. */
		while (g_ascii_isspace (*text))
			text++;

		scheme = g_uri_parse_scheme (text);
		sensitive = (scheme != NULL);
		g_free (scheme);
	}

	g_value_set_boolean (target_value, sensitive);

	return TRUE;
}

static void
url_entry_icon_release_cb (GtkEntry *entry,
                           GtkEntryIconPosition icon_position,
                           GdkEvent *event)
{
	gpointer toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (entry));
	toplevel = gtk_widget_is_toplevel (toplevel) ? toplevel : NULL;

	if (icon_position == ICON_POSITION) {
		const gchar *text;

		text = gtk_entry_get_text (entry);
		g_return_if_fail (text != NULL);

		/* Skip leading whitespace. */
		while (g_ascii_isspace (*text))
			text++;

		e_show_uri (toplevel, text);
	}
}

static void
e_url_entry_class_init (EUrlEntryClass *class)
{
}

static void
e_url_entry_init (EUrlEntry *url_entry)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (url_entry);

	gtk_entry_set_icon_from_icon_name (
		entry, ICON_POSITION, "go-jump");

	gtk_entry_set_icon_tooltip_text (
		entry, ICON_POSITION, _("Click here to open the URL"));

	gtk_entry_set_placeholder_text (entry, _("Enter a URL here"));

	/* XXX GtkEntryClass has no "icon_release" method pointer to
	 *     override, so instead we have to connect to the signal. */
	g_signal_connect (
		url_entry, "icon-release",
		G_CALLBACK (url_entry_icon_release_cb), NULL);

	e_binding_bind_property_full (
		url_entry, "text",
		url_entry, "secondary-icon-sensitive",
		G_BINDING_SYNC_CREATE,
		url_entry_text_to_sensitive,
		(GBindingTransformFunc) NULL,
		NULL, (GDestroyNotify) NULL);
}

GtkWidget *
e_url_entry_new (void)
{
	return g_object_new (E_TYPE_URL_ENTRY, NULL);
}

