project("emulationstation")

set(ES_HEADERS	
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EmulationStation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileSorts.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MetaData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlatformId.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemData.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Gamelist.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Genres.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileFilterIndex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemScreenSaver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CollectionSystemManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NetworkThread.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ContentInstaller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ThreadedScraper.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadedHasher.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadedBluetooth.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/SystemRandomPlaylist.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/LangParser.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/KeyboardMapping.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpServerThread.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpApi.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/httplib.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/RetroAchievements.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaveState.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaveStateRepository.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SaveStateConfigFile.h    
	${CMAKE_CURRENT_SOURCE_DIR}/src/CustomFeatures.h

    # GuiComponents    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/components/ScraperSearchComponent.h    

    # Guis    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMetaDataEd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameScraper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameOptions.h    
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGamelistOptions.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGeneralScreensaverOptions.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMenu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSettings.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiScraperStart.h		
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiHashStart.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGamelistFilter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiCollectionSystemsOptions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBackupStart.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInstallStart.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiUpdate.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiUpdate.cpp # batocera    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBackup.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBackup.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInstall.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInstall.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiLoading.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiThemeInstaller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBezelInstaller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiNetPlay.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiNetPlaySettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiRetroAchievements.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiRetroAchievementsSettings.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSystemInformation.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothDevices.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothPair.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothDeviceOptions.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiWifi.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiImageViewer.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBatoceraStore.h # batocera	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBios.h # batocera	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiControllersSettings.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyMappingEditor.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyboardLayout.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameAchievements.h		
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSaveState.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiFileBrowser.h	
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiFavoriteMusicSelector.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMoonlight.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiAnalogSticksLedControls.h

    # Scrapers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/Scraper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/GamesDBJSONScraper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/GamesDBJSONScraperResources.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ArcadeDBJSONScraper.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/HfsDBScraper.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/IGDBScraper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScreenScraper.h    

    # Views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/BasicGameListView.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/CarouselGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/DetailedGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/IGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/ISimpleGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/GridGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/VideoGameListView.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/DetailedContainer.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/GameNameFormatter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/SystemView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/ViewController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/UIModeController.h

    # Animations
    ${CMAKE_CURRENT_SOURCE_DIR}/src/animations/LaunchAnimation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/animations/MoveCameraAnimation.h

    ${CMAKE_CURRENT_SOURCE_DIR}/src/ApiSystem.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LibretroRatio.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32ApiSystem.h # batocera
)

set(ES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileSorts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MetaData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlatformId.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemData.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Gamelist.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Genres.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileFilterIndex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemScreenSaver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CollectionSystemManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NetworkThread.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ContentInstaller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ThreadedScraper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadedHasher.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadedBluetooth.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SystemRandomPlaylist.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/LangParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/KeyboardMapping.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpServerThread.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpApi.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/RetroAchievements.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaveState.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaveStateRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SaveStateConfigFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/CustomFeatures.cpp	

    # GuiComponents    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/components/ScraperSearchComponent.cpp	

    # Guis    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMetaDataEd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameScraper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGamelistOptions.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGeneralScreensaverOptions.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMenu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSettings.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiScraperStart.cpp		
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiHashStart.cpp	
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGamelistFilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiCollectionSystemsOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBackupStart.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInstallStart.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiUpdate.cpp # batocera    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBackup.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInstall.cpp # batocera    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiThemeInstaller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBezelInstaller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiNetPlay.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiNetPlaySettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiRetroAchievements.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiRetroAchievementsSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSystemInformation.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothDevices.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothPair.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothDeviceOptions.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiWifi.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiImageViewer.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBatoceraStore.cpp # batocera	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBios.cpp # batocera	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiControllersSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyMappingEditor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyboardLayout.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameAchievements.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSaveState.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiFileBrowser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiFavoriteMusicSelector.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMoonlight.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiAnalogSticksLedControls.cpp

    # Scrapers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/Scraper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/GamesDBJSONScraper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/GamesDBJSONScraperResources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ArcadeDBJSONScraper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/HfsDBScraper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/IGDBScraper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScreenScraper.cpp	

    # Views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/BasicGameListView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/CarouselGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/DetailedGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/IGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/ISimpleGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/GridGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/VideoGameListView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/DetailedContainer.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/GameNameFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/SystemView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/ViewController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/UIModeController.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/ApiSystem.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LibretroRatio.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/Win32ApiSystem.cpp # batocera
)

#-------------------------------------------------------------------------------
# define OS specific sources and headers
if(MSVC)
    LIST(APPEND ES_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EmulationStation.rc
    )
endif()

# Keep Directory structure in Visual Studio
if(MSVC)
	file(
		 GLOB_RECURSE source_list RELATIVE
		 "${CMAKE_CURRENT_SOURCE_DIR}"
		 *.cpp *.c
		)
	foreach(source IN LISTS source_list)
		get_filename_component(source_path "${source}" PATH)
		string(REPLACE "/" "\\" source_path_msvc "${source_path}")
		string(REPLACE "src" "" source_path_final "${source_path_msvc}")
		source_group("${source_path_final}" FILES "${source}")
	endforeach()  	
endif()

#-------------------------------------------------------------------------------
# define target
include_directories(${COMMON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(emulationstation ${ES_SOURCES} ${ES_HEADERS})
target_link_libraries(emulationstation ${COMMON_LIBRARIES} es-core)

# special properties for Windows builds
if(MSVC)
    # Always compile with the "WINDOWS" subsystem to avoid console window flashing at startup
    # when --debug is not set (see es-core/src/main.cpp for explanation).
    # The console will still be shown if launched with --debug.
    # Note that up to CMake 2.8.10 this feature is broken: http://public.kitware.com/Bug/view.php?id=12566
    set_target_properties(emulationstation PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:WINDOWS")
    set_target_properties(emulationstation PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS")
    set_target_properties(emulationstation PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    set_target_properties(emulationstation PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
endif()


#-------------------------------------------------------------------------------
# set up CPack install stuff so `make install` does something useful

install(TARGETS emulationstation
    RUNTIME
    DESTINATION bin)

INCLUDE(InstallRequiredSystemLibraries)

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A flexible graphical emulator front-end")
SET(CPACK_PACKAGE_DESCRIPTION "EmulationStation is a flexible, graphical front-end designed for keyboardless navigation of your multi-platform retro game collection.")

SET(CPACK_RESOURCE_FILE LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
SET(CPACK_RESOURCE_FILE README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Alec Lofquist <allofquist@yahoo.com>")
SET(CPACK_DEBIAN_PACKAGE_SECTION "misc")
SET(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libsdl2-2.0-0, libsdl2-mixer-2.0-0, libfreeimage3, libfreetype6, libcurl3, libasound2")
SET(CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS "debhelper (>= 8.0.0), cmake, g++ (>= 4.8), libsdl2-dev, libsdl2-mixer-dev, libfreeimage-dev, libfreetype6-dev, libcurl4-openssl-dev, libasound2-dev, libgl1-mesa-dev, rapidjson-dev, libgl1-mesa-dev")

SET(CPACK_PACKAGE_VENDOR "emulationstation.org")
SET(CPACK_PACKAGE_VERSION "2.0.0~rc1")
SET(CPACK_PACKAGE_VERSION_MAJOR "2")
SET(CPACK_PACKAGE_VERSION_MINOR "0")
SET(CPACK_PACKAGE_VERSION_PATCH "0")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "emulationstation_${CMAKE_PACKAGE_VERSION}")
SET(CPACK_PACKAGE_EXECUTABLES "emulationstation" "emulationstation")

SET(CPACK_GENERATOR "TGZ;DEB")

INCLUDE(CPack)
