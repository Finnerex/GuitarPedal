# Project Name
TARGET = GuitarPedal

# Sources
CPP_SOURCES = $(wildcard *.cpp)
# CPP_SOURCES = test.cpp util.cpp

# Library Locations
DAISYSP_DIR ?= ../../DaisySP
LIBDAISY_DIR ?= ../../libDaisy

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

