import ConfigParser
import argparse
import sys

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
    wifi={'ssid':OptionInfo('string'), 
          'pass':OptionInfo('string')},
    mqtt={'host':OptionInfo('string'), 
          'port':OptionInfo('u16', True), 
          'user':OptionInfo('string', True), 
          'pass':OptionInfo('string', True)},
    gpiox={'num':OptionInfo('u8', True),
           'sda':OptionInfo('u8', True),
           'scl':OptionInfo('u8', True),},
    thing={'relayOnLevel':OptionInfo('u8', True), 
           'lights': ProcessedOptionInfo(), 
           'doorbells': ProcessedOptionInfo(), 
           'motionsensors': ProcessedOptionInfo(), 
           'temperaturesensors': ProcessedOptionInfo(), 
           'fans': ProcessedOptionInfo()}
    )

def process_wifi(options, _):
    return '''wifi,namespace,,
ssid,data,string,{ssid}
pass,data,string,{pass}
'''.format(**options) 

def process_mqtt(options, _):
    mqtt = 'mqtt,namespace,,\n'
    for key,value in options.items():
        if value is not None:
            mqtt += '%s,data,%s,%s\n' % (key, defaults['mqtt'][key].type_name, value)
    return mqtt

def process_gpiox(options, _):
    gpiox = ''
    for key,value in options.items():
        if value is not None:
            gpiox += '%s,data,%s,%s\n' % (key, defaults['gpiox'][key].type_name, value)
    if gpiox:
        return 'gpiox,namespace,,\n' + gpiox
    return ''

def process_thing(options, config):
    thing = 'thing,namespace,,\n'
    profile = ''

    for key, value in options.items():
        if isinstance(defaults['thing'][key], OptionInfo):
            if value is not None:
                thing += '%s,data,%s,%s\n' % (key, defaults['thing'][key].type_name, value)
        else:
            count = int(value)
            profile += eval('process_%s(config, count)' % key)
    
    thing += 'profile,data,hex2bin,%s\n' % profile

    return thing


def process_lights(config, count):
    profile = ''
    for i in range(count):
        section_name = 'light%d' % i
        if not config.has_section(section_name):
            print("Missing section '%s'" % section_name)
            sys.exit(1)
        relay = config.getint(section_name, 'relay')
        switch = config.getint(section_name, 'switch')
        profile += '%02x%02x%02x' % (ord('L'), relay, switch)
    return profile

def process_simple_profile(config, section_name_prefix, count, func_char):
    profile = ''
    for i in range(count):
        section_name = '%s%d' % (section_name_prefix, i)
        if not config.has_section(section_name):
            print("Missing section '%s'" % section_name)
            sys.exit(1)
        pin = config.getint(section_name, 'pin')
        profile += '%02x%02x' % (ord(func_char), pin)
    return profile

def process_doorbells(config, count):
    return process_simple_profile(config, 'doorbell', count, 'B')

def process_motionsensors(config, count):
    return process_simple_profile(config, 'motionsensor', count, 'M')

def process_temperaturesensors(config, count):
    return process_simple_profile(config, 'temperaturesensor', count, 'T')

def process_fans(config, count):
    return process_simple_profile(config, 'fan', count, 'F')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Translates settings config file into csv file for NVS generation.')
    parser.add_argument("infile", type=argparse.FileType('r'))
    parser.add_argument("outfile", type=argparse.FileType('w'))

    args = parser.parse_args()

    config = ConfigParser.RawConfigParser()
    config.readfp(args.infile)

    csv = 'key,type,encoding,value\n'
    for section, option_defaults in defaults.items():
        options = {}
        for option,value in option_defaults.items():
            if config.has_option(section, option):
                options[option] = config.get(section, option)
            else:
                if value.optional:
                    options[option] = value.default
    
        csv += eval('process_%s(options, config)' % section)

    args.outfile.write(csv)
    args.outfile.close()

    


