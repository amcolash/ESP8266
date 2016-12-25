if (wifi.sta.getip () == nil) then
  print ('no connectivity');
else

--
-- create a master list of devices
--
devices = {};

--
-- set GPIO pin 3 for output and turn off its LED
--
led1 = 3;
gpio.mode (led1, gpio.OUTPUT);
gpio.write (led1, gpio.HIGH);
devices["GPIO3"] = {pin=3, mode=gpio.OUTPUT, on=gpio.LOW, off=gpio.HIGH};

--
-- set GPIO pin 4 for output and turn off its LED
--
led2 = 4
gpio.mode (led2, gpio.OUTPUT);
gpio.write (led2, gpio.HIGH);
devices["GPIO4"] = {pin=4, mode=gpio.OUTPUT, on=gpio.LOW, off=gpio.HIGH};

--
-- create a socket server on port 80 for to handle HTTP requests
--
srv = net.createServer (net.TCP)
srv:listen (80, function (connection)
    connection:on ("receive", function (client, request)


--[[
-- TODO:
-- 1) generalize this into a REST API.
-- 2) Provide main page(s).
--     /index.html
--     /
-- 3) Provide services with which to interact.
--     /device/GPIO3/on -- return JSON status
--     /device/GPIO3/off -- return JSON status
--     /device/GPIO3/status.json --> return JSON status
--     /status.json -- return JSON status
--     /devices.json -- return JSON list of devices
-- 4) Learn about the websocket module and see how it can be used for
--    monitoring the device states.
-- 5) Learn about the mqtt module and how to stream data to the Adafruit
--    cloud.
-- 6) Learn about the coap module and how to represent the device using
--    existing IoT mechanisms and designs.
--]]



--[[
JSON encoding into a string...

ok, json = pcall(cjson.encode, {key="value"})
if ok then
  print(json)
else
  print("failed to encode!")
end

Read ADC value...

print (adc.read(0));

Use PWM on the GPIO pin(s)...

pwm.setup(1, 500, 512)
pwm.setup(2, 500, 512)
pwm.setup(3, 500, 512)
pwm.start(1)
pwm.start(2)
pwm.start(3)
function led(r, g, b)
    pwm.setduty(1, g)
    pwm.setduty(2, b)
    pwm.setduty(3, r)
end
led(512, 0, 0) --  set led to red
led(0, 0, 512) -- set led to blue.

MQTT example...

TODO

COAP example...

TODO


-- use copper addon for firefox
cs=coap.Server()
cs:listen(5683)

myvar=1
cs:var("myvar") -- get coap://192.168.18.103:5683/v1/v/myvar will return the value of myvar: 1

all='[1,2,3]'
cs:var("all", coap.JSON) -- sets content type to json

-- function should tack one string, return one string.
function myfun(payload)
  print("myfun called")
  respond = "hello"
  return respond
end
cs:func("myfun") -- post coap://192.168.18.103:5683/v1/f/myfun will call myfun




--]]
        local _, _, method, path, vars = string.find (request, "([A-Z]+) (.+)?(.+) HTTP");
        if (method == nil) then
            _, _, method, path = string.find (request, "([A-Z]+) (.+) HTTP");
        end

        --[[
        print (request);
        print ("METHOD: " .. method);
        print ("PATH: " .. path);
        --]]

        mimetype = "text/plain";
        response = "ERROR";

        if ((path == "/") or (path == "/index.html")) then
            mimetype = "text/html";
            response = [[
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
    <link rel="icon" href="data:;base64,iVBORw0KGgo=">
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.1.1/jquery.min.js"></script>
    <script>
    function control (path) {
      $.get (path);
    }
    </script>
  </head>
  <body>
    <table>
      <tr>
        <td>GPIO3</td>
        <td><button onClick="control('/device/GPIO3/on');">ON</button></td>
        <td><button onClick="control('/device/GPIO3/off');">OFF</button></td>
      </tr>
      <tr>
        <td>GPIO4</td>
        <td><button onClick="control('/device/GPIO4/on');">ON</button></td>
        <td><button onClick="control('/device/GPIO4/off');">OFF</button></td>
      </tr>
      <!--
      <tr>
        <td>Kitchen Light</td>
        <td><a href="?pin=KITCHEN_ON"><button>ON</button></a></td>
        <td><a href="?pin=KITCHEN_OFF"><button>OFF</button></a></td>
      </tr>
      -->
      <tr>
        <td>Reboot</td>
        <td colspan=2><button onClick="control('/device/reboot');">Reboot</button></a></td>
      </tr>
    </table>
  </body>
</html>
]];
        elseif (path == "/device/GPIO3/on") then
              gpio.write(led1, gpio.LOW);
              mimetype = "text/plain";
              response = "OK";
        elseif (path == "/device/GPIO3/off") then
              gpio.write(led1, gpio.HIGH);
              mimetype = "text/plain";
              response = "OK";
        elseif (path == "/device/GPIO4/on") then
              gpio.write(led2, gpio.LOW);
              mimetype = "text/plain";
              response = "OK";
        elseif (path == "/device/GPIO4/off") then
              gpio.write(led2, gpio.HIGH);
              mimetype = "text/plain";
              response = "OK";
              --[[
        elseif (_GET.pin == "KITCHEN_ON") then
              print ("kitchen on?");
              ifttt = "https://maker.ifttt.com" ..  "/trigger/" .. "kitchen_sink_light_on" .. "/with/key/" .. "4LqV_vI_6EW-Yu1NFzohK";
              print (ifttt);
              mimetype = "text/plain";
              response = "OK";
        elseif (_GET.pin == "KITCHEN_OFF") then
              print ("kitchen off?");
              ifttt = "https://maker.ifttt.com" ..  "/trigger/" .. "kitchen_sink_light_off" .. "/with/key/" .. "4LqV_vI_6EW-Yu1NFzohK";
              print (ifttt);
              mimetype = "text/plain";
              response = "OK";
--              http.get (ifttt, nil, function (code, data)
--                if (code < 0) then
--                  print("HTTP request failed")
--                else
--                  print(code, data)
--                end
--              end)
              --]]
        elseif (path == "/device/reboot") then
              mimetype = "text/plain";
              response = "OK";
              node.restart ()
        end

        client:send ("HTTP/1.1 200 OK\r\n" ..
            "Content-Type: " .. mimetype .. "; charset=UTF-8\r\n\r\n" ..
            response);
        client:close ();

        collectgarbage ();
    end)
end)

--
-- broadcast the device's availability using mDNS
--
mdns.register ("huzzah", {hardware="ESP8266-Lua", description="example", service="http", port=80, location="home"})

--
-- print the current address when starting
--
print (wifi.sta.getip ());



-- use copper addon for firefox
cs=coap.Server()
cs:listen(5683)

myvar=1
cs:var("myvar") -- get coap://192.168.18.103:5683/v1/v/myvar will return the value of myvar: 1

all='[1,2,3]'
cs:var("all", coap.JSON) -- sets content type to json

-- function should tack one string, return one string.
function myfun(payload)
  print("myfun called")
  respond = "hello"
  return respond
end
cs:func("myfun") -- post coap://192.168.18.103:5683/v1/f/myfun will call myfun

-- scottmcolash
-- 958b1782aa8a0b180429d863ac49719fd0851491















end

