cmake_minimum_required(VERSION 3.19)
project(MSFS2024_SimConnector LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

qt_standard_project_setup(REQUIRES 6.10)

# directory paths
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/header)

# file list
file(GLOB PROJECT_SOURCES CONFIGURE_DEPENDS "${SRC_DIR}/*.cpp")
file(GLOB PROJECT_HEADERS CONFIGURE_DEPENDS "${HEADER_DIR}/*.h")

qt_add_executable(${PROJECT_NAME}
    ${PROJECT_HEADERS}
    ${PROJECT_SOURCES}
)

# --- SimConnect SDK path ---
set(SIMCONNECT_DIR "C:/MSFS 2024 SDK/SimConnect SDK"
    CACHE PATH "Path to the MSFS 2024 SimConnect SDK folder")

# --- SimConnect ---
if(NOT EXISTS "${SIMCONNECT_DIR}/include/SimConnect.h")
    message(FATAL_ERROR "SimConnect.h not found. Set SIMCONNECT_DIR (current: '${SIMCONNECT_DIR}')")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${HEADER_DIR}"
    "${SIMCONNECT_DIR}/include")

# Copy SimConnect.dll next to the .exe
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SIMCONNECT_DIR}/lib/SimConnect.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

target_link_libraries(${PROJECT_NAME}
    PRIVATE Qt::Core
    PRIVATE "${SIMCONNECT_DIR}/lib/SimConnect.lib"
)

# Group files
source_group("Source Files" FILES ${PROJECT_SOURCES})
source_group("Header Files" FILES ${PROJECT_HEADERS})

install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
