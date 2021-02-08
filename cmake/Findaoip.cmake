find_path(aoip_INCLUDE_DIR NAMES aoip.h)
find_library(aoip_LIBRARY NAMES aoip)
mark_as_advanced(aoip_INCLUDE_DIR aoip_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(aoip
	REQUIRED_VARS
	aoip_LIBRARY
	aoip_INCLUDE_DIR)

if(aoip_FOUND AND NOT TARGET aoip::aoip)
	add_library(aoip::aoip UNKNOWN IMPORTED)
	set_target_properties(aoip::aoip PROPERTIES
		IMPORTED_LINK_INTERFACE_LANGUAGES ["C"|"CXX"]
		IMPORTED_LOCATION "${aoip_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${aoip_INCLUDE_DIR}")
endif()

