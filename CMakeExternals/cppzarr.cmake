#-----------------------------------------------------------------------------
# ITK
#-----------------------------------------------------------------------------

# Sanity checks
if(DEFINED cppzarr_DIR AND NOT EXISTS ${cppzarr_DIR})
  message(FATAL_ERROR "cppzarr_DIR variable is defined but corresponds to non-existing directory")
endif()

set(proj cppzarr)
mitk_query_custom_ep_vars()

set(cppzarr_DEPENDS ${proj})

if(NOT DEFINED cppzarr_DIR)

  
  ExternalProject_Add(${proj}
     LIST_SEPARATOR ${sep}
     UPDATE_COMMAND ""
     GIT_REPOSITORY https://github.com/abcucberkeley/cpp-zarr.git
     GIT_TAG f89660d
     CMAKE_GENERATOR ${gen}
     CMAKE_GENERATOR_PLATFORM ${gen_platform}
     CMAKE_ARGS
       ${ep_common_args}
       ${additional_cmake_args}
       ${${proj}_CUSTOM_CMAKE_ARGS}
     CMAKE_CACHE_ARGS
       ${ep_common_cache_args}
       ${${proj}_CUSTOM_CMAKE_CACHE_ARGS}
     CMAKE_CACHE_DEFAULT_ARGS
       ${ep_common_cache_default_args}
       ${${proj}_CUSTOM_CMAKE_CACHE_DEFAULT_ARGS}
     DEPENDS ${proj_DEPENDENCIES}
    )

  set(cppzarr_DIR ${ep_prefix})
  mitkFunctionInstallExternalCMakeProject(${proj})

else()

  mitkMacroEmptyExternalProject(${proj} "${proj_DEPENDENCIES}")

endif()
