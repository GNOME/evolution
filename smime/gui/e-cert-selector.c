/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright 2003, Novell Inc.
 *
 * Author(s): Michael Zucchi <notzed@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/gnome-i18n.h>

#include <gal/util/e-util.h>

#include <gtk/gtktextview.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkstock.h>

#include "nss.h"
#include "pk11func.h"
#include "certdb.h"
#include "cert.h"

#include <glade/glade.h>

#include "e-cert-selector.h"

struct _ECertSelectorPrivate {
	CERTCertList *certlist;

	GtkWidget *menu, *description;
};

enum {
	ECS_SELECTED,
	ECS_LAST_SIGNAL
};

static guint ecs_signals[ECS_LAST_SIGNAL];

static GtkDialog *ecs_parent_class;

/* (this is what mozilla shows)
Issued to:
  Subject: E=notzed@ximian.com, CN=notzed@ximian.com, O=My Company Ltd, L=Adelaide, ST=SA, C=AU
  Serial Number: 03
  Valid from 23/10/03 06:35:29 to 22/10/04 06:35:29
  Purposes: Sign,Encrypt
Issued by:
  Subject: E=notzed@ximian.com, O=company, L=there, ST=Here, C=AU
*/

static CERTCertListNode *
ecs_find_current(ECertSelector *ecs)
{
	struct _ECertSelectorPrivate *p = ecs->priv;
	CERTCertListNode *node;
	int n;

	if (p->certlist == NULL || CERT_LIST_EMPTY(p->certlist))
		return NULL;

	n = gtk_option_menu_get_history((GtkOptionMenu *)p->menu);
	node = CERT_LIST_HEAD(p->certlist);
	while (n>0 && !CERT_LIST_END(node, p->certlist)) {
		n--;
		node = CERT_LIST_NEXT(node);
	}

	g_assert(!CERT_LIST_END(node, p->certlist));

	return node;
}

static void
ecs_response(GtkDialog *dialog, gint button)
{
	CERTCertListNode *node;

	switch (button) {
	case GTK_RESPONSE_OK:
		node = ecs_find_current((ECertSelector *)dialog);
		break;
	default:
		node = NULL;
		break;
	}

	g_signal_emit(dialog, ecs_signals[ECS_SELECTED], 0, node?node->cert->nickname:NULL);
}

static void
ecs_cert_changed(GtkWidget *w, ECertSelector *ecs)
{
	struct _ECertSelectorPrivate *p = ecs->priv;
	CERTCertListNode *node;
	GtkTextBuffer *buffer;
	GString *text;

	text = g_string_new("");
	node = ecs_find_current(ecs);
	if (node) {
		/* FIXME: add serial no, validity date, uses */
		g_string_append_printf(text, _("Issued to:\n  Subject: %s\n"), node->cert->subjectName);
		g_string_append_printf(text, _("Issued by:\n  Subject: %s\n"), node->cert->issuerName);
	}

	buffer = gtk_text_view_get_buffer((GtkTextView *)p->description);
	gtk_text_buffer_set_text(buffer, text->str, text->len);
	g_string_free(text, TRUE);
}

/**
 * e_cert_selector_new:
 * @type: 
 * @currentid: 
 * 
 * Create a new ECertSelector dialog.  @type specifies which type of cert to
 * be selected, E_CERT_SELECTOR_SIGNER for signing certs, and
 * E_CERT_SELECTOR_RECIPIENT for encrypting certs.
 * 
 * @currentid is the nickname of the cert currently selected for this user.
 *
 * You only need to connect to a single signal "selected" which will
 * be called with either a NULL nickname if cancelled, or the newly
 * selected nickname otherwise.
 *
 * Return value: A dialogue to be shown.
 **/
GtkWidget *
e_cert_selector_new(int type, const char *currentid)
{
	ECertSelector *ecs;
	struct _ECertSelectorPrivate *p;
	SECCertUsage usage;
	CERTCertList *certlist;
	CERTCertListNode *node;
	GladeXML *gui;
	GtkWidget *w, *menu;
	int n=0, active=0;

	ecs = g_object_new(e_cert_selector_get_type(), NULL);
	p = ecs->priv;

	gui = glade_xml_new(EVOLUTION_GLADEDIR "/smime-ui.glade", "cert_selector_vbox", NULL);

	p->menu = glade_xml_get_widget(gui, "cert_menu");
	p->description = glade_xml_get_widget(gui, "cert_description");

	w = glade_xml_get_widget(gui, "cert_selector_vbox");
	gtk_box_pack_start((GtkBox *)((GtkDialog *)ecs)->vbox, w, TRUE, TRUE, 3);
	gtk_window_set_title(GTK_WINDOW(ecs), _("Select certificate"));

	switch (type) {
	case E_CERT_SELECTOR_SIGNER:
	default:
		usage = certUsageEmailSigner;
		break;
	case E_CERT_SELECTOR_RECIPIENT:
		usage = certUsageEmailRecipient;
		break;
	}

	menu = gtk_menu_new();

	certlist = CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), usage, FALSE, TRUE, NULL);
	ecs->priv->certlist = certlist;
	if (certlist != NULL) {
		node = CERT_LIST_HEAD(certlist);
		while (!CERT_LIST_END(node, certlist)) {
			w = gtk_menu_item_new_with_label(node->cert->nickname);
			gtk_menu_shell_append((GtkMenuShell *)menu, w);
			gtk_widget_show(w);

			if (currentid != NULL
			    && (strcmp(node->cert->nickname, currentid) == 0
				|| strcmp(node->cert->emailAddr, currentid) == 0))
				active = n;

			n++;
			node = CERT_LIST_NEXT(node);
		}
	}

	gtk_option_menu_set_menu((GtkOptionMenu *)p->menu, menu);
	gtk_option_menu_set_history((GtkOptionMenu *)p->menu, active);

	g_signal_connect(p->menu, "changed", G_CALLBACK(ecs_cert_changed), ecs);

	g_object_unref(gui);

	ecs_cert_changed(p->menu, ecs);

	return GTK_WIDGET(ecs);
}

static void
ecs_init(ECertSelector *ecs)
{
	gtk_dialog_add_buttons((GtkDialog *)ecs,
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			       GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	ecs->priv = g_malloc0(sizeof(*ecs->priv));
}

static void
ecs_finalise(GObject *o)
{
	ECertSelector *ecs = (ECertSelector *)o;

	if (ecs->priv->certlist)
		CERT_DestroyCertList(ecs->priv->certlist);
		
	g_free(ecs->priv);

	((GObjectClass *)ecs_parent_class)->finalize(o);
}

static void
ecs_class_init(ECertSelectorClass *klass)
{
	ecs_parent_class = g_type_class_ref(gtk_dialog_get_type());

	((GObjectClass *)klass)->finalize = ecs_finalise;
	((GtkDialogClass *)klass)->response = ecs_response;

	ecs_signals[ECS_SELECTED] =
		g_signal_new("selected",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(ECertSelectorClass, selected),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

E_MAKE_TYPE(e_cert_selector, "ECertSelector", ECertSelector, ecs_class_init, ecs_init, gtk_dialog_get_type())
