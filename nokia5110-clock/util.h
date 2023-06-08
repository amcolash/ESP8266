#include "sundata.h"

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

#define MIN_BRIGHTNESS 10
#define MAX_BRIGHTNESS 500
#define FADE_TIME 60

uint16_t getBrightness(time_t time) {
  int dayOfYear = calculateDayOfYear(time);
  dayOfYear = constrain(dayOfYear, 0, 365);

  uint16_t sunriseMinute = pgm_read_word_near(SUNRISE_DATA + dayOfYear);
  uint16_t sunsetMinute = pgm_read_word_near(SUNSET_DATA + dayOfYear);

  uint16_t minutes = hour(time) * 60 + minute(time);
  bool isDay = minutes > sunriseMinute && minutes <= sunsetMinute;

  uint16_t brightness = MAX_BRIGHTNESS;
  if (isDay) {
    if (minutes - sunriseMinute <= FADE_TIME) brightness = map(minutes - sunriseMinute, 0, FADE_TIME, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    else brightness = MAX_BRIGHTNESS;
  } else {
    if (minutes < sunriseMinute) brightness = MIN_BRIGHTNESS;
    else if (minutes - sunsetMinute <= FADE_TIME) brightness = map(minutes - sunsetMinute, 0, FADE_TIME, MAX_BRIGHTNESS, MIN_BRIGHTNESS);
    else brightness = MIN_BRIGHTNESS;
  }

  return brightness;
}