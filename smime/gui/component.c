/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright (C) 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libgnome/gnome-i18n.h>

#include "ca-trust-dialog.h"
#include "e-cert-db.h"
#include "e-util/e-passwords.h"
#include "pk11func.h"

static gboolean
smime_pk11_passwd (ECertDB *db, PK11SlotInfo* slot, gboolean retry, char **passwd, gpointer arg)
{
	char *prompt;
	char *slot_name = g_strdup (PK11_GetSlotName (slot));

	g_strchomp (slot_name);

	prompt = g_strdup_printf (_("Enter the password for `%s'"), slot_name);
	g_free (slot_name);

	*passwd = e_passwords_ask_password (_("Enter password"), NULL, NULL,
					    prompt, TRUE,
					    E_PASSWORDS_DO_NOT_REMEMBER, NULL,
					    NULL);

	g_free (prompt);

	/* this should return FALSE if they canceled. */
	return TRUE;
}

static gboolean
smime_pk11_change_passwd (ECertDB *db, char **old_passwd, char **passwd, gpointer arg)
{
	char *prompt;

	/* XXX need better strings here, just copy mozilla's? */

	if (!old_passwd) {
		/* we're setting the password initially */
		prompt = _("Enter new password for certificate database");

		*passwd = e_passwords_ask_password (_("Enter new password"), NULL, NULL,
						    prompt, TRUE,
						    E_PASSWORDS_DO_NOT_REMEMBER, NULL,
						    NULL);
	}
	else {
		/* we're changing the password */
		/* XXX implement this... */
	}

	/* this should return FALSE if they canceled. */
	return TRUE;
}

static gboolean
smime_confirm_ca_cert_import (ECertDB *db, ECert *cert, gboolean *trust_ssl, gboolean *trust_email, gboolean *trust_objsign, gpointer arg)
{
	GtkWidget *dialog = ca_trust_dialog_show (cert, TRUE);
	int response;

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	ca_trust_dialog_get_trust (dialog, trust_ssl, trust_email, trust_objsign);

	gtk_widget_destroy (dialog);

	return response != GTK_RESPONSE_CANCEL;
}

void
smime_component_init (void)
{
	static gboolean init_done = FALSE;
	if (init_done)
		return;

	init_done = TRUE;
	g_signal_connect (e_cert_db_peek (),
			  "pk11_passwd",
			  G_CALLBACK (smime_pk11_passwd), NULL);

	g_signal_connect (e_cert_db_peek (),
			  "pk11_change_passwd",
			  G_CALLBACK (smime_pk11_change_passwd), NULL);

	g_signal_connect (e_cert_db_peek (),
			  "confirm_ca_cert_import",
			  G_CALLBACK (smime_confirm_ca_cert_import), NULL);
}
