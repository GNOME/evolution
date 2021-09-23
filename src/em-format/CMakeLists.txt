glib_mkenums(e-mail-formatter-enumtypes e-mail-formatter-enums.h E_MAIL_FORMATTER_ENUMTYPES_H)

set(DEPENDENCIES
	email-engine
	evolution-shell
	evolution-util
)

set(SOURCES
	e-mail-extension-registry.c
	e-mail-inline-filter.c
	e-mail-formatter.c
	e-mail-formatter-print.c
	e-mail-formatter-quote.c
	e-mail-formatter-utils.c
	e-mail-formatter-attachment.c
	e-mail-formatter-audio.c
	e-mail-formatter-error.c
	e-mail-formatter-extension.c
	e-mail-formatter-headers.c
	e-mail-formatter-image.c
	e-mail-formatter-message-rfc822.c
	e-mail-formatter-secure-button.c
	e-mail-formatter-source.c
	e-mail-formatter-text-enriched.c
	e-mail-formatter-text-html.c
	e-mail-formatter-text-plain.c
	e-mail-formatter-print-headers.c
	e-mail-formatter-quote-headers.c
	e-mail-formatter-quote-message-rfc822.c
	e-mail-formatter-quote-text-enriched.c
	e-mail-formatter-quote-text-html.c
	e-mail-formatter-quote-text-plain.c
	e-mail-parser-extension.c
	e-mail-parser.c
	e-mail-parser-application-mbox.c
	e-mail-parser-audio.c
	e-mail-parser-headers.c
	e-mail-parser-image.c
	e-mail-parser-inlinepgp-encrypted.c
	e-mail-parser-inlinepgp-signed.c
	e-mail-parser-message.c
	e-mail-parser-message-deliverystatus.c
	e-mail-parser-message-external.c
	e-mail-parser-message-rfc822.c
	e-mail-parser-multipart-alternative.c
	e-mail-parser-multipart-appledouble.c
	e-mail-parser-multipart-digest.c
	e-mail-parser-multipart-encrypted.c
	e-mail-parser-multipart-mixed.c
	e-mail-parser-multipart-related.c
	e-mail-parser-multipart-signed.c
	e-mail-parser-secure-button.c
	e-mail-parser-source.c
	e-mail-parser-text-enriched.c
	e-mail-parser-text-html.c
	e-mail-parser-text-plain.c
	e-mail-part.c
	e-mail-part-attachment.c
	e-mail-part-audio.c
	e-mail-part-headers.c
	e-mail-part-image.c
	e-mail-part-list.c
	e-mail-part-secure-button.c
	e-mail-part-utils.c
	e-mail-stripsig-filter.c
	${CMAKE_CURRENT_BINARY_DIR}/e-mail-formatter-enumtypes.c
)

if(ENABLE_SMIME)
	list(APPEND DEPENDENCIES
		essmime
		evolution-smime
	)

	list(APPEND SOURCES
		e-mail-parser-application-smime.c
	)
endif(ENABLE_SMIME)

if(ENABLE_MARKDOWN)
	list(APPEND SOURCES
		e-mail-formatter-text-markdown.c
		e-mail-parser-text-markdown.c
	)
endif(ENABLE_MARKDOWN)

set(HEADERS
	e-mail-extension-registry.h
	e-mail-formatter-extension.h
	e-mail-formatter.h
	e-mail-formatter-enums.h
	e-mail-formatter-print.h
	e-mail-formatter-quote.h
	e-mail-formatter-utils.h
	e-mail-inline-filter.h
	e-mail-parser-extension.h
	e-mail-parser.h
	e-mail-part.h
	e-mail-part-attachment.h
	e-mail-part-audio.h
	e-mail-part-headers.h
	e-mail-part-image.h
	e-mail-part-list.h
	e-mail-part-secure-button.h
	e-mail-part-utils.h
	e-mail-stripsig-filter.h
	${CMAKE_CURRENT_BINARY_DIR}/e-mail-formatter-enumtypes.h
)

add_library(evolution-mail-formatter SHARED
	${SOURCES}
	${HEADERS}
)

add_dependencies(evolution-mail-formatter
	${DEPENDENCIES}
)

target_compile_definitions(evolution-mail-formatter PRIVATE
	-DG_LOG_DOMAIN=\"evolution-mail-formatter\"
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"
)

target_compile_options(evolution-mail-formatter PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evolution-mail-formatter PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src/smime/lib
	${CMAKE_SOURCE_DIR}/src/smime/gui
	${CMAKE_CURRENT_SOURCE_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${MARKDOWN_INCLUDE_DIRS}
)

target_link_libraries(evolution-mail-formatter
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${MARKDOWN_LDFLAGS}
)

install(TARGETS evolution-mail-formatter
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/em-format
)
