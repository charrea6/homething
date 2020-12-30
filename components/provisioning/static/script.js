function variableId(setting, variable){
    return `${setting.name}_${variable.name}`;
}

function variableToStr(setting, variable){
    var html="";
    var varId = variableId(setting, variable);

    switch(variable.type) {
    case 'username':html=`<div class="six columns"><label for="${varId}">${variable.title}</label><input class="u-full-width" type="text" placeholder="${variable.title}" id="${varId}"></div>`;
        break;
    case 'password':html=`<div class="six columns"><label for="${varId}">${variable.title}</label><input class="u-full-width" type="password" placeholder="${variable.title}" id="${varId}"></div>`;
        break;
    case 'hostname':html=`<div class="nine columns"><label for="${varId}">${variable.title}</label><input class="u-full-width" type="text" placeholder="${variable.title}" id="${varId}"></div>`;
        break;
    case 'port':html=`<div class="three columns"><label for="${varId}">${variable.title}</label><input class="u-full-width" type="number" id="${varId}"></div>`;
        break;
    case 'checkbox':html=`<div class="six columns"><label><input type="checkbox" id="${varId}"}><span class="label-body">${variable.title}</span></label></div>`;
        break;
    case 'device_id':html=`<div><label for="${varId}">${variable.title}</label><input readonly class="u-full-width" type="text" placeholder="${variable.title}" id="${varId}"></div>`;
        break;
    case 'string':html=`<div><label for="${varId}">${variable.title}</label><input class="u-full-width" type="text" placeholder="${variable.title}" id="${varId}"></div>`;
        break;
    }
    return html;
}

function variableGetValue(setting, variable) {
    if (variable.type == 'device_id') {
        return "";
    }
    var el = document.getElementById(variableId(setting, variable));
    if (variable.type == "checkbox") {
        return el.checked;
    }
    if ((variable.type == "port") && (el.value != "")) {
        return parseInt(el.value);
    }
    return el.value;
}

function variableSetValue(setting, variable, values) {
    console.log(`Setting ${setting.name} ${variable.name}`);
    if (!values.hasOwnProperty(setting.name)) {
        console.log(`No value for setting ${setting.name}`);
        return;
    }
    if (!values[setting.name].hasOwnProperty(variable.name)) {
        console.log(`No value for variable ${variable.name}`);
        return;
    }
    var el = document.getElementById(variableId(setting,variable));
    var value = values[setting.name][variable.name];
    switch(variable.type) {
        case "checkbox": el.checked = value;
            break;
        case "device_id":
            mac = ''
            for (var i of value) {
                let byte = i.toString(16);
                if (byte.length < 2) {
                    byte = '0' + byte;
                }
                mac += byte;
            }
            el.value = 'homething-' + mac;
            break;
        default:
            el.value = value;
            break;
    }
}

function loadConfig(){
    var cfgEl = document.getElementById("config");
    for (var setting of settings) {
        var html = `<div class="row">
            <div class="six columns">
                <h3>${setting.title}</h3>
            </div>
        </div>`;
        cfgEl.innerHTML += html;
        
        for (var variable of setting.variables) {
            var html = "";
            if (Array.isArray(variable)) {
                for (var subVar of variable) {
                    html += variableToStr(setting, subVar);
                }
            } else {
                html = variableToStr(setting, variable);
            }
            cfgEl.innerHTML += `<div class="row">${html}</div>`;
        }
    }
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            var data = CBOR.decode(this.response);
            loadValues(data);
        }
    };
    xhttp.open("GET", "/config", true);
    xhttp.responseType = "arraybuffer";
    xhttp.send();
}

function loadValues(values) {
    for (var setting of settings) {
        
        for (var variable of setting.variables) {
            if (Array.isArray(variable)) {
                for (var subVar of variable) {
                    variableSetValue(setting, subVar, values);
                }
            } else {
                variableSetValue(setting, variable, values);
            }
        }
    }
}

function saveConfig() {
    config = {};
    for (var setting of settings) {
        config[setting.name] = {};
        var config_setting = config[setting.name];
        for (var variable of setting.variables){
            
            if (Array.isArray(variable)) {
                for (var subVar of variable) {    
                    var value = variableGetValue(setting, subVar);
                    if (value != "") {
                        config_setting[subVar.name] = value;
                    }
                }
            } else {       
                var value = variableGetValue(setting, variable);
                if (value != ""){
                    config_setting[variable.name] = value;
                }
            }
        }
    }
    var xhttp = new XMLHttpRequest();
    xhttp.open("POST", "/config", true);
    xhttp.setRequestHeader("Content-Type", "application/cbor");
    xhttp.onreadystatechange = function() {
    if (this.readyState == 4) {
        document.getElementById('savebtn').disabled=false;
        var el=document.getElementById('msg');
        if (this.status == 200) {
            msg.innerHTML = this.responseText;
        } else {
            msg.innerHTML =`<span class="error">${this.responseText}</span>`;
        }
    }
    };
    document.getElementById('savebtn').disabled=true;
    xhttp.send(CBOR.encode(config));
}

(function(){
    loadConfig(settings);
})();