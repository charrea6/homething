import os
import yaml
try:
    from yaml import CLoader as Loader
except ImportError:
    from yaml import Loader
import json


def get_variables_name(setting):
    variables_name = setting['name'].replace(' ', '_').replace('-', '_')
    return f'variables_{variables_name}'


def process_variables(settings, out):
    for setting in settings:
        variables = []
        for group in setting['variables']:
            if isinstance(group, list):
                for variable in group:
                    variables.append(variable)
            else:
                variables.append(group)
        variables_name = get_variables_name(setting)
        
        out.write(f'#define {variables_name}_len {len(variables)}\n')
        out.write(f'struct variable {variables_name}[] = {{\n')
        for variable in variables:
            out.write(f'        {{ .name="{variable["name"]}", .type=FT_{variable["type"].upper()} }},\n')
        out.write('};\n')


def process_settings(settings, out):
    out.write(f'const int nrofSettings={len(settings)};\n')
    out.write('static struct setting settings[]= {\n')
    for setting in data:
        variables_name = get_variables_name(setting)
        out.write(f'{{ .name="{setting["name"]}", .nrofVariables={variables_name}_len, .variables={variables_name} }},\n')
    out.write('};\n')


def process_json_description(settings, out):
    json_str = json.dumps(settings)
    print(f'var settings = {json_str};', file=out)


def load_settings(yaml_file):
    with open(yaml_file) as f:
        settings = yaml.load(f, Loader=Loader)
    return settings


if __name__ == '__main__':
    import sys
    yaml_file = sys.argv[1]
    c_file = sys.argv[2]
    js_file = sys.argv[3]

    yaml_file_last_mod = os.path.getmtime(yaml_file)
    if os.path.exists(c_file) and os.path.exists(js_file):
        c_file_last_mod = os.path.getmtime(c_file)
        js_file_last_mod = os.path.getmtime(js_file)
        if yaml_file_last_mod < c_file_last_mod and yaml_file_last_mod < js_file_last_mod:
            sys.exit(0)

    data = load_settings(yaml_file)

    with open(c_file, 'w') as f:
        process_variables(data, f)
        process_settings(data, f)

    with open(js_file, 'w') as f:
        process_json_description(data, f)
