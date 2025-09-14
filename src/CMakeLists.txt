cmake_minimum_required(VERSION 3.30)

project(jsonpg VERSION 1.0
        DESCRIPTION "C JSON Parser and Generator"
        LANGUAGES C)

add_library(jsonpg_static STATIC jsonpg.c)

target_include_directories(jsonpg_static PUBLIC include)

add_library(jsonpg SHARED jsonpg.c)

target_include_directories(jsonpg PUBLIC include)

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
