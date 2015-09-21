/*
 * e-mail-shell-view-actions.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_SHELL_VIEW_ACTIONS_H
#define E_MAIL_SHELL_VIEW_ACTIONS_H

#include <shell/e-shell-window-actions.h>

/* Mail Actions */
#define E_SHELL_WINDOW_ACTION_MAIL_ACCOUNT_DISABLE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-account-disable")
#define E_SHELL_WINDOW_ACTION_MAIL_ACCOUNT_EXPUNGE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-account-expunge")
#define E_SHELL_WINDOW_ACTION_MAIL_ACCOUNT_PROPERTIES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-account-properties")
#define E_SHELL_WINDOW_ACTION_MAIL_ACCOUNT_REFRESH(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-account-refresh")
#define E_SHELL_WINDOW_ACTION_MAIL_ADD_SENDER(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-add-sender")
#define E_SHELL_WINDOW_ACTION_MAIL_CARET_MODE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-caret-mode")
#define E_SHELL_WINDOW_ACTION_MAIL_CHECK_FOR_JUNK(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-check-for-junk")
#define E_SHELL_WINDOW_ACTION_MAIL_CLIPBOARD_COPY(window) \
	E_SHELL_WINDOw_ACTION ((window), "mail-clipboard-copy")
#define E_SHELL_WINDOW_ACTION_MAIL_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-copy")
#define E_SHELL_WINDOW_ACTION_MAIL_CREATE_SEARCH_FOLDER(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-create-search-folder")
#define E_SHELL_WINDOW_ACTION_MAIL_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-delete")
#define E_SHELL_WINDOW_ACTION_MAIL_DOWNLOAD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-download")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_RULE_FOR_MAILING_LIST(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-rule-for-mailing-list")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_RULE_FOR_RECIPIENTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-rule-for-recipients")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_RULE_FOR_SENDER(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-rule-for-sender")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_RULE_FOR_SUBJECT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-rule-for-subject")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTERS_APPLY(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filters-apply")
#define E_SHELL_WINDOW_ACTION_MAIL_FIND(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-find")
#define E_SHELL_WINDOW_ACTION_MAIL_FLAG_CLEAR(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-flag-clear")
#define E_SHELL_WINDOW_ACTION_MAIL_FLAG_COMPLETED(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-flag-completed")
#define E_SHELL_WINDOW_ACTION_MAIL_FLAG_FOR_FOLLOWUP(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-flag-for-followup")
#define E_SHELL_WINDOW_ACTION_MAIL_FLUSH_OUTBOX(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-flush-outbox")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-copy")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-delete")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_EXPUNGE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-expunge")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_MARK_ALL_AS_READ(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-mark-all-as-read")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_MOVE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-move")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-new")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_PROPERTIES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-properties")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_REFRESH(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-refresh")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_RENAME(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-rename")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_SELECT_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-select-all")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_SELECT_THREAD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-select-thread")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_SELECT_SUBTHREAD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-select-subthread")
#define E_SHELL_WINDOW_ACTION_MAIL_FOLDER_UNSUBSCRIBE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-folder-unsubscribe")
#define E_SHELL_WINDOW_ACTION_MAIL_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-forward")
#define E_SHELL_WINDOW_ACTION_MAIL_FORWARD_ATTACHED(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-forward-attached")
#define E_SHELL_WINDOW_ACTION_MAIL_FORWARD_INLINE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-forward-inline")
#define E_SHELL_WINDOW_ACTION_MAIL_FORWARD_QUOTED(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-forward-quoted")
#define E_SHELL_WINDOW_ACTION_MAIL_LABEL_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-label-new")
#define E_SHELL_WINDOW_ACTION_MAIL_LABEL_NONE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-label-none")
#define E_SHELL_WINDOW_ACTION_MAIL_LOAD_IMAGES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-load-images")
#define E_SHELL_WINDOW_ACTION_MAIL_MANAGE_SUBSCRIPTIONS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-manage-subscriptions")
#define E_SHELL_WINDOW_ACTION_MAIL_MARK_IMPORTANT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-mark-important")
#define E_SHELL_WINDOW_ACTION_MAIL_MARK_JUNK(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-mark-junk")
#define E_SHELL_WINDOW_ACTION_MAIL_MARK_NOTJUNK(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-mark-notjunk")
#define E_SHELL_WINDOW_ACTION_MAIL_MARK_READ(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-mark-read")
#define E_SHELL_WINDOW_ACTION_MAIL_MARK_UNIMPORTANT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-mark-unimportant")
#define E_SHELL_WINDOW_ACTION_MAIL_MARK_UNREAD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-mark-unread")
#define E_SHELL_WINDOW_ACTION_MAIL_MESSAGE_EDIT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-message-edit")
#define E_SHELL_WINDOW_ACTION_MAIL_MESSAGE_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-message-new")
#define E_SHELL_WINDOW_ACTION_MAIL_MESSAGE_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-message-open")
#define E_SHELL_WINDOW_ACTION_MAIL_MOVE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-move")
#define E_SHELL_WINDOW_ACTION_MAIL_NEXT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-next")
#define E_SHELL_WINDOW_ACTION_MAIL_NEXT_IMPORTANT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-next-important")
#define E_SHELL_WINDOW_ACTION_MAIL_NEXT_THREAD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-next-thread")
#define E_SHELL_WINDOW_ACTION_MAIL_NEXT_UNREAD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-next-unread")
#define E_SHELL_WINDOW_ACTION_MAIL_POPUP_FOLDER_MARK_ALL_AS_READ(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-popup-folder-mark-all-as-read")
#define E_SHELL_WINDOW_ACTION_MAIL_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-preview")
#define E_SHELL_WINDOW_ACTION_MAIL_PREVIOUS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-previous")
#define E_SHELL_WINDOW_ACTION_MAIL_PREVIOUS_IMPORTANT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-previous-important")
#define E_SHELL_WINDOW_ACTION_MAIL_PREVIOUS_UNREAD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-previous-unread")
#define E_SHELL_WINDOW_ACTION_MAIL_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-print")
#define E_SHELL_WINDOW_ACTION_MAIL_PRINT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-print-preview")
#define E_SHELL_WINDOW_ACTION_MAIL_REDIRECT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-redirect")
#define E_SHELL_WINDOW_ACTION_MAIL_REPLY_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-reply-all")
#define E_SHELL_WINDOW_ACTION_MAIL_REPLY_LIST(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-reply-list")
#define E_SHELL_WINDOW_ACTION_MAIL_REPLY_SENDER(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-reply-sender")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_ADVANCED_HIDDEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-advanced-hidden")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_FOLDER_FROM_MAILING_LIST(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-folder-from-mailing-list")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_FOLDER_FROM_RECIPIENTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-folder-from-recipients")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_FOLDER_FROM_SENDER(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-folder-from-sender")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_FOLDER_FROM_SUBJECT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-folder-from-subject")
#define E_SHELL_WINDOW_ACTION_MAIL_SELECT_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-select-all")
#define E_SHELL_WINDOW_ACTION_MAIL_SEND_RECEIVE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-send-receive")
#define E_SHELL_WINDOW_ACTION_MAIL_SEND_RECEIVE_RECEIVE_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-send-receive-receive-all")
#define E_SHELL_WINDOW_ACTION_MAIL_SEND_RECEIVE_SEND_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-send-receive-send-all")
#define E_SHELL_WINDOW_ACTION_MAIL_SEND_RECEIVE_SUBMENU(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-send-receive-submenu")
#define E_SHELL_WINDOW_ACTION_MAIL_SHOW_ALL_HEADERS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-show-all-headers")
#define E_SHELL_WINDOW_ACTION_MAIL_SHOW_DELETED(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-show-deleted")
#define E_SHELL_WINDOW_ACTION_MAIL_SHOW_SOURCE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-show-source")
#define E_SHELL_WINDOW_ACTION_MAIL_SMART_BACKWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-smart-backward")
#define E_SHELL_WINDOW_ACTION_MAIL_SMART_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-smart-forward")
#define E_SHELL_WINDOW_ACTION_MAIL_STOP(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-stop")
#define E_SHELL_WINDOW_ACTION_MAIL_THREADS_COLLAPSE_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-threads-collapse-all")
#define E_SHELL_WINDOW_ACTION_MAIL_THREADS_EXPAND_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-threads-expand-all")
#define E_SHELL_WINDOW_ACTION_MAIL_THREADS_GROUP_BY(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-threads-group-by")
#define E_SHELL_WINDOW_ACTION_MAIL_TOOLS_FILTERS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-tools-filters")
#define E_SHELL_WINDOW_ACTION_MAIL_TOOLS_SEARCH_FOLDERS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-tools-search-folders")
#define E_SHELL_WINDOW_ACTION_MAIL_TOOLS_SUBSCRIPTIONS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-tools-subscriptions")
#define E_SHELL_WINDOW_ACTION_MAIL_UNDELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-undelete")
#define E_SHELL_WINDOW_ACTION_MAIL_VFOLDER_UNMATCHED_ENABLE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-vfolder-unmatched-enable")
#define E_SHELL_WINDOW_ACTION_MAIL_VIEW_CLASSIC(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-view-classic")
#define E_SHELL_WINDOW_ACTION_MAIL_VIEW_VERTICAL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-view-vertical")
#define E_SHELL_WINDOW_ACTION_MAIL_ZOOM_100(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-zoom-100")
#define E_SHELL_WINDOW_ACTION_MAIL_ZOOM_IN(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-zoom-in")
#define E_SHELL_WINDOW_ACTION_MAIL_ZOOM_OUT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-zoom-out")

