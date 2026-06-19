set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_library(psijent SHARED psijent.c)
add_library(psijent_static psijent.c)

if (NOT MSVC)
    target_link_libraries(psijent m)
    target_link_libraries(psijent_static m)
endif ()

if (CMAKE_GENERATOR MATCHES "Makefiles")
    set_target_properties(psijent_static PROPERTIES OUTPUT_NAME psijent)
endif ()

install(TARGETS psijent psijent_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(FILES psijent.h DESTINATION include)
