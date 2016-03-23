Pebble.addEventListener("ready", function(e) {
  var config = localStorage.getItem("config");
  // set default configuration, if not yet present
  if(!config) {
    localStorage.setItem("config", JSON.stringify(defaults()));
   }
});

Pebble.addEventListener("showConfiguration", function(e) {
  var config = JSON.parse(localStorage.getItem("config")),
      web_config = defaults();
  for(var key in config) {
    if(config.hasOwnProperty(key)) {
      if(key == "now_color" || key == "now_background" || key == "now_battery_color"  || key == "now_battery_low" || key == "clock_color" || key == "clock_battery_color"  || key == "clock_battery_low" || key == "clock_background") {
        web_config[key] = '#' + ('00000' + (config[key] | 0).toString(16)).substr(-6);
      } else {
        web_config[key] = config[key];
      }
    }
  }
  var loc = 'https://brogon.github.io/now_plus_watchface/config.html#' + encodeURIComponent(JSON.stringify(web_config));
  Pebble.openURL(loc);
});

Pebble.addEventListener("webviewclosed", function(e) {
  if(e.response && e.response.length) {
    var config = decodeURIComponent(e.response),
        merged_config = defaults();
    if(config != "CANCELLED") {
      config = JSON.parse(config);
      for(var key in config) {
        if(config.hasOwnProperty(key)) {
          if(key == "now_color" || key == "now_background" || key == "now_battery_color"  || key == "now_battery_low" || key == "clock_color" || key == "clock_battery_color"  || key == "clock_battery_low" || key == "clock_background") {
            var ival = strtol(config[key]);
            if( ! Number.isNaN(ival))
              merged_config[key] = ival;
          } else if(key == "now_battery_blinks" || key == "clock_battery_blinks") {
            merged_config[key] = config[key] ? 1 : 0;
          } else if(key == "clock_delay") {
            if(typeof config[key] === "number")
              merged_config[key] = Math.round(config[key]);
          } else {
            if(typeof config[key] === "string")
              merged_config[key] = config[key];
          }
        }
      }
      localStorage.setItem("config", JSON.stringify(merged_config));
      update_pebble(merged_config);
    }
  }
});

function defaults() {
  var settings = {
    "now_text": "NOW",
    "now_color": 0xffffff,
    "now_background": 0x000000,
    "now_font": "GOTHIC_28_BOLD",
    "now_battery_show": "LOW",
    "now_battery_blinks": true,
    "now_battery_color": 0xffffff,
    "now_battery_low": 0xff0000,
    "clock_color": 0xffffff,
    "clock_background": 0x000000,
    "clock_font": "BITHAM_42_MEDIUM_NUMBERS",
    "clock_date_font": "GOTHIC_18",
    "clock_battery_show": "ALWAYS",
    "clock_battery_blinks": true,
    "clock_battery_color": 0xffffff,
    "clock_battery_low": 0xff0000,
    "clock_delay": 4
  };
  return settings;
}

function ack() {
}

function nack(config) {
  return function(e) {
      Pebble.sendAppMessage(config, ack, nack(config));
    };
}

function strtol(s) {
  if(typeof s === "string" && s[0] == '#')
    s = s.slice(1);
  return parseInt(s, 16);
}

function update_pebble(config) {
  console.log(JSON.stringify(config));
  Pebble.sendAppMessage(config, ack, nack(config));
}