/* Mail Query Actions */
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_ALL_MESSAGES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-all-messages")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_IMPORTANT_MESSAGES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-important-messages")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_LAST_5_DAYS_MESSAGES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-last-5-days-messages")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_MESSAGES_NOT_JUNK(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-messages-not-junk")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-messages-with-attachments")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_MESSAGES_WITH_NOTES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-messages-with-notes")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_NO_LABEL(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-no-label")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_READ_MESSAGES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-read-messages")
#define E_SHELL_WINDOW_ACTION_MAIL_FILTER_UNREAD_MESSAGES(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-filter-unread-messages")
#define E_SHELL_WINDOW_ACTION_MAIL_SCOPE_ALL_ACCOUNTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-scope-all-accounts")
#define E_SHELL_WINDOW_ACTION_MAIL_SCOPE_CURRENT_ACCOUNT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-scope-current-account")
#define E_SHELL_WINDOW_ACTION_MAIL_SCOPE_CURRENT_FOLDER(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-scope-current-folder")
#define E_SHELL_WINDOW_ACTION_MAIL_SCOPE_CURRENT_MESSAGE(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-scope-current-message")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_BODY_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-body-contains")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_FREE_FORM_EXPR(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-free-form-expr")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_MESSAGE_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-message-contains")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_RECIPIENTS_CONTAIN(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-recipients-contain")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_SENDER_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-sender-contains")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_SUBJECT_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-subject-contains")
#define E_SHELL_WINDOW_ACTION_MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-search-subject-or-addresses-contain")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_MAIL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail")
#define E_SHELL_WINDOW_ACTION_GROUP_MAIL_FILTER(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail-filter")
#define E_SHELL_WINDOW_ACTION_GROUP_MAIL_LABEL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail-label")
#define E_SHELL_WINDOW_ACTION_GROUP_SEARCH_FOLDERS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "search-folders")

#endif /* E_MAIL_SHELL_VIEW_ACTIONS_H */
