/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
 * SPDX-FileContributor: Michael Zucchi <notzed@ximian.com>
 */

#ifndef _CERT_TRUST_DIALOG_H_
#define _CERT_TRUST_DIALOG_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget * cert_trust_dialog_show (ECert *cert);

G_END_DECLS

#endif /* _CERT_TRUST_DIALOG_H_ */
