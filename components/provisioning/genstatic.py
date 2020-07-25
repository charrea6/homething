import sys
import os
import re
import datetime

RE_FILENAME_DATA_SECTION=re.compile('[./\- ]')

class StaticFile:
    def __init__(self, path, filename):
        self.path = path
        self.filename = filename
        self.data_section_name = 'sf_' + RE_FILENAME_DATA_SECTION.sub('_', filename)

    def generate_data_section(self):
        with open(self.path, 'rb') as fp:
            data = fp.read()

        
        if self.filename.endswith('.css'):
            content_type = 'CT_CSS'
        elif self.filename.endswith('.js'):
            content_type = 'CT_JS'
        elif self.filename.endswith('.woff2'):
            content_type = 'CT_WOFF2'
        else:
            content_type = 'CT_HTML'
        

        data_section = f"""static const struct static_file_data {self.data_section_name} = {{
    .size= {len(data)},
    .content_type= {content_type},
    .data= {{
        """

        for i,ch in enumerate(data):
            data_section += '0x%02x' % ch
            if i + 1 < len(data):
                data_section += ','
            if (i + 1) % 40 == 0:
                data_section += '\n        '
            else:
                data_section += ' '
        data_section += '}\n};\n'
        return data_section
        

    def generate_url_handler(self):
        filename = self.filename
        if filename == '/index.html':
            filename = '/'
        return f"""    {{
        .uri       = "{filename}",
        .method    = HTTP_GET,
        .handler   = provisioningStaticFileHandler,
        .user_ctx  = (void *)&{self.data_section_name}
    }}"""

if __name__ == '__main__':
    base_dir = os.path.abspath(sys.argv[1])
    static_files = []
    for root, dirs, files in os.walk(base_dir):
        for name in files:
            path = os.path.join(root, name)
            filename = path[len(base_dir):]
            static_files.append(StaticFile(path, filename))
    
    print(f'/* AUTO GENERATED FILE! {datetime.datetime.now()} */')
    print('#include "provisioning_int.h"')

    for sf in static_files:
        print(sf.generate_data_section())

    print('static const httpd_uri_t static_file_urls[] = {')
    for i, sf in enumerate(static_files):
        print(sf.generate_url_handler(), end='')
        if i +1 < len(static_files):
            print(',')
        
    print('\n};')

    print(f"""
void provisioningRegisterStaticFileHandlers(httpd_handle_t server)
{{
    int i;
    for (i = 0; i < {len(static_files)}; i ++) {{
        httpd_register_uri_handler(server, &static_file_urls[i]);
    }}
}}
""")

