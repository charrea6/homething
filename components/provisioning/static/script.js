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
    case 'choice':html=`<div><label for="${varId}">${variable.title}</label><select class="u-full-width" id="${varId}">`
        for (var option of variable.choices) {
            html += `<option value="${option[0]}">${option[1]}</option>`
        }
        html += '</select></div>'
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
    if (variable.type == "choice") {
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
            el.value = 'homething-' + value;
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
    fetch("/config").then((response) => response.json()).then((data) => loadValues(data));
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
                    if (value !== "") {
                        config_setting[subVar.name] = value;
                    }
                }
            } else {       
                var value = variableGetValue(setting, variable);
                if (value !== ""){
                    config_setting[variable.name] = value;
                }
            }
        }
    }

    fetch('/config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(config)
    }).then((response) => {
        document.getElementById('savebtn').disabled=false;
        var el=document.getElementById('msg');
        if (response.status == 200) {
            msg.innerHTML = response.statusText;
        } else {
            msg.innerHTML =`<span class="error">${response.statusText}</span>`;
        }
    });
}

(function(){
    loadConfig(settings);
})();