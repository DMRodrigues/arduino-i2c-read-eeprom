/*
 * Program for connecting to an Arduino running the i2c_read_eeprom.ino
 *
 * Reads the requested number of bytes from the eeprom and saves
 * them in a file with the given name
 *
 * Execute with --help to see instructions.
 *
 * =============================================================================
 *
 * Original author(s):
 *    Andre Richter, andre.o.richter a t gmail d o t com
 *    Lorenzo Cafaro, lorenzo@interstella.net
 *
 * =============================================================================
 *
 * This version by me:
 *    Diogo Rodrigues, https://github.com/DMRodrigues
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <getopt.h>

#include "eeprom.h"

int print, ctrlc = 0;
char *file_name = NULL;

void handle_sig(int signum)
{
	ctrlc = 1;
}

struct sigaction int_handler = {
	.sa_handler = handle_sig,
};

int to_termios_baudrate(unsigned int baudrate)
{
	switch (baudrate) {
		case 50:     return B50;
		case 75:     return B75;
		case 110:    return B110;
		case 134:    return B134;
		case 150:    return B150;
		case 200:    return B200;
		case 300:    return B300;
		case 600:    return B600;
		case 1200:   return B1200;
		case 1800:   return B1800;
		case 2400:   return B2400;
		case 4800:   return B4800;
		case 9600:   return B9600;
		case 19200:  return B19200;
		case 38400:  return B38400;
		case 57600:  return B57600;
		case 115200: return B115200;
		case 230400: return B230400;
		default:     return -1;
	}
}

static void print_eeprom()
{
	int i;
	printf("\n-----\nRead the datasheet of the EEPROM and identify the addressing type compatible with one implemented.\n");
	printf("\nDefault, EEPROMs bigger than 4Kbytes(32 Kbits)");
	printf("\n1 | 0 | 1 | 0 | A2 | A1/B1/P1/A17 | A0/P0/B0/A16\n");
	printf("	Devices:\n		");	
	for(i = 0; type[i]; i++)
		printf("%s, ", type[i]);
	printf("\n\nEEPROMs up to 2Kbytes(16Kbits)\n1 | 0 | 1 | 0 | A2/B2/P2/A10 | A1/B1/P1/A9 | A0/B0/P0/A8  =>  -d 1\n");
	printf("	Devices:\n		");
	for(i = 0; type1[i]; i++)
		printf("%s,", type1[i]);
	printf("\n\nOther type of addressing\n1 | 0 | 1 | 0 | P0/B0/A16 | A1 | A0  =>  -d 2\n");
	printf("	Devices:\n		");
	for(i = 0; type2[i]; i++)
		printf("%s,", type2[i]);
	printf("\n-----");
}

static void print_usage(const char *program_name, int flag_print_eeprom)
{
	printf("\nUsage: %s [OPTIONS...]\n", program_name);
	printf("\n");
	printf("  -t, --tty=/dev/*         tty the arduino is connected to.\n");
	printf("  -b, --baudrate=N         baudrate of tty. Default sketch is 115200.\n");
	printf("  -n, --num-bytes=N[k]     the number of bytes or Kbytes to read from the tty.\n");
	printf("  -d, --dev-type=type      read datasheet and see compatible type then set number 1 or 2 [REQUIRED IF NOT DEFAULT].\n");
	printf("  -o, --output-file=FILE   name to save FILE. Default is \"eeprom.bin\".\n");
	printf("  -f, --format=format      output format (a = ascii, d = dec, h = hex). Default is hexadecimal.\n");
	printf("  -p, --print=y/n          print the read content to screen. Default is yes.\n");
	printf("  -h, --help               print this screen.\n");
	if(flag_print_eeprom)
		print_eeprom();
	printf("\nExamples:\n");
	printf("  %s -t /dev/ttyACM0 -n 2k -p n -d 1\n", program_name);
	printf("  %s -t /dev/ttyACM0 -n 128k -o teste.bin\n", program_name);
	printf("  %s -t /dev/ttyACM0 -n 128K -d 2 -o teste.bin\n", program_name);
	printf("  %s -t /dev/ttyACM0 -n 256K -p n\n", program_name);
	printf("  %s -t /dev/ttyACM0 -n 64k -p n\n", program_name);
	printf("  %s -t /dev/ttyACM0 -n 64\n", program_name);
	exit(EXIT_FAILURE);
}

int transmit_num_bytes(int fd, unsigned long num_bytes, unsigned char type)
{
	unsigned int i;
	unsigned char num_bytes_array[5];

	/* we only need 4 bytes to send the size to read */
	for (i = 0; i < 4; i++)
		num_bytes_array[i] = (unsigned char)((num_bytes >> (i * 8)) & 0xff);

	/* and then put type of read/device */
	num_bytes_array[i] = type;
	
	return write(fd, num_bytes_array, sizeof(num_bytes_array));
}

