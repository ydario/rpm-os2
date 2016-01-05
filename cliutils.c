#include "system.h"
#if HAVE_MCHECK_H
#include <mcheck.h>
#endif
#include <errno.h>
#include <sys/wait.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmcli.h>
#include "cliutils.h"
#include "debug.h"

static pid_t pipeChild = 0;
#ifdef __KLIBC__
static FILE* pipeFD = NULL;
#endif

RPM_GNUC_NORETURN
void argerror(const char * desc)
{
    fprintf(stderr, _("%s: %s\n"), __progname, desc);
    exit(EXIT_FAILURE);
}

static void printVersion(FILE * fp)
{
    fprintf(fp, _("RPM version %s\n"), rpmEVR);
}

static void printBanner(FILE * fp)
{
    fprintf(fp, _("Copyright (C) 1998-2002 - Red Hat, Inc.\n"));
    fprintf(fp, _("This program may be freely redistributed under the terms of the GNU GPL\n"));
}

void printUsage(poptContext con, FILE * fp, int flags)
{
    printVersion(fp);
    printBanner(fp);
    fprintf(fp, "\n");

    if (rpmIsVerbose())
	poptPrintHelp(con, fp, flags);
    else
	poptPrintUsage(con, fp, flags);
}

int initPipe(void)
{
    int p[2];

#ifdef __KLIBC__

    char cmdline[16*1024];
    // place command line inside quotes to allow sh to execute all commands
    // itself (otherwise also cmd is involved)
    sprintf( cmdline, "sh -c \"%s\"", rpmcliPipeOutput);
    // start child and redirect its input to us
    pipeFD = popen( cmdline, "w");
    if (pipeFD == NULL) {
	fprintf(stderr, "creating a pipe for --pipe failed: %s\n", cmdline);
	return -1;
    }
    // now redirect stdout to input handle
    dup2( fileno(pipeFD), STDOUT_FILENO);

#else

    if (pipe(p) < 0) {
	fprintf(stderr, _("creating a pipe for --pipe failed: %m\n"));
	return -1;
    }

    if (!(pipeChild = fork())) {
	(void) signal(SIGPIPE, SIG_DFL);
	(void) close(p[1]);
	(void) dup2(p[0], STDIN_FILENO);
	(void) close(p[0]);
	(void) execl("/@unixroot/usr/bin/sh", "/@unixroot/usr/bin/sh", "-c", rpmcliPipeOutput, NULL);
	fprintf(stderr, _("exec failed\n"));
	exit(EXIT_FAILURE);
    }

    (void) close(p[0]);
    (void) dup2(p[1], STDOUT_FILENO);
    (void) close(p[1]);
#endif

    return 0;
}

int finishPipe(void)
{
    int rc = 0;

#ifdef __KLIBC__

    // close stdout to allow child to end
    (void) fclose(stdout);
    // wait child end and query exit code
    int status = pclose(pipeFD);
    pipeFD = NULL;
    if (!WIFEXITED(status) || WEXITSTATUS(status))
        rc = 1;

#else
    if (pipeChild) {
	int status;
	pid_t reaped;

	(void) fclose(stdout);
	do {
	    reaped = waitpid(pipeChild, &status, 0);
	} while (reaped == -1 && errno == EINTR);
	    
	if (reaped == -1 || !WIFEXITED(status) || WEXITSTATUS(status))
	    rc = 1;
    }
#endif

    return rc;
}
