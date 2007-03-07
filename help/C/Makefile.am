figs = \
	figures/contacts_mainwindow_a.png	\
	figures/evo_account_info_a.png		\
	figures/evo_cal_callout_a.gif		\
	figures/evo_contacteditor_a.png		\
	figures/evo_email_a.png			\
	figures/evo_identity_a.png		\
	figures/evo_mail_callout_a.gif		\
	figures/evo_mail_callout_a.png		\
	figures/evo_newmess_a.png		\
	figures/evo_receive_setup2_a.png	\
	figures/evo_receive_setup_a.png		\
	figures/evo_rule_a.png			\
	figures/evo_send_setup_a.png		\
	figures/evo_timezone_a.png		\
	figures/evo_gwreceive_a.png		\
	figures/evo_imapreceive_a.png		\
	figures/evo_mail_callout_a.eps		\
	figures/evo_maildirreceive_a.png	\
	figures/evo_mboxreceive_a.png		\
	figures/evo_mereceive_a.png		\
	figures/evo_mhreceive_a.png		\
	figures/evo_popreceive_a.png		\
	figures/evo_usereceive_a.png		\
	figures/mail_mainwindow_a.png		\
	figures/calendar.png			\
	figures/config-cal.png			\
	figures/config-mail.png			\
	figures/contact-editor.png		\
	figures/contact.png			\
	figures/exchange-identity.png		\
	figures/exchange-receive-options.png	\
	figures/exchange-receive.png		\
	figures/exchange-delegation.png		\
	figures/filter-new-fig.png		\
	figures/full-1.png			\
	figures/full-2.png			\
	figures/full-3.png			\
	figures/full-4.png			\
	figures/full-5.png			\
	figures/full-6.png			\
	figures/full-7.png			\
	figures/mail-druid-pic.png		\
	figures/mail-inbox.png			\
	figures/mail-threaded.png		\
	figures/mainwindow-pic.png		\
	figures/newmsg.png			\
	figures/replymsg.png			\
	figures/schedule.png			\
	figures/summary.png			\
	figures/vfolder-createrule-fig.png	\
	figures/account_editor_a.png		\
	figures/categories_a.png		\
	figures/delgt-add.png			\
	figures/evo_account_editor_a.png	\
	figures/evo_adv_search_a.png		\
	figures/evo_advsearch_a.png		\
	figures/evo_allday_a.png		\
	figures/evo_cal_a.gif			\
	figures/evo_calender_appointmnt.png	\
	figures/evo_cal_ prop_a.png		\
	figures/evo_edit_rule_a.png		\
	figures/evo_edit_search.png		\
	figures/evo_flag_follow_up_a.png	\
	figures/evo_gwreceiveedit_a.png		\
	figures/evo_gwreceiveoptedit_a.png	\
	figures/evo_identityedit_a.png		\
	figures/evo_import_asst_a.png		\
	figures/evo_mail_a.png			\
	figures/evo_message_filters_a.png	\
	figures/evo_proxyadd_a.png		\
	figures/evo_select_add_folder.png	\
	figures/evo_select_folder.png		\
	figures/evo_sendstatus_a.png		\
	figures/exchg-identity.png		\
	figures/exchng-identity.png		\
	figures/exchng-rec-mail.png		\
	figures/exchng-rec-mails.png		\
	figures/exchng-rec-option.png		\
	figures/exchng-rec-options.png		\
	figures/exchng-settings.png		\
	figures/free_busy.png			\
	figures/free-busy.png			\
	figures/mailer_preferences.png		\
	figures/proxy-cal.png			\
	figures/proxy-login.png			\
	figures/quick_add_a.png			\
	figures/sub-others-folder.png		\
	figures/sub-pub-fold.png

figdir = figures

all: $(docname).xml $(omffile)

doc_in_files = evolution.xml
%-$(BASE_VERSION).xml: $(doc_in_files)
	cp -f $< $@

docname = evolution-$(BASE_VERSION)
lang = C

omf_in_files = evolution-$(lang).omf
%-$(BASE_VERSION)-$(lang).omf: $(omf_in_files)
	cp -f $< $@
omffile = evolution-$(BASE_VERSION)-C.omf

include $(top_srcdir)/help/xmldocs.make
dist-hook: app-dist-hook

CLEANFILES +=			\
	$(docname).xml		\
	omf_timestamp		\
	$(omffile).out		\
	$(omffile)

EXTRA_DIST +=			\
	$(doc_in_files)		\
	$(omf_in_files)

