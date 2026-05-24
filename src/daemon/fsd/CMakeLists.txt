cmake_minimum_required(VERSION 3.22)

# slm-fsd: SLM File System Daemon (privileged access gateway)
# This target is also wired into the root CMakeLists.txt via the
# SLM_FSD_SOURCES variable + qt_add_executable(slm-fsd ...) block there.
#
# This file exists for standalone builds / IDE navigation only.
# The authoritative build definition is in the root CMakeLists.txt.

message(STATUS "[slm-fsd] src/daemon/fsd/CMakeLists.txt loaded (documentation only)")
