// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/UICommon.h"

#include <algorithm>
#include <clocale>
#include <cmath>
#include <locale>
#include <memory>
#ifdef _WIN32
#include <shlobj.h>  // for SHGetFolderPath

#include <wil/resource.h>
#endif

#include <fmt/format.h>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigLoaders/BaseConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/FreeLookManager.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/STM/STM.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapter.h"

#include "UICommon/DiscordPresence.h"

#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#ifdef HAVE_QTDBUS
#include "UICommon/DBusUtils.h"
#endif

#ifdef __APPLE__
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

namespace UICommon
{
static Config::ConfigChangedCallbackID s_config_changed_callback_id;
static Common::HookableEvent<> s_flush_unsaved_data_event_hook;

static void CreateDumpPath(std::string path)
{
  if (!path.empty())
    File::SetUserPath(D_DUMP_IDX, std::move(path));
  File::CreateFullPath(File::GetUserPath(D_DUMPAUDIO_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPSSL_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPFRAMES_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPOBJECTS_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDEBUG_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDEBUG_BRANCHWATCH_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDEBUG_JITBLOCKS_IDX));
}

static void CreateLoadPath(std::string path)
{
  if (!path.empty())
    File::SetUserPath(D_LOAD_IDX, std::move(path));
  File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
  File::CreateFullPath(File::GetUserPath(D_RIIVOLUTION_IDX));
  File::CreateFullPath(File::GetUserPath(D_GRAPHICSMOD_IDX));
  File::CreateFullPath(File::GetUserPath(D_DYNAMICINPUT_IDX));
}

static void CreateResourcePackPath(std::string path)
{
  if (!path.empty())
    File::SetUserPath(D_RESOURCEPACK_IDX, std::move(path));
}

static void CreateWFSPath(const std::string& path)
{
  if (!path.empty())
    File::SetUserPath(D_WFSROOT_IDX, path + '/');
}

// Pomegrade: in the Punica folder's layout (melonDS-android's PunicaLayout), Dolphin's user folder is
// Punica/GC-Wii/Core and the console's other folders are its siblings: the Wii NAND in System/Wii, the GameCube
// memory cards in Saves/GameCube, the states in States, Load (texture packs, mods) in Mods, the shader cache in
// Core/Cache (not the app's cache: clearing the app's data keeps it). Told by Config::GetPunicaConsoleDirectory.
//
// The configured paths are written to the Base layer (Dolphin.ini), so they outlive the folder they were computed
// from: they are recomputed at every start (and every config change), and a stored value is replaced when it is
// empty or is an earlier layout path (it ends with the layout's relative part, e.g. "/GC-Wii/System/Wii", under
// another Punica folder: the folder was moved or renamed). A path the user chose elsewhere is kept. A layer that is
// never saved was not used: it would sit above Base and hide the paths the user sets in Dolphin's settings.
// Returns the console folder, or an empty string outside the layout.
static std::string UsePunicaLayout()
{
#ifdef ANDROID
  const std::string console = Config::GetPunicaConsoleDirectory();
  if (console.empty())
    return {};
  // "/GC-Wii/": the console folder's own name, the start of every layout path's relative part
  const std::string console_name =
      console.substr(console.find_last_of('/', console.size() - 2));
  const auto use = [&](const Config::Info<std::string>& info, const std::string& relative) {
    const std::string current = Config::Get(info);
    const std::string path = console + relative;
    if (current != path && (current.empty() || current.ends_with(console_name + relative)))
      Config::SetBase(info, path);
  };
  use(Config::MAIN_FS_PATH, "System/Wii");
  use(Config::MAIN_LOAD_PATH, "Mods");
  const std::string memory_cards = "Saves/" "GameCube/";
  use(Config::MAIN_MEMCARD_A_PATH, memory_cards + GC_MEMCARDA "." USA_DIR ".raw");
  use(Config::MAIN_MEMCARD_B_PATH, memory_cards + GC_MEMCARDB "." USA_DIR ".raw");
  use(Config::MAIN_GCI_FOLDER_A_PATH, memory_cards + "Card A/" USA_DIR);
  use(Config::MAIN_GCI_FOLDER_B_PATH, memory_cards + "Card B/" USA_DIR);
  File::SetUserPath(D_STATESAVES_IDX, console + "States");
  File::SetUserPath(D_SHADERCACHE_IDX, File::GetUserPath(D_USER_IDX) + CACHE_DIR DIR_SEP SHADERCACHE_DIR);
  File::CreateFullPath(console + memory_cards);
  return console;
#else
  return {};
#endif
}

static void InitCustomPaths()
{
  const std::string punica_console = UsePunicaLayout();
  File::SetUserPath(D_WIIROOT_IDX, Config::Get(Config::MAIN_FS_PATH));
  CreateLoadPath(Config::Get(Config::MAIN_LOAD_PATH));
  CreateDumpPath(Config::Get(Config::MAIN_DUMP_PATH));
  CreateResourcePackPath(Config::Get(Config::MAIN_RESOURCEPACK_PATH));
  CreateWFSPath(Config::Get(Config::MAIN_WFS_PATH));
  // Pomegrade: in the layout the Wii SD card (image and sync folder) is console storage, in System/Wii SD rather than
  // Load (Mods); set as user paths only, not in the config, so a path the user sets in the settings (applied below)
  // still wins and nothing tied to this folder is saved. A card the app's migration moved from Dolphin's Load into
  // Mods (D_LOAD_IDX now) is used where it is, so its content is not traded for a new, empty card.
  if (!punica_console.empty())
  {
    const std::string sd = punica_console + "System/Wii SD/";
    const std::string load = File::GetUserPath(D_LOAD_IDX);
    const bool image_in_load = File::Exists(load + WII_SD_CARD_IMAGE);
    File::SetUserPath(F_WIISDCARDIMAGE_IDX, (image_in_load ? load : sd) + WII_SD_CARD_IMAGE);
    const bool sync_in_load = File::IsDirectory(load + WIISDSYNC_DIR);
    File::SetUserPath(D_WIISDCARDSYNCFOLDER_IDX, (sync_in_load ? load : sd) + WIISDSYNC_DIR);
  }
  File::SetUserPath(F_WIISDCARDIMAGE_IDX, Config::Get(Config::MAIN_WII_SD_CARD_IMAGE_PATH));
  File::SetUserPath(D_WIISDCARDSYNCFOLDER_IDX,
                    Config::Get(Config::MAIN_WII_SD_CARD_SYNC_FOLDER_PATH));
  File::CreateFullPath(File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX));
#ifdef HAS_LIBMGBA
  File::SetUserPath(F_GBABIOS_IDX, Config::Get(Config::MAIN_GBA_BIOS_PATH));
  File::SetUserPath(D_GBASAVES_IDX, Config::Get(Config::MAIN_GBA_SAVES_PATH));
  File::CreateFullPath(File::GetUserPath(D_GBASAVES_IDX));
#endif
}

