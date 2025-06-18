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

#include <shell/e-shell-view.h>

/* Mail Actions */
#define E_SHELL_VIEW_ACTION_MAIL_ACCOUNT_DISABLE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-account-disable")
#define E_SHELL_VIEW_ACTION_MAIL_ACCOUNT_EXPUNGE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-account-expunge")
#define E_SHELL_VIEW_ACTION_MAIL_ACCOUNT_EMPTY_JUNK(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-account-empty-junk")
#define E_SHELL_VIEW_ACTION_MAIL_ACCOUNT_PROPERTIES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-account-properties")
#define E_SHELL_VIEW_ACTION_MAIL_ACCOUNT_REFRESH(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-account-refresh")
#define E_SHELL_VIEW_ACTION_MAIL_ADD_SENDER(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-add-sender")
#define E_SHELL_VIEW_ACTION_MAIL_ATTACHMENT_BAR(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-attachment-bar")
#define E_SHELL_VIEW_ACTION_MAIL_CARET_MODE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-caret-mode")
#define E_SHELL_VIEW_ACTION_MAIL_CHECK_FOR_JUNK(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-check-for-junk")
#define E_SHELL_VIEW_ACTION_MAIL_CLIPBOARD_COPY(view) \
	E_SHELL_WINDOw_ACTION ((view), "mail-clipboard-copy")
