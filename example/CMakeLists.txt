include_directories(../src)

add_executable(psijent-uniform psijent-uniform.cpp)
add_executable(psijent-stream psijent-stream.cpp)

if (MSVC)
    target_link_libraries(psijent-uniform PRIVATE psijent_static)
    target_link_libraries(psijent-stream PRIVATE psijent_static)
else ()
    target_link_libraries(psijent-uniform PRIVATE psijent)
    target_link_libraries(psijent-stream PRIVATE psijent)
endif ()