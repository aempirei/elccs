/*************************************
 * - El Camino Control System v1.0 - *
 * - Copyright(c) 2015, 256 LLC    - *
 * - Written by Christopher Abad   - *
 *************************************/

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

struct linebuf;

struct command;
struct port;

struct adc_port;
struct pulldown_port;
struct pullup_port;
struct counting_port;

struct pwm_port;
struct digital_output_port;

void attach_frequency_counter(const port *);
void detach_frequency_counter(const port *);

template <typename T> int compare(const void *a, const void *b) {
		return strcasecmp(((const T *)a)->name, ((const T *)b)->name);
}

template <typename T> int search(const void *key, const void *member) {
		return strcasecmp((const char *)key, ((const T *)member)->name);

}

struct command {

	using function_type = void ();

	const char *name;
	const char *help;
	function_type *f;

	command(const char *my_name, const char *my_help, function_type *my_f) : name(my_name), help(my_help), f(my_f) { }
};

void cmd_left_up();
void cmd_left_down();
void cmd_right_up();
void cmd_right_down();
void cmd_all_up();
void cmd_all_down();
void cmd_lock();
void cmd_unlock();
void cmd_status();
void cmd_ping();
void cmd_uptime();
void cmd_nop();
void cmd_version();
void cmd_help();

void pattern(const int *, port **);

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

		void status(const char *msg = nullptr) {

				Serial.print(":");
				Serial.print(name);

				term("PIN"   , pin   , DEC);
				term("S"     , s0    , DEC);
				term("DS"    , ds()  , DEC);
				term("DM"    , dm()  , DEC);

				if (msg != nullptr) {
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

		int update(int s) {

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
				return update(get());
		}

		int write(int s) {
				return update(set(s));
		}

		int toggle() {
				return write(not s0);
		}

		int step() {
				return mode == OUTPUT ? write(s0) : read();
		}
};

struct pwm_port : port {
		pwm_port(const char *my_name, int my_pin, int s) : port(my_name, my_pin, OUTPUT, true, s) { }
};

struct digital_output_port : port {
		digital_output_port(const char *my_name, int my_pin, int s) : port(my_name, my_pin, OUTPUT, false, s) { }
};

struct adc_port : port {
		adc_port(const char *my_name, int my_pin) : port(my_name, my_pin, INPUT, true, 0) { }
};

struct pulldown_port : port {
		pulldown_port(const char *my_name, int my_pin) : port(my_name, my_pin, INPUT, false, LOW) { }
};

struct pullup_port : port {
		pullup_port(const char *my_name, int my_pin) : port(my_name, my_pin, INPUT_PULLUP, false, HIGH) { }
};

struct counting_port : port {
		counting_port(const char *my_name, int my_pin) : port(my_name, my_pin, INPUT, false, LOW) {
			attach_frequency_counter(this);
		}
		~counting_port() {
			detach_frequency_counter(this);
		}
};


void pattern(const int *xs, port **ys) {
		do {
			for(port **zs = ys; *zs != nullptr; zs++)
				(*zs)->write(*xs);
			delay(*++xs);
		} while(*xs++ > 0);
}


port ports[] = {

	pullup_port("EMERGENCY", 3),

	pulldown_port("IGN", 5),
	pulldown_port("ACC", 9),

	counting_port("RPM", 10),

	adc_port("BATTERY", A0),
	adc_port("OXYGEN" , A1),
	adc_port("COOLANT", A2),
	adc_port("OIL"    , A3),
	adc_port("MAP"    , A4),
	adc_port("LIGHT"  , A5),

	digital_output_port("L-UP"  ,  2, HIGH),
	digital_output_port("L-DOWN",  4, HIGH),
	digital_output_port("R-UP"  ,  6, HIGH),
	digital_output_port("R-DOWN",  7, HIGH),
	digital_output_port("LOCK"  ,  8, HIGH),
	digital_output_port("UNLOCK", 12, HIGH),
};

constexpr size_t ports_n =  sizeof(ports) / sizeof(port);

command commands[] = {
		command("l-up"   , "raise left window"           , cmd_left_up   ),
		command("l-down" , "lower left window"           , cmd_left_down ),
		command("r-up"   , "raise right window"          , cmd_right_up  ),
		command("r-down" , "lower right window"          , cmd_right_down),
		command("up"     , "raise all windows"           , cmd_all_up    ),
		command("down"   , "lower all windows"           , cmd_all_down  ),
		command("lock"   , "lock all doors"              , cmd_lock      ),
		command("unlock" , "unlock all doors"            , cmd_unlock    ),
		command("status" , "report status of all devices", cmd_status    ),
		command("ping"   , "request ping"                , cmd_ping      ),
		command("uptime" , "display system uptime"       , cmd_uptime    ),
		command("nop"    , "no operation"                , cmd_nop       ),
		command("version", "display firmware information", cmd_version   ),
		command("help"   , "display this help screen"    , cmd_help      ),
};

constexpr size_t commands_n = sizeof(commands) / sizeof(command);

void cmd_uptime() {
	Serial.print("RETURN::UPTIME=");
	Serial.print(millis(), DEC);
	Serial.write('\n');
}

void cmd_nop() {
	Serial.println("RETURN::NOTHING");
}

void cmd_status() {
	for(size_t n = 0; n < ports_n; n++)
		ports[n].status();
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

		for(size_t n = 0; n < commands_n; n++) {
				Serial.print("  ");
				Serial.print(commands[n].name);
				for(int k = 10 - strlen(commands[n].name); k >= 0; k--)
						Serial.write(' ');
				Serial.println(commands[n].help);
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

void process_ports() {
		for(size_t n = 0; n < ports_n; n++)
				ports[n].step();
}

linebuf buf;

void process_buffer() {

	buf.update();

	if (buf.is_ready()) {

		command *cmd = (command *)bsearch(buf.line, commands, commands_n, sizeof(command), search<command>);

		if(cmd == nullptr) {
				Serial.print("RETURN::ERROR=unknown-command,COMMAND=");
				Serial.println(buf.line);
		} else {
			cmd->f();
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
*/

void attach_frequency_counter(const port *) {
}

void detach_frequency_counter(const port *) {
}

void setup() {

		qsort(ports, ports_n, sizeof(port), compare<port>);
		qsort(commands, commands_n, sizeof(command), compare<command>);

		Serial.begin(115200);

		delay(1000);

		cmd_version();
		cmd_status();
		cmd_help();

		// attachInterrupt(0, intvec, FALLING);
}

void loop() {
	process_ports();
	process_buffer();
}
