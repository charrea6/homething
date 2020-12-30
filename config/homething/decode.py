from collections import defaultdict

from .constants import *
from .errors import *

switch_types = {
    DeviceProfile_SwitchType_Toggle: "toggleSwitch",
    DeviceProfile_SwitchType_OnOff: "onOffSwitch",
    DeviceProfile_SwitchType_Momentary: "momentarySwitch",
    DeviceProfile_SwitchType_Contact: "contactSwitch",
    DeviceProfile_SwitchType_Motion: "motionSensor"
}


def decode_switch(entry):
    pin = entry[1]
    switch_type = entry[2]
    return f'{switch_types[switch_type]}({pin})', None


relay_types = {
    DeviceProfile_RelayController_None: "relay",
    DeviceProfile_RelayController_Switch: "switchedRelay",
    DeviceProfile_RelayController_Temperature: "temperatureControlledRelay",
    DeviceProfile_RelayController_Humidity: "humidityControlledRelay"
}


def decode_relay(entry):
    pin = entry[1]
    level = entry[2]
    relay_type = entry[3]
    if relay_type == DeviceProfile_RelayController_None:
        return f'relay({pin}, {level})', None
    controller_id = entry[4]
    return f'{relay_types[relay_type]}({pin}, {level}, id{controller_id})', controller_id


def decode_dht22(entry):
    pin = entry[1]
    return f'dht22({pin})', None


def decode_i2c(type_name, entry):
    sda = entry[1]
    scl = entry[2]
    addr = entry[3]
    return f'{type_name}( {sda}, {scl}, {addr})', None


def decode_si7021(entry):
    return decode_i2c('si7021', entry)


def decode_tsl2561(entry):
    return decode_i2c('tsl2561', entry)


def decode_bme280(entry):
    return decode_i2c('bme280', entry)


decoders = {
    DeviceProfile_EntryType_GPIOSwitch: decode_switch,
    DeviceProfile_EntryType_Relay: decode_relay,
    DeviceProfile_EntryType_DHT22: decode_dht22,
    DeviceProfile_EntryType_SI7021: decode_si7021,
    DeviceProfile_EntryType_TSL2561: decode_tsl2561,
    DeviceProfile_EntryType_BME280: decode_bme280
}


def decode(profile):
    version = profile[0]
    if version != 1:
        raise UnsupportedProfileVersionError()
    entries = []
    referenced_entries = defaultdict(list)
    for entry in profile[1:]:
        desc, used = decoders[entry[0]](entry)
        if used is not None:
            referenced_entries[used].append(len(entries))
        entries.append(desc)
    source = ''
    for i, entry in enumerate(entries):
        if i in referenced_entries:
            used_by = referenced_entries[i]
            if len(used_by) == 1 and used_by[0] == i + 1:
                entries[i + 1] = entries[i + 1].replace(f'id{i}', entry)
            else:
                source += f'id{i} = {entry}\n'
        else:
            source += entry + '\n'

    return source
