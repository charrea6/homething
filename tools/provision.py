import requests
import yaml
import argparse
import collections
import sys
import os

class UnknownTypeError(RuntimeError):
    pass

class BadChoiceError(RuntimeError):
    pass

class Setting:
    def __init__(self, setting) -> None:
        self.title = setting['title']
        self.name = setting['name']
        self.type = setting['type']
        self.choices = setting.get('choices')
    
    def convert(self, arg):
        if self.type == 'choice':
            if self.choices.find(arg) == -1:
                raise BadChoiceError(f"{self.name}: No such choice {arg}, possible choices {self.choices}")
        elif self.type in ('string', 'ssid', 'password', 'hostname', 'username'):
            return str(arg)
        elif self.type == 'port':
            return int(arg)
        elif self.type == 'checkbox':
            if arg.lower() == 'true':
                return True
            if arg.lower() == 'false':
                return False
            raise ValueError("expected true or false")
        raise UnknownTypeError(f"{self.name}: unknown type {self.type}")

class Settings:
    def __init__(self, path) -> None:
        with open(path) as f:
            settings = yaml.safe_load(f)
        self.sections = {}
        for section_def in settings:
            section_name = section_def['name']
            section_groups = section_def['variables']
            section = {}
            for section_setting_group in section_groups:
                if isinstance(section_setting_group, dict):
                    setting = Setting(section_setting_group)
                    section[setting.name] = setting
                else:
                    for section_setting in section_setting_group:
                        setting = Setting(section_setting)
                        section[setting.name] = setting
            self.sections[section_name] = section
    
    def parse_settings(self, d):
        config = {}
        for section, section_settings in d.items():
            setting_section = self.sections.get(section)
            if setting_section is None:
                raise ValueError(f"unknown section {section}")
            config_section = {}
            for key, value in section_settings.items():
                setting = setting_section.get(key)
                if setting is None:
                    raise ValueError(f"unknown setting {key} in section {section}")
                config_section[key] = setting.convert(value)
            if len(config_section):
                config[section] = config_section
        return config     

class Device:
    def __init__(self, ip_address) -> None:
        self.ip_address = ip_address

    def get_config(self):
        response = requests.get(f"http://{self.ip_address}/config")
        response.raise_for_status()
        return response.json()

    def set_config(self, config):
        response = requests.post(f"http://{self.ip_address}/config", json=config)
        response.raise_for_status()
        return response.json()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="tool to set and retrieve provisioning settings.")
    parser.add_argument("ip_address", help="IP Address of HomeThing device")
    parser.add_argument("setting", nargs="*", help='Optional list of settings to send to the device, in the form "<section>.<key>=<value>"')
    default_settings_yml = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), "../components/provisioning/settings.yml"))
    parser.add_argument("--settingsdef", required=False, default=default_settings_yml, help="Location of settings.yml")
    args = parser.parse_args()
    device = Device(args.ip_address)
    if len(args.setting) == 0:
        settings = device.get_config()
        for section, section_settings in settings.items():
            print(f"{section}:")
            for key,value in section_settings.items():
                print(f"    {key}: {value}")
    else:
        settings = Settings(args.settingsdef)
        config = collections.defaultdict(dict)
        for s in args.setting:
            key, value = s.split('=', 1)
            section, key = key.split('.', 1)
            config[section][key] = value
        
        config = settings.parse_settings(config)
        print(config)