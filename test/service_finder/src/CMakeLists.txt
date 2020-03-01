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
add_executable(test_service_finder
               test_service_finder.cpp)

target_link_libraries(test_service_finder PRIVATE lssdpcpp)
add_test(NAME test_service_finder_test
         COMMAND $<TARGET_FILE:test_service_finder>)
set_target_properties(test_service_finder PROPERTIES FOLDER tests)
set_property(TARGET test_service_finder PROPERTY CXX_STANDARD 14)