unsigned long get_bytes(char* bytes)
{
	unsigned long res = 0;
	if((bytes[strlen(bytes) - 1] == 'k') || (bytes[strlen(bytes) - 1] == 'K')) {
		char temp_bytes[strlen(bytes) - 1];
		memcpy(temp_bytes, bytes, strlen(bytes) - 1);
		res = atoi(temp_bytes) * 1024; /* convert to bytes */
	}
	else
		res = atoi(bytes);
	return res;
}

int print_option(char* option) {
	if((strcmp(option, "y") == 0) || (strcmp(option, "yes") == 0))
		return 1;
	return 0;
}

int eeprom_read(int fd, unsigned long num_bytes, char format)
{
	errno = 0;
	unsigned long i = 0;
	unsigned char byte;
	FILE *fp = fopen(file_name, "w");

	while((i < num_bytes) && !ctrlc) {
		if (read(fd, &byte, sizeof(byte)) < 0) {
			printf("\nError while reading, errno: %s\n", strerror(errno));
			break;
		}

		if(print) {
			switch (format) {
				case 'a':
					printf("0x%06lx: %c\n", i, byte);
					break;
				case 'd':
					printf("0x%06lx: %02d\n", i, byte);
					break;
				case 'h':
					printf("0x%06lx: 0x%02x\n", i, byte);
					break;
				default:
					printf("0x%06lx: 0x%02x\n", i, byte);
					break;
			}
		} else printf("\r%ld %%", ((i*100)/(num_bytes-1)));
		
		if (fwrite(&byte, sizeof(byte), 1, fp) < 0) {
			printf("\nError while writing, errno: %s\n", strerror(errno));
			break;
		}
		
		i++;
	}
	if(!print)
		printf("\n");
	
	fclose(fp);
	
	return errno;
}

