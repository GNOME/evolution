/*
 * e-mail-formatter-enums.h
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

#ifndef E_MAIL_FORMATTER_ENUMS_H
#define E_MAIL_FORMATTER_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	E_MAIL_FORMATTER_COLOR_BODY,		/* header area background */
	E_MAIL_FORMATTER_COLOR_CITATION,	/* citation font color */
	E_MAIL_FORMATTER_COLOR_CONTENT,		/* message area background */
	E_MAIL_FORMATTER_COLOR_FRAME,		/* frame around message area */
	E_MAIL_FORMATTER_COLOR_HEADER,		/* header font color */
	E_MAIL_FORMATTER_COLOR_TEXT,		/* message font color */
	E_MAIL_FORMATTER_NUM_COLOR_TYPES	/*< skip >*/
} EMailFormatterColor;

typedef enum { /*< flags >*/
	E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE = 1 << 0,
	E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED = 1 << 1,
	E_MAIL_FORMATTER_HEADER_FLAG_HTML = 1 << 2,
	E_MAIL_FORMATTER_HEADER_FLAG_NOCOLUMNS = 1 << 3,
	E_MAIL_FORMATTER_HEADER_FLAG_BOLD = 1 << 4,
	E_MAIL_FORMATTER_HEADER_FLAG_NODEC = 1 << 5,
	E_MAIL_FORMATTER_HEADER_FLAG_HIDDEN = 1 << 6,
	E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS = 1 << 7,
	E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE = 1 << 8,
	E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING = 1 << 9
} EMailFormatterHeaderFlags;

typedef enum {
	E_MAIL_FORMATTER_MODE_INVALID = -1,
	E_MAIL_FORMATTER_MODE_NORMAL = 0,
	E_MAIL_FORMATTER_MODE_SOURCE,
	E_MAIL_FORMATTER_MODE_RAW,
	E_MAIL_FORMATTER_MODE_PRINTING,
	E_MAIL_FORMATTER_MODE_ALL_HEADERS
} EMailFormatterMode;

typedef enum { /*< flags >*/
	E_MAIL_FORMATTER_QUOTE_FLAG_CITE = 1 << 0,
	E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS = 1 << 1,
	E_MAIL_FORMATTER_QUOTE_FLAG_KEEP_SIG	= 1 << 2,  /* do not strip signature */
	E_MAIL_FORMATTER_QUOTE_FLAG_NO_FORMATTING = 1 << 3,
	E_MAIL_FORMATTER_QUOTE_FLAG_SKIP_INSECURE_PARTS = 1 << 4
} EMailFormatterQuoteFlags;

/**
 * EMailParserExtensionFlags:
 * @E_MAIL_PARSER_EXTENSION_INLINE:
 *    Don't parse as attachment.
 * @E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION:
 *    Always expand.
 * @E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE:
 *    Always check what's inside.
 **/
typedef enum { /*< flags >*/
	E_MAIL_PARSER_EXTENSION_INLINE = 1 << 0,
	E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION = 1 << 1,
	E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE = 1 << 2
} EMailParserExtensionFlags;

typedef enum { /*< flags >*/
	E_MAIL_PART_VALIDITY_NONE = 0,
	E_MAIL_PART_VALIDITY_PGP = 1 << 0,
	E_MAIL_PART_VALIDITY_SMIME = 1 << 1,
	E_MAIL_PART_VALIDITY_SIGNED = 1 << 2,
	E_MAIL_PART_VALIDITY_ENCRYPTED = 1 << 3,
	E_MAIL_PART_VALIDITY_SENDER_SIGNER_MISMATCH = 1 << 4,
	E_MAIL_PART_VALIDITY_VERIFIED = 1 << 5
} EMailPartValidityFlags;

typedef enum { /*< flags >*/
	E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE = 0,
	E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_IS_POSSIBLE = 1 << 0
} EMailParserWrapAttachmentFlags;

G_END_DECLS

#endif /* E_MAIL_FORMATTER_ENUMS_H */

