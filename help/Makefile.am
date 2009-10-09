# Suppress the Posix / GNU Make Warnings
AUTOMAKE_OPTIONS=-Wno-portability

SUBDIRS = quickref

include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook

DOC_MODULE = evolution
DOC_ENTITIES =
DOC_INCLUDES =
DOC_FIGURES =  \
	figures/attach_reminder_a.png		\
	figures/calendar_preference_display.png \
	figures/categories_a.png 		\
	figures/contacts_mainwindow_a.png 	\
	figures/delgt-add.png 			\
	figures/evo_adv_search_a.png 		\
	figures/evo_attachreminder_plugin.png	\
	figures/evo_backup_prgsbar.png		\
	figures/evo_backup_warning.png		\
	figures/evo_blink.png			\
	figures/evo_cal_callout_a.png 		\
	figures/evo_calender_appointmnt.png 	\
	figures/evo_calstatus_a.png		\
	figures/evo_caltasks_a.png              \
	figures/evo_contacteditor_a.png 	\
	figures/evo_delegate_permission_a.png	\
	figures/evo_dialog-info.png		\
	figures/evo_dialog-warning.png		\
	figures/evo_edit_search.png 		\
	figures/evo_exchng_mapi.png             \
	figures/evo_flag_follow_up_a.png 	\
	figures/evo_gwreceive_a.png 		\
	figures/evo_gwstatustrack.png		\
	figures/evo_imapreceive_a.png 		\
	figures/evo_label_a.png			\
	figures/evo_labels_a.png		\
	figures/evo_mail_a.png 			\
	figures/evo_mail_callout_a.png 		\
	figures/evo_maildirreceive_a.png 	\
	figures/evo_mboxreceive_a.png 		\
	figures/evo_memo_a.png 			\
	figures/evo_mhreceive_a.png 		\
	figures/evo_mail_notifier.png		\
	figures/evo_memo_a.png			\
	figures/evo_newmess_a.png 		\
	figures/evo_newmail.png			\
	figures/evo_popreceive_a.png 		\
	figures/evo_proxyadd_a.png 		\
	figures/evo_rule_a.png 			\
	figures/evo_send_option_a.png 		\
	figures/evo_send_setup_a.png 		\
	figures/evo_sendstatus_a.png 		\
	figures/evo_shd_memo_a.png 		\
	figures/evo_usereceive_a.png 		\
	figures/evo_junk_a.png			\
	figures/evo_offline.png			\
	figures/evo_Wcal_prop_a.png             \
	figures/evolution_contact_preference.png\
	figures/evolution_mail_preference.png	\
	figures/exchng-rec-mails.png 		\
	figures/exchng-rec-options.png 		\
	figures/filter-new-fig.png 		\
	figures/folder_plus.png			\
	figures/folder_size_mapi.png		\
	figures/folder_size_preference.png	\
	figures/google_cal_view.png		\
	figures/groupwise_resend.png		\
	figures/groupwise_resend_retract.png	\
	figures/local_ics_calendar.png		\
	figures/mailer_preferences.png 		\
	figures/meeting.png			\
	figures/minus.png 			\
	figures/plus.png 			\
	figures/quick_add_a.png 		\
	figures/quick_reference.png		\
	figures/stock_search.png 		\
	figures/ver_view_a.png

DOC_LINGUAS = cs de el en_GB es eu fr oc ru sv mk

check:
	xmllint --valid --noout $(top_srcdir)/help/C/evolution.xml

-include $(top_srcdir)/git.mk
