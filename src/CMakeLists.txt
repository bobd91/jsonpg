cmake_minimum_required(VERSION 3.30)

project(jsonpg VERSION 1.0.0
        DESCRIPTION "C JSON Parser and Generator"
        LANGUAGES C)

include(GNUInstallDirs)

add_library(jsonpg_static STATIC jsonpg.c)
set_target_properties(jsonpg_static PROPERTIES
        VERSION ${PROJECT_VERSION}
        PUBLIC_HEADER ../include/jsonpg.h)

add_library(jsonpg SHARED jsonpg.c)
set_target_properties(jsonpg PROPERTIES 
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER ../include/jsonpg.h)

configure_file(jsonpg.pc.in jsonpg.pc @ONLY)

add_compile_options(
         -O3 
         -std=c23 
         -Wall 
         -Wextra 
         -Wstrict-prototypes 
         -Wwrite-strings 
         -Wshadow 
         -Winit-self 
         -Wcast-align 
         -Wformat=2 
         -Wmissing-prototypes 
         -Wstrict-overflow=2 
         -Wundef 
         -Wswitch-default 
         -Wconversion 
         -fstack-protector-strong 
         -Wdouble-promotion 
         -Wparentheses 
         -Wformat-overflow 
         -Wunused-macros 
         -Wmissing-variable-declarations 
         -Wcast-qual
 )

install(TARGETS jsonpg jsonpg_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libjsonpg)
install(FILES ../include/jsonpg_def_macros.h ../include/jsonpg_undef_macros.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libjsonpg)
install(FILES ${CMAKE_BINARY_DIR}/jsonpg.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
