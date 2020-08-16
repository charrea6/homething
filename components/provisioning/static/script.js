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
    }
    return html;
}

function variableGetValue(setting, variable) {
    var el = document.getElementById(variableId(setting,variable));
    if (variable.type == "checkbox") {
        return el.checked;
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
    if (variable.type == "checkbox") {
        el.checked = value;
    } else {
        el.value = value;
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
            var data = JSON.parse(this.responseText);
            loadValues(data);
        }
    };
    xhttp.open("GET", "/config", true);
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
                    config_setting[subVar.name] = value
                }
            } else {       
                config_setting[variable.name] = variableGetValue(setting, variable);
            }
        }
    }
    var xhttp = new XMLHttpRequest();
    xhttp.open("POST", "/config", true);
    xhttp.setRequestHeader("Content-Type", "application/json;charset=UTF-8");
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
    xhttp.send(JSON.stringify(config));
}

(function(){
    loadConfig(settings);
})();