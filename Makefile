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

APP_OTA=$(APP_BIN:.bin=.ota)

app-ota:$(APP_BIN)
	$(PYTHON) main/ota.py $(APP_BIN) $(APP_OTA)

include $(IDF_PATH)/make/project.mk

