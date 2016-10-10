foreach(plugin ${build_plugins})
	add_subdirectory(${plugin})
endforeach(plugin)
