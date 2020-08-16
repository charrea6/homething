import sys
import os
import re
import datetime
import argparse
from bs4 import BeautifulSoup

RE_FILENAME_DATA_SECTION=re.compile('[./\- ]')

class StaticFile:
    def __init__(self, path, filename):
        self.path = path
        self.filename = filename
        self.data_section_name = 'sf_' + RE_FILENAME_DATA_SECTION.sub('_', filename)
        self.data = None
    
    def load(self):
        if self.data is None:
            with open(self.path, 'rb') as fp:
                self.data = fp.read()
        return self.data

    def generate_data_section(self):
        if self.filename.endswith('.css'):
            content_type = 'CT_CSS'
        elif self.filename.endswith('.js'):
            content_type = 'CT_JS'
        elif self.filename.endswith('.woff2'):
            content_type = 'CT_WOFF2'
        else:
            content_type = 'CT_HTML'
        
        data = self.load()
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


def process_html_file(html_file, files):
    files_linked = set()
    files_embedded = set()
    html_data = html_file.load()
    print('Processing html file ' + html_file.filename)
    soup = BeautifulSoup(html_data, "html.parser")
    stylesheets_to_embed = []
    js_to_embed = []
    for ss in soup.find_all('link', rel='stylesheet'):
        if ss.get("embed") is not None:
            stylesheets_to_embed.append(os.path.join(os.path.dirname(html_file.filename), ss['href']))
            ss.extract()
    for js in soup.find_all('script'):
        if js.get("embed") is not None:
            js_to_embed.append(os.path.join(os.path.dirname(html_file.filename), js['src'])) 
            js.extract()
    
    style_data = ''
    for ss in stylesheets_to_embed:
        for f in files:
            if f.filename == ss:
                style_data += f.load().decode()
                files.remove(f)
                break
        else:
            raise RuntimeError("Failed to find stylesheet to embed! %s" % ss)
    
    head = soup.find('head')
    ss = soup.new_tag('style')
    ss.append(style_data)
    head.append(ss)

    js_data = ''
    for js in js_to_embed:
        for f in files:
            if f.filename == js:
                js_data += f.load().decode()
                files.remove(f)
                break
        else:
            raise RuntimeError("Failed to find script to embed! %s" % ss)
    
    body = soup.find('body')
    script = soup.new_tag('script')
    script.append(js_data)
    body.append(script)
    html_file.data = str(soup).encode()
    return files_linked, files_embedded


def process_files(files):
    processed_files = []
    files_linked = set()
    files_embedded = set()
    for f in files:
        if f.filename.endswith('.html'):
            linked, embedded = process_html_file(f, files)
            files_linked |= linked
            files_embedded |= embedded

    files_embedded -= files_linked & files_embedded
    for f in files:
        if f not in files_embedded:
            processed_files.append(f)

    return processed_files


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('paths', type=str, nargs='+')
    parser.add_argument("--output", "-o", type=argparse.FileType('w'))

    args = parser.parse_args()
    static_files = []
    for path in args.paths:
        if os.path.isdir(path):
            base_dir = os.path.abspath(path)        
            for root, dirs, files in os.walk(base_dir):
                for name in files:
                    filepath = os.path.join(root, name)
                    filename = filepath[len(base_dir):]
                    static_files.append(StaticFile(filepath, filename))
        else:
            static_files.append(StaticFile(path, '/' + os.path.basename(path)))
    
    static_files = process_files(static_files)

    print(f'/* AUTO GENERATED FILE! {datetime.datetime.now()} */', file=args.output)
    print('#include "provisioning_int.h"', file=args.output)

    for sf in static_files:
        print(sf.generate_data_section(), file=args.output)

    print('static const httpd_uri_t static_file_urls[] = {', file=args.output)
    for i, sf in enumerate(static_files):
        print(sf.generate_url_handler(), end='', file=args.output)
        if i +1 < len(static_files):
            print(',', file=args.output)
        
    print('\n};', file=args.output)

    print(f"""
void provisioningRegisterStaticFileHandlers(httpd_handle_t server)
{{
    int i;
    for (i = 0; i < {len(static_files)}; i ++) {{
        httpd_register_uri_handler(server, &static_file_urls[i]);
    }}
}}
""", file=args.output)

