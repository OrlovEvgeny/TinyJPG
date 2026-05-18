set(required_source_files
    "packaging/systemd/tinyjpg.service.in"
    "packaging/systemd/tinyjpg.sysusers"
    "packaging/systemd/tinyjpg.tmpfiles"
    "packaging/launchd/org.tinyjpg.plist.in"
    "packaging/windows/install-service.ps1"
    "packaging/windows/uninstall-service.ps1"
    "packaging/homebrew/tinyjpg.rb.in"
    "packaging/scoop/tinyjpg.json.in")

foreach(file IN LISTS required_source_files)
  if(NOT EXISTS "${TINYJPG_SOURCE_DIR}/${file}")
    message(FATAL_ERROR "missing packaging file: ${file}")
  endif()
endforeach()

file(READ "${TINYJPG_SOURCE_DIR}/packaging/systemd/tinyjpg.service.in" systemd_unit)
foreach(token
        "ExecStart=@CMAKE_INSTALL_FULL_BINDIR@/tinyjpg watch"
        "NoNewPrivileges=yes"
        "ProtectSystem=strict"
        "MemoryDenyWriteExecute=yes"
        "UMask=0077")
  if(NOT systemd_unit MATCHES "${token}")
    message(FATAL_ERROR "systemd unit missing token: ${token}")
  endif()
endforeach()

file(READ "${TINYJPG_SOURCE_DIR}/packaging/launchd/org.tinyjpg.plist.in" launchd_plist)
foreach(token "org.tinyjpg" "RunAtLoad" "KeepAlive" "ExitTimeOut" "_tinyjpg")
  if(NOT launchd_plist MATCHES "${token}")
    message(FATAL_ERROR "launchd plist missing token: ${token}")
  endif()
endforeach()

file(READ "${TINYJPG_SOURCE_DIR}/packaging/windows/install-service.ps1" windows_install)
foreach(token "New-Service" "sc.exe failure" "config print --defaults" "Start-Service")
  if(NOT windows_install MATCHES "${token}")
    message(FATAL_ERROR "windows install script missing token: ${token}")
  endif()
endforeach()

set(required_generated_files
    "packaging/systemd/tinyjpg.service"
    "packaging/launchd/org.tinyjpg.plist"
    "packaging/homebrew/tinyjpg.rb"
    "packaging/scoop/tinyjpg.json")

foreach(file IN LISTS required_generated_files)
  if(NOT EXISTS "${TINYJPG_BINARY_DIR}/${file}")
    message(FATAL_ERROR "missing generated packaging file: ${file}")
  endif()
endforeach()
