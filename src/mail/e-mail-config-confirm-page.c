/*
 * e-mail-config-confirm-page.h
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

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include "e-mail-config-confirm-page.h"

struct _EMailConfigConfirmPagePrivate {
	gchar *text;
};

enum {
	PROP_0,
	PROP_TEXT
};

/* Forward Declarations */
static void	e_mail_config_confirm_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailConfigConfirmPage, e_mail_config_confirm_page, GTK_TYPE_SCROLLED_WINDOW,
	G_ADD_PRIVATE (EMailConfigConfirmPage)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_confirm_page_interface_init))

static void
mail_config_confirm_page_set_property (GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TEXT:
			e_mail_config_confirm_page_set_text (
				E_MAIL_CONFIG_CONFIRM_PAGE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_confirm_page_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TEXT:
			g_value_set_string (
				value,
				e_mail_config_confirm_page_get_text (
				E_MAIL_CONFIG_CONFIRM_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_confirm_page_finalize (GObject *object)
{
	EMailConfigConfirmPage *self = E_MAIL_CONFIG_CONFIRM_PAGE (object);

	g_free (self->priv->text);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_config_confirm_page_parent_class)->finalize (object);
}

static void
mail_config_confirm_page_constructed (GObject *object)
{
	EMailConfigConfirmPage *page;
	GtkWidget *main_box, *widget;

	page = E_MAIL_CONFIG_CONFIRM_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_confirm_page_parent_class)->constructed (object);

	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_valign (main_box, GTK_ALIGN_CENTER);

	widget = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		page, "text",
		widget, "label",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);

	e_extensible_load_extensions (E_EXTENSIBLE (page));
}

static void
e_mail_config_confirm_page_class_init (EMailConfigConfirmPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_confirm_page_set_property;
	object_class->get_property = mail_config_confirm_page_get_property;
	object_class->finalize = mail_config_confirm_page_finalize;
	object_class->constructed = mail_config_confirm_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_TEXT,
		g_param_spec_string (
			"text",
			"Text",
			"Confirmation message",
			_("Congratulations, your mail configuration is "
			"complete.\n\nYou are now ready to send and "
			"receive email using Evolution.\n\nClick "
			"“Apply” to save your settings."),
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_confirm_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Done");
	iface->page_type = GTK_ASSISTANT_PAGE_CONFIRM;
	iface->sort_order = E_MAIL_CONFIG_CONFIRM_PAGE_SORT_ORDER;
}

static void
e_mail_config_confirm_page_init (EMailConfigConfirmPage *page)
{
	page->priv = e_mail_config_confirm_page_get_instance_private (page);
}

EMailConfigPage *
e_mail_config_confirm_page_new (void)
{
	return g_object_new (E_TYPE_MAIL_CONFIG_CONFIRM_PAGE, NULL);
}

const gchar *
e_mail_config_confirm_page_get_text (EMailConfigConfirmPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_CONFIRM_PAGE (page), NULL);

	return page->priv->text;
}

void
e_mail_config_confirm_page_set_text (EMailConfigConfirmPage *page,
                                     const gchar *text)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_CONFIRM_PAGE (page));

	if (g_strcmp0 (page->priv->text, (text != NULL) ? text : "") == 0)
		return;

	g_free (page->priv->text);
	page->priv->text = g_strdup ((text != NULL) ? text : "");

	g_object_notify (G_OBJECT (page), "text");
}

