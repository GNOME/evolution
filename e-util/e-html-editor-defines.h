/*
 * e-html-editor-defines.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_DEFINES_H
#define E_HTML_EDITOR_DEFINES_H

#define E_HTML_EDITOR_NODE_IS_HR         (1 << 0)
#define E_HTML_EDITOR_NODE_IS_TEXT       (1 << 1)
#define E_HTML_EDITOR_NODE_IS_ANCHOR     (1 << 2)
#define E_HTML_EDITOR_NODE_IS_IMAGE      (1 << 3)
#define E_HTML_EDITOR_NODE_IS_TABLE_CELL (1 << 4)
#define E_HTML_EDITOR_NODE_IS_TABLE      (1 << 5)
#define E_HTML_EDITOR_LAST_FLAG          (1 << 6)

#endif /* E_HTML_EDITOR_DEFINES_H */