static void RefreshConfig()
{
  Common::SetEnableAlert(Config::Get(Config::MAIN_USE_PANIC_HANDLERS));
  Common::SetAbortOnPanicAlert(Config::Get(Config::MAIN_ABORT_ON_PANIC_ALERT));
}

void Init()
{
  Core::RestoreWiiSettings(Core::RestoreReason::CrashRecovery);

  Config::Init();
  const auto config_changed_callback = [] {
    InitCustomPaths();
    RefreshConfig();
  };
  s_config_changed_callback_id = Config::AddConfigChangedCallback(config_changed_callback);
  Config::AddLayer(ConfigLoaders::GenerateBaseConfigLoader());
  SConfig::Init();
  g_Config.Init();
  Discord::Init();
  Common::Log::LogManager::Init();
  VideoBackendBase::ActivateBackend(Config::Get(Config::MAIN_GFX_BACKEND));
  Statistics::Init();

  RefreshConfig();
}

void Shutdown()
{
  Config::RemoveConfigChangedCallback(s_config_changed_callback_id);

  Statistics::Shutdown();
  GCAdapter::Shutdown();
  WiimoteReal::Shutdown();
  Common::Log::LogManager::Shutdown();
  Discord::Shutdown();
  g_Config.Shutdown();
  SConfig::Shutdown();
  Config::Shutdown();
}

