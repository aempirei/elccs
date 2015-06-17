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

  uint8_t pin;
  uint16_t mode;

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

  template <typename T, typename U> void term(const char *key, T x, U b) {
    Serial.print(" ");
    Serial.print(key);
    Serial.print("=");
    Serial.print(x, b);
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

  void status(const char *msg = NULL) {

    Serial.print(";");

    term("PIN"   , pin   , DEC);
    term("ACTIVE", active, DEC);
    term("S"     , s0    , DEC);
    term("DS"    , ds()  , DEC);
    term("DM"    , dm()  , DEC);

    if (msg != NULL) {
      Serial.print(" :");
      Serial.print(msg);
    }

    Serial.println("");
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

enum port_pin : uint8_t {
  port_fault_pin     = 2,
  port_emergency_pin = 4,
  port_ignition_pin  = 5,
  port_unlock_pin    = 8,
  port_lock_pin      = 12,
  port_battery_pin   = A0
};

port ports[] = {
  port(port_fault_pin    , INPUT_PULLUP             ),
  port(port_emergency_pin, INPUT_PULLUP             ),
  port(port_ignition_pin , INPUT_PULLUP             ),
  port(port_unlock_pin   , OUTPUT      , false, HIGH),
  port(port_lock_pin     , OUTPUT      , false, HIGH),
  port(port_battery_pin  , INPUT       , true       ),
};

port& port_fault     = ports[0];
port& port_emergency = ports[1];
port& port_ignition  = ports[2];
port& port_unlock    = ports[3];
port& port_lock      = ports[4];
port& port_battery   = ports[5];

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

void port_ignition_handler(struct port& p) {

  // deactivate emergency port on ignition rising edge
  // activate emergency port on ignition falling edge

  if (p.active and p.ds() != 0)
    p.status("ignition switch triggered");

}

void port_fault_handler(port& p) {

  // trigger fault on edge detect
  // deactivate emergency port on falling edge
  // activate emergency port on rising edge.

  if (p.active and p.ds() != 0)
    p.status("fault switch level change");

}

void port_emergency_handler(port& p) {

  // when emergency port is active if level is LOW  for 3000ms, trigger unlock port and deactivate emergency port
  // activate emergency port on rising edge when inactive

  if (p.active) {

    if (p.s0 == LOW and p.ds() == 0 and p.dm() >= 3000) {
      p.active = false;
      p.status("emergency unlock switch triggered");

      port_unlock.write(LOW);
      port_unlock.status();

      delay(100);

      port_unlock.write(HIGH);
      port_unlock.status();

      delay(100);

      port_unlock.write(LOW);
      port_unlock.status();

      delay(100);

      port_unlock.write(HIGH);
      port_unlock.status();
    }

  } else {

    if (p.ds() > 0) {
      p.active = true;
      p.status("emergency unlock switch reactivated");
    }

  }

}

void port_handler(port& p) {

  p.step();

  switch (p.pin) {
    case port_emergency_pin: port_emergency_handler(p); break;
    case port_ignition_pin : port_ignition_handler (p); break;
    case port_fault_pin    : port_fault_handler    (p); break;
  }


}

unsigned int idx = 0;

void loop() {

  port_handler(ports[idx]);
  buffer_handler();

  ++idx %= ports_n;
}



