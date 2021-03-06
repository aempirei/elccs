/*************************************
 * - El Camino Control System v1.0 - *
 * - Copyright(c) 2015, 256 LLC    - *
 * - Written by Christopher Abad   - *
 *************************************/

// #include <Wire.h>
// #include <LiquidCrystal_I2C.h>

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

struct linebuf;
struct config;
struct command;
struct port;

struct adc_port;
struct pulldown_port;
struct pullup_port;
template <int> struct counting_port;

struct pwm_port;
struct digital_output_port;

void cmd_left_up();
void cmd_left_down();
void cmd_right_up();
void cmd_right_down();
void cmd_all_up();
void cmd_all_down();
void cmd_lock();
void cmd_unlock();
void cmd_status();
void cmd_uptime();
void cmd_nop();
void cmd_version();
void cmd_help();
void cmd_cmds();
void cmd_echo();

template <typename> int compare(const void *, const void *);
template <typename> int is_named(const void *, const void *);
template <typename T> T *lookup(const char *, T *, size_t);

struct command {

		using function_type = void ();

		const char *name;
		const char *help;
		function_type *f;

		command(const char *my_name, const char *my_help, function_type *my_f) : name(my_name), help(my_help), f(my_f) { }
};

struct config {
		bool echo;
		config() : echo(false) { }
};

config cfg;

void pattern(const int *, port **);

struct linebuf {

		static constexpr uint8_t line_max_sz = 32;
		static constexpr char LF = '\n';
		static constexpr char CR = '\r';
		static constexpr char BS = '\b';
		static constexpr char DEL = '\x7f';
		static constexpr char NUL = '\0';

		char line[line_max_sz + 1];
		uint8_t line_sz;
		bool ready;
		bool ignore;

		linebuf() : line_sz(0), ready(false), ignore(false) {
		}

		bool is_empty() const {
				return line_sz == 0;
		}

		bool is_full() const {
				return line_sz == line_max_sz;
		}

		bool is_ready() const {
				return ready;
		}

		bool is_ignored() const {
				return ignore;
		}

		void clear() {
				line_sz = 0;
				ready = false;
				ignore = false;
		}

		void update() {

				while (not is_ready() and Serial.available() > 0) {

						char ch = Serial.read();

						if(ch == CR or ch == LF) {
								line[line_sz] = NUL;
								ready = true;
								if(cfg.echo)
										Serial.write("\r\n");
						} else if(ch == BS or ch == DEL) {
								if(line_sz > 0)
										line_sz--;
								if(cfg.echo) {
										Serial.print("\33[D \33[D");
								}
						} else {
								line[line_sz++] = ch;
								if(cfg.echo)
										Serial.write(ch);
						}

						if (is_full()) {
								clear();
								ignore = true;
						}

						if(is_ready() and (is_empty() or is_ignored()))
								clear();
				}
		}
};

template <typename T, typename U> void term(const char *key, T x, U b) {
		Serial.write(' ');
		Serial.print(key);
		Serial.write('=');
		Serial.print(x, b);
}

template <typename T> void term(const char *key, T x) {
		Serial.write(' ');
		Serial.print(key);
		Serial.write('=');
		Serial.print(x);
}

struct port {

		const char *name;

		uint8_t pin;
		uint16_t mode;

		bool analog;

		int s0;
		int s1;

		unsigned long m0;

		int ds() {
				return s0 - s1;
		}

		long dm() {
				return millis() - m0;
		}

		port(const char *my_name, int my_pin, int my_mode, bool my_analog, int s)
				: name(my_name), pin(my_pin), mode(my_mode), analog(my_analog), m0(millis())
		{

				pinMode(pin, mode);

				if (mode == OUTPUT) {
						write(s);
				} else {
						s0 = get();
						read();
				}
		}

