cmake_minimum_required (VERSION 2.8.3)
project (pdfCleanse)

#set_property(TARGET pdfCleanse PROPERTY CXX_STANDARD 11)
add_definitions(-std=c++11)
add_definitions(-g)

include_directories (. include/
    "/home/hitech/Downloads/VoterLists/browsers/podofo-install/include//"
    "/home/hitech/Downloads/VoterLists/browsers/podofo-install/include/podofo/base/"
    "/home/hitech/Downloads/VoterLists/browsers/podofo-install/include/podofo/doc/"
    "/usr/include/qt4/"
)
#  CMAKE_LIBRARY_PATH
link_directories (
    "/home/hitech/Downloads/VoterLists/browsers/podofo-install/lib"
)
#add_subdirectory (MathFunctions) 
 
# add the executable
add_executable(pdfCleanse pdfCleanse.cpp)
target_link_libraries (pdfCleanse podofo freetype fontconfig jpeg z pthread unistring crypto idn )

