import argparse
import sys

import yaml
import json

class OptionInfo:
    def __init__(self, type_name, optional=False):
        self.type_name = type_name
        self.optional = optional
        self.default = None

class ProcessedOptionInfo:
    def __init__(self):
        self.default = 0
        self.optional = True

defaults = dict(
    wifi={'ssid':OptionInfo('string', True), 
          'pass':OptionInfo('string', True)},
    mqtt={'host':OptionInfo('string'), 
          'port':OptionInfo('u16', True), 
          'user':OptionInfo('string', True), 
          'pass':OptionInfo('string', True)},
    thing={'profile':ProcessedOptionInfo(),}
    )

def process_wifi(options, _):
    if options['ssid']:
        return '''wifi,namespace,,
ssid,data,string,{ssid}
pass,data,string,{pass}
'''.format(**options) 
    else:
        return ''

def process_mqtt(options, _):
    mqtt = 'mqtt,namespace,,\n'
    for key,value in options.items():
        if value is not None:
            mqtt += '%s,data,%s,%s\n' % (key, defaults['mqtt'][key].type_name, value)
    return mqtt

def process_thing(options, config):
    thing = 'thing,namespace,,\n'
    profile = { 'version': '1.0', 'components': options.get('profile', {})}
    thing += 'deviceprofile,data,string,%s\n' % json.dumps(profile)
    return thing


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Translates settings config file into csv file for NVS generation.')
    parser.add_argument("infile", type=argparse.FileType('r'))
    parser.add_argument("outfile", type=argparse.FileType('w'))

    args = parser.parse_args()

    config = yaml.safe_load(args.infile)

    csv = 'key,type,encoding,value\n'
    for section, option_defaults in defaults.items():
        options = {}
        section_config = config.get(section, {})
        for option,value in option_defaults.items():
            if option in section_config:
                options[option] = section_config.get(option)
            else:
                if value.optional:
                    options[option] = value.default
    
        csv += eval('process_%s(options, config)' % section)

    args.outfile.write(csv)
    args.outfile.close()

