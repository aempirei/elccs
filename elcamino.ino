/*************************************
 * - El Camino Control System v1.0 - *
 * - Copyright(c) 2015, 256 LLC    - *
 * - Written by Christopher Abad   - *
 *************************************/

#include <stdint.h>
#include <ctype.h>
#include <string.h>

typedef typeof(LOW) pinstate_t;

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

/*
 * DPST relays
 */

struct relay {

  uint8_t pin;
  uint16_t ms;

  const char *command;

  relay(uint8_t my_pin, uint16_t my_ms, const char *my_command) :
  pin(my_pin), ms(my_ms), command(my_command) {

    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }

  void report(pinstate_t state) const {
    Serial.print(":digital ");
    Serial.print(state == HIGH ? "HI" : "LO");
    Serial.print(" gpio ");
    Serial.print(pin, DEC);
    Serial.println("");
  }

#define SETPIN(_PIN_, _LEVEL_, _MS_) do { digitalWrite(_PIN_, _LEVEL_); report(_LEVEL_); delay(_MS_); } while(false)

  void go() const {

    if (ms == 0) {

      pinstate_t state = digitalRead(pin);
      report(state);
      digitalWrite(pin, not state);

    }
    else {

      SETPIN(pin,LOW ,ms);
      SETPIN(pin,HIGH,ms);
      SETPIN(pin,LOW ,ms);
      SETPIN(pin,HIGH,ms);
    }
  }
};

/*
 * analog/digital sensors
 */

struct sensor {

  enum mode_t {
    ANALOG,
    DIGITAL
  };

  uint8_t pin;
  mode_t mode;
  const char *command;
  int16_t state;
  bool pullup;

  sensor(uint8_t my_pin, mode_t my_mode, const char *my_command, bool my_pullup = false) :
  pin(my_pin), mode(my_mode), command(my_command), pullup(my_pullup)
  {
    if(mode == DIGITAL)
      pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
  }

  void update() {
    state = (mode == ANALOG ? analogRead : digitalRead)(pin);
  }

  void go() {

    update();

    Serial.print(":");
Serial.print(mode == DIGITAL ? pullup ? "inverted" : "digital" :
    "analog");
    Serial.print(" ");

    if (mode == ANALOG)
      Serial.print((float)state / 1023.0, 2);
    else
      Serial.print(state == HIGH ? "HI" : "LO");

    Serial.print(" ");
    Serial.print(command);

    Serial.println("");
  }
};

/*
 * global variables
 */

struct relay relays[] = {
  relay(12,  50, "doors lock"  ),
  relay( 8,  50, "doors unlock"),
};

relay& emergency_unlock_relay = relays[1];

const int fault_pin = 2;
const int backdoor_pin = 4;

const uint8_t relays_sz = sizeof(relays) / sizeof(*relays);

struct sensor sensors[] = {

  sensor(A0, sensor::ANALOG, "sensor battery"),
  sensor(A1, sensor::ANALOG, "sensor oxygen" ),
  sensor(A2, sensor::ANALOG, "sensor coolant"),
  sensor(A3, sensor::ANALOG, "sensor rpm"    ),
  sensor(A4, sensor::ANALOG, "sensor oil"    ),
  sensor(A5, sensor::ANALOG, "sensor map"    ),

  sensor(fault_pin   , sensor::DIGITAL, "sensor fault"   , true),
  sensor(backdoor_pin, sensor::DIGITAL, "sensor backdoor", true),

  sensor( 5, sensor::DIGITAL, "sensor acc"     ),
  sensor( 6, sensor::DIGITAL, "sensor ignition"),
  sensor( 9, sensor::DIGITAL, "sensor door"    ),
  sensor(10, sensor::DIGITAL, "sensor choke"   ),
  sensor(11, sensor::DIGITAL, "sensor 3rd"     ),
};

const uint8_t sensors_sz = sizeof(sensors) / sizeof(*sensors);

linebuf buf;

bool get_emergency_unlock_state() {
  return digitalRead(backdoor_pin) or not digitalRead(fault_pin);
}

bool emergency_unlock_state_prev;

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

  Serial.begin(57600);

  delay(250);

  emergency_unlock_state_prev = get_emergency_unlock_state();

  version();
}

void fault_lockout() {
  if(digitalRead(fault_pin) == LOW) {
    sensors[fault_pin].go();
    Serial.println("lockout 500ms");
    delay(500);
  }
}

#define LEVELNAME(X) (((X) == LOW) ? "LO" : "HI")
#define EDGENAME(X) (((X) == LOW) ? "RISE" : "FALL")

void edge_detect() {

  if(get_emergency_unlock_state() != emergency_unlock_state_prev) {

    Serial.print(":edge ");
    Serial.print(emergency_unlock_state_prev == LOW ? "+" : "-");
    Serial.print(" logic emergency");
    Serial.println("");

    if(emergency_unlock_state_prev == LOW)
      emergency_unlock_relay.go();

    emergency_unlock_state_prev = not emergency_unlock_state_prev;

  }
}

void buffer_handler() {

  buf.update();

  if (buf.is_ready()) {

    if (buf.match("help")) {

      help();

    }
    else if (buf.match("version")) {

      version();

    }
    else if (buf.match("sensors")) {

      for (uint8_t n = 0; n < sensors_sz; n++)
        sensors[n].go();

    }
    else {

      bool done = false;

      for (uint8_t n = 0; not done and n < relays_sz; n++) {
        if (buf.match(relays[n].command)) {
          relays[n].go();
          done = true;
        }
      }

      for (uint8_t n = 0; not done and n < sensors_sz; n++) {
        if (buf.match(sensors[n].command)) {
          sensors[n].go();
          done = true;
        }
      }

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

void loop() {
  fault_lockout();
  edge_detect();
  buffer_handler();
}


