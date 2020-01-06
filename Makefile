#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := homething
VER:=$(shell git describe --dirty)

VERSION_PATH=$(BUILD_DIR_BASE)/include/version.h

app-version:
	mkdir -p $(BUILD_DIR_BASE)/include
	echo "Creating version.h..."
	echo "char appVersion[]=\"$(VER)\";" > $(VERSION_PATH)
	echo "char deviceProfile[]=\"$(DEVICE_PROFILE)\";" >> $(VERSION_PATH)

.phony: app-version, nvs-info

all: app-version

nvs-info:
	$(eval NVS_DATA_SIZE := $(shell $(GET_PART_INFO) --type data --subtype nvs --size $(PARTITION_TABLE_BIN) || echo 0))
	$(eval NVS_DATA_OFFSET := $(shell $(GET_PART_INFO) --type data --subtype nvs --offset $(PARTITION_TABLE_BIN)))

config.csv: config.ini
	python config/configgen.py $< $@

config.bin: config.csv nvs-info
	
	python $(IDF_PATH)/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py --input $< --output $@ --size $(NVS_DATA_SIZE)

flash-config: config.bin nvs-info
	$(ESPTOOLPY_WRITE_FLASH) $(NVS_DATA_OFFSET) $<

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
ifeq ($(CONFIG_LIGHT), y)
	PROFILE += L
endif

ifeq ($(CONFIG_DHT22), y)
	PROFILE += T

ifeq ($(CONFIG_FAN), y)
	PROFILE += F
endif # FAN
endif

ifeq ($(CONFIG_DOORBELL), y)
PROFILE += B
endif

ifeq ($(CONFIG_MOTION), y)
PROFILE += M
endif

DEVICE_PROFILE=$(shell  echo $(PROFILE) | sed 's/ //g')
