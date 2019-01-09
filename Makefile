#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := mqtt_switch

VER:=$(shell git describe --dirty)

version:
	echo "Creating version.h..."
	echo "char appVersion[]=\"$(VER)\";" > components/updater/version.h

.phony: version

all: version

include $(IDF_PATH)/make/project.mk