[[nodiscard]] Common::EventHook AddFlushUnsavedDataCallback(std::function<void()> callback)
{
  return s_flush_unsaved_data_event_hook.Register(std::move(callback));
}

void FlushUnsavedData()
{
  INFO_LOG_FMT(CORE, "Flushing unsaved data...");
  s_flush_unsaved_data_event_hook.Trigger();
}

void InitControllers(const WindowSystemInfo& wsi)
{
  if (g_controller_interface.IsInit())
    return;

  g_controller_interface.Initialize(wsi);

  if (!g_controller_interface.HasDefaultDevice())
  {
    // Note that the CI default device could be still temporarily removed at any time
    WARN_LOG_FMT(CONTROLLERINTERFACE, "No default device has been added in time. Premade control "
                                      "mappings intended for the default device may not work.");
  }

  GCAdapter::Init();
  Pad::Initialize();
  Pad::InitializeGBA();
  Keyboard::Initialize();
  Wiimote::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
  HotkeyManagerEmu::Initialize();
  FreeLook::Initialize();
}

void ShutdownControllers()
{
  Pad::Shutdown();
  Pad::ShutdownGBA();
  Keyboard::Shutdown();
  Wiimote::Shutdown();
  HotkeyManagerEmu::Shutdown();
  FreeLook::Shutdown();

  g_controller_interface.Shutdown();
}

void SetLocale(std::string locale_name)
{
  auto set_locale = [](const std::string& locale) {
#ifdef __linux__
    std::string adjusted_locale = locale;
    if (!locale.empty())
      adjusted_locale += ".UTF-8";
#else
    const std::string& adjusted_locale = locale;
#endif

    // setlocale sets the C locale, and global sets the C and C++ locales, so the call to setlocale
    // would be redundant if it wasn't for not having any other good way to check whether
    // the locale name is valid. (Constructing a std::locale object for an unsupported
    // locale name throws std::runtime_error, and exception handling is disabled in Dolphin.)
    if (!std::setlocale(LC_ALL, adjusted_locale.c_str()))
      return false;
    std::locale::global(std::locale(adjusted_locale));
    return true;
  };

#ifdef _WIN32
  constexpr char PREFERRED_SEPARATOR = '-';
  constexpr char OTHER_SEPARATOR = '_';
#else
  constexpr char PREFERRED_SEPARATOR = '_';
  constexpr char OTHER_SEPARATOR = '-';
#endif

  // Users who use a system language other than English are unlikely to prefer American date and
  // time formats, so let's explicitly request "en_GB" if Dolphin's language is set to "en".
  // (The settings window only allows setting "en", not anything like "en_US" or "en_GB".)
  // Users who prefer the American formats are likely to have their system language set to en_US,
  // and are thus likely to leave Dolphin's language as the default value "" (<System Language>).
  if (locale_name == "en")
    locale_name = "en_GB";

  std::ranges::replace(locale_name, OTHER_SEPARATOR, PREFERRED_SEPARATOR);

  // Use the specified locale if supported.
  if (set_locale(locale_name))
    return;

  // Remove subcodes until we get a supported locale. If that doesn't give us a supported locale,
  // "" is passed to set_locale in order to get the system default locale.
  while (!locale_name.empty())
  {
    const size_t separator_index = locale_name.rfind(PREFERRED_SEPARATOR);
    locale_name.erase(separator_index == std::string::npos ? 0 : separator_index);
    if (set_locale(locale_name))
      return;
  }

  // If none of the locales tried above are supported, we just keep using whatever locale is set
  // (which is the classic locale by default).
}

