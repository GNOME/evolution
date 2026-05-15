/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_DISPLAY_POPUP_EXTENSION_H
#define E_MAIL_DISPLAY_POPUP_EXTENSION_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION \
	(e_mail_display_popup_extension_get_type ())
#define E_MAIL_DISPLAY_POPUP_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION, EMailDisplayPopupExtension))
#define E_MAIL_DISPLAY_POPUP_EXTENSION_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION, EMailDisplayPopupExtensionInterface))
#define E_IS_MAIL_DISPLAY_POPUP_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION))
#define E_IS_MAIL_DISPLAY_POPUP_EXTENSION_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION))
#define E_MAIL_DISPLAY_POPUP_EXTENSION_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION, EMailDisplayPopupExtensionInterface))

G_BEGIN_DECLS

typedef struct _EMailDisplayPopupExtension EMailDisplayPopupExtension;
typedef struct _EMailDisplayPopupExtensionInterface EMailDisplayPopupExtensionInterface;

struct _EMailDisplayPopupExtensionInterface {
	GTypeInterface parent_interface;

	void	(*update_actions)		(EMailDisplayPopupExtension *extension,
						 const gchar *popup_iframe_src,
						 const gchar *popup_iframe_id);
};

GType		e_mail_display_popup_extension_get_type	(void);

void		e_mail_display_popup_extension_update_actions
							(EMailDisplayPopupExtension *extension,
							 const gchar *popup_iframe_src,
							 const gchar *popup_iframe_id);

G_END_DECLS

#endif /* E_MAIL_DISPLAY_POPUP_EXTENSION_H */
