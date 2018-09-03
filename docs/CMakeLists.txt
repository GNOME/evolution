install(FILES evolution.1
	DESTINATION ${MAN_INSTALL_DIR}/man1
)

if(ENABLE_GTK_DOC)
	add_subdirectory(reference)
endif(ENABLE_GTK_DOC)
