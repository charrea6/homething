

class ProfileEntryError(Exception):
    def __init__(self, location, message):
        super().__init__()
        self.location = location
        self.message = message


class ProfileEntryWrongArgumentTypeError(ProfileEntryError):
    def __init__(self, arg, expected_types):
        type_name = type(arg.arg).__name__
        if isinstance(expected_types, tuple):
            expected_types_str = ''
            for t in expected_types:
                if expected_types_str:
                    expected_types_str = f"{expected_types_str} or {t.__name__}"
                else:
                    expected_types_str = f"{t.__name__}"
        else:
            expected_types_str = f"{expected_types.__name__}"
        super().__init__(arg.pos, f"Incorrect type for argument, should be {expected_types_str} not {type_name}")


class ProfileEntryIncorrectNumberArgumentsError(ProfileEntryError):
    def __init__(self, pos, type_name, args_count, required_args):
        super().__init__(pos, f"Incorrect number of arguments for {type_name}, should be {required_args} was {args_count}")


class UnsupportedProfileVersionError(Exception):
    pass