/*
 * SPDX-FileCopyrightText: (C) 2008 Novell, Inc.
 * SPDX-FileCopyrightText: (C) 2012 Dan Vrátil <dvratil@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EMOTICON_CHOOSER_H
#define E_EMOTICON_CHOOSER_H

#include <e-util/e-emoticon.h>

/* Standard GObject macros */
#define E_TYPE_EMOTICON_CHOOSER \
	(e_emoticon_chooser_get_type ())
#define E_EMOTICON_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EMOTICON_CHOOSER, EEmoticonChooser))
#define E_IS_EMOTICON_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EMOTICON_CHOOSER))
#define E_EMOTICON_CHOOSER_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_EMOTICON_CHOOSER, EEmoticonChooserInterface))

G_BEGIN_DECLS

typedef struct _EEmoticonChooser EEmoticonChooser;
typedef struct _EEmoticonChooserInterface EEmoticonChooserInterface;

struct _EEmoticonChooserInterface {
	GTypeInterface parent_interface;

	/* Methods */
	EEmoticon *	(*get_current_emoticon)	(EEmoticonChooser *chooser);
	void		(*set_current_emoticon)	(EEmoticonChooser *chooser,
						 EEmoticon *emoticon);

	/* Signals */
	void		(*item_activated)	(EEmoticonChooser *chooser);
};

GType		e_emoticon_chooser_get_type	(void) G_GNUC_CONST;
EEmoticon *	e_emoticon_chooser_get_current_emoticon
						(EEmoticonChooser *chooser);
void		e_emoticon_chooser_set_current_emoticon
						(EEmoticonChooser *chooser,
						 EEmoticon *emoticon);
void		e_emoticon_chooser_item_activated
						(EEmoticonChooser *chooser);

GList *		e_emoticon_chooser_get_items	(void);
const EEmoticon *
		e_emoticon_chooser_lookup_emoticon
						(const gchar *icon_name);

G_END_DECLS

#endif /* E_EMOTICON_CHOOSER_H */