int main(int argc, char *argv[])
{
	int fd, next_option, baudrate = B115200;
	unsigned long num_bytes = 0;
	char format = 0;
	char *tty_name = NULL;
	unsigned char dev_type = 0;
	unsigned char baudrate_user = 0, format_user = 0, file_name_user = 0, print_user = 0;
	
	struct termios tty_attr, tty_attr_orig;
	speed_t i_speed, o_speed, user_speed;

	extern char *optarg;
	const char* short_options = "t:b:n:d:o:f:p:h";
	const struct option long_options[] = {
		{ "tty",          required_argument, NULL, 't' },
		{ "baudrate",     required_argument, NULL, 'b' },
		{ "num-bytes",    required_argument, NULL, 'n' },
		{ "dev-type",     required_argument, NULL, 'd' },
		{ "output-name",  required_argument, NULL, 'o' },
		{ "format",       required_argument, NULL, 'f' },
		{ "print",        required_argument, NULL, 'p' },
		{ "help",         no_argument,       NULL, 'h' },
		{ 0,              0,                 0,     0  }
	};

	/* no argument provided */
	if(argc == 1) {
		printf("\n	NO ARGUMENTS PROVIDED!\n\n");
		print_usage(argv[0], 0);
		return EXIT_FAILURE;
	}

	do {
		next_option = getopt_long(argc, argv, short_options, long_options, NULL);
		switch (next_option) {
			case 't':
				tty_name = optarg;
				break;
			case 'b':
				baudrate = to_termios_baudrate(atoi(optarg));
				baudrate_user = 1;
				break;
			case 'n':
				num_bytes = get_bytes(optarg);
				break;
			case 'd':
				dev_type = atoi(optarg);
				break;
			case 'o':
				file_name = optarg;
				file_name_user = 1;
				break;
			case 'f':
				format = optarg[0];
				format_user = 1;
				break;
			case 'p':
				print = print_option(optarg);
				print_user = 1;
				break;
			case 'h':
				print_usage(argv[0], 1);
				break;
			case '?':
				print_usage(argv[0], 1);
				break;
			case -1:
				break;
			default:
				abort();
		}
	} while (next_option != -1);

	/* check if setup parameters given and valid */
	if (!tty_name) {
		printf("\nSorry, you did not provide the name of "
		       "your arduino's tty device.\n\n");
		print_usage(argv[0], 0);
	}

	if (baudrate < 0) {
		printf("\nSorry, you did not provide a valid baudrate.\n\n");
		print_usage(argv[0], 0);
	}
	user_speed = (speed_t)baudrate;

	if (!num_bytes) {
		printf("\nSorry, you did not provide how many bytes you "
		       "want to read from the EEPROM.\n\n");
		print_usage(argv[0], 0);
	}
	
	if ((dev_type < 0) || (dev_type > 2)) {
		printf("\nSorry, invalid device type for reading.\n");
		print_usage(argv[0], 1);
	}
	else if((dev_type == 1) && (num_bytes > 2048)) {
		printf("ERROR: trying to read more than available memory!!!\n");
		print_usage(argv[0], 1);
	}

	if (!format)
		format = 'h';
	else if (format != 'a' && format != 'd' && format != 'h') {
		printf("\nSorry, invalid output format specified.\n\n");
		print_usage(argv[0], 0);
	}

	if (!file_name)
		file_name = "eeprom.bin";

	if (!print_user)
		print = 1;

	/* After setup parameters print if any fallback to default */
	if (!baudrate_user)
		printf("You did not provide a baudrate, defaulting to 115200.\n");
	if (!file_name_user)
			printf("You did not provide a output name, defaulting to "
		       "\"eeprom.bin\".\n");
	if (!format_user)
		printf("You did not specified a output format, defaulting to "
		       "h = hexadecimal.\n");
	if(!print_user)
		printf("You did not provide a print option, defaulting to yes.\n");

	/* Install CTRL^C signal handler */
	sigaction(SIGINT, &int_handler, 0);

	/* Open tty */
	fd = open(tty_name, O_RDWR);
	if (fd == -1) {
		printf("Error opening %s: %s\n", tty_name, strerror(errno));
		return EXIT_FAILURE;
	}
	printf("%s opened.\n", tty_name);

	/* Get tty config and make backup */
	tcgetattr(fd, &tty_attr);
	tty_attr_orig = tty_attr;

	/* Configure tty */
	i_speed = cfgetispeed(&tty_attr);
	if (i_speed != user_speed)
		cfsetispeed(&tty_attr, user_speed);

	o_speed = cfgetospeed(&tty_attr);
	if (o_speed != user_speed)
		cfsetospeed(&tty_attr, user_speed);

	tty_attr.c_cc[VMIN]  = 1; /* Block reads until 1 byte is available */
	tty_attr.c_cc[VTIME] = 0; /* Never return from read due to timeout */

	tty_attr.c_iflag &= ~(ICRNL | IXON);
	tty_attr.c_oflag &= ~OPOST;
	tty_attr.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE);

	if (tcsetattr(fd, TCSANOW, &tty_attr) == -1) {
		printf("Error while setting %s options: %s\n", tty_name, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	printf("%s successfully configured.\n", tty_name);

	sleep(3); /* Wait a second; Prevents that first byte send to arduino gets corrupted */
	/* More time because UN0 resets on opening device
	 * maybe using 10uF cap between reset and ground */

	if ((transmit_num_bytes(fd, num_bytes, dev_type)) < 0) {
		printf("Error while transfering dump size to arduino: %s\n", strerror(errno));
		tcsetattr(fd, TCSANOW, &tty_attr_orig);
		close(fd);
		return EXIT_FAILURE;
	}

	if (eeprom_read(fd, num_bytes, format) != 0) {
		printf("Going to remove file with errors\n");
		if (remove(file_name) != 0)
			printf("Unable to delete %s: %s\n", file_name, strerror(errno));
	}

	/* Revert to original tty config */
	if(tcsetattr(fd, TCSANOW, &tty_attr_orig) == -1)
		printf("Error while reverting original tty config: %s\n", strerror(errno));

	close(fd);
	return EXIT_SUCCESS;
}
