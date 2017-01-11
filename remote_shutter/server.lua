focus = 1
shutter = 2
gpio.mode(focus, gpio.OUTPUT)
gpio.mode(shutter, gpio.OUTPUT)

gpio.write(focus, gpio.HIGH)
gpio.write(shutter, gpio.HIGH)

srv=net.createServer(net.TCP)
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
    data = "<title>Remote Shutter</title>";
    data = data.."<center><h1>Remote Shutter</h1>";
    data = data.."<a href=\"?req=FOCUS\"><button>FOCUS</button></a><br>";
    data = data.."<a href=\"?req=SHUTTER\"><button>SHUTTER</button></a>";
    local _on,_off = "",""

    if(_GET.req == "FOCUS")then
      gpio.write(focus, gpio.LOW)
      delay = 2
      tmr.alarm(0, delay * 1000, tmr.ALARM_SINGLE, function() gpio.write(focus, gpio.HIGH) end)
    elseif(_GET.req == "SHUTTER")then
      gpio.write(shutter, gpio.LOW)
      delay = 0.5
      tmr.alarm(1, delay * 1000, tmr.ALARM_SINGLE, function() gpio.write(shutter, gpio.HIGH) end)
    end

    client:send(data);
    client:close();
    collectgarbage();
  end)
end)
