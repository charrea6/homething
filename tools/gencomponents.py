import yaml
from collections import defaultdict
from jinja2 import Environment, FileSystemLoader, select_autoescape
env = Environment(
    loader=FileSystemLoader("templates"),
    autoescape=select_autoescape(),
    trim_blocks=True,
    lstrip_blocks=True,
    extensions=['jinja2_strcase.StrcaseExtension']
)

class Argument:
    def __init__(self, component, name, type, options) -> None:
        self.component = component
        self.name = name
        self.type = type
        self.options = options
    
    @property
    def enum_name(self) -> str:
        return f"Choices_{self.component.title()}_{self.name.title()}"
    
    @property
    def is_optional(self) -> bool:
        return self.options.get('optional', False)
    
    @property
    def has_default(self) -> bool:
        return 'default' in self.options
    
    @property
    def default(self):
        return self.options['default']

    def __str__(self) -> str:
        return self.name

class Component:
    def __init__(self, name, arguments, condition) -> None:
        self.name = name
        self.arguments = arguments
        self.condition = condition
        self.arguments.append(Argument(name, "name", "string", {"optional": True}))
        self.arguments.append(Argument(name, "id", "string", {"optional": True}))
    
    def get_arg(self, name):
        for arg in self.arguments:
            if arg.name == name:
                return arg
    
    @property
    def mandatory_args(self):
        return {arg for arg in self.arguments if not arg.is_optional}
    
    @property
    def normalised_name(self) -> str:
        parts = self.name.split('_')
        name = ''
        for part in parts:
            name += part[0].upper() + part[1:]
        return name

    @property
    def normalised_field_name(self) -> str:
        parts = self.name.split('_')
        name = parts.pop(0)
        for part in parts:
            name += part[0].upper() + part[1:]
        return name

    def __str__(self) -> str:
        return self.name

def process_option(component, argument, details):
    arg_type_options = {}
    if isinstance(details, dict):
        arg_type = details.pop('type')
        arg_type_options = details
    else:
        arg_type = details
    
    return Argument(component, argument, arg_type, arg_type_options)


def load_component(name, details):
    arguments = []
    for argument, argument_details in details['args'].items():
        arguments.append(process_option(name, argument, argument_details))
    return Component(name, arguments, details.get('condition'))


def main():
    with open("components.yml") as fp:
        components_dict = yaml.safe_load(fp)
    
    components = []
    uint_types = {"uint", "gpioPin", "gpioLevel", "i2cAddr"}
    used_types = set()
    type_conditions = defaultdict(list)
    for name, details in components_dict.items():
        component = load_component(name, details)
        components.append(component)
        for arg in component.arguments:
            used_types.add(arg.type)
            type_conditions[arg.type].append(component.condition)
    
    for type_name,conditions in type_conditions.items():
        combined = ""
        for condition in conditions:
            if condition is None:
                combined = ""
                break

            if combined:
                combined = f"{combined} || ({condition})"
            else:
                combined = f"({condition})"
        type_conditions[type_name] = combined

    uint_condition = ""
    for uint_type in uint_types:
        if uint_type == "uint":
            continue
        
        if type_conditions[uint_type]:
            if uint_condition:
                uint_condition = f"{uint_condition} || {type_conditions[uint_type]}"
            else:
                uint_condition = type_conditions[uint_type]
        else:
            uint_condition = ""
            break
    type_conditions["uint"] = uint_condition

    with open("components/deviceprofile/include/component_config.h", "w") as fp:
        template = env.get_template("component_config.h.jinja")
        fp.write(template.render(components=components, used_types=used_types))

    with open("components/deviceprofile/component_config_internal.h", "w") as fp:
        template = env.get_template("component_config_internal.h.jinja")
        fp.write(template.render(components=components, used_types=used_types, uint_types=uint_types, type_conditions=type_conditions))
    
    with open("components/deviceprofile/field_types_used.h", "w") as fp:
        template = env.get_template("field_types_used.h.jinja")
        fp.write(template.render(used_types=used_types, uint_types=uint_types, type_conditions=type_conditions))
    
    with open("components/provisioning/components_json.c", "w") as fp:
        template = env.get_template("components_json.c.jinja")
        fp.write(template.render(components=components))
    

if __name__ == '__main__':
    main()