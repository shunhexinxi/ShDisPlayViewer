# Only known, source-controlled configuration templates are copied. Never walk
# an existing project, external map dataset, or cache directory.
if(NOT DEFINED SOURCE OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "SOURCE and OUTPUT are required")
endif()
set(_templates "${SOURCE}/resources/default-project")
set(_files
    Data/Config/systemConfig.xml
    Data/Config/fixed-spatial.xml
    Data/Config/normal-annotations.xml
    Data/Config/squad-annotations.xml
    Data/Config/dynamic-plans.xml
    Data/Config/customLayerConfig.xml
    Data/Config/marksConfig.xml
    Data/Config/plotsConfig.xml
    Data/Config/viewPointConfig.xml
    Data/Config/featureConfig.xml
    Data/earth/FreeEarth_flat.earth
    Data/earth/examples/HttpTms.earth
    Data/earth/examples/HttpXyz.earth
    Data/model/H6/routes/flight_routes.xml
    shdisplay-project.json)
file(MAKE_DIRECTORY "${OUTPUT}")
foreach(_file IN LISTS _files)
    # Do not follow a symlink into another project or overwrite a user's file.
    set(_current "${OUTPUT}")
    if(IS_SYMLINK "${_current}")
        message(FATAL_ERROR "Release output cannot be a symlink")
    endif()
    string(REPLACE "/" ";" _parts "${_file}")
    foreach(_part IN LISTS _parts)
        string(APPEND _current "/${_part}")
        if(IS_SYMLINK "${_current}")
            message(FATAL_ERROR "Release configuration cannot be a symlink: ${_file}")
        endif()
    endforeach()
    if(NOT EXISTS "${OUTPUT}/${_file}")
        get_filename_component(_parent "${OUTPUT}/${_file}" DIRECTORY)
        file(MAKE_DIRECTORY "${_parent}")
        configure_file("${_templates}/${_file}" "${OUTPUT}/${_file}" COPYONLY)
    endif()
endforeach()
# Documentation is maintained by the application; configuration files above are user-owned.
file(MAKE_DIRECTORY "${OUTPUT}/doc")
configure_file("${SOURCE}/docs/USER_GUIDE.pdf" "${OUTPUT}/doc/USER_GUIDE.pdf" COPYONLY)
file(MAKE_DIRECTORY "${OUTPUT}/icons")
foreach(_size 16 24 32 48 64 128 256 512)
    configure_file("${SOURCE}/resources/application/shdisplayviewer-${_size}.png"
        "${OUTPUT}/icons/shdisplayviewer-${_size}.png" COPYONLY)
endforeach()
foreach(_extension svg ico)
    configure_file("${SOURCE}/resources/application/shdisplayviewer.${_extension}"
        "${OUTPUT}/icons/shdisplayviewer.${_extension}" COPYONLY)
endforeach()
configure_file("${SOURCE}/docs/CONFIGURATION.md" "${OUTPUT}/CONFIGURATION.md" COPYONLY)
# Keep relative documentation links usable in the runtime package.
foreach(_doc USER_GUIDE.md USER_GUIDE.html CONFIGURATION.md BUILD.md images/interface-overview.svg)
    get_filename_component(_parent "${OUTPUT}/docs/${_doc}" DIRECTORY)
    file(MAKE_DIRECTORY "${_parent}")
    configure_file("${SOURCE}/docs/${_doc}" "${OUTPUT}/docs/${_doc}" COPYONLY)
endforeach()
foreach(_example README.md dynamic-plans.xml generate.py)
    set(_destination "${OUTPUT}/examples/warehouse-fire/${_example}")
    if(NOT EXISTS "${_destination}")
        file(MAKE_DIRECTORY "${OUTPUT}/examples/warehouse-fire")
        configure_file("${SOURCE}/examples/warehouse-fire/${_example}" "${_destination}" COPYONLY)
    endif()
endforeach()
