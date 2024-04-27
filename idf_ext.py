import os
import json
import sys
import re
import hashlib
import struct
import click
import subprocess
import configparser
import fnmatch

build_action = None


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
    version = get_version(args)
    device_type = config['IDF_TARGET']
    
    print("Generating OTA file(s)")
    print('Project: %s' % project_name)
    print('Version: %s' % version)
    ota_prefix = f'{project_name}.{device_type}'
    ota_suffix = f'.{version}.ota'
    build_dir = args['build_dir']

    build_action('app', ctx, args)
    
    if config['ESPTOOLPY_FLASHSIZE_1MB']:
        os.makedirs(build_dir, exist_ok=True)
        new_sdkconfig = os.path.join(build_dir, 'ota_part1_sdkconfig')
        new_parttable = os.path.join(build_dir, 'ota_part1_parttable.csv')
        
        sdkconfig = os.path.join(args.project_dir, 'sdkconfig')
        define_cache_entry = []
        for d in args.define_cache_entry:
            if d.startswith('SDKCONFIG='):
                sdkconfig = os.path.join(args.project_dir, d[10:])
                continue
            define_cache_entry.append(d)
        define_cache_entry.append('SDKCONFIG=' + new_sdkconfig)
        args.define_cache_entry = define_cache_entry

        with open(sdkconfig) as sdkconfig_in:
            with open(new_sdkconfig, 'w') as sdkconfig_out:
                for line in sdkconfig_in:
                    sdkconfig_out.write(line)
                sdkconfig_out.write(f'''
# Added by OTAGEN
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="{new_parttable}"''')
        
        idf_path = os.getenv('IDF_PATH')

        with open(os.path.join(idf_path, "components/partition_table/partitions_two_ota.1MB.csv")) as csv_in:
            with open(new_parttable, 'w') as csv_out:
                for line in csv_in:
                    if line.startswith('ota_0,'):
                        continue
                    csv_out.write(line)
                    
        ota_part1_build_dir = os.path.join(build_dir, 'ota_part1')
        args.build_dir = ota_part1_build_dir
        build_action('app', ctx, args)

        ota_files = [os.path.join(build_dir, f'{ota_prefix}.app1{ota_suffix}'),
                    os.path.join(build_dir, f'{ota_prefix}.app2{ota_suffix}')]
        
        generate_ota_file(os.path.join(build_dir, '%s.bin' % project_name), ota_files[0])

        generate_ota_file(os.path.join(ota_part1_build_dir, '%s.bin' % project_name), ota_files[1])    
    else:
        ota_files = [os.path.join(build_dir,  f'{ota_prefix}{ota_suffix}')]
        generate_ota_file(os.path.join(build_dir, '%s.bin' % project_name), ota_files[0])
    
    print(f"Generated {len(ota_files)} ota file{'s' if len(ota_files) > 1 else ''}")
    for ota_file in ota_files:
        print(f'    {ota_file}')
    


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
    subprocess.check_call(['python', 'config/configgen.py', configfile, csv_filename])
    
    nvs_part_gen_path = os.path.join(idf_path, 'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py')
    subprocess.check_call(['python', nvs_part_gen_path, 'generate', '--input', csv_filename, '--output', bin_filename, '--size', size])
    
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



def format_code(action, ctx, args):
    options = ['--suffix=none', '--style=1tbs']
    submodules = configparser.ConfigParser()
    submodules.read('.gitmodules')
    ignore_paths = []
    for submodule in submodules.sections():
        path = submodules[submodule]['path']
        ignore_paths.append(path)
        if path.startswith("components/"):
            ignore_paths.append(os.path.join(os.path.dirname(path), "include"))

    print(f'Will ignore: {ignore_paths}')

    def should_ignore(path):
        for d in ignore_paths:
            if path.startswith(d):
                return True
        return False

    for root, dirs, files in os.walk('components'):
        if should_ignore(root):
            continue
        run_astyle = False
        for f in files:
            if fnmatch.fnmatch(f, '*.[ch]'):
                run_astyle = True

        for d in dirs:
            full_path = os.path.join(root, d)
            if full_path in ignore_paths:
                dirs.remove(d)
        
        if run_astyle:
            subprocess.check_call(['astyle'] + options + [f'{root}/*.c,*.h'])
    subprocess.check_call(['astyle'] + options + ['main/*.c,*.h'])

def logs(action, ctx, args):
    import socket
    import struct

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', 5555))
    header_struct = struct.Struct("!4sL6s")

    last_sequence = -1
    while True:
        data, addr = sock.recvfrom(1200)
        sig, sequence, mac = header_struct.unpack_from(data)
        if sig == b'LOG\x00':
            if sequence != last_sequence + 1:
                print(f'Missing {sequence - (last_sequence + 1)} logging packets!')

            prefix = ''
            for b in mac:
                prefix += '%02x' % b
            print(prefix + ': '+  data[14:].decode(), end='')
            last_sequence = sequence

def gencomponents(actions, ctx, args):
    subprocess.check_call(['python', "tools/gencomponents.py"])

def action_extensions(base_actions, project_dir):
    global build_action
    build_action = base_actions['actions']['app']['callback']

    actions = {
                'otagen' : {
                    'callback': ota_gen
                }, 
                'flashconfig': {
                    'callback': build_config, 
                    "arguments":[{'names':['configfile'], 'type':click.Path(exists=True)}]
                },
                'ensureformat': {
                    'callback': format_code
                },
                'logs': {
                    'callback': logs
                },
                'gencomponents': {
                    'callback': gencomponents
                }
            }

    return {'actions': actions }