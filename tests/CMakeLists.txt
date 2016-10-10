set(BEHAVE_INSTALLED_TESTS
	addressbook_contacts
	contacts_shortcuts
	general_shortcuts
	mail_shortcuts
	memos_shortcuts
	view_shortcuts
)

install_behave_tests_if_enabled(BEHAVE_INSTALLED_TESTS session-exclusive "")
