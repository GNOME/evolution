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

#include "evolution-config.h"

#include "e-url-entry.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-misc-utils.h"

#define ICON_POSITION GTK_ENTRY_ICON_SECONDARY

struct _EUrlEntryPrivate {
	gboolean dummy;
};

enum {
	PROP_0,
	PROP_ICON_VISIBLE
};

enum {
	OPEN_URL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EUrlEntry, e_url_entry, GTK_TYPE_ENTRY)

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
		/* Skip leading whitespace. */
		while (g_ascii_isspace (*text))
			text++;

		sensitive = *text != '\0';
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
	const gchar *text;

	if (icon_position != ICON_POSITION)
		return;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (entry));
	toplevel = gtk_widget_is_toplevel (toplevel) ? toplevel : NULL;

	text = gtk_entry_get_text (entry);
	g_return_if_fail (text != NULL);

	/* Skip leading whitespace. */
	while (g_ascii_isspace (*text))
		text++;

	if (text && *text) {
		gboolean handled = FALSE;

		g_signal_emit (entry, signals[OPEN_URL], 0, toplevel, text, &handled);

		if (!handled)
			e_show_uri (toplevel, text);
	}
}

static void
e_url_entry_set_property (GObject *object,
			  guint property_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ICON_VISIBLE:
			e_url_entry_set_icon_visible (
				E_URL_ENTRY (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_url_entry_get_property (GObject *object,
			  guint property_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ICON_VISIBLE:
			g_value_set_boolean (
				value,
				e_url_entry_get_icon_visible (
				E_URL_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_url_entry_class_init (EUrlEntryClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_url_entry_set_property;
	object_class->get_property = e_url_entry_get_property;

	g_object_class_install_property (
		object_class,
		PROP_ICON_VISIBLE,
		g_param_spec_boolean (
			"icon-visible",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[OPEN_URL] = g_signal_new (
		"open-url",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 2,
		GTK_TYPE_WINDOW,
		G_TYPE_STRING);
}

static void
e_url_entry_init (EUrlEntry *url_entry)
{
	GtkEntry *entry;

	url_entry->priv = e_url_entry_get_instance_private (url_entry);

	entry = GTK_ENTRY (url_entry);

	gtk_entry_set_icon_tooltip_text (
		entry, ICON_POSITION, _("Click here to open the URL"));

	e_url_entry_set_icon_visible (url_entry, TRUE);

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

void
e_url_entry_set_icon_visible (EUrlEntry *url_entry,
			      gboolean visible)
{
	GtkEntry *entry;

	g_return_if_fail (E_IS_URL_ENTRY (url_entry));

	entry = GTK_ENTRY (url_entry);

	if (visible) {
		gtk_entry_set_icon_from_icon_name (entry, ICON_POSITION, "go-jump");
		gtk_entry_set_placeholder_text (entry, _("Enter a URL here"));
	} else {
		gtk_entry_set_icon_from_icon_name (entry, ICON_POSITION, NULL);
		gtk_entry_set_placeholder_text (entry, NULL);
	}
}

gboolean
e_url_entry_get_icon_visible (EUrlEntry *url_entry)
{
	g_return_val_if_fail (E_IS_URL_ENTRY (url_entry), FALSE);

	return gtk_entry_get_icon_name (GTK_ENTRY (url_entry), ICON_POSITION) != NULL;
}
