#option(ENABLE_IPO "Enable Interprocedural Optimization, aka Link Time Optimization (LTO)" OFF)
#
#if(ENABLE_IPO)
#	include(CheckIPOSupported)
#	check_ipo_supported(RESULT result OUTPUT output)
#	if(result)
#		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
#		message(STATUS "IPO enabled")
#	else()
#		message(STATUS "IPO is not supported")
#		message(DEBUG "${output}")
#	endif()
#endif()

set_property(TARGET ... PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)