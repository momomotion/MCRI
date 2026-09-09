# Install script for directory: /opt/nordic/ncs/v3.1.0/zephyr

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/opt/nordic/ncs/toolchains/5c0d382932/opt/zephyr-sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/arch/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/lib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/soc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/boards/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/subsys/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/drivers/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/nrf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/hostap/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/mcuboot/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/mbedtls/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/trusted-firmware-m/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/cjson/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/azure-sdk-for-c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/cirrus-logic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/openthread/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/memfault-firmware-sdk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/canopennode/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/chre/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/lz4/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/nanopb/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/zscilib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/cmsis/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/cmsis-dsp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/cmsis-nn/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/cmsis_6/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/fatfs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/hal_nordic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/hal_st/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/hal_tdk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/hal_wurthelektronik/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/liblc3/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/libmetal/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/littlefs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/loramac-node/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/lvgl/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/mipi-sys-t/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/nrf_wifi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/open-amp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/percepio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/picolibc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/segger/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/tinycrypt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/uoscore-uedhoc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/zcbor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/nrfxlib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/nrf_hw_models/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/modules/connectedhomeip/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/kernel/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/cmake/flash/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/cmake/usage/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/momo/Desktop/MCRI-Files/MCRI/cbpm-dongle/build-dongle/cbpm-dongle/zephyr/cmake/reports/cmake_install.cmake")
endif()

