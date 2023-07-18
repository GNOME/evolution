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
	E_MAIL_REPLY_STYLE_UNKNOWN = -1,
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

/**
 * EMailReplyFlags:
 * @E_MAIL_REPLY_FLAG_NONE: no flags used
 * @E_MAIL_REPLY_FLAG_FORCE_STYLE: Force use of the passed-in reply style; if not set,
 *    then also checks reply style setting for the used mail account.
 * @E_MAIL_REPLY_FLAG_FORMAT_PLAIN: Force compose in Plain Text format; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_FORMAT_HTML, @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN,
 *    @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN nor @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML.
 *    If none of these is set, then uses global setting.
 * @E_MAIL_REPLY_FLAG_FORMAT_HTML: Force compose in HTML format; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_FORMAT_PLAIN, @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN,
 *    @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN nor @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML.
 *    If none of these is set, then uses global setting.
 * @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN: Force compose in Markdown format; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_FORMAT_PLAIN, @E_MAIL_REPLY_FLAG_FORMAT_HTML,
 *    @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN nor @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML.
 *    If none of these is set, then uses global setting. (Since: 3.44)
 * @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN: Force compose in Markdown as Plain Text format; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_FORMAT_PLAIN, @E_MAIL_REPLY_FLAG_FORMAT_HTML,
 *    @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN nor @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML.
 *    If none of these is set, then uses global setting. (Since: 3.44)
 * @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML: Force compose in Markdown as HTML format; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_FORMAT_PLAIN, @E_MAIL_REPLY_FLAG_FORMAT_HTML,
 *    @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN nor @E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN.
 *    If none of these is set, then uses global setting. (Since: 3.44)
 * @E_MAIL_REPLY_FLAG_TOP_POSTING: Force top posting; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_BOTTOM_POSTING. If none is set, then uses global settings.
 * @E_MAIL_REPLY_FLAG_BOTTOM_POSTING: Force bottom posting; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_TOP_POSTING. If none is set, then uses global settings.
 * @E_MAIL_REPLY_FLAG_TOP_SIGNATURE: Force placing signature to the top; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_BOTTOM_SIGNATURE. If none is set, then uses global settings.
 * @E_MAIL_REPLY_FLAG_BOTTOM_SIGNATURE: Force placing signature to the bottom; cannot be used together
 *    with @E_MAIL_REPLY_FLAG_TOP_SIGNATURE. If none is set, then uses global settings.
 * @E_MAIL_REPLY_FLAG_FORCE_SENDER_REPLY: Force sender reply, to not switch it to reply-all, when
 *    the From address of the message is the user.
 * @E_MAIL_REPLY_FLAG_SKIP_INSECURE_PARTS: Skip insecure parts, that's those without validity. Since: 3.50
 *
 * Flags influencing behavior of em_utils_reply_to_message().
 *
 * Since: 3.30
 **/
typedef enum { /*< flags >*/
	E_MAIL_REPLY_FLAG_NONE			= 0,
	E_MAIL_REPLY_FLAG_FORCE_STYLE		= 1 << 0,
	E_MAIL_REPLY_FLAG_FORMAT_PLAIN		= 1 << 1,
	E_MAIL_REPLY_FLAG_FORMAT_HTML		= 1 << 2,
	E_MAIL_REPLY_FLAG_TOP_POSTING		= 1 << 3,
	E_MAIL_REPLY_FLAG_BOTTOM_POSTING	= 1 << 4,
	E_MAIL_REPLY_FLAG_TOP_SIGNATURE		= 1 << 5,
	E_MAIL_REPLY_FLAG_BOTTOM_SIGNATURE	= 1 << 6,
	E_MAIL_REPLY_FLAG_FORCE_SENDER_REPLY	= 1 << 7,
	E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN	= 1 << 8,
	E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN	= 1 << 9,
	E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML	= 1 << 10,
	E_MAIL_REPLY_FLAG_SKIP_INSECURE_PARTS	= 1 << 11
} EMailReplyFlags;

G_END_DECLS

#endif /* E_MAIL_ENUMS_H */

