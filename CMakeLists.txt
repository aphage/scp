cmake_minimum_required(VERSION 3.15)

project(scp VERSION 0.0.1)

add_executable(scp main.cc)

set_property(TARGET scp PROPERTY C_STANDARD 23)
set_property(TARGET scp PROPERTY CXX_STANDARD 20)