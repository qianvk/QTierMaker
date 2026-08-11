Unicode true
RequestExecutionLevel admin
SilentInstall silent
AutoCloseWindow true
SetCompressor /SOLID lzma

!include "MUI2.nsh"

!define PRODUCT_REGISTRY_KEY "Software\qianvk\QTierMaker"
!define UNINSTALL_REGISTRY_KEY \
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTierMaker"

!ifndef APP_EXECUTABLE
    !error "APP_EXECUTABLE is required"
!endif
!ifndef OUTPUT_FILE
    !error "OUTPUT_FILE is required"
!endif
!ifndef APP_VERSION
    !error "APP_VERSION is required"
!endif
!ifndef NUMERIC_VERSION
    !error "NUMERIC_VERSION is required"
!endif
!ifndef RUNTIME_VERSION
    !error "RUNTIME_VERSION is required"
!endif

Name "QTierMaker Update ${APP_VERSION}"
OutFile "${OUTPUT_FILE}"
VIProductVersion "${NUMERIC_VERSION}.0"
VIAddVersionKey /LANG=1033 "CompanyName" "qianvk"
VIAddVersionKey /LANG=1033 "FileDescription" "QTierMaker Update"
VIAddVersionKey /LANG=1033 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright 2026 QTierMaker contributors"
VIAddVersionKey /LANG=1033 "ProductName" "QTierMaker"
VIAddVersionKey /LANG=1033 "ProductVersion" "${APP_VERSION}"

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

LangString NotInstalled ${LANG_ENGLISH} \
    "QTierMaker is not installed. Please use the full installer."
LangString NotInstalled ${LANG_SIMPCHINESE} \
    "未找到 QTierMaker 安装。请使用完整安装包。"
LangString RuntimeMismatch ${LANG_ENGLISH} \
    "This update requires a different application runtime. Please use the full installer."
LangString RuntimeMismatch ${LANG_SIMPCHINESE} \
    "此更新需要不同的应用运行环境。请使用完整安装包。"
LangString WaitTimeout ${LANG_ENGLISH} \
    "QTierMaker did not close in time. Close all application windows and try again."
LangString WaitTimeout ${LANG_SIMPCHINESE} \
    "QTierMaker 未能及时退出。请关闭所有应用窗口后重试。"
LangString ReplaceFailed ${LANG_ENGLISH} \
    "The application could not be updated. The previous executable has been restored."
LangString ReplaceFailed ${LANG_SIMPCHINESE} \
    "无法更新应用程序。已恢复原有可执行文件。"

Var InstallRoot
Var AppPath
Var StagedPath
Var WaitCount
Var RegistryView
Var FoundInstallation

Function .onInit
    StrCpy $FoundInstallation "0"

    SetRegView 64
    ReadRegStr $InstallRoot HKLM "${PRODUCT_REGISTRY_KEY}" ""
    StrCmp $InstallRoot "" read_uninstall_64 validate_64

read_uninstall_64:
    ReadRegStr $InstallRoot HKLM "${UNINSTALL_REGISTRY_KEY}" "InstallLocation"

validate_64:
    StrCmp $InstallRoot "" try_32
    StrCpy $AppPath "$InstallRoot\bin\QTierMaker.exe"
    IfFileExists "$AppPath" check_runtime_64 try_32

check_runtime_64:
    StrCpy $FoundInstallation "1"
    ReadRegStr $0 HKLM "${PRODUCT_REGISTRY_KEY}" "RuntimeVersion"
    StrCmp $0 "${RUNTIME_VERSION}" use_64 try_32

try_32:
    SetRegView 32
    ReadRegStr $InstallRoot HKLM "${PRODUCT_REGISTRY_KEY}" ""
    StrCmp $InstallRoot "" read_uninstall_32 validate_32

read_uninstall_32:
    ReadRegStr $InstallRoot HKLM "${UNINSTALL_REGISTRY_KEY}" "InstallLocation"

validate_32:
    StrCmp $InstallRoot "" no_compatible_install
    StrCpy $AppPath "$InstallRoot\bin\QTierMaker.exe"
    IfFileExists "$AppPath" check_runtime_32 no_compatible_install

check_runtime_32:
    StrCpy $FoundInstallation "1"
    ReadRegStr $0 HKLM "${PRODUCT_REGISTRY_KEY}" "RuntimeVersion"
    StrCmp $0 "${RUNTIME_VERSION}" use_32 no_compatible_install

use_64:
    StrCpy $RegistryView "64"
    Goto ready

use_32:
    StrCpy $RegistryView "32"
    Goto ready

no_compatible_install:
    StrCmp $FoundInstallation "1" runtime_mismatch not_installed

not_installed:
    MessageBox MB_OK|MB_ICONSTOP "$(NotInstalled)"
    Abort

runtime_mismatch:
    MessageBox MB_OK|MB_ICONSTOP "$(RuntimeMismatch)"
    Abort

ready:
FunctionEnd

Section "Update"
    InitPluginsDir
    SetOutPath "$PLUGINSDIR"
    File "/oname=QTierMaker.exe" "${APP_EXECUTABLE}"
    StrCpy $StagedPath "$PLUGINSDIR\QTierMaker.exe"

    Delete "$AppPath.old"
    IfFileExists "$AppPath.old" replace_failed
    StrCpy $WaitCount 0

wait_for_exit:
    ClearErrors
    Rename "$AppPath" "$AppPath.old"
    IfErrors wait_retry replace_executable

wait_retry:
    Sleep 250
    IntOp $WaitCount $WaitCount + 1
    IntCmp $WaitCount 240 wait_timeout wait_for_exit wait_timeout

replace_executable:
    ClearErrors
    CopyFiles /SILENT "$StagedPath" "$AppPath"
    IfErrors restore_executable
    IfFileExists "$AppPath" update_registry restore_executable

update_registry:
    Delete "$AppPath.old"
    StrCmp $RegistryView "64" select_64_registry
    SetRegView 32
    Goto write_registry

select_64_registry:
    SetRegView 64

write_registry:
    WriteRegStr HKLM "${PRODUCT_REGISTRY_KEY}" "RuntimeVersion" "${RUNTIME_VERSION}"
    WriteRegStr HKLM "${UNINSTALL_REGISTRY_KEY}" "DisplayVersion" "${APP_VERSION}"
    Exec '"$AppPath"'
    Goto done

restore_executable:
    Delete "$AppPath"
    Rename "$AppPath.old" "$AppPath"

replace_failed:
    MessageBox MB_OK|MB_ICONSTOP "$(ReplaceFailed)"
    SetErrorLevel 1
    Goto done

wait_timeout:
    MessageBox MB_OK|MB_ICONSTOP "$(WaitTimeout)"
    SetErrorLevel 1

done:
SectionEnd
