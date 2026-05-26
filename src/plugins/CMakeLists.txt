foreach(plugin ${build_plugins})
	add_subdirectory(${plugin})
endforeach(plugin)

# Converted EExtension modules (built unconditionally)
add_subdirectory(attachment-reminder)
add_subdirectory(sender-validation)
