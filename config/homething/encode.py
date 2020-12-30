from .constants import *
from .errors import *


class Component:
    TYPE_NAME = "Unknown"

    def __init__(self, pos, args):
        self.pos = pos
        self.args = args

    def check_arg_count(self, required_args):
        if len(self.args) != required_args:
            raise ProfileEntryIncorrectNumberArgumentsError(self.pos, self.TYPE_NAME, len(self.args), required_args)

    def get_pin(self, index):
        pin = self.get_arg(index, int)
        if pin.arg < 0:
            raise ProfileEntryError(pin.pos, "Pin cannot be negative")
        return pin.arg

    def get_arg(self, index, types):
        try:
            arg = self.args[index]
            if isinstance(arg.arg, types):
                return arg
            raise ProfileEntryWrongArgumentTypeError(arg, types)
        except IndexError:
            raise ProfileEntryError(self.pos, f"Not enough arguments when looking for argument {index}")


class Switch(Component):
    TYPE_NAME = "switch"

    def __init__(self, type, pos, args):
        super().__init__(pos, args)
        self.type = type

    def process(self, id_table, profile):
        self.check_arg_count(1)
        entry = [DeviceProfile_EntryType_GPIOSwitch, self.get_pin(0), self.type]
        profile.append(entry)


class ToggleSwitch(Switch):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_SwitchType_Toggle, pos, args)


class OnOffSwitch(Switch):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_SwitchType_OnOff, pos, args)


class MomentarySwitch(Switch):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_SwitchType_Momentary, pos, args)


class ContactSwitch(Switch):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_SwitchType_Contact, pos, args)


class MotionSensor(Switch):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_SwitchType_Motion, pos, args)


class BaseRelay(Component):
    TYPE_NAME = "relay"

    def __init__(self, type, pos, args):
        super().__init__(pos, args)
        self.type = type

    def process(self, id_table, profile):
        self.check_arg_count(2 if type == DeviceProfile_RelayController_None else 3)
        entry = [DeviceProfile_EntryType_Relay, self.get_pin(0), self.get_level(), self.type]
        if self.type != DeviceProfile_RelayController_None:
            id = self.get_arg(2, (ID, Component))
            if isinstance(id.arg, ID):
                try:
                    entry.append(id_table[id.arg.name])
                except KeyError:
                    raise ProfileEntryError(id.pos, f'ID "{id.arg.name}" not found')
            else:
                entry.append(len(profile) - 1)
                id.arg.process(id_table, profile)
        profile.append(entry)

    def get_level(self):
        level = self.get_arg(1, int)
        if level.arg not in (0, 1):
            raise ProfileEntryError(level.pos, f'Level must be either 0 or 1 not "{level.arg}"')
        return level.arg


class Relay(BaseRelay):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_RelayController_None, pos, args)


class SwitchedRelay(BaseRelay):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_RelayController_Switch, pos, args)


class TemperatureControlledRelay(BaseRelay):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_RelayController_Temperature, pos, args)


class HumidityControlledRelay(BaseRelay):
    def __init__(self, pos, args):
        super().__init__(DeviceProfile_RelayController_Humidity, pos, args)


class Dht22(Component):
    TYPE_NAME = "dht22"

    def process(self, id_table, profile):
        self.check_arg_count(1)
        entry = [DeviceProfile_EntryType_DHT22, self.get_pin(0)]
        profile.append(entry)


class I2cDevice(Component):
    def __init__(self, type, pos, args):
        super().__init__(pos, args)
        self.type = type

    def process(self, id_table, profile):
        self.check_arg_count(3)
        sda = self.get_pin(0)
        scl = self.get_pin(1)
        addr = self.get_arg(2, int)
        if addr.arg < 0:
            raise ProfileEntryError(addr.pos, "I2C address cannot be negative")
        entry = [self.type, sda, scl, addr.arg]
        profile.append(entry)


class Si7021(I2cDevice):
    TYPE_NAME = "SI7021"

    def __init__(self, pos, args):
        super().__init__(DeviceProfile_EntryType_SI7021, pos, args)


class Tsl2561(I2cDevice):
    TYPE_NAME = "TSL2561"

    def __init__(self, pos, args):
        super().__init__(DeviceProfile_EntryType_TSL2561, pos, args)


class Bme280(I2cDevice):
    TYPE_NAME = "BME280"

    def __init__(self, pos, args):
        super().__init__(DeviceProfile_EntryType_BME280, pos, args)


class ID:
    def __init__(self, name):
        self.name = name

    def __str__(self):
        return f"ID({self.name})"


class Arg:
    def __init__(self, pos, arg):
        self.pos = pos
        self.arg = arg


component_base_classes = (Component, BaseRelay, I2cDevice)
components = {}
for name in dir():
    v = eval(name)
    if isinstance(v, type) and issubclass(v, Component) and v not in component_base_classes:
        name = name[0].lower() + name[1:]
        components[name] = v
