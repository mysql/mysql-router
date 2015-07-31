# Add custom target to build reference documentation

find_package(Doxygen)

if (DOXYGEN_FOUND)
  configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile)
  add_custom_target(docs
    ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating Doxygen documentation" VERBATIM)
else()
  message(WARNING "Doxygen not found, no documentation target will be created")
endif()


