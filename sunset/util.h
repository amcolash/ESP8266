// Debug vars

//#define DEBUG

#ifdef DEBUG
  #define MAX_BRIGHTNESS 150
#else
  #define MAX_BRIGHTNESS 1023
#endif

#define ALARM_HOUR_ADDRESS 0
#define ALARM_MINUTE_ADDRESS 4

// Persisted data
typedef struct {
  uint8_t alarmHour;
  uint8_t alarmMinute;
} Persisted;

Persisted data = { 255, 255 };

// Internal State

bool on = true;
bool fastFade = false;
int brightness = 0;
int targetBrightness = 0;
Timezone tz;

String dataPath = "data.txt";
String logPath = "log.txt";

int fadeDuration = 30 * 60 * 1000;
int fadeDelay = 90 * 60 * 1000;

//int fadeDuration = 3 * 1000;
int fadeSegment = fadeDuration / MAX_BRIGHTNESS;
long timer;

//////////////////////////////////////////////////////////////////////

void toggle() {
  fastFade = true;

  if (targetBrightness > 0) targetBrightness = 0;
  else targetBrightness = MAX_BRIGHTNESS;
}

// From: https://gist.github.com/jrleeman/3b7c10712112e49d8607
int calculateDayOfYear(int day, int month, int year) {
  // Given a day, month, and year (4 digit), returns 
  // the day of year. Errors return 999.
  
  int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  
  // Verify we got a 4-digit year
  if (year < 1000) {
    return 999;
  }
  
  // Check if it is a leap year, this is confusing business
  // See: https://support.microsoft.com/en-us/kb/214019
  if (year%4  == 0) {
    if (year%100 != 0) {
      daysInMonth[1] = 29;
    }
    else {
      if (year%400 == 0) {
        daysInMonth[1] = 29;
      }
    }
   }

  // Make sure we are on a valid day of the month
  if (day < 1) 
  {
    return 999;
  } else if (day > daysInMonth[month-1]) {
    return 999;
  }
  
  int doy = 0;
  for (int i = 0; i < month - 1; i++) {
    doy += daysInMonth[i];
  }
  
  doy += day;
  return doy;
}

int calculateDayOfYear(time_t time) {
  tmElements_t tm;
  breakTime(time, tm);

  return calculateDayOfYear(tm.Day, tm.Month, tm.Year + 1970);
}

// Util functions

void log(String s) {
   File f = LittleFS.open(logPath, "a");
   f.println(dateTime(now(), RSS) + ":\t" + s);
   f.close();

   Serial.println(s);
}

void rotateLog() {
  LittleFS.remove(logPath);
  log("Rotated Log File");
}

void fadeOn() {
  log("Fading On");
  targetBrightness = MAX_BRIGHTNESS;
}

void fadeOff() {
  log("Fading Off");
  targetBrightness = 0;
}

void schedule(String name, void (*function)(), time_t time) {
  if (now() > time) {
    log("Skipping past event:\t" + name + " \t[" + dateTime(time, RSS) + "]");
    return;
  }
  
  setEvent(function, time);
  log("Event added:\t" + name + " \t[" + dateTime(time, RSS) + "]");
}

// Runs every day at midnight, also schedules a new event at midnight
void scheduleEvents() {
  log("Scheduling events, deleting old events");

  // Delete old events
  deleteEvent(fadeOn);
  deleteEvent(fadeOff);
  deleteEvent(scheduleEvents);
  
  time_t now = tz.now();
  tmElements_t tm;
  breakTime(now, tm);
  
  int day = calculateDayOfYear(now);

  if (day != 999) {
    day--;
    
    int sunriseTotal = SUNRISE_DATA[day];
    int sunsetTotal = SUNSET_DATA[day];

    int sunriseHour = sunriseTotal / 60;
    int sunriseMinute = ((sunriseTotal / 60.) - sunriseHour) * 60;

    if (data.alarmHour != 255 && data.alarmMinute != 255) {
      time_t sunrise = makeTime(sunriseHour, sunriseMinute, 0, tm.Day, tm.Month, tm.Year);
      time_t alarm = makeTime(data.alarmHour, data.alarmMinute, 0, tm.Day, tm.Month, tm.Year);
      
      log("Comparing sunrise \t[" + dateTime(sunrise, RSS) + "]");
      log("Comparing alarm \t[" + dateTime(alarm, RSS) + "]");

      int alarmTotal = data.alarmHour * 60 + data.alarmMinute;
      if (alarmTotal > sunriseTotal) {
        sunriseHour = data.alarmHour;
        sunriseMinute = data.alarmMinute;

        alarm = makeTime(sunriseHour, sunriseMinute, 0, tm.Day, tm.Month, tm.Year);
        log("New Sunrise Time \t[" + dateTime(alarm, RSS) + "]"); 
      }
    }

    int sunsetHour = sunsetTotal / 60;
    int sunsetMinute = ((sunsetTotal / 60.) - sunsetHour) * 60;

    time_t sunriseOn = makeTime(sunriseHour, sunriseMinute, 0, tm.Day, tm.Month, tm.Year);
    time_t sunriseDelay = makeTime(sunriseHour, sunriseMinute + fadeDuration / 60000, 0, tm.Day, tm.Month, tm.Year);
    time_t sunriseOff = makeTime(sunriseHour, sunriseMinute + fadeDuration / 60000 + fadeDelay / 60000, 0, tm.Day, tm.Month, tm.Year);
    
    time_t sunsetOn = makeTime(sunsetHour, sunsetMinute, 0, tm.Day, tm.Month, tm.Year);
    time_t sunsetDelay = makeTime(sunsetHour, sunsetMinute + fadeDuration / 60000, 0, tm.Day, tm.Month, tm.Year);
    time_t sunsetOff = makeTime(sunsetHour, sunsetMinute + fadeDuration / 60000 + fadeDelay / 60000, 0, tm.Day, tm.Month, tm.Year);

    // Handle the case where we are already in the middle of fade-in
    int tempBrightness = 0;
    if (now > sunriseOn && now < sunriseDelay) {
      tempBrightness = (now - sunriseOn) / (now - sunriseDelay) * MAX_BRIGHTNESS;
    } else if (now > sunsetOn && now < sunsetDelay) {
      tempBrightness = (now - sunsetOn) / (now - sunsetDelay) * MAX_BRIGHTNESS;
    } else if ((now > sunriseDelay && now < sunriseOff) || (now > sunsetDelay && now < sunsetOff)) {
      tempBrightness = MAX_BRIGHTNESS;
    }

    if (tempBrightness != 0) {
      log("Overriding brightness due to schedule: " + String(tempBrightness));
      brightness = tempBrightness;
      targetBrightness = MAX_BRIGHTNESS;
    }

    // Fade-out, fade down in a normal time
    if ((now > sunriseOff && now < sunriseOff + fadeDuration / 1000) || (now > sunsetOff && now < sunsetOff + fadeDuration / 1000)) {
      brightness = MAX_BRIGHTNESS;
      targetBrightness = 0;

      log("Overriding brightness due to schedule: " + String(brightness) + ", and now fading off");
    }

    // Schedule actual fading events    
    schedule("Sunrise On", fadeOn, sunriseOn);
    schedule("Sunrise Off", fadeOff, sunriseOff);

    schedule("Sunset On", fadeOn, sunsetOn);
    schedule("Sunset Off", fadeOff, sunsetOff);
  }
  
  time_t nextDay = makeTime(0, 0, 0, tm.Day + 1, tm.Month, tm.Year);
  schedule("ScheduleEvents", scheduleEvents, nextDay);
}
