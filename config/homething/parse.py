from parsimonious.grammar import Grammar
from parsimonious.nodes import NodeVisitor
from parsimonious.exceptions import VisitationError

from .encode import *

grammar = Grammar(r"""
start      = line* 
line       = ws? (definition / assignment) ws?
id         = ~"[a-z_][a-z0-9_]*"i
number     = ~"-?[0-9]+"
hex        = ~"0x[0-9a-f]+"i
ws         = ~"\s*"
lpar       = "("
rpar       = ")"
equal      =  ws? "=" ws?
str        = ~'"[^\"]+"'
arg        = ws? ( definition / id / str / hex/ number) ws?
args       =  (arg "," args) / arg 
definition = id "(" args? ")"
assignment = id equal definition 
""")


class Assignment:
    def __init__(self, source, pos, id, call):
        self.source = source
        self.pos = pos
        self.id = id
        self.call = call

    def process(self, id_table, profile):
        if self.id in id_table:
            self.source.print_message_for_location(self.pos, "warning", f'Duplicate id "{self.id}"')

        id_table[self.id] = len(profile) - 1
        self.call.process(id_table, profile)

    def __str__(self):
        return f"Assign({self.id}, {self.call})"


class ProfileVisitor(NodeVisitor):
    def __init__(self, source):
        super().__init__()
        self.source = source
        self.error = None

    def raise_error(self, error):
        self.error = error
        raise error

    def visit_start(self, node, visited_children):
        return visited_children

    def visit_str(self, node, visited_children):
        return node.text[1:-1]

    def visit_hex(self, node, visited_children):
        return int(node.text[2:], 16)

    def visit_number(self, node, visited_children):
        return int(node.text)

    def visit_id(self, node, visited_children):
        return ID(node.text)

    def visit_arg(self, node, visited_children):
        return [Arg(node.start, visited_children[1][0])]

    def visit_args(self, node, visited_children):
        if len(visited_children[0]) == 1:
            return visited_children[0]
        return visited_children[0][0] + visited_children[0][2]

    def visit_definition(self, node, visited_children):
        id, _, args, _ = visited_children
        try:
            if isinstance(args, list):
                return components[id.name](node.start, args[0])
            return components[id.name](node.start, [])
        except KeyError:
            self.raise_error(ProfileEntryError(node.start, f'Unknown definition name "{id.name}"'))

    def visit_assignment(self, node, visited_children):
        id, _, definition = visited_children
        return Assignment(self.source, node.start, id.name, definition)

    def visit_line(self, node, visited_children):
        return visited_children[1][0]

    def generic_visit(self, node, visited_children):
        """ The generic visit method. """
        return visited_children or node


class ProfileSource:
    def __init__(self):
        self.text = ""
        self.filename = ""
        self.tree = None

    def load(self, filename):
        self.filename = filename
        with open(filename) as f:
            self.text = f.read()

    def print_message_for_location(self, location, message_type, message):
        start_of_line = self.text.rfind('\n', 0, location)
        end_of_line = self.text.find('\n', location)
        if start_of_line == -1:
            start_of_line = 0
            line_no = 1
        else:
            start_of_line += 1
            line_no = 1
            index = 0
            while index < start_of_line:
                index = self.text.find('\n', index + 1)
                if index == -1:
                    break
                line_no += 1

        if end_of_line == -1:
            end_of_line = len(self.text)

        print(f'{self.filename}: {line_no}: {message_type}: {message}')
        line = self.text[start_of_line:end_of_line]
        print(line)
        indent = ' ' * (location - start_of_line)
        print(f'{indent}^')

    def parse(self):
        self.tree = grammar.parse(self.text)

    def process(self):
        id_table = {}
        profile = [1]
        visitor = ProfileVisitor(self)
        try:
            for entry in visitor.visit(self.tree):
                entry.process(id_table, profile)
        except VisitationError:
            if visitor.error is not None:
                raise visitor.error
            raise
        return profile
