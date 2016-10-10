file(GLOB directories RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

foreach(dir ${directories})
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/quickref.pdf")
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/quickref.pdf
			DESTINATION ${evolutionhelpdir}/quickref/${dir}/
		)
	endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/quickref.pdf")
endforeach(dir)
