cmake_minimum_required(VERSION 3.9)
project(MLFProtoLib VERSION 1.0.0 DESCRIPTION "Library for communicating with MegaLeaf (MLF) Controller")

add_library(MLFProtoLib SHARED
    MLFProtoLib.cpp
)

set_target_properties(MLFProtoLib PROPERTIES VERSION ${PROJECT_VERSION})
target_include_directories(MLFProtoLib PRIVATE .)
