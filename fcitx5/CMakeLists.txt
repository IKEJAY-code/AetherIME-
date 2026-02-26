find_package(Gettext REQUIRED)
find_package(Fcitx5Core REQUIRED)
find_package(Boost 1.61 QUIET)
find_package(LibIMECore QUIET)
find_package(LibIMEPinyin QUIET)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

add_library(aetherime SHARED
  src/aetherime_addon.cpp
  src/daemon_client.cpp
  src/ghost_session.cpp
  src/libime_backend.cpp
)

target_compile_features(aetherime PRIVATE cxx_std_17)
target_include_directories(aetherime PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(aetherime PRIVATE Fcitx5::Core)
if (TARGET LibIME::Pinyin)
  target_link_libraries(aetherime PRIVATE LibIME::Pinyin LibIME::Core)
  target_compile_definitions(aetherime PRIVATE AETHERIME_HAS_LIBIME=1)
  message(STATUS "AetherIME: LibIME Pinyin backend enabled")
else()
  set(AETHERIME_LIBIME_REASON "unknown")
  if (NOT Boost_FOUND)
    set(AETHERIME_LIBIME_REASON "Boost headers not found (install libboost-dev)")
  elseif (NOT TARGET LibIME::Core)
    set(AETHERIME_LIBIME_REASON "LibIME core not found (install libimecore-dev)")
  else()
    set(AETHERIME_LIBIME_REASON "LibIME pinyin not found (install libimepinyin-dev)")
  endif()
  message(WARNING "AetherIME: LibIME backend disabled, fallback lexicon will be used. Reason: ${AETHERIME_LIBIME_REASON}")
endif()
install(TARGETS aetherime DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")

configure_file(aetherime-addon.conf.in.in aetherime-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/aetherime-addon.conf.in" aetherime-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/aetherime-addon.conf" RENAME aetherime.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

fcitx5_translate_desktop_file(aetherime.conf.in aetherime.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/aetherime.conf" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