void CreateDirectories()
{
  File::CreateFullPath(File::GetUserPath(D_RESOURCEPACK_IDX));
  File::CreateFullPath(File::GetUserPath(D_USER_IDX));
  File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
  File::CreateFullPath(File::GetUserPath(D_COVERCACHE_IDX));
  File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
  File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX) + GRAPHICSMOD_CONFIG_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPSSL_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDEBUG_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDEBUG_BRANCHWATCH_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDEBUG_JITBLOCKS_IDX));
  File::CreateFullPath(File::GetUserPath(D_GAMESETTINGS_IDX));
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + USA_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + EUR_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + JAP_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_TRIUSER_IDX));
  File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
  File::CreateFullPath(File::GetUserPath(D_GRAPHICSMOD_IDX));
  File::CreateFullPath(File::GetUserPath(D_LOGS_IDX));
  File::CreateFullPath(File::GetUserPath(D_MAPS_IDX));
  File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
  File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX));
  File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX) + ANAGLYPH_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_RETROACHIEVEMENTSCACHE_IDX));
  File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
  File::CreateFullPath(File::GetUserPath(D_ASM_ROOT_IDX));
  File::CreateFullPath(File::GetUserPath(D_WFSROOT_IDX));
#ifndef ANDROID
  File::CreateFullPath(File::GetUserPath(D_THEMES_IDX));
  File::CreateFullPath(File::GetUserPath(D_STYLES_IDX));
#else
  // Disable media scanning in directories with a .nomedia file
  File::CreateEmptyFile(File::GetUserPath(D_COVERCACHE_IDX) + DIR_SEP NOMEDIA_FILE);
#endif
}

// Pomegrade: the folders Dolphin derives from its user folder (Punica/GC-Wii/Core in the layout) that the layout puts
// beside it, set as soon as the user folder is: CreateDirectories (Android's Initialize) and the first config load
// would otherwise make Dolphin's defaults inside Core (StateSaves, Load, Wii, GC), which stayed there empty beside
// the layout's own (owner, 29 October). The GameCube folder (its SRAM, and the boot ROMs GetBootROMPath also finds
// there) is System/GC; UsePunicaLayout sets the paths kept in the config. Nothing outside the layout.
static void UsePunicaUserPaths()
{
#ifdef ANDROID
  const std::string console = Config::GetPunicaConsoleDirectory();
  if (console.empty())
    return;
  File::SetUserPath(D_STATESAVES_IDX, console + "States");
  File::SetUserPath(D_LOAD_IDX, console + "Mods");
  File::SetUserPath(D_WIIROOT_IDX, console + "System/Wii");
  File::SetUserPath(D_GCUSER_IDX, console + "System/" GC_USER_DIR);
#endif
}