#define E_SHELL_VIEW_ACTION_MAIL_COPY(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-copy")
#define E_SHELL_VIEW_ACTION_MAIL_CREATE_SEARCH_FOLDER(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-create-search-folder")
#define E_SHELL_VIEW_ACTION_MAIL_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-delete")
#define E_SHELL_VIEW_ACTION_MAIL_DOWNLOAD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-download")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_RULE_FOR_MAILING_LIST(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-rule-for-mailing-list")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_RULE_FOR_RECIPIENTS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-rule-for-recipients")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_RULE_FOR_SENDER(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-rule-for-sender")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_RULE_FOR_SUBJECT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-rule-for-subject")
#define E_SHELL_VIEW_ACTION_MAIL_FILTERS_APPLY(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filters-apply")
#define E_SHELL_VIEW_ACTION_MAIL_FIND(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-find")
#define E_SHELL_VIEW_ACTION_MAIL_FLAG_CLEAR(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-flag-clear")
#define E_SHELL_VIEW_ACTION_MAIL_FLAG_COMPLETED(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-flag-completed")
#define E_SHELL_VIEW_ACTION_MAIL_FLAG_FOR_FOLLOWUP(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-flag-for-followup")
#define E_SHELL_VIEW_ACTION_MAIL_FLUSH_OUTBOX(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-flush-outbox")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_COPY(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-copy")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-delete")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_EDIT_SORT_ORDER(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-edit-sort-order")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_EXPUNGE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-expunge")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_MARK_ALL_AS_READ(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-mark-all-as-read")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_MOVE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-move")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-new")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_NEW_FULL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-new-full")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_PROPERTIES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-properties")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_REFRESH(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-refresh")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_RENAME(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-rename")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_SELECT_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-select-all")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_SELECT_THREAD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-select-thread")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_SELECT_SUBTHREAD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-select-subthread")
#define E_SHELL_VIEW_ACTION_MAIL_FOLDER_UNSUBSCRIBE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-folder-unsubscribe")
#define E_SHELL_VIEW_ACTION_MAIL_FORWARD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-forward")
#define E_SHELL_VIEW_ACTION_MAIL_FORWARD_ATTACHED(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-forward-attached")
#define E_SHELL_VIEW_ACTION_MAIL_FORWARD_INLINE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-forward-inline")
#define E_SHELL_VIEW_ACTION_MAIL_FORWARD_QUOTED(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-forward-quoted")
#define E_SHELL_VIEW_ACTION_MAIL_LOAD_IMAGES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-load-images")
#define E_SHELL_VIEW_ACTION_MAIL_MANAGE_SUBSCRIPTIONS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-manage-subscriptions")
#define E_SHELL_VIEW_ACTION_MAIL_MARK_IMPORTANT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-mark-important")
#define E_SHELL_VIEW_ACTION_MAIL_MARK_JUNK(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-mark-junk")
#define E_SHELL_VIEW_ACTION_MAIL_MARK_NOTJUNK(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-mark-notjunk")
#define E_SHELL_VIEW_ACTION_MAIL_MARK_READ(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-mark-read")
#define E_SHELL_VIEW_ACTION_MAIL_MARK_UNIMPORTANT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-mark-unimportant")
#define E_SHELL_VIEW_ACTION_MAIL_MARK_UNREAD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-mark-unread")
#define E_SHELL_VIEW_ACTION_MAIL_MESSAGE_EDIT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-message-edit")
#define E_SHELL_VIEW_ACTION_MAIL_MESSAGE_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-message-new")
#define E_SHELL_VIEW_ACTION_MAIL_MESSAGE_OPEN(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-message-open")
#define E_SHELL_VIEW_ACTION_MAIL_MOVE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-move")
#define E_SHELL_VIEW_ACTION_MAIL_NEXT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-next")
#define E_SHELL_VIEW_ACTION_MAIL_NEXT_IMPORTANT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-next-important")
#define E_SHELL_VIEW_ACTION_MAIL_NEXT_THREAD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-next-thread")
#define E_SHELL_VIEW_ACTION_MAIL_NEXT_UNREAD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-next-unread")
#define E_SHELL_VIEW_ACTION_MAIL_POPUP_FOLDER_MARK_ALL_AS_READ(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-popup-folder-mark-all-as-read")
#define E_SHELL_VIEW_ACTION_MAIL_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-preview")
#define E_SHELL_VIEW_ACTION_MAIL_PREVIOUS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-previous")
#define E_SHELL_VIEW_ACTION_MAIL_PREVIOUS_IMPORTANT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-previous-important")
#define E_SHELL_VIEW_ACTION_MAIL_PREVIOUS_UNREAD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-previous-unread")
#define E_SHELL_VIEW_ACTION_MAIL_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-print")
#define E_SHELL_VIEW_ACTION_MAIL_PRINT_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-print-preview")
#define E_SHELL_VIEW_ACTION_MAIL_REDIRECT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-redirect")
#define E_SHELL_VIEW_ACTION_MAIL_REPLY_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-reply-all")
#define E_SHELL_VIEW_ACTION_MAIL_REPLY_LIST(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-reply-list")
#define E_SHELL_VIEW_ACTION_MAIL_REPLY_SENDER(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-reply-sender")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_ADVANCED_HIDDEN(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-advanced-hidden")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_FOLDER_FROM_MAILING_LIST(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-folder-from-mailing-list")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_FOLDER_FROM_RECIPIENTS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-folder-from-recipients")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_FOLDER_FROM_SENDER(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-folder-from-sender")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_FOLDER_FROM_SUBJECT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-folder-from-subject")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_FREE_FORM_EXPR(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-free-form-expr")
#define E_SHELL_VIEW_ACTION_MAIL_SELECT_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-select-all")
#define E_SHELL_VIEW_ACTION_MAIL_SEND_RECEIVE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-send-receive")
#define E_SHELL_VIEW_ACTION_MAIL_SEND_RECEIVE_RECEIVE_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-send-receive-receive-all")
#define E_SHELL_VIEW_ACTION_MAIL_SEND_RECEIVE_SEND_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-send-receive-send-all")
#define E_SHELL_VIEW_ACTION_MAIL_SHOW_ALL_HEADERS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-show-all-headers")
#define E_SHELL_VIEW_ACTION_MAIL_SHOW_DELETED(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-show-deleted")
#define E_SHELL_VIEW_ACTION_MAIL_SHOW_JUNK(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-show-junk")
#define E_SHELL_VIEW_ACTION_MAIL_SHOW_PREVIEW_TOOLBAR(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-show-preview-toolbar")
#define E_SHELL_VIEW_ACTION_MAIL_SHOW_SOURCE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-show-source")
#define E_SHELL_VIEW_ACTION_MAIL_SMART_BACKWARD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-smart-backward")
#define E_SHELL_VIEW_ACTION_MAIL_SMART_FORWARD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-smart-forward")
#define E_SHELL_VIEW_ACTION_MAIL_STOP(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-stop")
#define E_SHELL_VIEW_ACTION_MAIL_THREADS_COLLAPSE_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-threads-collapse-all")
#define E_SHELL_VIEW_ACTION_MAIL_THREADS_EXPAND_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-threads-expand-all")
#define E_SHELL_VIEW_ACTION_MAIL_THREADS_GROUP_BY(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-threads-group-by")
#define E_SHELL_VIEW_ACTION_MAIL_TOOLS_FILTERS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-tools-filters")
#define E_SHELL_VIEW_ACTION_MAIL_TOOLS_SEARCH_FOLDERS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-tools-search-folders")
#define E_SHELL_VIEW_ACTION_MAIL_TOOLS_SUBSCRIPTIONS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-tools-subscriptions")
#define E_SHELL_VIEW_ACTION_MAIL_TO_DO_BAR(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-to-do-bar")
#define E_SHELL_VIEW_ACTION_MAIL_UNDELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-undelete")
#define E_SHELL_VIEW_ACTION_MAIL_VIEW_CLASSIC(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-view-classic")
#define E_SHELL_VIEW_ACTION_MAIL_VIEW_VERTICAL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-view-vertical")
#define E_SHELL_VIEW_ACTION_MAIL_ZOOM_100(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-zoom-100")
#define E_SHELL_VIEW_ACTION_MAIL_ZOOM_IN(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-zoom-in")
#define E_SHELL_VIEW_ACTION_MAIL_ZOOM_OUT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-zoom-out")

