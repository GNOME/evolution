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
	figures/vfolder-createrule-fig.png
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

