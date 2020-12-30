import os
import json
import sys
import re
import hashlib
import struct
import click
import subprocess

build_action = None

profile_keys = [('LIGHT', 'L'), ('DHT22', 'T'), ('FAN', 'F'), ('DOORBELL', 'B'), ('MOTION', 'M')]


def load_project_description(args):
    build_dir = args['build_dir']
    with open(os.path.join(build_dir, 'project_description.json')) as f:
        return json.load(f)


def load_config(args):
    build_dir = args['build_dir']
    with open(os.path.join(build_dir, 'config', 'sdkconfig.json')) as f:
        return json.load(f)


def get_version(args):
    build_dir = args['build_dir']
    with open(os.path.join(build_dir, 'version.c')) as f:
        line = f.readline().strip()
        m = re.match(r'char appVersion\[\]=\"([^\"]+)\";', line)
        if m:
            return m.group(1)
    return 'unknown'
        

def generate_ota_file(in_file, out_file):
    with open(in_file, 'rb') as inf:  
        with open(out_file, 'wb') as outf:
            outf.write(b'OTA\0')
            outf.write(b'\0' * 20)
            m = hashlib.md5()
            in_len = 0
            while True:
                data = inf.read(1024)
                if not data:
                    break
                in_len += len(data)
                m.update(data)
                outf.write(data)
            
            outf.seek(4)
            outf.write(struct.pack('<l', in_len))
            outf.write(m.digest())


def print_dict(d, indent=0):
    sp_indent = ' ' * indent
    for k,v in d.items():
        print('%s%s:' %(sp_indent, k ))
        if isinstance(v, dict):
            print_dict(v, indent+4)
        else:
            print('%s  %r' % (sp_indent, v))


def ota_gen(action, ctx, args):    
    config = load_config(args)
    project_desc = load_project_description(args)
    project_name = project_desc['project_name']
    profile = ''
    for key, profile_id in profile_keys:
        if config[key]:
            profile += profile_id
    version = get_version(args)
    
    print("Generating OTA file(s)")
    print('Project: %s' % project_name)
    print('Profile: %s' % profile)
    print('Version: %s' % version)

    ota_suffix = '__%s__%s.ota' % (profile, version)
    build_dir = args['build_dir']

    if config['ESPTOOLPY_FLASHSIZE_1MB']:
        generate_ota_file(os.path.join(build_dir, '%s.app0.bin' % project_name), 
            os.path.join(build_dir, '%s.app0%s' % (project_name, ota_suffix)))

        generate_ota_file(os.path.join(build_dir, '%s.app1.bin' % project_name), 
            os.path.join(build_dir, '%s.app1%s' % (project_name, ota_suffix)))    
    else:
        generate_ota_file(os.path.join(build_dir, '%s.bin' % project_name), 
            os.path.join(build_dir, project_name + ota_suffix))


def build_config(action, ctx, args, configfile=None):
    build_dir = args['build_dir']
    config = load_config(args)
    idf_path = os.getenv('IDF_PATH')
    offset = None
    size = None
    with open(os.path.join(idf_path, 'components', 'partition_table', config['PARTITION_TABLE_FILENAME'])) as csvfile:
        for line in csvfile:
            if line.startswith('#'):
                continue
            row = [e.strip() for e in line.split(',')]
            if row[1] == 'data' and row[2] == 'nvs':
                offset = row[3]
                size = row[4]
                break
    if offset is None:
        raise RuntimeError('Failed to find offset!')
    
    csv_filename = os.path.join(build_dir, 'config.csv')
    bin_filename = os.path.join(build_dir, 'config.bin')
    if configfile.endswith('.yml'):
        subprocess.check_call(['python', 'config/newconfiggen.py', configfile, csv_filename])    
    else:
        subprocess.check_call(['python', 'config/configgen.py', configfile, csv_filename])
    
    nvs_part_gen_path = os.path.join(idf_path, 'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py')
    subprocess.check_call(['python', nvs_part_gen_path, '--input', csv_filename, '--output', bin_filename, '--size', size])
    
    esptool_path = os.path.join(idf_path, 'components', 'esptool_py', 'esptool', 'esptool.py')

    esptool_args = ['--chip', config['IDF_TARGET'], '--port', args['port'], '--baud', str(args['baud'])]
    esptool_args += ['--before', config['ESPTOOLPY_BEFORE'], '--after', config['ESPTOOLPY_AFTER']]
    esptool_args += ['write_flash']

    if config['ESPTOOLPY_COMPRESSED']:
        esptool_args.append('-z')
    else:
        esptool_args.append('-u')
    
    esptool_args += ['--flash_mode', config['ESPTOOLPY_FLASHMODE'], '--flash_freq', config['ESPTOOLPY_FLASHFREQ'], '--flash_size']
    if config.get('ESPTOOLPY_FLASHSIZE_DETECT', False):
        esptool_args.append('detect')
    else:
        esptool_args.append(config['ESPTOOLPY_FLASHSIZE'])

    esptool_args += [offset, bin_filename]
    subprocess.check_call(['python', esptool_path] + esptool_args)

def action_extensions(base_actions, project_dir):
    global build_action
    build_action = base_actions['actions']['all']['callback']

    actions = {
                'otagen' : {
                    'callback': ota_gen
                }, 
                'flashconfig': {
                    'callback': build_config, 
                    "arguments":[{'names':['configfile'], 'type':click.Path(exists=True)}]
                }
            }

    return {'actions': actions }