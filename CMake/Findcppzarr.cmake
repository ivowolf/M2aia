set(cppzarr_DIR ${MITK_EXTERNAL_PROJECT_PREFIX}/src/cppzarr-build/)

# Look for the header.
find_path(cppzarr_INCLUDE_DIR NAMES zarr.h
  PATH ${MITK_EXTERNAL_PROJECT_PREFIX}/include
  )
mark_as_advanced(cppzarr_INCLUDE_DIR)

# Look for the library.
find_library(cppzarr_LIBRARY 
  NAMES cppZarr libcppZarr
  PATH ${MITK_EXTERNAL_PROJECT_PREFIX}/lib
)
mark_as_advanced(cppzarr_LIBRARY)

# Handle the QUIETLY and REQUIRED arguments and set cppzarr_FOUND true if all
# the listed variables are TRUE.

find_package(PackageHandleStandardArgs)
# FIND_PACKAGE_HANDLE_STANDARD_ARGS(cppzarr DEFAULT_MSG cppzarr_LIBRARY cppzarr_INCLUDE_DIR)

find_package_handle_standard_args(cppzarr
  FOUND_VAR cppzarr_FOUND
  REQUIRED_VARS cppzarr_INCLUDE_DIR cppzarr_LIBRARY
)

if(cppzarr_FOUND)
  set(cppzarr_LIBRARIES ${cppzarr_LIBRARY})
  set(cppzarr_INCLUDE_DIRS ${cppzarr_INCLUDE_DIR})
endif()
