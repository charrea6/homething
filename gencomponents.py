import yaml
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


class Component:
    def __init__(self, name, arguments, condition) -> None:
        self.name = name
        self.arguments = arguments
        self.condition = condition
        self.arguments.append(Argument(name, "name", "string", {"optional": True}))
    
    @property
    def struct_name(self) -> str:
        return f'Config_{self.normalised_name}'
    
    @property
    def normalised_name(self) -> str:
        parts = self.name.split('_')
        name = ''
        for part in parts:
            name += part.title()
        return name

    @property
    def normalised_field_name(self) -> str:
        parts = self.name.split('_')
        name = parts.pop(0)
        for part in parts:
            name += part.title()
        return name


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
    for name, details in components_dict.items():
        components.append(load_component(name, details))

    with open("components/deviceprofile/include/component_config.h", "w") as fp:
        template = env.get_template("component_config.h.jinja")
        fp.write(template.render(components=components))

    with open("components/deviceprofile/component_config.c", "w") as fp:
        template = env.get_template("component_config.c.jinja")
        fp.write(template.render(components=components))

if __name__ == '__main__':
    main()