/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Michael Zucchi <notzed@ximian.com>
 * SPDX-FileContributor: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef MAIL_AUTOFILTER_H
#define MAIL_AUTOFILTER_H

#include <camel/camel.h>
#include <libemail-engine/libemail-engine.h>

#include <mail/em-filter-context.h>

enum {
	AUTO_SUBJECT = 1,
	AUTO_FROM = 2,
	AUTO_TO = 4,
	AUTO_MLIST = 8
};

EFilterRule *	em_vfolder_rule_from_message	(EMVFolderContext *context,
						 CamelMimeMessage *msg,
						 gint flags,
						 CamelFolder *folder);
EFilterRule *	filter_rule_from_message	(EMFilterContext *context,
						 CamelMimeMessage *msg,
						 gint flags);
EFilterRule *	em_vfolder_rule_from_address	(EMVFolderContext *context,
						 CamelInternetAddress *addr,
						 gint flags,
						 CamelFolder *folder);

/* easiest place to put this */
void		filter_gui_add_from_message	(EMailSession *session,
						 CamelMimeMessage *msg,
						 const gchar *source,
						 gint flags);

/* Also easiest place for these, we should really
 * share a global rule context for this stuff ... */
void		mail_filter_rename_folder	(CamelStore *store,
						 const gchar *old_folder_name,
						 const gchar *new_folder_name);
void		mail_filter_delete_folder	(CamelStore *store,
						 const gchar *folder_name,
						 EAlertSink *alert_sink);

#endif /* MAIL_AUTOFILTER_H */
