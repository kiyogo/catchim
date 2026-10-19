configure_file("${SOURCE}" "${DESTINATION}/catchim_app.html" COPYONLY)
configure_file("${SOURCE}" "${DESTINATION}/qt-editor.html" COPYONLY)
# Do not overwrite an installed HTML editor with the Qt launcher on a later
# native-UI rebuild. Fresh builds still have a usable Qt fallback.
set(INSTALL_QT_INDEX TRUE)
if(EXISTS "${DESTINATION}/index.html")
    file(READ "${DESTINATION}/index.html" INDEX_CONTENTS)
    if(INDEX_CONTENTS MATCHES "type=\"module\"")
        set(INSTALL_QT_INDEX FALSE)
    endif()
endif()
if(INSTALL_QT_INDEX)
    configure_file("${SOURCE}" "${DESTINATION}/index.html" COPYONLY)
endif()
