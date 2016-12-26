--
-- show the network status
--
print (wifi.sta.getip ());
print (wifi.sta.gethostname ());

--
-- set GPIO pin 3 for output and turn off its LED
--
led1 = 3;
gpio.mode (led1, gpio.OUTPUT);
gpio.write (led1, gpio.HIGH);

--
-- set GPIO pin 4 for output and turn off its LED
--
led2 = 4
gpio.mode (led2, gpio.OUTPUT);
gpio.write (led2, gpio.HIGH);

--
-- connect to the MQTT broker host
--
clientid = "esp8266"
username = "scottmcolash"
password = "958b1782aa8a0b180429d863ac49719fd0851491"

gpio3_topic = username .. "/feeds/gpio3"
gpio4_topic = username .. "/feeds/gpio4"
example_topic = username .. "/feeds/example"

m = mqtt.Client (clientid, 120, username, password, 1)

m:on ("connect",
    function (client)
        print ("connected")
    end)

m:on ("offline",
    function (client)
        print ("offline")
    end)

-- on publish message receive event
m:on ("message",
    function (client, topic, data) 
        print ("message")
        if data ~= nil then
            print (topic .. ": " .. data) 

            value = gpio.HIGH
            if data == "ON" then
                value = gpio.LOW
            end

            pin = nil
            if topic == gpio3_topic then
                pin = led1
            elseif topic == gpio4_topic then
                pin = led2
            end

            if pin then
                gpio.write (pin, value)
            end
        end
    end)

initialized = false
m:connect ("io.adafruit.com", 1883, 0, 1,
    function (client)
        print ("connected")
        if not initialized then
            m:subscribe ({[gpio3_topic] = 1, [gpio4_topic] = 1},
                function (client)
                    print ("subscribe success")
                    m:publish (gpio3_topic, "OFF", 1, 1)
                    print ("publish GPIO3");
                    m:publish (gpio4_topic, "OFF", 1, 1)
                    print ("publish GPIO4");
                end)
            print ("initialized")
            initialized = true
        end
    end, 
    function (client, reason)
        print ("failed reason: " .. reason)
    end)

tmr.alarm (0, 10000, tmr.ALARM_AUTO,
    function ()
        if initialized then
            print ("publish analog");
            m:publish (example_topic, adc.read (0), 1, 1)
        end
    end)
