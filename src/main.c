#define _GNU_SOURCE /* SO_BINDTODEVICE */
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
#include "chain_master.h"
#include "register_map.h"

#define READ_PERIOD 20

/* Max number of connections */
#define NB_CONNECTION 5

#define BIND_ADDR "0.0.0.0"
#define BIND_PORT 502

#define SERIAL_PORT "/dev/ttymxc1"
// #define SERVER_ID   17

/* Matches ne-fw-api/ttne/network_config.py: LAN1_IFACE / LAN2_IFACE */
#define ETH1_IFACE "eth1"
#define ETH2_IFACE "eth0"

static modbus_mapping_t *mb_mapping;
static modbus_t *ctx_tcp = NULL;
static uint8_t* query_tcp;
static modbus_t *ctx_rtu = NULL;
static uint8_t* query_rtu;

/* Up to one listening socket per enabled NIC; the TCP server binds
 * 0.0.0.0, so each socket is pinned to its interface with
 * SO_BINDTODEVICE to let each NIC be turned off independently. */
static int server_sockets[2] = { -1, -1 };
static int server_socket_count = 0;

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

	for (int i = 0; i < server_socket_count; i++) {
		close(server_sockets[i]);
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
	/* RTU and TCP are independent: a problem on this serial side (address
	 * unavailable, port busy, cable unplugged) must not take down the TCP
	 * Modbus server, so failures here end only this thread, not the process. */
	modbus_t* ctx;
	uint8_t* query;

	controller_get_modbus();
	const models_modbus_t* modbus = models_get_modbus();
	syslog(LOG_INFO, "Modbus RTU settings: role=%s addr=%d baud=%d parity=%c stop_bits=%d",
			modbus->master ? "master" : "slave", modbus->addr,
			modbus->baud_rate, modbus->parity, modbus->stop_bits);
	if (modbus->addr == -1) {
		syslog(LOG_ERR, "Error opening file for reading.");
		return NULL;
	}

	query = malloc(MODBUS_RTU_MAX_ADU_LENGTH);
	ctx = modbus_new_rtu(SERIAL_PORT, modbus->baud_rate, modbus->parity, 8,
			modbus->stop_bits);
	if (ctx == NULL) {
		syslog(LOG_ERR, "Unable to create RTU context: %s", modbus_strerror(errno));
		free(query);
		return NULL;
	}

	modbus_set_slave(ctx, modbus->addr);
	modbus_rtu_set_serial_mode(ctx, MODBUS_RTU_RS485);

	if (modbus_connect(ctx)  == -1) {
		syslog(LOG_ERR, "Unable to connect %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		free(query);
		return NULL;
	}

	ctx_rtu = ctx;
	query_rtu = query;

	/* A daisy chain master asks the other PDUs on the bus instead of
	 * answering; the bus can have only one master. */
	if (modbus->master) {
		models_modbus_t cfg = *modbus;
		chain_master_run(ctx, &cfg);
	}

	/* When a request is for another unit, libmodbus (modbus-rtu.c,
	 * confirmation_to_ignore) discards the next message as that unit's
	 * reply, waiting up to the response timeout (500 ms by default). A
	 * master scanning for PDUs asks for unit IDs that don't exist, so the
	 * next message is its next request, which would be lost, possibly the
	 * one for this unit. Real PDUs start replying within a few ms, and the
	 * master waits at least 100 ms per unit, so a 50 ms wait still skips
	 * real replies but gives up on missing units before the next request. */
	modbus_set_response_timeout(ctx, 0, 50000);

	for(;;) {
		int rc = modbus_receive(ctx, query);
		/* Ignore events addresed to others */
		if (rc == 0) {
			continue;
		}

		if (rc == -1) {
			/* The port itself is gone: nothing more to do on this side. */
			if (errno == EBADF || errno == EIO || errno == ENXIO) {
				syslog(LOG_ERR, "RTU link error: %s", modbus_strerror(errno));
				break;
			}
			/* Noise, a bad checksum or a partial frame on a shared bus:
			 * drop it and keep listening. */
			modbus_flush(ctx);
			continue;
		}

		pthread_mutex_lock(&mapping_mutex);
		rc = modbus_reply(ctx, query, rc, mb_mapping);
		pthread_mutex_unlock(&mapping_mutex);
		if (rc == -1) {
			syslog(LOG_WARNING, "RTU reply error: %s", modbus_strerror(errno));
		}
	}
	modbus_close(ctx);
	modbus_free(ctx);
	free(query);
	ctx_rtu = NULL;
	query_rtu = NULL;
	return NULL;
}


/* Binds a TCP listening socket to one NIC only, so each of the two Ethernet
 * ports can serve Modbus independently of the other. modbus_tcp_listen()
 * does not support this, so the socket is set up by hand instead. */
static int bind_tcp_socket(const char* iface)
{
	int fd;
	int on = 1;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		syslog(LOG_ERR, "Modbus TCP socket() failed for %s: %s", iface, strerror(errno));
		return -1;
	}
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface) + 1) == -1) {
		syslog(LOG_ERR, "Modbus TCP could not bind to device %s: %s", iface, strerror(errno));
		close(fd);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(BIND_PORT);

	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		syslog(LOG_ERR, "Modbus TCP bind() failed for %s: %s", iface, strerror(errno));
		close(fd);
		return -1;
	}
	if (listen(fd, NB_CONNECTION) == -1) {
		syslog(LOG_ERR, "Modbus TCP listen() failed for %s: %s", iface, strerror(errno));
		close(fd);
		return -1;
	}
	syslog(LOG_INFO, "Modbus TCP listening on %s:%d", iface, BIND_PORT);
	return fd;
}

static bool is_server_socket(int fd)
{
	for (int i = 0; i < server_socket_count; i++) {
		if (server_sockets[i] == fd) {
			return true;
		}
	}
	return false;
}

static void* main_tcp(void* args)
{
	modbus_t* ctx;
	uint8_t* query;
	int master_socket;
	int rc;

	/* Set of sockets to monitor */
	fd_set refset;
	fd_set rdset;

	/* Maximum file descriptor number */
	int fdmax = -1;

	query = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
	ctx = modbus_new_tcp(BIND_ADDR, BIND_PORT);

	controller_get_modbus();
	const models_modbus_t* modbus = models_get_modbus();

	FD_ZERO(&refset);
	if (modbus->tcp_eth1) {
		int fd = bind_tcp_socket(ETH1_IFACE);
		if (fd != -1) {
			server_sockets[server_socket_count++] = fd;
		}
	}
	if (modbus->tcp_eth2) {
		int fd = bind_tcp_socket(ETH2_IFACE);
		if (fd != -1) {
			server_sockets[server_socket_count++] = fd;
		}
	}
	if (server_socket_count == 0) {
		syslog(LOG_WARNING, "Modbus TCP is not listening on either Ethernet port");
		free(query);
		modbus_free(ctx);
		return NULL;
	}
	for (int i = 0; i < server_socket_count; i++) {
		FD_SET(server_sockets[i], &refset);
		if (server_sockets[i] > fdmax) {
			fdmax = server_sockets[i];
		}
	}

	ctx_tcp = ctx;
	query_tcp = query;

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
			if (is_server_socket(master_socket)) {
				socklen_t addrlen;
				struct sockaddr_in clientaddr;
				int newfd;

				/* Handle new connections */
				addrlen = sizeof(clientaddr);
				memset(&clientaddr, 0, sizeof(clientaddr));
				newfd = accept(master_socket, (struct sockaddr *) &clientaddr,
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
