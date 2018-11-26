cmake_minimum_required (VERSION 2.8.3)
project (ReadPDF)

#set_property(TARGET ReadPDF PROPERTY CXX_STANDARD 11)
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
add_executable(ReadPDF ReadPDF.cpp)
target_link_libraries (ReadPDF podofo freetype fontconfig jpeg z pthread unistring crypto idn )

