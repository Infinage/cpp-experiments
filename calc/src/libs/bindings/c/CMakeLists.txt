add_library(c_api SHARED calc_c_api.cc)
target_sources(c_api PUBLIC FILE_SET HEADERS FILES "calc_c_api.h")
target_link_libraries(c_api PRIVATE calculator)
set_target_properties(c_api PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(TARGETS c_api EXPORT CalcTargets FILE_SET HEADERS)

set(PY_DST "${CMAKE_CURRENT_BINARY_DIR}/../python")
add_custom_command(TARGET c_api POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PY_DST}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:c_api> ${PY_DST}/calc
)
