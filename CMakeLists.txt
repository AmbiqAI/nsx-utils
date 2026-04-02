if(NOT DEFINED NSX_BOARD_FLAGS_TARGET)
    message(FATAL_ERROR "Board must define NSX_BOARD_FLAGS_TARGET before ns-utils is added.")
endif()
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before ns-utils is added.")
endif()

set(NSX_UTILS_ARCH_DIR "")
if(NSX_SOC_FAMILY STREQUAL "apollo3" OR NSX_SOC_FAMILY STREQUAL "apollo3p")
    # Apollo3P uses the Apollo3-class CTIMER implementation in AmbiqSuite R3.
    # The apollo330 sources use the newer Apollo4/5-style timer and PMU APIs
    # and should be reserved for a future Apollo330P/Apollo5-class target.
    set(NSX_UTILS_ARCH_DIR "apollo3")
elseif(NSX_SOC_FAMILY STREQUAL "apollo4l" OR NSX_SOC_FAMILY STREQUAL "apollo4p")
    set(NSX_UTILS_ARCH_DIR "apollo4")
elseif(NSX_SOC_FAMILY STREQUAL "apollo5b" OR NSX_SOC_FAMILY STREQUAL "apollo510" OR NSX_SOC_FAMILY STREQUAL "apollo510b")
    set(NSX_UTILS_ARCH_DIR "apollo5")
elseif(NSX_SOC_FAMILY STREQUAL "apollo330P")
    set(NSX_UTILS_ARCH_DIR "apollo330")
else()
    message(FATAL_ERROR "Unsupported NSX_SOC_FAMILY for ns-utils: ${NSX_SOC_FAMILY}")
endif()

file(GLOB nsx_utils_common_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
)
list(FILTER nsx_utils_common_sources EXCLUDE REGEX ".*/ns_malloc\\.c$")
file(GLOB nsx_utils_arch_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_UTILS_ARCH_DIR}/*.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_UTILS_ARCH_DIR}/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_UTILS_ARCH_DIR}/*.cpp
)

if(TARGET nsx_pmu_armv8m)
    list(FILTER nsx_utils_arch_sources EXCLUDE REGEX ".*/ns_pmu_(utils|accumulator)\\.c$")
endif()

add_library(nsx_utils STATIC
    ${nsx_utils_common_sources}
    ${nsx_utils_arch_sources}
)
set_target_properties(nsx_utils PROPERTIES EXPORT_NAME utils)
add_library(nsx::utils ALIAS nsx_utils)

target_link_libraries(nsx_utils PUBLIC ${NSX_BOARD_FLAGS_TARGET} nsx_soc_hal nsx_core)

target_include_directories(nsx_utils PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/${NSX_UTILS_ARCH_DIR}>
    $<BUILD_INTERFACE:${NSX_ROOT}/modules/nsx-core/includes-api>
    $<BUILD_INTERFACE:${NSX_ROOT}/modules/nsx-harness/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-utils/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-utils/src/${NSX_UTILS_ARCH_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-harness/includes-api>
)

target_link_libraries(nsx_runtime_core INTERFACE nsx_utils)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-utils
)
install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_UTILS_ARCH_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-utils/src
)

install(TARGETS
    nsx_utils
    EXPORT nsxTargets
)
