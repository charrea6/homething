import os
import json
import sys
import re
import hashlib
import struct


profile_keys = [('LIGHT', 'L'), ('DHT22', 'T'), ('FAN', 'F'), ('DOORBELL', 'D'), ('MOTION', 'M')]


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
        m = re.match('char version\[\]=\"([^\"]+)\";', line)
        if m:
            return m.group(1)
    return 'unknown'
        

def generate_ota_file(in_file, out_file):
    with open(in_file, 'rb') as inf:  
        with open(out_file, 'wb') as outf:
            outf.write('OTA\0')
            outf.write('\0' * 20)
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
    

def action_extensions(base_actions, project_dir):
    return {'actions': {'otagen' : {'callback': ota_gen}}}