/* Mail Query Actions */
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_ALL_MESSAGES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-all-messages")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_IMPORTANT_MESSAGES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-important-messages")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_LAST_5_DAYS_MESSAGES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-last-5-days-messages")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_MESSAGE_THREAD(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-message-thread")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_MESSAGES_NOT_JUNK(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-messages-not-junk")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-messages-with-attachments")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_MESSAGES_WITH_NOTES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-messages-with-notes")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_NO_LABEL(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-no-label")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_READ_MESSAGES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-read-messages")
#define E_SHELL_VIEW_ACTION_MAIL_FILTER_UNREAD_MESSAGES(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-filter-unread-messages")
#define E_SHELL_VIEW_ACTION_MAIL_SCOPE_ALL_ACCOUNTS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-scope-all-accounts")
#define E_SHELL_VIEW_ACTION_MAIL_SCOPE_CURRENT_ACCOUNT(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-scope-current-account")
#define E_SHELL_VIEW_ACTION_MAIL_SCOPE_CURRENT_FOLDER(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-scope-current-folder")
#define E_SHELL_VIEW_ACTION_MAIL_SCOPE_CURRENT_FOLDER_AND_SUBFOLDERS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-scope-current-folder-and-subfolders")
#define E_SHELL_VIEW_ACTION_MAIL_SCOPE_CURRENT_MESSAGE(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-scope-current-message")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_BODY_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-body-contains")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_FREE_FORM_EXPR(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-free-form-expr")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_MESSAGE_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-message-contains")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_RECIPIENTS_CONTAIN(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-recipients-contain")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_SENDER_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-sender-contains")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_SUBJECT_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-subject-contains")
#define E_SHELL_VIEW_ACTION_MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN(view) \
	E_SHELL_VIEW_ACTION ((view), "mail-search-subject-or-addresses-contain")

/* Action Groups */
#define E_SHELL_VIEW_ACTION_GROUP_MAIL(view) \
	E_SHELL_VIEW_ACTION_GROUP ((view), "mail")
#define E_SHELL_VIEW_ACTION_GROUP_MAIL_FILTER(view) \
	E_SHELL_VIEW_ACTION_GROUP ((view), "mail-filter")
#define E_SHELL_VIEW_ACTION_GROUP_SEARCH_FOLDERS(view) \
	E_SHELL_VIEW_ACTION_GROUP ((view), "search-folders")

#endif /* E_MAIL_SHELL_VIEW_ACTIONS_H */
