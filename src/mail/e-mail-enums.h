/*
 * e-mail-enums.h
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

#ifndef E_MAIL_ENUMS_H
#define E_MAIL_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	E_MAIL_FORWARD_STYLE_ATTACHED,
	E_MAIL_FORWARD_STYLE_INLINE,
	E_MAIL_FORWARD_STYLE_QUOTED
} EMailForwardStyle;

typedef enum {
	E_MAIL_REPLY_STYLE_QUOTED,
	E_MAIL_REPLY_STYLE_DO_NOT_QUOTE,
	E_MAIL_REPLY_STYLE_ATTACH,
	E_MAIL_REPLY_STYLE_OUTLOOK
} EMailReplyStyle;

typedef enum {
	E_MAIL_REPLY_TO_SENDER,
	E_MAIL_REPLY_TO_RECIPIENT,
	E_MAIL_REPLY_TO_FROM,
	E_MAIL_REPLY_TO_ALL,
	E_MAIL_REPLY_TO_LIST
} EMailReplyType;

G_END_DECLS

#endif /* E_MAIL_ENUMS_H */

