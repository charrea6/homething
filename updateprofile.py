import paho.mqtt.client as mqtt
import time
import yaml
import collections
import gencomponents
import requests
import cbor2
import json
import sys


VERSION=2

class Messages:
    def __init__(self) -> None:
        self.errors = False
        self.messages = []
    
    @staticmethod
    def _location(component, idx):
        if component is None:
            return ""
        if idx is None:
            return f"{component}: "
        return f"{component}:{idx}: "

    def warning(self, msg, component=None, idx=None):
        self.messages.append(f"Warning: {self._location(component, idx)}{msg}")
    
    def error(self, msg, component=None, idx=None):
        self.messages.append(f"Error: {self._location(component, idx)}{msg}")
        self.errors = True

    def print(self):
        for msg in self.messages:
            print(msg)


class IdTable:
    def __init__(self) -> None:
        self.defined = []
        self.used = collections.defaultdict(list)
    
    def define(self, name):
        self.defined.append(name)
    
    def use(self, name, location):
        self.used[name].append(location)
    
    def validate(self, messages):
        for id_name in self.used.keys():
            if id_name not in self.defined:
                msg = f"Unknown id {id_name} used by:"
                for location in self.used[id_name]:
                    msg += f"\n    - {location}"
                messages.error(msg)


def load_components(device_ip):
    response = requests.get(f"http://{device_ip}/components.json")
    response.raise_for_status()
    
    components = {}
    for name, details in response.json().items():
        component = gencomponents.load_component(name, {'args': details})
        components[name] = component
    return components    

def load_profile(profile_file) -> dict:
    with open(profile_file, "r") as fp:
        profile_dict = yaml.safe_load(fp)
    return profile_dict

def validate_arg(messages, id_table, component, idx, argument, value):
    if argument.type == 'choices':
        if value not in argument.options['choices']:
            messages.warning(f'choice "{value}" not known for this component, valid choices are {argument.options["choices"]}', component, idx)

    elif argument.type in ('uint', 'gpioPin', 'gpioLevel', 'i2cAddr'):
        if not isinstance(value, int) or value < 0:
            messages.error(f"{value} is not a {argument.type}", component, idx)

    elif argument.type == 'int':
        if not isinstance(value, int):
            messages.error(f"{value} is not an int", component, idx)

    elif argument.type == 'bool':
        if not isinstance(value, bool):
            messages.error(f"{value} is not an bool", component, idx)

    elif argument.type == 'string':
        if not isinstance(value, str):
            messages.error(f"{value} is not an string", component, idx)

    elif argument.type == 'id':
        id_table.use(value, f"{component.name}[{idx}].{argument.name}")

def validate_component(messages, id_table, component, idx, details):
    defined_args = set()
    for arg_name, arg_value in details.items():
        if arg_name == 'id':
            id_table.define(arg_value)
            
        arg_def = component.get_arg(arg_name)
        if arg_def is None:
            messages.warning(f'Unknown argument "{arg_name}"', component, idx)
            continue
        validate_arg(messages, id_table, component, idx, arg_def, arg_value)
        defined_args.add(arg_def)
    missing_args = component.mandatory_args - defined_args
    if missing_args:
        for arg in missing_args:
            if arg.has_default:
                details[arg.name] = arg.default
            else:
                messages.error(f'missing argument "{arg.name}"', component, idx)

def validate_profile(messages, components, profile_dict):
    if not isinstance(profile_dict, dict):
        messages.error(f"Top level must be a dictionary not {type(profile_dict)}")
        return

    to_remove = []
    id_table = IdTable()
    for component, details in profile_dict.items():
        if component not in components:
            messages.warning(f'Unknown component "{component}"')
        else:
            if details is None:
                to_remove.append(component)
            elif not isinstance(details, list):
                messages.error(f"Expected an array got {type(details).__name__}", component)
            else:
                for idx, instance_details in enumerate(details):
                    validate_component(messages, id_table, components[component], idx, instance_details)

    id_table.validate(messages)

    for component in to_remove:
        del profile_dict[component]
        

def get_device_ip(mqtt_host, device_id):
    device_info = {}
    client = mqtt.Client(userdata=device_info)
    
    def on_connect(client, userdata, flags, rc):
        client.subscribe(f"homething/{device_id}/device/info")
    
    def on_message(client, userdata, msg):
        info = json.loads(msg.payload)
        userdata.update(info)

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(mqtt_host, 1883, 60)

    start_time = time.time()
    while (time.time() < start_time + 10) and not device_info:
        client.loop()
    client.disconnect()

    if not device_info:
        raise RuntimeError(f"No information recieved about {device_id}")
    return device_info['ip']


def upload_profile(mqtt_host, device_id, cbor_dict):
    device_info = {}
    client = mqtt.Client(userdata=device_info)
    
    def on_connect(client, userdata, flags, rc):
        client.subscribe(f"homething/{device_id}/device/profile")
    
    def on_message(client, userdata, msg):
        userdata['profile'] = msg.payload

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(mqtt_host, 1883, 60)

    start_time = time.time()
    while (time.time() < start_time + 10) and device_info.get("profile") is None:
        client.loop()
    
    if device_info.get("profile") is None:
        raise RuntimeError(f"No profile information recieved about {device_id}")

    print("Got current profile details.")
    original_profile = device_info.pop('profile')

    print("Sending new profile")
    #client.publish(f"homething/{device_id}/device/ctrl", b"restart")
    client.publish(f"homething/{device_id}/device/ctrl", b"setprofile\0" + cbor_dict)
    
    print("Waiting for device to restart...")
    start_time = time.time()
    while (time.time() < start_time + 10) and device_info.get('profile') is None: 
        client.loop()
    client.disconnect()
    
    if device_info.get("profile") is None:
        raise RuntimeError(f"No profile information recieved about {device_id}")
    
    print("Device restarted.")
    if device_info['profile'] == original_profile:
        raise RuntimeError(f'Profile update of {device_id} failed') 


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <mqtt host> <device id> <new profile yaml>")
        sys.exit(255)

    mqtt_host = sys.argv[1]
    device_id = sys.argv[2]
    profile_file = sys.argv[3]
    device_ip = get_device_ip(mqtt_host, device_id)
    try:
        components = load_components(device_ip)
    except requests.exceptions.HTTPError:
        print("Device doesn't support new style profiles.")
        sys.exit(1)

    profile_dict = load_profile(profile_file)
    messages = Messages()
    validate_profile(messages, components, profile_dict)
    if messages.errors:
        messages.print()
        sys.exit(1)

    profile = [VERSION, profile_dict]
    cbor_profile = cbor2.dumps(profile)
    upload_profile(mqtt_host, device_id, cbor_profile)
    print("Profile updated!")


if __name__ == '__main__':
    main()