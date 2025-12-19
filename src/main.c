#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <time.h>

#include <modbus/modbus.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>


#include "models.h"
#include "controller.h"
#include "daemonize.h"

#define MAX_INPUTS 6
#define MAX_OUTLETS 48
#define READ_PERIOD 20

/* Max number of connections */
#define NB_CONNECTION 5

#define N_READ_BITS       MAX_OUTLETS
#define N_INPUT_BITS      0 
#define N_READ_REGISTERS  (2*MAX_INPUTS + 2*MAX_OUTLETS)
#define N_INPUT_REGISTERS 0

#define BIND_ADDR "0.0.0.0"
#define BIND_PORT 502

#define SERIAL_PORT "/dev/ttymxc1"
// #define SERVER_ID   17

static modbus_mapping_t *mb_mapping;
static modbus_t *ctx_tcp = NULL;
static uint8_t* query_tcp;
static modbus_t *ctx_rtu = NULL;
static uint8_t* query_rtu;

static int server_socket = -1;

static pthread_mutex_t mapping_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* write_values_cb(void* arg)
{
	while (1) {
		int16_t in_c[MAX_INPUTS];
		int16_t in_p[MAX_INPUTS];
		int16_t out_c[MAX_OUTLETS];
		int16_t out_p[MAX_OUTLETS];
		static uint8_t out_s[MAX_OUTLETS];
		static int sw_init_flag = 0;

		controller_get_license();
		models_license_t license = models_get_license();

		if (license == B1 || license == B2) {
			pthread_mutex_lock(&mapping_mutex);
			for (int i = 0; i < MAX_OUTLETS; i++) {
				int sw = mb_mapping->tab_bits[i];
				if (sw != out_s[i]) {
					out_s[i] = sw;
					if (sw_init_flag) {
						models_out_sw_t sw;
						sw.status = out_s[i] != 0;
						sw.line_id = i;
						controller_put_out_sw(&sw, i);
					}
				}
			}
			pthread_mutex_unlock(&mapping_mutex);

			sw_init_flag = 1;

			controller_get_out_sw();
			int len = 0;
			const models_out_sw_t* out_sw = models_get_out_sw(&len);
			for (int i = 0; i < MAX_OUTLETS; i++) {
				if (i < len) {
					out_s[i] = out_sw[i].status;
				} else {
					out_s[i] = 0;
				}
			}
		}
	
		if (license == A1 || license == B1) {
			memset(in_c, 0, sizeof(in_c));
			memset(in_p, 0, sizeof(in_p));
			memset(out_c, 0, sizeof(out_c));
			memset(out_p, 0, sizeof(out_p));
		} else { /* License A2 or B2 */
			for (int i = 0; i < MAX_INPUTS; i++) {
				controller_get_in_data(i);
				const models_in_data_t* in_data = models_get_in_data();
				in_c[i] = in_data->current * 100; // MAX C = +-327,68 A
				in_p[i] = in_data->active_power; // MAX P = +-32,768 kW
			}
			for (int i = 0; i < MAX_OUTLETS; i++) {
				controller_get_out_data(i);
				const models_out_data_t* out_data = models_get_out_data();
				out_c[i] = out_data->current * 100; // MAX C = +-327,68 A
				out_p[i] = out_data->active_power; // MAX P = +-32,768 kW
			}
		}
		pthread_mutex_lock(&mapping_mutex);
		// Aqui dentro se escriben los registros mb_mapping->tab_registers[0] = x;
		// REG 0-5    INPUT CURRENT
		// REG 6-11   INPUT POWER
		// REG 12-59  OUTPUT CURRENT
		// REG 60-107 OUTPUT POWER
		// Y las input coils mb_mapping->tab_input_coil[0] = x;
		// COIL 0-47  SWITCH STATUS

		for (int i = 0; i < MAX_INPUTS; i++) {
			mb_mapping->tab_registers[i] = in_c[i];
		}
		for (int i = 0; i < MAX_INPUTS; i++) {
			mb_mapping->tab_registers[i+MAX_INPUTS] = in_p[i];
		}
		for (int i = 0; i < MAX_OUTLETS; i++) {
			mb_mapping->tab_registers[i+2*MAX_INPUTS] = out_c[i];
		}
		for (int i = 0; i < MAX_OUTLETS; i++) {
			mb_mapping->tab_registers[i+2*MAX_INPUTS + MAX_OUTLETS] = out_p[i];
		}
		for (int i = 0; i < MAX_OUTLETS; i++) {
			mb_mapping->tab_bits[i] = out_s[i];
		}
		pthread_mutex_unlock(&mapping_mutex);

		sleep(READ_PERIOD);
	}

	pthread_exit(NULL);
}

static void close_sigint(int sig)
{
	(void)sig;

	if (server_socket != -1) {
		close(server_socket);
	}
	modbus_free(ctx_tcp);
	modbus_free(ctx_rtu);

	free(query_tcp);
	free(query_rtu);

	modbus_mapping_free(mb_mapping);

	exit(-1);
}

