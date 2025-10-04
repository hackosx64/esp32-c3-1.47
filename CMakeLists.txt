cmake_minimum_required(VERSION 3.16)

# Avoid attempts to call `git describe` when building from a PlatformIO package
# that lacks the ESP-IDF Git metadata. This prevents configuration failures on
# Windows hosts where the framework is installed without its Git repository.
set(IDF_SKIP_GIT_VERSION ON)

# Provide an explicit firmware version string so ESP-IDF does not try to infer
# it from Git state.
set(PROJECT_VER "0.2.0" CACHE STRING "Firmware version" FORCE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_c6_wavegen)