void SetUserDirectory(std::string custom_path)
{
  if (!custom_path.empty())
  {
    File::CreateFullPath(custom_path + DIR_SEP);
    File::SetUserPath(D_USER_IDX, std::move(custom_path));
    UsePunicaUserPaths();
    return;
  }

  std::string user_path;
#ifdef _WIN32
  // Detect where the User directory is. There are five different cases
  // (on top of the command line flag, which overrides all this):
  // 1. GetExeDirectory()\portable.txt exists
  //    -> Use GetExeDirectory()\User
  // 2. HKCU\Software\Dolphin Emulator\LocalUserConfig exists and is true
  //    -> Use GetExeDirectory()\User
  // 3. HKCU\Software\Dolphin Emulator\UserConfigPath exists
  //    -> Use this as the user directory path
  // 4. My Documents\Dolphin Emulator exists (default user folder before PR 10708)
  //    -> Use this as the user directory path
  // 5. AppData\Roaming exists
  //    -> Use AppData\Roaming\Dolphin Emulator as the User directory path
  // 6. Default
  //    -> Use GetExeDirectory()\User

  // Get AppData path in case we need it.
  wil::unique_cotaskmem_string appdata;
  bool appdata_found = SUCCEEDED(
      SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, appdata.put()));

  // Check our registry keys
  wil::unique_hkey hkey;
  DWORD local = 0;
  std::unique_ptr<TCHAR[]> configPath;
  if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Dolphin Emulator"), 0, KEY_QUERY_VALUE,
                   hkey.put()) == ERROR_SUCCESS)
  {
    DWORD size = sizeof(local);
    if (RegQueryValueEx(hkey.get(), TEXT("LocalUserConfig"), nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(&local), &size) != ERROR_SUCCESS)
    {
      local = 0;
    }

    size = 0;
    RegQueryValueEx(hkey.get(), TEXT("UserConfigPath"), nullptr, nullptr, nullptr, &size);
    configPath = std::make_unique<TCHAR[]>(size / sizeof(TCHAR));
    if (RegQueryValueEx(hkey.get(), TEXT("UserConfigPath"), nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(configPath.get()), &size) != ERROR_SUCCESS)
    {
      configPath.reset();
    }
  }

  local = local != 0 || File::Exists(File::GetExeDirectory() + DIR_SEP "portable.txt");

  // Attempt to check if the old User directory exists in Documents.
  wil::unique_cotaskmem_string documents;
  bool documents_found = SUCCEEDED(
      SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, documents.put()));

  std::optional<std::string> old_user_folder;
  if (documents_found)
  {
    old_user_folder = TStrToUTF8(documents.get()) + DIR_SEP NORMAL_USER_DIR DIR_SEP;
  }

  if (local)  // Case 1-2
  {
    user_path = File::GetExeDirectory() + DIR_SEP PORTABLE_USER_DIR DIR_SEP;
  }
  else if (configPath)  // Case 3
  {
    user_path = TStrToUTF8(configPath.get());
  }
  else if (old_user_folder && File::Exists(old_user_folder.value()))  // Case 4
  {
    user_path = old_user_folder.value();
  }
  else if (appdata_found)  // Case 5
  {
    user_path = TStrToUTF8(appdata.get()) + DIR_SEP NORMAL_USER_DIR DIR_SEP;

    // Set the UserConfigPath value in the registry for backwards compatibility with older Dolphin
    // builds, which will look for the default User directory in Documents. If we set this key,
    // they will use this as the User directory instead.
    // (If we're in this case, then this key doesn't exist, so it's OK to set it.)
    std::wstring wstr_path = UTF8ToWString(user_path);
    RegSetKeyValueW(HKEY_CURRENT_USER, TEXT("Software\\Dolphin Emulator"), TEXT("UserConfigPath"),
                    REG_SZ, wstr_path.c_str(),
                    static_cast<DWORD>((wstr_path.size() + 1) * sizeof(wchar_t)));
  }
  else  // Case 6
  {
    user_path = File::GetExeDirectory() + DIR_SEP PORTABLE_USER_DIR DIR_SEP;
  }
#else
  if (File::IsDirectory(ROOT_DIR DIR_SEP EMBEDDED_USER_DIR))
  {
    user_path = ROOT_DIR DIR_SEP EMBEDDED_USER_DIR DIR_SEP;
  }
  else
  {
    const char* env_path = getenv("DOLPHIN_EMU_USERPATH");
    const char* home = getenv("HOME");
    if (!home)
      home = getenv("PWD");
    if (!home)
      home = "";
    std::string home_path = std::string(home) + DIR_SEP;

    // On a non-Apple and non-Android POSIX system, there are 4 cases:
    // 1. GetExeDirectory()/portable.txt exists
    //    -> Use GetExeDirectory()/User
    // 2. $DOLPHIN_EMU_USERPATH is set
    //    -> Use $DOLPHIN_EMU_USERPATH
    // 3. ~/.dolphin-emu directory exists, and we're not in flatpak
    //    -> Use ~/.dolphin-emu
    // 4. Default
    //    -> Use XDG basedir, see
    //    http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
    //
    // On macOS:
    // 1. GetExeDirectory()/portable.txt exists
    //    -> Use GetExeDirectory()/User
    // 2. $DOLPHIN_EMU_USERPATH is set
    //    -> Use $DOLPHIN_EMU_USERPATH
    // 3. Default
    //
    // On Android, custom_path is set, so this code path is never reached.
    std::string exe_path = File::GetExeDirectory();
    if (File::Exists(exe_path + DIR_SEP "portable.txt"))
    {
      user_path = exe_path + DIR_SEP PORTABLE_USER_DIR DIR_SEP;
    }
    else if (env_path)
    {
      user_path = env_path;
    }
#if defined(__APPLE__) || defined(ANDROID)
    else
    {
      user_path = home_path + NORMAL_USER_DIR DIR_SEP;
    }
#else
    else
    {
      user_path = home_path + "." NORMAL_USER_DIR DIR_SEP;

      if (File::Exists("/.flatpak-info") || !File::Exists(user_path))
      {
        const char* data_home = getenv("XDG_DATA_HOME");
        std::string data_path =
            std::string(data_home && data_home[0] == '/' ? data_home :
                                                           (home_path + ".local" DIR_SEP "share")) +
            DIR_SEP NORMAL_USER_DIR DIR_SEP;

        const char* config_home = getenv("XDG_CONFIG_HOME");
        std::string config_path =
            std::string(config_home && config_home[0] == '/' ? config_home :
                                                               (home_path + ".config")) +
            DIR_SEP NORMAL_USER_DIR DIR_SEP;

        const char* cache_home = getenv("XDG_CACHE_HOME");
        std::string cache_path =
            std::string(cache_home && cache_home[0] == '/' ? cache_home : (home_path + ".cache")) +
            DIR_SEP NORMAL_USER_DIR DIR_SEP;

        File::SetUserPath(D_USER_IDX, data_path);
        File::SetUserPath(D_CONFIG_IDX, config_path);
        File::SetUserPath(D_CACHE_IDX, cache_path);
        return;
      }
    }
#endif
  }
