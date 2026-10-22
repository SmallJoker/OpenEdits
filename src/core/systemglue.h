#pragma once

#include <string>
#include <vector>

void change_working_dir();

/// Windows:    %APPDATA%\<appname>         (AppData\Roaming\...)
/// UNIX-alike: $XDG_CONFIG_HOME/<appname>  (~./.config/...)
std::vector<std::string> get_user_paths(bool create_if_needed);
