#############################################################################################
#
#  Copyright 2020 Pierre Voigtländer (jeanreP)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this 
# software and associated documentation files (the "Software"), to deal in the Software 
# without restriction, including without limitation the rights to use, copy, modify, 
# merge, publish, distribute, sublicense, and/or sell copies of the Software, and to 
# permit persons to whom the Software is furnished to do so, subject to the following 
# conditions:
#
# The above copyright notice and this permission notice shall be included in all copies 
# or substantial portions of the Software.
#  
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
# PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR 
# THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
#############################################################################################
cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(lssdpcpp_enable_tests ON CACHE BOOL "Enables the tests")
project(lssdpcpp-project VERSION 0.1.0)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(src)
if (${lssdpcpp_enable_tests})
   add_subdirectory(test)
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(lssdpcpp-config.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}/lssdpcpp-config.cmake
                              INSTALL_DESTINATION lib/cmake)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/lssdpcpp-config-version.cmake
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lssdpcpp-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/lssdpcpp-config-version.cmake
        DESTINATION lib/cmake)
install(EXPORT lssdpcpp FILE lssdpcpp_targets.cmake DESTINATION lib/cmake)