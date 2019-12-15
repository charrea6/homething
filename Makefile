#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := homething

EXTRA_CFLAGS+= -DMAX_MESSAGE_HANDLERS=8 
VER:=$(shell git describe --dirty)

VERSION_PATH=$(BUILD_DIR_BASE)/include/version.h

app-version:
	mkdir -p $(BUILD_DIR_BASE)/include
	echo "Creating version.h..."
	echo "char appVersion[]=\"$(VER)\";" > $(VERSION_PATH)
	echo "char deviceProfile[]=\"$(DEVICE_PROFILE)\";" >> $(VERSION_PATH)

.phony: app-version

all: app-version

EXTRA_COMPONENT_DIRS:=esp-idf-lib/components
EXCLUDE_COMPONENTS := max7219 mcp23x17
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
ifneq ($(CONFIG_NROF_LIGHTS), 0)
	PROFILE += L$(CONFIG_NROF_LIGHTS)
endif

ifeq ($(CONFIG_DHT22), y)
ifeq ($(CONFIG_DHT22_2), y)
		PROFILE += T2
else
		PROFILE += T1
endif # DHT22_2

ifeq ($(CONFIG_FAN), y)
ifeq ($(CONFIG_FAN_2), y)
	PROFILE += F2
else
	PROFILE += F1
endif # FAN_2
endif # FAN
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
