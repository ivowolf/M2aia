set(MITK_MODULE_NAME_PREFIX "")

find_package(OpenSlide REQUIRED)
include_directories(${OpenSlide_INCLUDE_DIR})

MITK_CREATE_MODULE(
  DEPENDS MitkCore 
  PACKAGE_DEPENDS PUBLIC ITK|IOImageBase
  ADDITIONAL_LIBS ${OpenSlide_LIBRARY}
  )
