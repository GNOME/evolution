/*
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
 *
 * Authors:
 *		Parthasarathi Susarla <sparthasarathi@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __GW_SEND_OPTIONS__
#define __GW_SEND_OPTIONS__

/*Headers for send options*/
#define X_SEND_OPTIONS	      "X-gw-send-options"
/*General Options*/
#define X_SEND_OPT_PRIORITY   "X-gw-send-opt-priority"
#define X_SEND_OPT_SECURITY   "X-gw-send-opt-security"
#define X_REPLY_CONVENIENT    "X-reply-convenient"
#define X_REPLY_WITHIN	      "X-reply-within"
#define X_EXPIRE_AFTER	      "X-expire-after"
#define X_DELAY_UNTIL	      "X-delay-until"

/*Status Tracking Options*/
#define	X_TRACK_WHEN		"X-track-when"
#define	X_AUTODELETE		"X-auto-delete"
#define	X_RETURN_NOTIFY_OPEN	"X-return-notify-open"
#define	X_RETURN_NOTIFY_DELETE	"X-return-notify-delete"

#endif /*__GW_SEND_OPTIONS__*/
