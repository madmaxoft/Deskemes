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





add_subdirectory(Lib/PolarSSL     EXCLUDE_FROM_ALL)
add_subdirectory(Lib/PolarSSL-cpp EXCLUDE_FROM_ALL)




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
	Src/Comm/AdbAppInstaller.cpp
	Src/Comm/AdbCommunicator.cpp
	Src/Comm/Connection.cpp
	Src/Comm/ConnectionMgr.cpp
	Src/Comm/DetectedDevices.cpp
	Src/Comm/TcpListener.cpp
	Src/Comm/UdpBroadcaster.cpp
	Src/Comm/UsbDeviceEnumerator.cpp

	# The Comm Channel protocols:
	Src/Comm/Channels/ChannelSmsSend.cpp
	Src/Comm/Channels/InfoChannel.cpp

	# DB-related sources:
	Src/DB/Database.cpp
	Src/DB/DatabaseBackup.cpp
	Src/DB/DatabaseUpgrade.cpp
	Src/DB/DevicePairings.cpp
	Src/DB/DeviceBlacklist.cpp

	# The UI sources:
	Src/UI/DetectedDevicesModel.cpp
	Src/UI/DlgSendText.cpp
	Src/UI/NewDeviceWizard.cpp
	Src/UI/WgtDevice.cpp
	Src/UI/WgtImage.cpp
	Src/UI/WndDevices.cpp

	# The Wizard pages:
	Src/UI/Pages/PgDeviceList.cpp
	Src/UI/Pages/PgNeedApp.cpp
	Src/UI/Pages/PgNeedAuth.cpp
	Src/UI/Pages/PgPairConfirm.cpp
	Src/UI/Pages/PgPairInit.cpp
	Src/UI/Pages/PgPairingInProgress.cpp
	Src/UI/Pages/PgSucceeded.cpp

	Src/BackgroundTasks.cpp
	Src/ComponentCollection.cpp
	Src/DebugLogger.cpp
	Src/Device.cpp
	Src/DeviceMgr.cpp
	Src/InstallConfiguration.cpp
	Src/Logger.cpp
	Src/main.cpp
	Src/MultiLogger.cpp
	Src/Settings.cpp
	Src/Utils.cpp
)

set(HEADERS
	# The Communication-related headers:
	Src/Comm/AdbCommunicator.hpp
	Src/Comm/AdbAppInstaller.hpp
	Src/Comm/Connection.hpp
	Src/Comm/ConnectionMgr.hpp
	Src/Comm/DetectedDevices.hpp
	Src/Comm/TcpListener.hpp
	Src/Comm/UdpBroadcaster.hpp
	Src/Comm/UsbDeviceEnumerator.hpp

	# The Comm Channel protocols:
	Src/Comm/Channels/ChannelSmsSend.hpp
	Src/Comm/Channels/InfoChannel.hpp

	# DB-related headers:
	Src/DB/Database.hpp
	Src/DB/DatabaseBackup.hpp
	Src/DB/DatabaseUpgrade.hpp
	Src/DB/DevicePairings.hpp
	Src/DB/DeviceBlacklist.hpp

	# The UI headers:
	Src/UI/DetectedDevicesModel.hpp
	Src/UI/DlgSendText.hpp
	Src/UI/NewDeviceWizard.hpp
	Src/UI/WgtDevice.hpp
	Src/UI/WgtImage.hpp
	Src/UI/WndDevices.hpp

	# The Wizard pages:
	Src/UI/Pages/PgDeviceList.hpp
	Src/UI/Pages/PgNeedApp.hpp
	Src/UI/Pages/PgNeedAuth.hpp
	Src/UI/Pages/PgPairConfirm.hpp
	Src/UI/Pages/PgPairInit.hpp
	Src/UI/Pages/PgPairingInProgress.hpp
	Src/UI/Pages/PgSucceeded.hpp

	Src/BackgroundTasks.hpp
	Src/ComponentCollection.hpp
	Src/DebugLogger.hpp
	Src/Device.hpp
	Src/DeviceMgr.hpp
	Src/Exception.hpp
	Src/InstallConfiguration.hpp
	Src/Logger.hpp
	Src/MultiLogger.hpp
	Src/Optional.hpp
	Src/Settings.hpp
	Src/Utils.hpp
)

set(UI
	Src/UI/DlgSendText.ui
	Src/UI/WgtDevice.ui
	Src/UI/WndDevices.ui

	Src/UI/Pages/PgDeviceList.ui
	Src/UI/Pages/PgNeedApp.ui
	Src/UI/Pages/PgNeedAuth.ui
	Src/UI/Pages/PgPairConfirm.ui
	Src/UI/Pages/PgPairInit.ui
	Src/UI/Pages/PgPairingInProgress.ui
)

set(RESOURCES
	Res/Deskemes.qrc
	Res/img/usb.png
	Res/img/wifi.png
	Res/img/bluetooth.png
)

set(DEVDOCS
	../DevDocs/Architecture.md
	../DevDocs/Connection.md
	"../DevDocs/Channel - Info.md"
	"../DevDocs/Channel - SMS send.md"
)

if(WIN32)
	list(APPEND RESOURCES
		Res/win.rc
		Res/img/Deskemes.ico
	)
endif()

set(TSFILES
	Translations/Deskemes_cs.ts
)





# Special handling: The WgtImage.hpp file needs to be accessible from ui_PgPairConfirm.h, included from PgPairConfirm.cpp:
set_source_files_properties (
	Src/UI/Pages/PgPairConfirm.cpp
	PROPERTIES COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/Src/UI"
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
