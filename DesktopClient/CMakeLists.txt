cmake_minimum_required(VERSION 3.1)

project(Deskemes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

find_package(Qt5 COMPONENTS Widgets Network Sql Xml LinguistTools REQUIRED)





if(MSVC)
	# Make build use multiple threads under MSVC:
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

	# Make release builds use link-time code generation:
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
	set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   /GL")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}    /LTCG")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
	set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
	set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")

	option(FORCE_STATIC_RUNTIME "Force MSVC to use static runtime(use with static builds)" FALSE)
	if(FORCE_STATIC_RUNTIME)
		message(STATUS "Forcing a static runtime")
		string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

		# Precompiled Ffmpeg libs seem to trigger a SAFESEH failure, fix it:
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SAFESEH:NO")

		# For some reason the Qt modules don't have their dependencies on other libraries set right, we need to explicitly add them:
		set(QT_LIB_PATH ${CMAKE_PREFIX_PATH}/lib)
		set(ADDITIONAL_LIBRARIES
			ws2_32.lib
			opengl32.lib
			imm32.lib
			winmm.lib
			iphlpapi.lib
			crypt32.lib
			netapi32.lib
			userenv.lib
			version.lib
			dwmapi.lib
			strmiids.lib
			${QT_LIB_PATH}/qtfreetype.lib
			${QT_LIB_PATH}/qtharfbuzz.lib
			${QT_LIB_PATH}/qtlibpng.lib
			${QT_LIB_PATH}/qtpcre2.lib
			${QT_LIB_PATH}/Qt5FontDatabaseSupport.lib
			${QT_LIB_PATH}/Qt5EventDispatcherSupport.lib
			${QT_LIB_PATH}/Qt5ThemeSupport.lib
			${QT_LIB_PATH}/Qt5AccessibilitySupport.lib
			${CMAKE_PREFIX_PATH}/plugins/audio/qtaudio_windows.lib
			${CMAKE_PREFIX_PATH}/plugins/platforms/qwindows.lib
			${CMAKE_PREFIX_PATH}/plugins/sqldrivers/qsqlite.lib
		)
		add_definitions(-DFORCE_STATIC_RUNTIME)
	endif()

	find_library(ZLIB_RELEASE NAMES zlib z PATHS ENV LIB)
	find_library(ZLIB_DEBUG NAMES zlibd PATHS ENV LIB)

else()  # MSVC

	# Add the preprocessor macros used for distinguishing between debug and release builds(CMake does this automatically for MSVC):
	set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -D_DEBUG")
	set(CMAKE_C_FLAGS_DEBUG      "${CMAKE_C_FLAGS_DEBUG}      -D_DEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE}  -DNDEBUG")
	set(CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE}    -DNDEBUG")

endif()





add_subdirectory(lib/PolarSSL     EXCLUDE_FROM_ALL)
add_subdirectory(lib/PolarSSL-cpp EXCLUDE_FROM_ALL)




# Qt5-specific setup(adapted from https://www.kdab.com/using-cmake-with-qt-5/ ):

# Tell CMake to run moc / uic / rcc when necessary:
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(
	${Qt5Widgets_INCLUDES}
	${Qt5Network_INCLUDES}
	${Qt5Sql_INCLUDES}
	${Qt5Xml_INCLUDES}
	${Qt5Multimedia_INCLUDES}
)
add_definitions(
	${Qt5Widgets_DEFINITIONS}
	${Qt5Network_DEFINITIONS}
	${Qt5Sql_DEFINITIONS}
	${Qt5Xml_DEFINITIONS}
	${Qt5Multimedia_DEFINITIONS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")




# Set up extra compiler warnings:
if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunknown-pragmas")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wundef")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wold-style-cast")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdisabled-optimization")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Winit-self")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpointer-arith")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wswitch")
endif()





# Warn on using deprecated Qt APIs:
add_definitions(-DQT_DEPRECATED_WARNINGS)

# Enable message log context information even in Release builds:
add_definitions(-DQT_MESSAGELOGCONTEXT)





# Source files:
set(SOURCES
	# The Communication-related sources:
	src/Comm/AdbCommunicator.cpp
	src/Comm/Connection.cpp
	src/Comm/ConnectionMgr.cpp
	src/Comm/DetectedDevices.cpp
	src/Comm/TcpListener.cpp
	src/Comm/UdpBroadcaster.cpp
	src/Comm/UsbDeviceEnumerator.cpp

	# The Comm Channel protocols:
	src/Comm/Channels/ChannelSmsSend.cpp
	src/Comm/Channels/InfoChannel.cpp

	# DB-related sources:
	src/DB/Database.cpp
	src/DB/DatabaseBackup.cpp
	src/DB/DatabaseUpgrade.cpp
	src/DB/DevicePairings.cpp
	src/DB/DeviceBlacklist.cpp

	# The UI sources:
	src/UI/DlgSendText.cpp
	src/UI/NewDeviceWizard.cpp
	src/UI/WgtDevice.cpp
	src/UI/WgtImage.cpp
	src/UI/WndDevices.cpp

	# The Wizard pages:
	src/UI/Pages/PgConnectionType.cpp
	src/UI/Pages/PgPairConfirm.cpp
	src/UI/Pages/PgPairInit.cpp
	src/UI/Pages/PgPairingInProgress.cpp
	src/UI/Pages/PgSucceeded.cpp
	src/UI/Pages/PgTcpDeviceList.cpp
	src/UI/Pages/PgUsbDeviceList.cpp

	src/BackgroundTasks.cpp
	src/ComponentCollection.cpp
	src/DebugLogger.cpp
	src/Device.cpp
	src/DeviceMgr.cpp
	src/InstallConfiguration.cpp
	src/main.cpp
	src/Settings.cpp
	src/Utils.cpp
)

