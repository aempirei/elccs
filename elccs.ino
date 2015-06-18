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

	const char *name;

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

	port(const char *my_name, int my_pin, int my_mode, bool my_analog = false, int s = LOW) : name(my_name), pin(my_pin), mode(my_mode), analog(my_analog), active(true) {

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

		Serial.print(":");
		Serial.print(name);

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

			if(not analog)
				status();
		}


		return ds();
	}

	int read() {
		return push(get());
	}

	int write(int s) {
		 return push(set(s));
	}

	int toggle() {
		return write(not s0);
	}

	int step() {
		return mode == OUTPUT ? write(s0) : read();
	}

	void pattern(int *xs, const char *msg) {

		do {
			write(*xs++);
			delay(*xs);
		} while(*xs++ > 0);

		step();

		status(msg);
	}
};

enum port_pin {
	port_fault_pin     = 2,
	port_third_pin     = 3,
	port_emergency_pin = 4,
	port_ignition_pin  = 5,
	port_acc_pin       = 6,
	port_rpm_pin       = 7,

	port_unlock_pin    = 8,
	port_lock_pin      = 12,

	port_battery_pin   = A0,
	port_oxygen_pin    = A1,
	port_coolant_pin   = A2,
	port_oil_pin       = A3,
	port_map_pin       = A4,
	port_light_pin     = A5,
};

port ports[] = {
	port("FAULT"    , port_fault_pin    , INPUT_PULLUP             ),
	port("3RD"      , port_third_pin    , INPUT_PULLUP             ),
	port("EMERGENCY", port_emergency_pin, INPUT_PULLUP             ),
	port("IGNITION" , port_ignition_pin , INPUT_PULLUP             ),
	port("ACC"      , port_acc_pin      , INPUT_PULLUP             ),
	port("RPM"      , port_rpm_pin      , INPUT_PULLUP             ),

	port("UNLOCK"   , port_unlock_pin   , OUTPUT      , false, HIGH),
	port("LOCK"     , port_lock_pin     , OUTPUT      , false, HIGH),

	port("BATTERY"  , port_battery_pin  , INPUT       , true       ),
	port("OXYGEN"   , port_oxygen_pin   , INPUT       , true       ),
	port("COOLANT"  , port_coolant_pin  , INPUT       , true       ),
	port("OIL"      , port_oil_pin      , INPUT       , true       ),
	port("MAP"      , port_map_pin      , INPUT       , true       ),
	port("LIGHT"    , port_light_pin    , INPUT       , true       ),
};

port& port_fault     = ports[0];
port& port_third     = ports[1];
port& port_emergency = ports[2];
port& port_ignition  = ports[3];
port& port_acc       = ports[4];
port& port_rpm       = ports[5];

port& port_unlock    = ports[6];
port& port_lock      = ports[7];

port& port_battery   = ports[8];
port& port_oxygen    = ports[9];
port& port_coolant   = ports[10];
port& port_oil       = ports[11];
port& port_map       = ports[12];
port& port_light     = ports[13];

const unsigned int ports_n = sizeof(ports) / sizeof(*ports);

// SENSORS, RELAYS, SWITCHES
// =========================
// battery, oxygen, coolant,
// rpm, oil, map, acc, ign,
// doors, seat belts, choke,
// 2nd, 3rd, brake

linebuf buf;

void status() {
	for(unsigned int n = 0; n < ports_n; n++)
		ports[n].status();
}

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

	Serial.println("  lock     lock doors");
	Serial.println("  unlock   unlock doors");
	Serial.println("  status   report status of all devices");
	Serial.println("  ping     request ping response");
	Serial.println("  nop      no operation");
	Serial.println("  version  display system version information");
	Serial.println("  help     display this help screen");
	
	Serial.println("");
}

int pattern_servo[] = { LOW, 100, HIGH, 100, LOW, 100, HIGH, 0 };

void port_third_handler(struct port& p) {
	if (p.active and p.ds() != 0) {
	}
}

void port_rpm_handler(struct port& p) {
	if (p.active and p.ds() != 0) {
	}
}

void port_acc_handler(struct port& p) {
	if (p.active and p.ds() != 0) {
	}
}

void port_ignition_handler(struct port& p) {
	if (p.active and p.ds() != 0) {
		// deactivate emergency port on ignition rising edge
		// activate emergency port on ignition falling edge
	}
}

void port_fault_handler(port& p) {
	if (p.active and p.ds() != 0) {
		// trigger fault on edge detect
	}
}

void port_emergency_handler(port& p) {

	// when emergency port is active if level is LOW for 3000ms, trigger unlock port and deactivate emergency port
	// activate emergency port on rising edge when inactive

	if (p.active) {

		if (p.s0 == LOW and p.ds() == 0 and p.dm() >= 3000) {
			p.active = false;
			p.status("emergency triggered");
			port_unlock.pattern(pattern_servo, "unlock issued");
		}

	} else {

		if (p.ds() > 0) {
			p.active = true;
			p.status("emergency trigger reactivated");
		}

	}

}

void port_handler(port& p) {

	p.step();

	switch (p.pin) {
		case port_emergency_pin: port_emergency_handler(p); break;
		case port_ignition_pin : port_ignition_handler (p); break;
		case port_acc_pin      : port_acc_handler      (p); break;
		case port_rpm_pin      : port_rpm_handler      (p); break;
		case port_fault_pin    : port_fault_handler    (p); break;
		case port_third_pin    : port_third_handler    (p); break;


	}


}

void buffer_handler() {

	buf.update();

	if (buf.is_ready()) {

		if (buf.match("nop")) {

			// NOP

		} else if (buf.match("ping")) {

			Serial.println("return::pong");

		} else if (buf.match("help")) {

			help();

		} else if (buf.match("version")) {

			version();

		} else if (buf.match("lock")) {

			port_lock.pattern(pattern_servo, "lock issued");

		} else if (buf.match("unlock")) {

			port_unlock.pattern(pattern_servo, "unlock issued");

		} else if (buf.match("status")) {

			status();

		} else {

			bool done = false;

			//
			// do things...
			//

			if (not done) {
				Serial.print("error::");
				Serial.print(buf.line);
				Serial.println(" command unknown");
			}
		}

		buf.clear();
	}
}

unsigned int idx = 0;

void setup() {

	Serial.begin(115200);

	delay(500);

	version();
}

void loop() {

	port_handler(ports[idx]);
	buffer_handler();

	++idx %= ports_n;
}
