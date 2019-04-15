#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := homething


VER:=$(shell git describe --dirty)

app-version:
	echo "Creating version.h..."
	echo "char appVersion[]=\"$(VER)\";" > components/updater/version.h
	echo "char deviceProfile[]=\"$(DEVICE_PROFILE)\";" >> components/updater/version.h

.phony: app-version

all: app-version

include $(IDF_PATH)/make/project.mk

ifeq ($(CONFIG_ESPTOOLPY_FLASHSIZE), "1MB")
APP_OTA_1=$(OTA1_BIN:.bin=__$(DEVICE_PROFILE)__$(VER).ota)
APP_OTA_2=$(OTA2_BIN:.bin=__$(DEVICE_PROFILE)__$(VER).ota)

app-ota:ota
	echo "Generating 2 OTA files"
	$(PYTHON) main/ota.py $(OTA1_BIN) $(APP_OTA_1)
	$(PYTHON) main/ota.py $(OTA2_BIN) $(APP_OTA_2)
else
APP_OTA=$(APP_BIN:.bin=__$(DEVICE_PROFILE)__$(VER).ota)

app-ota:$(APP_BIN)
	echo "Generating single OTA file"
	$(PYTHON) main/ota.py $(APP_BIN) $(APP_OTA)

endif


#
# Work out the build profile string
# 
# This is used to ensure that when updating we get a new build with the same functionality.
#
PROFILE:=
ifeq ($(CONFIG_LIGHTS_1), y)
PROFILE += L
endif
ifeq ($(CONFIG_LIGHTS_2), y)
PROFILE += LL
endif
ifeq ($(CONFIG_LIGHTS_3), y)
PROFILE += LLL
endif

ifeq ($(CONFIG_DHT22), y)
PROFILE += T
ifeq ($(CONFIG_FAN), y)
PROFILE += F
endif
endif

ifeq ($(CONFIG_DOORBELL), y)
PROFILE += B
endif

ifeq ($(CONFIG_MOTION), y)
PROFILE += M
endif

ifeq ($(CONFIG_RELAY_ON_HIGH), y)
PROFILE += +
endif

DEVICE_PROFILE=$(shell  echo $(PROFILE) | sed 's/ //g')