#endif
  File::SetUserPath(D_USER_IDX, std::move(user_path));
}

bool TriggerSTMPowerEvent()
{
  const auto ios = Core::System::GetInstance().GetIOS();
  if (!ios)
    return false;

  const auto stm = ios->GetDeviceByName("/dev/stm/eventhook");
  if (!stm || !std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->HasHookInstalled())
    return false;

  Core::DisplayMessage("Shutting down", 30000);
  ios->GetSystem().GetProcessorInterface().PowerButton_Tap();

  return true;
}

void InhibitScreenSaver(bool inhibit)
{
  // Inhibit the screensaver. Depending on the operating system this may also
  // disable low-power states and/or screen dimming.

#ifdef HAVE_QTDBUS
  DBusUtils::InhibitScreenSaver(inhibit);
#endif

#ifdef _WIN32
  // Prevents Windows from sleeping, turning off the display, or idling
  SetThreadExecutionState(ES_CONTINUOUS |
                          (inhibit ? (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED) : 0));
#endif

#ifdef __APPLE__
  static IOPMAssertionID s_power_assertion = kIOPMNullAssertionID;
  if (inhibit)
  {
    CFStringRef reason_for_activity = CFSTR("Emulation Running");
    if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                    kIOPMAssertionLevelOn, reason_for_activity,
                                    &s_power_assertion) != kIOReturnSuccess)
    {
      s_power_assertion = kIOPMNullAssertionID;
    }
  }
  else
  {
    if (s_power_assertion != kIOPMNullAssertionID)
    {
      IOPMAssertionRelease(s_power_assertion);
      s_power_assertion = kIOPMNullAssertionID;
    }
  }
#endif
}

std::string FormatSize(u64 bytes, int decimals)
{
  // i18n: The symbol for the unit "bytes"
  const char* const unit_symbols[] = {_trans("B"),   _trans("KiB"), _trans("MiB"), _trans("GiB"),
                                      _trans("TiB"), _trans("PiB"), _trans("EiB")};

  // Find largest power of 2 less than size.
  // div 10 to get largest named unit less than size
  // 10 == log2(1024) (number of B in a KiB, KiB in a MiB, etc)
  // Max value is 63 / 10 = 6
  const int unit = MathUtil::IntLog2(std::max<u64>(bytes, 1)) / 10;

  // Don't need exact values, only 5 most significant digits
  const double unit_size = std::pow(2, unit * 10);
  return fmt::format("{:.{}Lf} {}", bytes / unit_size, decimals,
                     Common::GetStringT(unit_symbols[unit]));
}

}  // namespace UICommon
