/*
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
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include <libedataserverui/libedataserverui.h>

#include "nss.h"
#include "pk11func.h"
#include "certdb.h"
#include "cert.h"

#include "e-cert-selector.h"

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "smime/lib/e-cert.h"

struct _ECertSelectorPrivate {
	CERTCertList *certlist;

	GtkWidget *combobox;
	GtkWidget *cert_widget;
};

enum {
	ECS_SELECTED,
	ECS_LAST_SIGNAL
};

static guint ecs_signals[ECS_LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (ECertSelector, e_cert_selector, GTK_TYPE_DIALOG)

/* (this is what mozilla shows)
 * Issued to:
 *  Subject: E=notzed@ximian.com, CN=notzed@ximian.com, O=My Company Ltd, L=Adelaide, ST=SA, C=AU
 *  Serial Number: 03
 *  Valid from 23/10/03 06:35:29 to 22/10/04 06:35:29
 *  Purposes: Sign,Encrypt
 * Issued by:
 *  Subject: E=notzed@ximian.com, O=company, L=there, ST=Here, C=AU
 */

static CERTCertListNode *
ecs_find_current (ECertSelector *ecs)
{
	struct _ECertSelectorPrivate *p = ecs->priv;
	CERTCertListNode *node;
	gint n;

	if (p->certlist == NULL || CERT_LIST_EMPTY (p->certlist))
		return NULL;

	n = gtk_combo_box_get_active (GTK_COMBO_BOX (p->combobox));
	node = CERT_LIST_HEAD (p->certlist);
	while (n > 0 && !CERT_LIST_END (node, p->certlist)) {
		n--;
		node = CERT_LIST_NEXT (node);
	}

	g_return_val_if_fail (!CERT_LIST_END (node, p->certlist), NULL);

	return node;
}

static void
e_cert_selector_response (GtkDialog *dialog,
                          gint button)
{
	CERTCertListNode *node;

	switch (button) {
	case GTK_RESPONSE_OK:
		node = ecs_find_current ((ECertSelector *) dialog);
		break;
	default:
		node = NULL;
		break;
	}

	g_signal_emit (dialog, ecs_signals[ECS_SELECTED], 0, node ? node->cert->nickname : NULL);
}

static void
ecs_cert_changed (GtkWidget *w,
                  ECertSelector *ecs)
{
	struct _ECertSelectorPrivate *p = ecs->priv;
	CERTCertListNode *node;

	node = ecs_find_current (ecs);
	if (node && node->cert)
		e_certificate_widget_set_der (E_CERTIFICATE_WIDGET (p->cert_widget), node->cert->derCert.data, node->cert->derCert.len);
	else
		e_certificate_widget_set_der (E_CERTIFICATE_WIDGET (p->cert_widget), NULL, 0);
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
e_cert_selector_new (gint type,
                     const gchar *currentid)
{
	ECertSelector *ecs;
	struct _ECertSelectorPrivate *p;
	SECCertUsage usage;
	CERTCertList *certlist;
	CERTCertListNode *node;
	GtkBuilder *builder;
	GtkWidget *content_area;
	GtkWidget *w;
	GtkListStore *store;
	GtkTreeIter iter;
	gint n = 0, active = 0;

	ecs = g_object_new (e_cert_selector_get_type (), NULL);
	p = ecs->priv;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "smime-ui.ui");

	p->combobox = e_builder_get_widget (builder, "cert_combobox");
	p->cert_widget = e_certificate_widget_new ();

	w = e_builder_get_widget (builder, "cert_selector_vbox");
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (ecs));
	gtk_container_add (GTK_CONTAINER (w), p->cert_widget);
	gtk_widget_show_all (w);
	gtk_box_pack_start (GTK_BOX (content_area), w, TRUE, TRUE, 3);
	gtk_window_set_title (GTK_WINDOW (ecs), _("Select certificate"));

	switch (type) {
	case E_CERT_SELECTOR_SIGNER:
	default:
		usage = certUsageEmailSigner;
		break;
	case E_CERT_SELECTOR_RECIPIENT:
		usage = certUsageEmailRecipient;
		break;
	}

	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (p->combobox)));
	gtk_list_store_clear (store);

	certlist = CERT_FindUserCertsByUsage (CERT_GetDefaultCertDB (), usage, FALSE, TRUE, NULL);
	ecs->priv->certlist = certlist;
	if (certlist != NULL) {
		node = CERT_LIST_HEAD (certlist);
		while (!CERT_LIST_END (node, certlist)) {
			if (node->cert->nickname || node->cert->emailAddr) {
				gtk_list_store_append (store, &iter);
				gtk_list_store_set (
					store, &iter,
					0, node->cert->nickname ? node->cert->nickname : node->cert->emailAddr,
					-1);

				if (currentid != NULL
				    && ((node->cert->nickname != NULL && strcmp (node->cert->nickname, currentid) == 0)
					|| (node->cert->emailAddr != NULL && strcmp (node->cert->emailAddr, currentid) == 0)))
					active = n;

				n++;
			}

			node = CERT_LIST_NEXT (node);
		}
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (p->combobox), active);

	g_signal_connect (
		p->combobox, "changed",
		G_CALLBACK (ecs_cert_changed), ecs);

	g_object_unref (builder);

	ecs_cert_changed (p->combobox, ecs);

	return GTK_WIDGET (ecs);
}

static void
e_cert_selector_init (ECertSelector *ecs)
{
	ecs->priv = e_cert_selector_get_instance_private (ecs);
	gtk_window_set_default_size (GTK_WINDOW (ecs), 400, 300);

	gtk_dialog_add_buttons (
		GTK_DIALOG (ecs),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);
}

static void
e_cert_selector_finalize (GObject *object)
{
	ECertSelector *self = E_CERT_SELECTOR (object);

	if (self->priv->certlist)
		CERT_DestroyCertList (self->priv->certlist);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cert_selector_parent_class)->finalize (object);
}

static void
e_cert_selector_class_init (ECertSelectorClass *class)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_cert_selector_finalize;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = e_cert_selector_response;

	ecs_signals[ECS_SELECTED] = g_signal_new (
		"selected",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECertSelectorClass, selected),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
}

