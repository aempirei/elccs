#include <WProgram.h>
/*************************************
 * - El Camino Control System v1.0 - *
 * - Copyright(c) 2015, 256 LLC    - *
 * - Written by Christopher Abad   - *
 *************************************/

#include <stdint.h>
#include <ctype.h>
#include <string.h>

/*
 * linebuf - line buffer for serial device
 */

struct linebuf {

  static const uint8_t line_max_sz = 80;
  static const char EOL = '\n';
  static const char NUL = '\0';

  char line[line_max_sz + 1];
  uint8_t line_sz;
  bool locked;

  linebuf() :
    line_sz(0), locked(false) {
  }

  bool empty() const {
    return line_sz == 0;
  }

  int lastch() const {
    return empty() ? -1 : line[line_sz - 1];
  }

  bool full() const {
    return line_sz == line_max_sz;
  }

  bool is_ready() const {
    return locked;
  }

  bool match(const char *s) const {
    return is_ready() ? (strcasecmp(line, s) == 0) : false;
  }

  void update() {

    while (not is_ready() and Serial.available() > 0) {

      line[line_sz++] = Serial.read();

      if (lastch() == EOL) {

        line[--line_sz] = NUL;
        locked = true;

      }
      else if (full()) {

        line[line_sz] = NUL;
        locked = true;
      }
    }
  }

  void clear() {
    line_sz = 0;
    locked = false;
  }
};

struct port {

  int pin;
  int mode;

  bool analog;

  int s0;
  int s1;

  unsigned long m0;
  unsigned long m1;
  long dm0;

  bool active;

  int ds() {
    return s0 - s1;
  }

  port(int my_pin, int my_mode, bool my_analog = false, int s = LOW) : pin(my_pin), mode(my_mode), analog(my_analog), active(true) {

    pinMode(pin, mode);

    m1 = millis();

    if (mode == OUTPUT) {
      s0 = s;
      write(s);
    } else {
      s0 = get();
      read();
    }
  }

  void status() {
    Serial.print("; PIN ");
    Serial.print(pin, DEC);
    Serial.print(active ? " ON" : " --");
    Serial.print(s0 == LOW ? " LO" : " HI");
    Serial.print(ds() < 0 ? " FALL " : ds() > 0 ? " RISE " : " ---- ");
    Serial.print(dm(), DEC);
    Serial.println(" MS");
  }

  int get() {
    return (analog ? analogRead : digitalRead)(pin);
  }

  int set(int s) {
    if (analog)
      analogWrite(pin, s);
    else
      digitalWrite(pin, s);
    return s;
  }

  int dm() {
    return ds() == 0 ? m0 - m1 : dm0;
  }

  int push(int s) {

    s1 = s0;
    s0 = s;

    m0 = millis();

    if (ds() != 0) {
      dm0 = m0 - m1;
      m1 = m0;
    }

    return ds();
  }

  int read() {
    return push(get());
  }

  int write(int s) {
    return push(set(s));
  }

  int step() {
    return mode == OUTPUT ? write(s0) : read();
  }
};

enum port_name {
  port_fault     = 2,
  port_emergency = 4,
  port_ignition  = 5,
  port_unlock    = 8,
  port_lock      = 12,
  port_battery   = A0,
};

struct port ports[] = {
  port(port_fault    , INPUT_PULLUP             ),
  port(port_emergency, INPUT_PULLUP             ),
  port(port_ignition , INPUT                    ),
  port(port_unlock   , OUTPUT      , false, HIGH),
  port(port_lock     , OUTPUT      , false, HIGH),
  port(port_battery  , INPUT       , true       ),
};

const unsigned int ports_n = sizeof(ports) / sizeof(*ports);

// SENSORS, RELAYS, SWITCHES
// =========================
// battery, oxygen, coolant,
// rpm, oil, map, acc, ign,
// doors, seat belts, choke,
// 2nd, 3rd, brake

linebuf buf;

void version() {
  Serial.println("");
  Serial.println("*************************************");
  Serial.println("* - El Camino Control System v1.0 - *");
  Serial.println("* - Copyright(c) 2015, 256 LLC    - *");
  Serial.println("* - Written by Christopher Abad   - *");
  Serial.println("*************************************");
  Serial.println("");
}

void help() {

  Serial.println("");
  Serial.println("********************");
  Serial.println("* - Command Help - *");
  Serial.println("********************");
  Serial.println("");
  Serial.println("  help");
  Serial.println("  version");
  Serial.println("  doors {lock|unlock}");
  Serial.println("  sensor {oxygen|coolant|rpm|oil|map}");
  Serial.println("  sensor {fault|backdoor|acc|ignition|door|choke|3rd}");
  Serial.println("  sensors");
  Serial.println("");
}

void setup() {

  Serial.begin(115200);

  delay(500);

  version();
}

void buffer_handler() {

  buf.update();

  if (buf.is_ready()) {

    if (buf.match("help")) {

      help();

    } else if (buf.match("version")) {

      version();

    } else {

      bool done = false;

      if (not done) {
        Serial.print("ERROR: \"");
        Serial.print(buf.line);
        Serial.print("\" unknown command");
        Serial.println("");
      }
    }

    buf.clear();
  }
}

void port_handler(struct port& p) {

  p.step();

  if (p.pin == port_emergency) {

    if (p.active) {

      if (p.s0 == LOW and p.ds() == 0 and p.dm() >= 3000) {
        p.active = false;
        p.status();
      }

    } else {

      if (p.ds() > 0) {
        p.active = true;
        p.status();
      }

    }
  }
  

}

unsigned int idx = 0;

void loop() {

  port_handler(ports[idx]);
  buffer_handler();
  
  ++idx %= ports_n;
}


