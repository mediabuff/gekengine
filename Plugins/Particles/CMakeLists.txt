get_filename_component(ProjectID ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectID ${ProjectID})

project(${ProjectID})

file(GLOB HEADERS "GEK/${ProjectID}/*.hpp")
file(GLOB SOURCES "*.cpp")
add_library(${ProjectID} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${ProjectID} BEFORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/../system" "${CMAKE_CURRENT_SOURCE_DIR}/../engine" "${CMAKE_CURRENT_SOURCE_DIR}/../components")

target_link_libraries(${ProjectID} Math Utility Shapes signals imgui)

set_target_properties(${ProjectID}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
)