		void status(const char *msg = nullptr) {

				Serial.write(':');
				Serial.print(name);

				term("PIN"   , pin   , DEC);
				term("S"     , s0    , DEC);
				term("DS"    , ds()  , DEC);
				term("M"     , m0    , DEC);
				term("DM"    , dm()  , DEC);

				if (msg != nullptr) {
						Serial.print(" \"");
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

		int update(int s) {

				s1 = s0;
				s0 = s;

				if (ds() != 0) {

						m0 = millis();

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

		virtual int step() {
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

template <int PIN> struct counting_port : port {

		static_assert(PIN == 2 or PIN == 3, "external interrupts are only available on pins 2 & 3");

		using interrupt_type = decltype(INT0);

		static constexpr interrupt_type interrupt = (PIN == 2) ? INT0 : INT1;

		static constexpr int coef0 = 2;
		static constexpr int coef1 = 1;

		static counting_port *reference_instance;

		static volatile int ms0;
		static volatile int ms1;

		static volatile int dms0;
		static volatile int dms1;

		static void isr() {
			ms1 = millis();
			dms1 = ms1 - ms0;
			ms0 = ms1;
			dms0 = (coef0 * dms0 + coef1 * dms1) / (coef0 + coef1);
		}

		int rpm() {
			return 15 * 1000 / dms0;
		}

		virtual int step() {
			return rpm();
		}

		counting_port(const char *my_name) : port(my_name, PIN, INPUT, false, LOW) {
				reference_instance = this;
				ms0 = millis();
				attachInterrupt(interrupt, isr, CHANGE);
		}

		~counting_port() {
				detachInterrupt(interrupt);
		}
};

template <int PIN> counting_port<PIN> *counting_port<PIN>::reference_instance = nullptr;

template <int PIN> volatile int counting_port<PIN>::ms0;
template <int PIN> volatile int counting_port<PIN>::ms1;

template <int PIN> volatile int counting_port<PIN>::dms0 = INT_MAX;
template <int PIN> volatile int counting_port<PIN>::dms1;

linebuf buf;

void pattern(const int *xs, port **ys) {
		do {
				for(port **zs = ys; *zs != nullptr; zs++)
						(*zs)->write(*xs);
				delay(*++xs);
		} while(*xs++ > 0);
}

port ports[] = {

		counting_port<3>("RPM"),

		// pulldown_port("IGN", 8),
		// pulldown_port("ACC", 9),

		// pullup_port("EMERGENCY", 10),

		adc_port("BATTERY", A0),
		adc_port("OXYGEN" , A1),
		adc_port("COOLANT", A2),
		adc_port("OIL"    , A3),
		adc_port("MAP"    , A4),
		adc_port("LIGHT"  , A5),

		digital_output_port("L-UP"  ,  4, HIGH),
		digital_output_port("L-DOWN",  5, HIGH),
		digital_output_port("R-DOWN",  6, HIGH),
		digital_output_port("R-UP"  ,  7, HIGH),
		digital_output_port("LOCK"  , 12, HIGH),
		digital_output_port("UNLOCK", 13, HIGH),
};

constexpr size_t ports_n =  sizeof(ports) / sizeof(port);

command commands[] = {
		command("lu"     , "raise left window"           , cmd_left_up   ),
		command("ld"     , "lower left window"           , cmd_left_down ),
		command("ru"     , "raise right window"          , cmd_right_up  ),
		command("rd"     , "lower right window"          , cmd_right_down),
		command("up"     , "raise all windows"           , cmd_all_up    ),
		command("down"   , "lower all windows"           , cmd_all_down  ),
		command("lock"   , "lock all doors"              , cmd_lock      ),
		command("cmds"   , "list commands"               , cmd_cmds      ),
		command("unlock" , "unlock all doors"            , cmd_unlock    ),
		command("status" , "report status of all devices", cmd_status    ),
		command("uptime" , "display system uptime"       , cmd_uptime    ),
		command("nop"    , "no operation"                , cmd_nop       ),
		command("version", "display firmware information", cmd_version   ),
		command("help"   , "display this help screen"    , cmd_help      ),
		command("echo"   , "toggle local echo"           , cmd_echo      ),
};

constexpr size_t commands_n = sizeof(commands) / sizeof(command);

int pattern_door[] = {
		LOW, 75, HIGH, 75,
		LOW, 75, HIGH, 75,
		LOW, 75, HIGH, 75,
		LOW, 75, HIGH, 0
};

int pattern_window[] = {
		LOW, 1000, HIGH, 0
};

void cmd_uptime() {
		Serial.print(":UPTIME");
		term("MS", millis(), DEC);
		Serial.println("");
}

void cmd_nop() {
		Serial.println(":VOID");
}

void cmd_echo() {
		cfg.echo = not cfg.echo;
		Serial.print(":ECHO");
		term("ENABLED", cfg.echo ? "TRUE" : "FALSE");
		Serial.println("");
}

void cmd_status() {
		for(size_t n = 0; n < ports_n; n++)
				ports[n].status();
}

void cmd_version() {
		Serial.println("");
		Serial.println("*************************************");
		Serial.println("* - El Camino Control System v1.0 - *");
		Serial.println("* - Copyright(c) 2015, 256 LLC    - *");
		Serial.println("* - Written by Christopher Abad   - *");
		Serial.println("*************************************");
		Serial.println("");
}

void cmd_cmds() {
		for(size_t n = 0; n < commands_n; n++) {
				Serial.write(':');
				Serial.print(commands[n].name);
				term("HELP", commands[n].help);
				Serial.println("");
		}
}

void cmd_help() {

		Serial.println("");
		Serial.println("********************");
		Serial.println("* - Command Help - *");
		Serial.println("********************");
		Serial.println("");

		for(size_t n = 0; n < commands_n; n++) {
				Serial.print("  ");
				Serial.print(commands[n].name);
				for(int k = 10 - strlen(commands[n].name); k >= 0; k--)
						Serial.write(' ');
				Serial.println(commands[n].help);
		}

		Serial.println("");
}

void cmd_left_up() {
		port *ys[] = {
				lookup("L-UP", ports, ports_n),
				nullptr
		};
		pattern(pattern_window, ys);
}

void cmd_left_down() {
		port *ys[] = {
				lookup("L-DOWN", ports, ports_n),
				nullptr
		};
		pattern(pattern_window, ys);
}

void cmd_right_up() {
		port *ys[] = {
				lookup("R-UP", ports, ports_n),
				nullptr
		};
		pattern(pattern_window, ys);
}

void cmd_right_down() {
		port *ys[] = {
				lookup("R-DOWN", ports, ports_n),
				nullptr
		};
		pattern(pattern_window, ys);
}

void cmd_all_up() {
		port *ys[] = {
				lookup("L-UP", ports, ports_n),
				lookup("R-UP", ports, ports_n),
				nullptr
		};
		pattern(pattern_window, ys);
}

void cmd_all_down() {
		port *ys[] = {
				lookup("L-DOWN", ports, ports_n),
				lookup("R-DOWN", ports, ports_n),
				nullptr
		};
		pattern(pattern_window, ys);
}

void cmd_lock() {
		port *ys[] = {
				lookup("LOCK", ports, ports_n),
				nullptr
		};
		pattern(pattern_door, ys);
}

void cmd_unlock() {
		port *ys[] = {
				lookup("UNLOCK", ports, ports_n),
				nullptr
		};
		pattern(pattern_door, ys);
}

void process_ports() {
		for(size_t n = 0; n < ports_n; n++)
				ports[n].step();
}

template <typename T> int compare(const void *a, const void *b) {
		return is_named<T>(((const T *)a)->name, b);
}

template <typename T> int is_named(const void *key, const void *member) {
		return strcasecmp((const char *)key, ((const T *)member)->name);
}

template <typename T> T *lookup(const char *name, T *objects, size_t n) {
		return (T *)bsearch(name, objects, n, sizeof(T), is_named<T>);
}

void process_buffer() {

		buf.update();

		if (buf.is_ready()) {

				command *cmd = lookup(buf.line, commands, commands_n);

				if(cmd == nullptr) {
						Serial.print(":ERROR");
						term("TYPE", "unknown-command");
						term("COMMAND", buf.line);
						Serial.println("");
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

LiquidCrystal_I2C lcd(0x27);

void setup_lcd()
{
lcd.begin(20,4);

lcd.home();

lcd.print("El Camino ECM v1.0");
lcd.setCursor ( 0, 1 );
lcd.print("Copyright(c) 2016");
lcd.setCursor ( 0, 2 );
lcd.print("Christopher Abad");
lcd.setCursor ( 0, 3 );
lcd.print("Iteration No: ");
}

void update_lcd() {
static int n = 1;
lcd.setCursor(14,3);
lcd.print(n++,DEC);
}

 */

void setup() {

		interrupts();

		qsort(ports, ports_n, sizeof(port), compare<port>);
		qsort(commands, commands_n, sizeof(command), compare<command>);

		Serial.begin(115200);

		delay(1000);

		cmd_version();

		// setup_lcd();
}

void loop() {
		process_ports();
		process_buffer();
		// update_lcd();
}
