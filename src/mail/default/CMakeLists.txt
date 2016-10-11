file(GLOB directories RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

foreach(dir ${directories})
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/Inbox")
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/Inbox
			DESTINATION ${privdatadir}/default/${dir}/mail/local
		)
	endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/Inbox")
endforeach(dir)
