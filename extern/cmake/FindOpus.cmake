############################################################################
# FindOpus.cmake
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################
#
# Find the opus library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  opus - If the opus library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Opus_FOUND - The opus library has been found
#  Opus_TARGET - The name of the CMake target for the opus library
#
# This module may set the following variable:
#
#  Opus_USE_BUILD_INTERFACE - If the opus library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_Opus_REQUIRED_VARS Opus_TARGET)
set(_Opus_CACHE_VARS ${_Opus_REQUIRED_VARS})

if(TARGET opus)

  set(Opus_TARGET opus)
  set(Opus_USE_BUILD_INTERFACE TRUE)

else()

  find_path(_Opus_INCLUDE_DIRS
          NAMES opus/opus.h
          PATH_SUFFIXES include
          )

  find_library(_Opus_LIBRARY NAMES opus)
  if(_Opus_LIBRARY)
    find_library(_m_LIBRARY NAMES m)
  endif()

  if(_Opus_INCLUDE_DIRS AND _Opus_LIBRARY)
    add_library(opus UNKNOWN IMPORTED)
    if(WIN32)
      set_target_properties(opus PROPERTIES
              INTERFACE_INCLUDE_DIRECTORIES "${_Opus_INCLUDE_DIRS}"
              IMPORTED_IMPLIB "${_Opus_LIBRARY}"
              IMPORTED_LINK_INTERFACE_LIBRARIES "${_m_LIBRARY}"
              )
    else()
      set_target_properties(opus PROPERTIES
              INTERFACE_INCLUDE_DIRECTORIES "${_Opus_INCLUDE_DIRS}"
              IMPORTED_LOCATION "${_Opus_LIBRARY}"
              IMPORTED_LINK_INTERFACE_LIBRARIES "${_m_LIBRARY}"
              )
    endif()

    set(Opus_TARGET opus)
  endif()

endif()

find_package_handle_standard_args(Opus
        REQUIRED_VARS ${_Opus_REQUIRED_VARS}
        )
mark_as_advanced(${_Opus_CACHE_VARS})