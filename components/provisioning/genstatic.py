import sys
import os
import re
import io
import datetime
import argparse
import gzip
import zlib
from bs4 import BeautifulSoup
import requests

RE_FILENAME_DATA_SECTION=re.compile(r'[./\- ]')
SIZE_FLAG_COMPRESSED = 0x80000000
cache_dir = None

def compress(data):
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode='wb') as f:
        f.compress = zlib.compressobj(9, zlib.DEFLATED, -10, 9, 0)
        f.write(data)
    return buf.getvalue()


def load(path):
    if cache_dir is None:
        with open(path, 'rb') as fp:
            return fp.read()
    else:
        filename = os.path.basename(path)
        cache_path = os.path.join(cache_dir, filename)
        if os.path.exists(cache_path) and os.path.getmtime(path) < os.path.getmtime(cache_path):
            with open(cache_path, 'rb') as fp:
                return fp.read()
        else:
            with open(path, 'rb') as fp:
                data = fp.read()

            url = None
            if path.endswith(".html"):
                url = 'https://html-minifier.com/raw'
            elif path.endswith(".js"):
                url = 'https://javascript-minifier.com/raw'
            elif path.endswith(".css"):
                url = 'https://cssminifier.com/raw'
                
            if url is None:
                return data
            else:
                response = requests.post(url, data={'input': data})
                if response.status_code == 200:
                    data = response.content
                    with open(cache_path, "wb") as fp:
                        fp.write(data)
                    return data
                else:
                    print(f"Response: {response}")
                    response.raise_for_status()
            
class StaticFile:
    def __init__(self, path, filename):
        self.path = path
        self.filename = filename
        self.data_section_name = 'sf_' + RE_FILENAME_DATA_SECTION.sub('_', filename)
        self.data = None
    
    def load(self):
        if self.data is None:
            self.data = load(self.path)
        return self.data

    def generate_data_section(self):
        can_compress = True
        if self.filename.endswith('.css'):
            content_type = 'CT_CSS'
        elif self.filename.endswith('.js'):
            content_type = 'CT_JS'
        else:
            content_type = 'CT_HTML'
        
        data = self.load()
        if can_compress:
            data = compress(data)
            size = len(data)
            compression = '| SIZE_FLAG_COMPRESSED'
        else:
            size = len(data)
            compression = ''
        data_section = f"""static const struct static_file_data {self.data_section_name} = {{
    .size= {size} {compression},
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
    parser.add_argument("--output", "-o", type=str)
    parser.add_argument("--cache-dir", "-c", type=str)

    args = parser.parse_args()
    if args.cache_dir is not None:
        cache_dir = args.cache_dir
        os.makedirs(cache_dir, exist_ok=True)

    if os.path.exists(args.output):
        output_mod_time = os.path.getmtime(args.output)
    else:
        output_mod_time = 0
    
    modified = False
    static_files = []
    for path in args.paths:
        if os.path.isdir(path):
            base_dir = os.path.abspath(path)        
            for root, dirs, files in os.walk(base_dir):
                for name in files:
                    filepath = os.path.join(root, name)
                    filename = filepath[len(base_dir):]
                    if os.path.getmtime(filepath) > output_mod_time:
                        modified = True
                    static_files.append(StaticFile(filepath, filename))
        else:
            static_files.append(StaticFile(path, '/' + os.path.basename(path)))
    
    if not modified:
        sys.exit(0)

    static_files = process_files(static_files)
    with open(args.output, 'w') as output:

        print(f'/* AUTO GENERATED FILE! {datetime.datetime.now()} */', file=output)
        print('#include "provisioning_int.h"', file=output)

        for sf in static_files:
            print(sf.generate_data_section(), file=output)

        print('static const httpd_uri_t static_file_urls[] = {', file=output)
        for i, sf in enumerate(static_files):
            print(sf.generate_url_handler(), end='', file=output)
            if i +1 < len(static_files):
                print(',', file=output)
            
        print('\n};', file=output)

        print(f"""
void provisioningRegisterStaticFileHandlers(httpd_handle_t server)
{{
    int i;
    for (i = 0; i < {len(static_files)}; i ++) {{
        httpd_register_uri_handler(server, &static_file_urls[i]);
    }}
}}
""", file=output)

