focus = 1  -- PIN 4
shutter = 2  -- PIN 5
gpio.mode(focus, gpio.OUTPUT)
gpio.mode(shutter, gpio.OUTPUT)

gpio.write(focus, gpio.HIGH)
gpio.write(shutter, gpio.HIGH)

content = ""

if file.open("index.html", "r") then
  content = file.read()
  file.close()
end

print(content)

srv=net.createServer(net.TCP)

srv:listen(9, function(conn)
  conn:on("recieve", function(client,request)
    print(client)
    print(request)

    client:send(content);
    client:close();
    collectgarbage();
  end)
end)

srv:listen(80,function(conn)
  conn:on("receive", function(client,request)
    local buf = "";
    local _, _, method, path, vars = string.find(request, "([A-Z]+) (.+)?(.+) HTTP");
    if(method == nil)then
      _, _, method, path = string.find(request, "([A-Z]+) (.+) HTTP");
    end
    local _GET = {}
    if (vars ~= nil)then
      for k, v in string.gmatch(vars, "(%w+)=(%w+)&*") do
        _GET[k] = v
      end
    end


    if(_GET.req == "FOCUS")then
      gpio.write(focus, gpio.LOW)
      delay = 3
      tmr.alarm(0, delay * 1000, tmr.ALARM_SINGLE, function() gpio.write(focus, gpio.HIGH) end)
    elseif(_GET.req == "SHUTTER")then
      gpio.write(shutter, gpio.LOW)
      delay = 2
      tmr.alarm(1, delay * 1000, tmr.ALARM_SINGLE, function() gpio.write(shutter, gpio.HIGH) end)
    end

    client:send(content);
    client:close();
    collectgarbage();
  end)
end)
