/*************************************
 * - El Camino Control System v1.0 - *
 * - Copyright(c) 2015, 256 LLC    - *
 * - Written by Christopher Abad   - *
 *************************************/

#include <stdint.h>
#include <ctype.h>
#include <string.h>

struct linebuf;

struct port;

struct adc_port;
struct pulldown_port;
struct pullup_port;

struct pwm_port;
struct digital_output_port;

struct command;

port *ports;
command *commands;

struct command {
	typedef void command_fn();
	const char *name;
	const char *help;
	command_fn *f;
	command(const char *my_name, const char *my_help, command_fn *my_f) : name(my_name), help(my_help), f(my_f) {
	}
};

/*
 * linebuf - line buffer for serial device
 */

struct linebuf {

		static const uint8_t line_max_sz = 128;
		static const char EOL = '\n';
		static const char NUL = '\0';

		char line[line_max_sz + 1];
		uint8_t line_sz;
		bool locked;

		linebuf() : line_sz(0), locked(false) {
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

						} else if (full()) {

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

		int ds() {
				return s0 - s1;
		}

		template <typename T, typename U> void term(const char *key, T x, U b) {
				Serial.print(" ");
				Serial.print(key);
				Serial.print("=");
				Serial.print(x, b);
		}

		port(const char *my_name, int my_pin, int my_mode, bool my_analog, int s)
				: name(my_name), pin(my_pin), mode(my_mode), analog(my_analog)
		{

				pinMode(pin, mode);

				m1 = millis();

				if (mode == OUTPUT) {
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

struct pwm_port : port {
		using port::port;
		pwm_port(const char *my_name, int my_pin, int s) : pwm_port(my_name, my_pin, OUTPUT, true, s) { }
};

struct digital_output_port : port {
		using port::port;
		digital_output_port(const char *my_name, int my_pin, int s) : digital_output_port(my_name, my_pin, OUTPUT, false, s) { }
};

struct adc_port : port {
		using port::port;
		adc_port(const char *my_name, int my_pin) : adc_port(my_name, my_pin, INPUT, true, 0) { }
};

struct pulldown_port : port {
		using port::port;
		pulldown_port(const char *my_name, int my_pin) : pulldown_port(my_name, my_pin, INPUT, false, LOW) { }
};

struct pullup_port : port {
		using port::port;
		pullup_port(const char *my_name, int my_pin) : pullup_port(my_name, my_pin, INPUT_PULLUP, false, HIGH) { }
};

enum pin {

		pin_emergency = 3,
		pin_ignition  = 5,
		pin_acc       = 9,
		pin_rpm       = 10,

		pin_unlock    = 8,
		pin_lock      = 12,

		pin_leftup    = 2,
		pin_leftdown  = 4,
		pin_rightup   = 6,
		pin_rightdown = 7,

		pin_battery   = A0,
		pin_oxygen    = A1,
		pin_coolant   = A2,
		pin_oil       = A3,
		pin_map       = A4,
		pin_light     = A5
};

linebuf buf;

void cmd_uptime() {
	Serial.print("RETURN::UPTIME=");
	Serial.print(millis(), DEC);
	Serial.write('\n');
}

void cmd_nop() {
}

void cmd_status() {
	for(port *p = ports; p != NULL; p++)
		p->status();
}

void cmd_version() {
		Serial.write('\n');
		Serial.println("*************************************");
		Serial.println("* - El Camino Control System v1.0 - *");
		Serial.println("* - Copyright(c) 2015, 256 LLC    - *");
		Serial.println("* - Written by Christopher Abad   - *");
		Serial.println("*************************************");
		Serial.write('\n');
}

void cmd_help() {

		Serial.write('\n');
		Serial.println("********************");
		Serial.println("* - Command Help - *");
		Serial.println("********************");
		Serial.write('\n');

		for(command *cmd = commands; cmd != NULL; cmd++) {
				Serial.print("  ");
				Serial.print(cmd->name);
				for(int n = 20 - strlen(cmd->name); n >= 0; n--)
						Serial.write(' ');
				Serial.println(cmd->help);
		}

		Serial.write('\n');
}

void cmd_left_up() {
}

void cmd_left_down() {
}

void cmd_right_up() {
}

void cmd_right_down() {
}

void cmd_all_up() {
}

void cmd_all_down() {
}

void cmd_lock() {
}

void cmd_unlock() {
}

void cmd_ping() {
		Serial.println("RETURN::PONG");
}

void ports_handler() {
	for(port *p = ports; p != NULL; p++)
		p->step();
}

void buffer_handler() {

	buf.update();

	if (buf.is_ready()) {

		command *cmd = commands;

		while(cmd != NULL && strcasecmp(cmd->name, buf.line) != 0)
			cmd++;

		if(cmd != NULL) {
			cmd->f();
		} else {
				Serial.print("RETURN::ERROR=unknown-command,COMMAND=");
				Serial.println(buf.line);
		}

		buf.clear();
	}
}

/*

	/////////////////////////
	// interrupt handler code
	/////////////////////////

	volatile int qstate = LOW;

	void intvec() {
		qstate = not qstate;
	}

*/

/*

		} else if (buf.match("lock")) {

			port_lock.pattern(pattern_servo, "lock issued");

		} else if (buf.match("unlock")) {

			port_unlock.pattern(pattern_servo, "unlock issued");

		} else if (buf.match("left window up")) {

			port_lwindowp.pattern(pattern_window, "left window up issued");

		} else if (buf.match("left window down")) {

			port_lwindowm.pattern(pattern_window, "left window down issued");

		} else if (buf.match("right window up")) {

			port_rwindowp.pattern(pattern_window, "right window up issued");

		} else if (buf.match("right window down")) {

			port_rwindowm.pattern(pattern_window, "right window down issued");

 */



/*
int pattern_servo[] = {
	LOW, 75, HIGH, 75,
	LOW, 75, HIGH, 75,
	LOW, 75, HIGH, 75,
	LOW, 75, HIGH, 0
};

int pattern_window[] = {
	LOW, 4000, HIGH, 0
};

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


port ports[] = {

	port("EMERGENCY", port_emergency_pin, INPUT_PULLUP             ),
	port("IGNITION" , port_ignition_pin , INPUT_PULLUP             ),
	port("ACC"      , port_acc_pin      , INPUT_PULLUP             ),
	port("RPM"      , port_rpm_pin      , INPUT_PULLUP             ),

	port("UNLOCK"   , port_unlock_pin   , OUTPUT      , false, HIGH),
	port("LOCK"     , port_lock_pin     , OUTPUT      , false, HIGH),

	port("BATTERY"  , port_battery_pin  , INPUT       , true       ),
	port("COOLANT"  , port_coolant_pin  , INPUT       , true       ),
	port("OIL"      , port_oil_pin      , INPUT       , true       ),
	port("MAP"      , port_map_pin      , INPUT       , true       ),
	port("LIGHT"    , port_light_pin    , INPUT       , true       ),

	port("LWINDOWP" , port_lwindowp_pin , OUTPUT      , false, HIGH),
	port("LWINDOWM" , port_lwindowm_pin , OUTPUT      , false, HIGH),
	port("RWINDOWP" , port_rwindowp_pin , OUTPUT      , false, HIGH),
	port("RWINDOWM" , port_rwindowm_pin , OUTPUT      , false, HIGH),
};
*/

command *commands = new command[14]{
		new command("left-up"   , "raise left window"           , cmd_left_up),
		new command("left-down" , "lower left window"           , cmd_left_down),
		new command("right-up"  , "raise right window"          , cmd_right_up),
		new command("right-down", "lower right window"          , cmd_right_down),
		new command("all-up"    , "raise all windows"           , cmd_all_up),
		new command("all-down"  , "lower all windows"           , cmd_all_down),
		new command("lock"      , "lock all doors"              , cmd_lock),
		new command("unlock"    , "unlock all doors"            , cmd_unlock),
		new command("status"    , "report status of all devices", cmd_status),
		new command("ping"      , "request ping"                , cmd_ping),
		new command("uptime"    , "display system uptime"       , cmd_uptime),
		new command("nop"       , "no operation"                , cmd_nop),
		new command("version"   , "display firmware information", cmd_version),
		new command("help"      , "display this help screen"    , cmd_help),
		NULL
};


void init() {

}

void setup() {

	Serial.begin(38400);

	delay(1000);

	init();

	cmd_version();

	// attachInterrupt(0, intvec, FALLING);
}

void loop() {
	ports_handler();
	buffer_handler();
}
