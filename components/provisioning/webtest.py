import os
import io
from flask import Flask, send_file, jsonify, redirect, url_for, request, make_response, Response
import gensettings
import json
app = Flask(__name__)

class StaticFile:
    def __init__(self, path):
        self.path = path
    
    def __call__(self):
        return send_file(self.path)

base_dir = os.path.abspath('static')
for root, dirs, files in os.walk(base_dir):
    for name in files:
        path = os.path.join(root, name)
        filename = path[len(base_dir):].replace('/index.html', '/')
        
        app.add_url_rule(filename, endpoint=filename, view_func=StaticFile(path))

@app.route('/settings.js')
def settings():
    settings = gensettings.load_settings('settings.yml')
    out = io.StringIO()
    gensettings.process_json_description(settings, out)
    return Response(out.getvalue(), mimetype='text/javascript')

setting_values = {
    'thing': {
        'id': '010203A1B2C3',
    }
}

@app.route('/config', methods=['GET', 'POST'])
def values():
    global setting_values
    if request.method == 'GET':
        return Response(json.dumps(setting_values), mimetype='application/json')
    else:
        setting_values = json.loads(request.get_data())
        setting_values['thing']['id'] = '010203A1B2C3'
        if 'wifi' in setting_values and 'pass' in setting_values['wifi']:
            if setting_values['wifi']['pass'] == 'error':
                return make_response('Error found!', 500)
        return 'Saved'


if __name__ == '__main__':
    app.run()