static void* main_rtu(void* args)
{
	modbus_t* ctx = ctx_rtu;
	uint8_t* query = query_rtu;

	query = malloc(MODBUS_RTU_MAX_ADU_LENGTH);
	ctx = modbus_new_rtu(SERIAL_PORT, 115200, 'N', 8, 1);

	controller_get_modbus();
	const models_modbus_t* modbus = models_get_modbus();
	syslog(LOG_INFO, "Modbus address readed: %d", modbus->addr);
	if (modbus->addr == -1) {
		syslog(LOG_ERR, "Error opening file for reading.");
		close_sigint(0);
	}

	modbus_set_slave(ctx, modbus->addr);
	modbus_rtu_set_serial_mode(ctx, MODBUS_RTU_RS485);

	if (modbus_connect(ctx)  == -1) {
		syslog(LOG_ERR, "Unable to connect %s\n", modbus_strerror(errno));
		close_sigint(0);
	}

	for(;;) {
		int rc = modbus_receive(ctx, query);
		/* Ignore events addresed to others */
		if (rc == 0) {
			continue;
		}


		if (rc == -1 && errno != EMBBADCRC) {
			close_sigint(0);
		}

		rc = modbus_reply(ctx, query, rc, mb_mapping);
		if (rc == -1) {
			close_sigint(0);
		}
	}
	return NULL;
}


static void* main_tcp(void* args)
{
	modbus_t* ctx = ctx_tcp;
	uint8_t* query = query_tcp;
	query = malloc(MODBUS_TCP_MAX_ADU_LENGTH);

	int master_socket;
	int rc;

	/* Set of sockets to monitor */
	fd_set refset;
	fd_set rdset;

	/* Maximum file descriptor number */
	int fdmax;

	ctx = modbus_new_tcp(BIND_ADDR, BIND_PORT);

	server_socket = modbus_tcp_listen(ctx, NB_CONNECTION);
	if (server_socket == -1) {
		syslog(LOG_ERR, "Unable to listen TCP connection\n");
		close_sigint(0);
	}

	/* Clear the reference set of socket */
	FD_ZERO(&refset);

	/* Add the server socket */
	FD_SET(server_socket, &refset);

	/* Keep track of the max file descriptor */
	fdmax = server_socket;

	for (;;) {
		rdset = refset;

		/* Wait until one connection changes */
		if (select(fdmax + 1, &rdset, NULL, NULL, NULL) == -1) {
			syslog(LOG_ERR, "Server select() failure.");
			close_sigint(0);
		}

		/* Run through the existing connections looking for data to be read */
		for (master_socket = 0; master_socket <= fdmax; master_socket++) {

			if (!FD_ISSET(master_socket, &rdset)) {
				continue;
			}

			/* A client is asking a new connection */
			if (master_socket == server_socket) {
				socklen_t addrlen;
				struct sockaddr_in clientaddr;
				int newfd;

				/* Handle new connections */
				addrlen = sizeof(clientaddr);
				memset(&clientaddr, 0, sizeof(clientaddr));
				newfd = accept(server_socket, (struct sockaddr *) &clientaddr,
						&addrlen);

				if (newfd == -1) {
					syslog(LOG_ERR, "Server accept() error");
				} else {
					/* Add connection to set */
					FD_SET(newfd, &refset);

					if (newfd > fdmax) {
						/* Keep track of the maximum */
						fdmax = newfd;
					}
					syslog(LOG_INFO, "New connection from %s:%d on socket %d\n",
							inet_ntoa(clientaddr.sin_addr),
							clientaddr.sin_port,
							newfd);
				}
			} else { /* Client event */
				modbus_set_socket(ctx, master_socket);
				rc = modbus_receive(ctx, query);
				if (rc > 0) {
					/* Client asking for data */
					pthread_mutex_lock(&mapping_mutex);
					modbus_reply(ctx, query, rc, mb_mapping);
					pthread_mutex_unlock(&mapping_mutex);

				} else if (rc == -1) {
					/* Client ending connection or error ocurred */
					syslog(LOG_INFO, "Connection closed on socket %d\n", master_socket);
					close(master_socket);

					/* Remove from reference set */
					FD_CLR(master_socket, &refset);

					if (master_socket == fdmax) {
						fdmax--;
					}
				}
			}
		}
	}
	return NULL;
}


int main(void) {
	printf("Starting daemon...\n");
	daemonize();


	mb_mapping = modbus_mapping_new(N_READ_BITS, N_INPUT_BITS,
			N_READ_REGISTERS, N_INPUT_REGISTERS);

	if (mb_mapping == NULL) {
		syslog(LOG_ERR, "Failed to allocate the mapping: %s\n",
				modbus_strerror(errno));
		return -1;
	}

	pthread_t write_thread;
	pthread_create(&write_thread, NULL, write_values_cb, NULL);
	pthread_t tcp_thread;
	pthread_create(&tcp_thread, NULL, main_tcp, NULL);
	pthread_t rtu_thread;
	pthread_create(&rtu_thread, NULL, main_rtu, NULL);

	signal(SIGINT, close_sigint);

	pthread_join(tcp_thread, NULL);
	pthread_join(rtu_thread, NULL);
	pthread_join(write_thread, NULL);


}