set(HEADERS
	# The Communication-related headers:
	src/Comm/AdbCommunicator.hpp
	src/Comm/Connection.hpp
	src/Comm/ConnectionMgr.hpp
	src/Comm/DetectedDevices.hpp
	src/Comm/TcpListener.hpp
	src/Comm/UdpBroadcaster.hpp
	src/Comm/UsbDeviceEnumerator.hpp

	# The Comm Channel protocols:
	src/Comm/Channels/ChannelSmsSend.hpp
	src/Comm/Channels/InfoChannel.hpp

	# DB-related headers:
	src/DB/Database.hpp
	src/DB/DatabaseBackup.hpp
	src/DB/DatabaseUpgrade.hpp
	src/DB/DevicePairings.hpp
	src/DB/DeviceBlacklist.hpp

	# The UI headers:
	src/UI/DlgSendText.hpp
	src/UI/NewDeviceWizard.hpp
	src/UI/WgtDevice.hpp
	src/UI/WgtImage.hpp
	src/UI/WndDevices.hpp

	# The Wizard pages:
	src/UI/Pages/PgConnectionType.hpp
	src/UI/Pages/PgPairConfirm.hpp
	src/UI/Pages/PgPairInit.hpp
	src/UI/Pages/PgPairingInProgress.hpp
	src/UI/Pages/PgSucceeded.hpp
	src/UI/Pages/PgTcpDeviceList.hpp
	src/UI/Pages/PgUsbDeviceList.hpp

	src/BackgroundTasks.hpp
	src/ComponentCollection.hpp
	src/DebugLogger.hpp
	src/Device.hpp
	src/DeviceMgr.hpp
	src/Exception.hpp
	src/InstallConfiguration.hpp
	src/Optional.hpp
	src/Settings.hpp
	src/Utils.hpp
)

set(UI
	src/UI/DlgSendText.ui
	src/UI/WgtDevice.ui
	src/UI/WndDevices.ui

	src/UI/Pages/PgConnectionType.ui
	src/UI/Pages/PgPairConfirm.ui
	src/UI/Pages/PgPairInit.ui
	src/UI/Pages/PgPairingInProgress.ui
	src/UI/Pages/PgTcpDeviceList.ui
	src/UI/Pages/PgUsbDeviceList.ui
)

set(RESOURCES
	res/Deskemes.qrc
	res/img/usb.png
	res/img/wifi.png
	res/img/bluetooth.png
)

set(DEVDOCS
	../DevDocs/Architecture.md
	../DevDocs/Connection.md
	"../DevDocs/Channel - Info.md"
	"../DevDocs/Channel - SMS send.md"
)

if(WIN32)
	list(APPEND RESOURCES
		res/win.rc
		res/img/Deskemes.ico
	)
endif()

set(TSFILES
	translations/Deskemes_cs.ts
)





# Special handling: The WgtImage.hpp file needs to be accessible from ui_PgPairConfirm.h, included from PgPairConfirm.cpp:
set_source_files_properties (
	src/UI/Pages/PgPairConfirm.cpp
	PROPERTIES COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/src/UI"
)





# Process translations(update, compile):
set(ts_lst_file "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ts_lst_file")
set(lst_file_srcs)
foreach(lst_file_src ${SOURCES} ${HEADERS} ${UI})
	set(lst_file_srcs "${lst_file_src}\n${lst_file_srcs}")
endforeach()
file(WRITE ${ts_lst_file} "${lst_file_srcs}")

add_custom_target(tsfiles
	COMMENT "Updating TS files"
	COMMAND "${Qt5_LUPDATE_EXECUTABLE}" ${_lupdate_options} @${ts_lst_file} -ts ${TSFILES}
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ts_byproduct_file"
	DEPENDS ${SOURCES} ${HEADERS} ${TSFILES} VERBATIM
	SOURCES ${TSFILES}
)
set(QMFILES "")
make_directory("${CMAKE_CURRENT_BINARY_DIR}/translations")
foreach(_current_FILE ${TSFILES})
	get_filename_component(_abs_FILE ${_current_FILE} ABSOLUTE)
	get_filename_component(qm ${_abs_FILE} NAME)
	# everything before the last dot has to be considered the file name(including other dots)
	string(REGEX REPLACE "\\.[^.]*$" "" FILE_NAME ${qm})
	set(qm "${CMAKE_CURRENT_BINARY_DIR}/translations/${FILE_NAME}.qm")

	add_custom_command(OUTPUT ${qm}
		COMMENT "Compiling QM files"
		COMMAND ${Qt5_LRELEASE_EXECUTABLE}
		ARGS ${_abs_FILE} -qm ${qm}
		DEPENDS ${_abs_FILE} VERBATIM
	)
	list(APPEND QMFILES ${qm})
endforeach()





add_executable(Deskemes WIN32 MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${UI} ${RESOURCES} ${QMFILES} ${DEVDOCS})

target_link_libraries(Deskemes
	Qt5::Widgets
	Qt5::Network
	Qt5::Sql
	Qt5::Xml
	${ADDITIONAL_LIBRARIES}
	PolarSSL-cpp
)

if(MSVC)
	target_link_libraries(Deskemes
		$<$<CONFIG:Debug>:${ZLIB_DEBUG}>
		$<$<CONFIG:Release>:${ZLIB_RELEASE}>
		$<$<CONFIG:RelWithDebInfo>:${ZLIB_RELEASE}>
		$<$<CONFIG:MinSizeRel>:${ZLIB_RELEASE}>
	)
else()
	target_link_libraries(${PROJECT_NAME}
		z
	)
endif()

add_dependencies(Deskemes tsfiles)
