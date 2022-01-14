// Debug vars

//#define DEBUG

#ifdef DEBUG
  #define MAX_BRIGHTNESS 150
#else
  #define MAX_BRIGHTNESS 1023
#endif

// Internal State

bool on = true;
int brightness = 0;
int targetBrightness = 0;
Timezone tz;


int fadeDuration = 30 * 60 * 1000;
//int fadeDuration = 3 * 1000;
int fadeSegment = fadeDuration / MAX_BRIGHTNESS;
long timer;

//////////////////////////////////////////////////////////////////////

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

void fadeOn() {
  Serial.println("Fading On");
  targetBrightness = MAX_BRIGHTNESS;
}

void fadeOff() {
  Serial.println("Fading Off");
  targetBrightness = 0;
}

void schedule(String name, void (*function)(), time_t time) {
  if (now() > time) {
    Serial.println("Skipping past event: " + name + " \t[" + dateTime(time, RSS) + "]");
    return;
  }
  
  setEvent(function, time);
  Serial.println("Event added: " + name + " \t[" + dateTime(time, RSS) + "]");
}

// Runs every day at midnight, also schedules a new event at midnight
void scheduleEvents() {
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

    int sunsetHour = sunsetTotal / 60;
    int sunsetMinute = ((sunsetTotal / 60.) - sunsetHour) * 60;

    time_t sunriseOn = makeTime(sunriseHour, sunriseMinute, 0, tm.Day, tm.Month, tm.Year);
    time_t sunriseOff = makeTime(sunriseHour, sunriseMinute + fadeDuration / 60000, 0, tm.Day, tm.Month, tm.Year);
    
    time_t sunsetOn = makeTime(sunsetHour, sunsetMinute, 0, tm.Day, tm.Month, tm.Year);
    time_t sunsetOff = makeTime(sunsetHour, sunsetMinute + fadeDuration / 60000, 0, tm.Day, tm.Month, tm.Year);
    
    schedule("Sunrise On", fadeOn, sunriseOn);
    schedule("Sunrise Off", fadeOff, sunriseOff);

    schedule("Sunset On", fadeOn, sunsetOn);
    schedule("Sunrset Off", fadeOff, sunsetOff);
  }
  
  time_t nextDay = makeTime(0, 0, 0, tm.Day + 1, tm.Month, tm.Year);
  schedule("ScheduleEvents", scheduleEvents, nextDay);
}
