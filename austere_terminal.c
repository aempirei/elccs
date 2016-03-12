#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int nonblock(int fd) {
		int flags = fcntl(fd, F_GETFL, 0);
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {

		int fd = open("/dev/ttyUSB0", O_RDWR);
		if(fd == -1) {
				perror("open()");
				exit(-1);
		}

		if(nonblock(fd) == -1) {
			perror("nonblock(fd)");
			exit(-1);
		}
		if(nonblock(STDIN_FILENO) == -1) {
			perror("nonblock(STDIN_FILENO)");
			exit(-1);
		}

		int flags = fcntl(fd, F_GETFL, 0);
		if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
				perror("fcntl()");
				exit(-1);
		}

		for(;;) {
				int ch;
				ssize_t n;

				n = read(STDIN_FILENO, &ch, 1);

				if(n == -1 && errno != EAGAIN) {
					perror("read(STDIN_FILENO)");
					break;
				}

				if(n == 1) {
						n = write(fd, &ch, 1);
						if(n == -1) {
								perror("write(fd)");
								break;
						}
						if(n <= 0)
								break;
				}

				n = read(fd, &ch, 1);

				if(n == -1 && errno != EAGAIN) {
						perror("read(fd)");
						break;
				}

				if(n == 1) {
						n = write(STDOUT_FILENO, &ch, 1);
						if(n == -1) {
								perror("write(STDOUT_FILENO)");
								break;
						}
						if(n <= 0)
								break;
				}
		}

		exit(0);
}
