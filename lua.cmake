set(LUA_TOLIB "liblua.a")
set(LUA_TOINC "")

foreach(HEADER
  # default
  lua.h
  luaconf.h
  lualib.h
  lauxlib.h
  # extra
  lstate.h
  lobject.h
  llimits.h
  lmem.h
  ltm.h
  lzio.h
)
  set(LUA_TOINC "${LUA_TOINC} ${HEADER}")
endforeach()

if(LUA_PATCH)
  set(LUA_PATCH_COMMAND patch -Np1 < ${CMAKE_CURRENT_SOURCE_DIR}/lua.patch)
else()
  set(LUA_PATCH_COMMAND "")
endif()

ExternalProject_Add(lua
  URL "https://www.lua.org/work/lua-5.4.0-work2.tar.gz"
  URL_HASH SHA256=68b7e8f1ff561b9a7e1c29de26ff99ac2a704773c0965a4fe1800b7657d5a057
  PATCH_COMMAND ${LUA_PATCH_COMMAND}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND make linux
    -C<SOURCE_DIR>
    CC=${CMAKE_C_COMPILER}
    LD=${CMAKE_C_COMPILER}
  INSTALL_COMMAND make install
    -C<SOURCE_DIR>
    INSTALL_TOP=${CMAKE_INSTALL_PREFIX}
    TO_LIB=${LUA_TOLIB}
    TO_INC=${LUA_TOINC}
  BUILD_IN_SOURCE 1
  LOG_DOWNLOAD 1 LOG_BUILD 1 LOG_INSTALL 1
)
