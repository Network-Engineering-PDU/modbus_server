#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

#include "config.h"

#include <fcntl.h>

#define CPF_CLOEXEC 1

#define PID_FILE "/tmp/" PACKAGE_NAME ".pid"

/* Global variables ***********************************************************/

static int pid_fd = 0;

/* Function prototypes ********************************************************/

static int lockReg(int fd, int cmd, int type, int whence, int start, off_t len);
static int lockRegion(int fd, int type, int whence, int start, int len);
static int createPidFile(const char* progName, const char* pidFile, int flags);
static void checkPidFile(const char* progName, const char* pidFile);
static pid_t regionIsLocked(int fd, int type, int whence, int start, int len);

/* Function definitions *******************************************************/

static int lockReg(int fd, int cmd, int type, int whence, int start, off_t len)
{
	struct flock fl;

	fl.l_type = type;
	fl.l_whence = whence;
	fl.l_start = start;
	fl.l_len = len;

	return fcntl(fd, cmd, &fl);
}

static int lockRegion(int fd, int type, int whence, int start, int len)
{
	return lockReg(fd, F_SETLK, type, whence, start, len);
}

static void checkPidFile(const char* progName, const char* pidFile)
{
	int fd = open(pidFile, O_RDONLY);
	if (fd == -1) {
		return;
	}

	if (regionIsLocked(fd, F_RDLCK, SEEK_SET, 0, 0) != 0) {
		fprintf(stderr, "PID file '%s' is locked; probably "
				"'%s' is already running\n", pidFile, progName);
		close(fd);
		exit(EXIT_FAILURE);
	}

	char buf[100];
	ssize_t bytes_read = read(fd, buf, sizeof(buf) - 1);
	close(fd);

	if (bytes_read <= 0) {
		return;
	}

	long pid;
	if (sscanf(buf, "%ld", &pid) != 1) {
		return;
	}

	if (kill(pid, 0) == 0) {
		fprintf(stderr, "Can't start: %s is already running\n", progName);
		exit(EXIT_FAILURE);
	}
}

static pid_t regionIsLocked(int fd, int type, int whence, int start, int len)
{

	struct flock fl;

	fl.l_type = type;
	fl.l_whence = whence;
	fl.l_start = start;
	fl.l_len = len;

	if (fcntl(fd, F_GETLK, &fl) == -1) {
		return -1;
	}

	return (fl.l_type == F_UNLCK) ? 0 : fl.l_pid;
}

static int createPidFile(const char *progName, const char *pidFile, int flags)
{
	int fd;
	char buf[100];

	fd = open(pidFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		syslog(LOG_ERR, "Could not open PID file %s", pidFile);
		exit(EXIT_FAILURE);
	}

	if (flags & CPF_CLOEXEC) {

		/* Set the close-on-exec file descriptor flag */

		/* Instead of the following steps, we could (on Linux) have opened the
			file with O_CLOEXEC flag. However, not all systems support open()
			O_CLOEXEC (which was standardized only in SUSv4), so instead we use
			fcntl() to set the close-on-exec flag after opening the file */

		flags = fcntl(fd, F_GETFD);						/* Fetch flags */
		if (flags == -1) {
			syslog(LOG_ERR, "Could not get flags for PID file %s", pidFile);
			exit(EXIT_FAILURE);
		}

		flags |= FD_CLOEXEC;							/* Turn on FD_CLOEXEC */

		if (fcntl(fd, F_SETFD, flags) == -1) {			/* Update flags */
			syslog(LOG_ERR, "Could not set flags for PID file %s", pidFile);
			exit(EXIT_FAILURE);
		}
	}

	if (lockRegion(fd, F_WRLCK, SEEK_SET, 0, 0) == -1) {
		if (errno  == EAGAIN || errno == EACCES) {
			syslog(LOG_ERR, "PID file '%s' is locked; probably "
					"'%s' is already running", pidFile, progName);
			exit(EXIT_FAILURE);
		} else {
			syslog(LOG_ERR, "Unable to lock PID file '%s'", pidFile);
		}
	}

	if (ftruncate(fd, 0) == -1) {
		syslog(LOG_ERR, "Could not truncate PID file '%s'", pidFile);
		exit(EXIT_FAILURE);
	}

	snprintf(buf, sizeof(buf), "%ld\n", (long) getpid());
	if (write(fd, buf, strlen(buf)) != strlen(buf)) {
		syslog(LOG_ERR, "Writing to PID file '%s'", pidFile);
		exit(EXIT_FAILURE);
	}

	return fd;
}

/* Public fucntions ***********************************************************/

void daemonize()
{

	checkPidFile(PACKAGE_NAME, PID_FILE);

	pid_t pid;
	
	/* Fork off the parent process */
	pid = fork();
	
	/* An error occurred */
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	
	 /* Success: Let the parent terminate */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}
	
	/* On success: The child process becomes session leader */
	if (setsid() < 0) {
		exit(EXIT_FAILURE);
	}
	
	/* Catch, ignore and handle signals */
	/*TODO: Implement a working signal handler */
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	
	/* Fork off for the second time*/
	pid = fork();
	
	/* An error occurred */
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	
	/* Success: Let the parent terminate */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}
	
	/* Set new file permissions */
	umask(0);
	
	/* Change the working directory to the root directory */
	/* or another appropriated directory */
	int rc = chdir("/");
	(void)rc; //TODO
	
	/* Close all open file descriptors */
	int x;
	for (x = sysconf(_SC_OPEN_MAX); x>=0; x--) {
		close (x);
	}
	
	/* Open the log file */
	openlog (PACKAGE_NAME, LOG_PID, LOG_DAEMON);

	pid_fd = createPidFile(PACKAGE_NAME, PID_FILE, CPF_CLOEXEC);

	syslog(LOG_NOTICE, PACKAGE_NAME " started with PID: %ld", (long) getpid());
}

void exit_daemon(int err)
{
	struct flock file_lock;
	file_lock.l_type = F_UNLCK;
	file_lock.l_whence = SEEK_SET;
	file_lock.l_start = 0;
	file_lock.l_len = 0;
	fcntl(pid_fd, F_SETLK, &file_lock);

	close(pid_fd);
	unlink(PID_FILE);

	syslog(LOG_NOTICE, "Program exit.\n");
	closelog();
	exit(err);
}

