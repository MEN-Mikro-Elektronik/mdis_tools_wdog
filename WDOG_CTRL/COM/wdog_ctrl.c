/****************************************************************************
 ************                                                    ************
 ************                    WDOG_CTRL                       ************
 ************                                                    ************
 ******************************************************************************/
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*!
 *        \file  wdog_ctrl.c
 *      \author  dieter.pfeuffer@men.de
 *
 *       \brief  Control tool for WDOG profile drivers (e.g. Z47)
 *
 *     Required: libraries: mdis_api, usr_oss, usr_utl
 *    \switches  (none)
 */
 /*
 *---------------------------------------------------------------------------
 * Copyright 2016-2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/

/*--------------------------------------+
|  INCLUDES                             |
+--------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <MEN/men_typs.h>
#include <MEN/mdis_api.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/wdog.h>

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
#define ERR_OK		0
#define ERR_PARAM	1
#define ERR_FUNC	2

#define PRINT_ERR	printf("*** error: %s\n", M_errstring(UOS_ErrnoGet()));

/*--------------------------------------+
|   GLOBALS                             |
+--------------------------------------*/
static MDIS_PATH G_path;
static u_int32 G_sigCount = 0;
static int32 G_rst;

/*--------------------------------------+
|  PROTOTYPES                           |
+--------------------------------------*/
static void usage(void);
static int PrintError(char *info);
static int GetInfo( void );
static void __MAPILIB SignalHandler( u_int32 sig );

/********************************* usage ***********************************/
/**  Print program usage
 */
static void usage(void)
{
	printf("Usage:    wdog_ctrl <device> <opts> [<opts>]                         \n");
	printf("Function: Control tool for WDOG profile drivers (e.g. Z47)           \n");
	printf("Options:                                                [default]    \n");
	printf("    device     device name (e.g. wdog_1)                             \n");
	printf("    -g         get watchdog info                                     \n");
	printf("    -r         reset wdog (counter, output/irq pin)                  \n");
	printf("    -c         clear reason of last output/irq pin assertion         \n");
	printf("               -------------- Time Setting -------------------       \n");
	printf("    -u=<ms>    set wdog max/upper time [ms]                          \n");
	printf("                 0 disables the upper limit                          \n");
	printf("    -l=<ms>    set wdog min/lower time [ms]                          \n");
	printf("                 0 disables the lower limit                          \n");
	printf("    -q=<ms>    set wdog irq time [ms]                                \n");
	printf("                 0 disables the irq usage                            \n");
	printf("               -------------- Manual Switching -------------------   \n");
	printf("    -o=<0,1>   0=clear, 1=set output pin                             \n");
	printf("    -i=<0,1>   0=clear, 1=set irq pin                                \n");
	printf("    -e=<0,1>   0=clear, 1=set error pin                              \n");
	printf("               -------------- Loop Operations -------------------    \n");
	printf("    -T=<ms>    start wdog, trigger all <ms> until keypress, stop wdog\n");
	printf("    -P=<ms>    same as -T but trigger with alternating pattern       \n");
	printf("    -I=<ms>    increment trigger time at each loop pass [0]          \n");
	printf("    -R=<ms>    reset wdog at irq signal after <ms>                   \n");
	printf("    -A=<n>     abort after n passes                                  \n");
	printf("    -V         verbose output                                        \n");
	printf("\n");
	printf("Copyright 2016-2019, MEN Mikro Elektronik GmbH\n%s\n", IdentString);
}

/***************************************************************************/
/** Program main function
 *
 *  \param argc       \IN  argument counter
 *  \param argv       \IN  argument vector
 *
 *  \return           success (0) or error code
 */
int main(int argc, char *argv[])
{
	char	*device, *str, *errstr, buf[40];
	u_int32	count = 0;
	int32	get, reset, clear, maxT, minT, irqT, outP, irqP, errP;
	int32	trig, trigPat, trigT, incrT, pat, patIdx=0;
	int32	abort, loop, loopcnt, verbose;
	int		n;

	int		ret=ERR_OK;

	/*----------------------+
	|  check arguments      |
	+----------------------*/
	if ((errstr = UTL_ILLIOPT("grcu=l=q=o=i=e=T=P=I=R=A=V?", buf))) {
		printf("*** %s\n", errstr);
		return ERR_PARAM;
	}
	if (UTL_TSTOPT("?")) {
		usage();
		return ERR_PARAM;
	}
	if (argc < 3) {
		usage();
		return ERR_PARAM;
	}

	/*----------------------+
	|  get arguments        |
	+----------------------*/
	for (device = NULL, n=1; n<argc; n++) {
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}
	}
	if (!device) {
		usage();
		return ERR_PARAM;
	}

	get     = (UTL_TSTOPT("g") ? 1 : 0);
	reset   = (UTL_TSTOPT("r") ? 1 : 0);
	clear   = (UTL_TSTOPT("c") ? 1 : 0);
	maxT    = ((str = UTL_TSTOPT("u=")) ? atoi(str) : -1);
	minT    = ((str = UTL_TSTOPT("l=")) ? atoi(str) : -1);
	irqT    = ((str = UTL_TSTOPT("q=")) ? atoi(str) : -1);
	outP    = ((str = UTL_TSTOPT("o=")) ? atoi(str) : -1);
	irqP    = ((str = UTL_TSTOPT("i=")) ? atoi(str) : -1);
	errP    = ((str = UTL_TSTOPT("e=")) ? atoi(str) : -1);
	trig    = ((str = UTL_TSTOPT("T=")) ? atoi(str) : -1);
	trigPat = ((str = UTL_TSTOPT("P=")) ? atoi(str) : -1);
	incrT   = ((str = UTL_TSTOPT("I=")) ? atoi(str) : 0);
	G_rst   = ((str = UTL_TSTOPT("R=")) ? atoi(str) : -1);
	abort   = ((str = UTL_TSTOPT("A=")) ? atoi(str) : -1);
	verbose = (UTL_TSTOPT("V") ? 1 : 0);

	/* further parameter checking */
	if ((trig != -1) && (trigPat != -1)) {
		printf("*** -T and -P specified, this is not supported\n");
		return ERR_PARAM;
	}

	/* determine trigger time */
	trigT = -1;
	if (trig != -1)
		trigT = trig;
	else if (trigPat != -1)
		trigT = trigPat;

	/* further parameter checking */
	if ((incrT != 0) && (trigT == -1)) {
		printf("*** -I requires -T/-P\n");
		return ERR_PARAM;
	}
	if ((G_rst != -1) && ((trigT == -1) || (irqT == 0) )) {
		printf("*** -R requires -T/-P and -q>0\n");
		return ERR_PARAM;
	}

	/*----------------------+
	|  open path            |
	+----------------------*/
	if ((G_path = M_open(device)) < 0) {
		return PrintError("open");
	}

	/*----------------------+
	|  reset                |
	+----------------------*/
	if (reset){
		if ((M_setstat(G_path, WDOG_RESET_CTRL ,0)) < 0) {
			ret = PrintError("setstat WDOG_RESET_CTRL");
			goto ABORT;
		}
	}

	/*----------------------+
	|  clear reason         |
	+----------------------*/
	if (clear) {
		if ((M_setstat(G_path, WDOG_OUT_REASON, 0)) < 0) {
			ret = PrintError("setstat WDOG_OUT_REASON");
		}
		if ((M_setstat(G_path, WDOG_IRQ_REASON, 0)) < 0) {
			ret = PrintError("setstat WDOG_IRQ_REASON");
		}
		if (ret!=ERR_OK)
			goto ABORT;
	}

	/*----------------------+
	|  time setting [ms]    |
	+----------------------*/
	if (maxT != -1) {
		if ((M_setstat(G_path, WDOG_TIME_MAX, maxT * 1000)) < 0) {
			PrintError("setstat WDOG_TIME_MAX");

			/* try to set max time with older setstat code */
			if ((M_setstat(G_path, WDOG_TIME, maxT)) < 0) {
				ret = PrintError("setstat WDOG_TIME");
				goto ABORT;
			}

			printf("max time set with older setstat code WDOG_TIME\n");
		}
	}

	if (minT != -1) {
		if ((M_setstat(G_path, WDOG_TIME_MIN, minT * 1000)) < 0) {
			ret = PrintError("setstat WDOG_TIME_MIN");
			goto ABORT;
		}
	}

	if (irqT != -1) {
		if ((M_setstat(G_path, WDOG_TIME_IRQ, irqT * 1000)) < 0) {
			ret = PrintError("setstat WDOG_TIME_IRQ");
			goto ABORT;
		}
	}

	/*----------------------+
	|  manual switching     |
	+----------------------*/
	if (outP != -1) {
		if ((M_setstat(G_path, WDOG_OUT_PIN, outP)) < 0) {
			ret = PrintError("setstat WDOG_OUT_PIN");
			goto ABORT;
		}
	}

	if (irqP != -1) {
		if ((M_setstat(G_path, WDOG_IRQ_PIN, irqP)) < 0) {
			ret = PrintError("setstat WDOG_IRQ_PIN");
			goto ABORT;
		}
	}

	if (errP != -1) {
		if ((M_setstat(G_path, WDOG_ERR_PIN, errP)) < 0) {
			ret = PrintError("setstat WDOG_ERR_PIN");
			goto ABORT;
		}
	}

	/*----------------------+
	|  configure interrupt  |
	+----------------------*/
	if (irqT != -1){
					
		/* install signal handler */
		UOS_SigInit( SignalHandler );
		UOS_SigInstall( UOS_SIG_USR1 );
			
		if ((M_setstat(G_path, WDOG_IRQ_SIGSET, UOS_SIG_USR1)) < 0) {
			ret = PrintError("setstat WDOG_IRQ_SIGSET");
			goto ABORT;
		}	

		/* enable interrupt */
		if ((M_setstat(G_path, M_MK_IRQ_ENABLE, TRUE)) < 0) {
			ret = PrintError("setstat M_MK_IRQ_ENABLE");
			goto ABORT;
		}
	}

	/*----------------------+
	|  get info             |
	+----------------------*/
	if (get)
		GetInfo();

	/*--------------------+
	|  watch              |
	+--------------------*/
	loopcnt = 0;
	loop = 1;
	if (trigT != -1){

		/* trigger with pattern */
		if (trigPat != -1) {

			/* get last used pattern */
			if ((M_getstat(G_path, WDOG_TRIG_PAT, &pat)) < 0) {
				PrintError("getstat WDOG_TRIG_PAT");
				goto ABORT;
			}

			/* compute initial pattern to use */
			if( pat == WDOG_TRIGPAT(0))
				patIdx = 1;
			else
				patIdx = 0;
		}

		/* start watchdog */
		if ((M_setstat(G_path, WDOG_START, 0)) < 0) {
			PrintError("setstat WDOG_START");
			goto ABORT;
		}
		printf("Watchdog started - trigger all %dmsec\n", trigT);

		/* trigger loop */
		do {
			UOS_Delay(trigT);
			count++;

			/* trigger with pattern */
			if (trigPat != -1){
				pat = WDOG_TRIGPAT(patIdx);
				if (verbose)
					printf("#%06d: Trigger watchdog with pattern 0x%x after %dms (press any key to abort)\n",
						count, pat, trigT);
				if ((M_setstat(G_path, WDOG_TRIG_PAT, pat)) < 0) {
					PrintError("setstat WDOG_TRIG_PAT");
					goto ABORT;
				}
				patIdx ^= 1;
			}
			/* trigger without pattern */
			else {
				if (verbose)
					printf("#%06d: Trigger watchdog after %dms (press any key to abort)\n",
						count, trigT);
				if ((M_setstat(G_path, WDOG_TRIG, 0)) < 0) {
					PrintError("setstat WDOG_TRIG");
					goto ABORT;
				}
			}

			if (!verbose) {
				printf(".");
				fflush(stdout);
			}

			/* increment delay for next pass */
			if (incrT)
				trigT += incrT;

			/* repeat until keypress */
			if (UOS_KeyPressed() != -1)
				loop = 0;

			/* abort after n passes */
			if (abort){
				loopcnt++;
				if (loopcnt==abort)
					loop = 0;
			}
					
		} while (loop);

		if (!verbose)
			printf("\n");

		/* try to stop watchdog */
		if ((M_setstat(G_path, WDOG_STOP, 0)) < 0) {
			PrintError("setstat WDOG_STOP");
			goto ABORT;
		}
		printf("Watchdog stopped\n");
	}

	/*----------------------+
	|  cleanup              |
	+----------------------*/
	ret=ERR_OK;
	
	if (irqT != -1) {
		if ((M_setstat(G_path, WDOG_IRQ_SIGCLR, UOS_SIG_USR1)) < 0) {
			ret = PrintError("setstat WDOG_IRQ_SIGCLR");
			goto ABORT;
		}

		UOS_SigRemove(UOS_SIG_USR1);
		UOS_SigExit();
	}

	ret = ERR_OK;

ABORT:
	if (M_close(G_path) < 0)
		ret = PrintError("close");

	return ret;
}

/***************************************************************************/
/** Read and print input values
*
*  \return           success (0) or error code
*/
static int GetInfo(void)
{
	int32 val;
	char *str;

	/* ----------- codes before 2016 ----------- */
	printf("WDOG_TIME (MAX time)                  : ");
	if ((M_getstat(G_path, WDOG_TIME, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("%dms\n", val);
	}

	printf("WDOG_STATUS (counter state)           : ");
	if ((M_getstat(G_path, WDOG_STATUS, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("%s\n", val ? "enabled" : "disabled");
	}

	printf("WDOG_SHOT (shot info)                 : ");
	if ((M_getstat(G_path, WDOG_SHOT, &val)) < 0) {
		PRINT_ERR
	}
	else {
		switch (val) {
			case   0: str = "no wdog shot"; break;
			case   1: str = "wdog shot"; break;
			case 255: str = "not identifiable"; break;
			default : str = "*** illegal value";
		}
		printf("%s\n", str);
	}

	/* ----------- additional codes since 2016 ----------- */
	printf("WDOG_TRIG_PAT (last used pattern)     : ");
	if ((M_getstat(G_path, WDOG_TRIG_PAT, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("0x%x\n", val);
	}

	printf("WDOG_TIME_MIN (MIN time)              : ");
	if ((M_getstat(G_path, WDOG_TIME_MIN, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("%dms (%dus)\n", val/1000, val);
	}

	printf("WDOG_TIME_MAX (MAX time)              : ");
	if ((M_getstat(G_path, WDOG_TIME_MAX, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("%dms (%dus)\n", val / 1000, val);
	}

	printf("WDOG_TIME_IRQ (IRQ time)              : ");
	if ((M_getstat(G_path, WDOG_TIME_IRQ, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("%dms (%dus) - may be cleared from drv-exit\n", val / 1000, val);
	}

	printf("WDOG_OUT_PIN (out pin)                : ");
	if ((M_getstat(G_path, WDOG_OUT_PIN, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("%s\n", val ? "set" : "cleared");
	}

	printf("WDOG_OUT_REASON (last out pin reason) : ");
	if ((M_getstat(G_path, WDOG_OUT_REASON, &val)) < 0) {
		PRINT_ERR
	}
	else {
		switch (val) {
		case 0: str = "not triggered"; break;
		case 1: str = "min timeout triggered"; break;
		case 2: str = "max timeout triggered"; break;
		case 3: str = "manually triggered"; break;
		default: str = "*** illegal value";
		}
		printf("%s\n", str);
	}

	printf("WDOG_IRQ_PIN (irq pin)                : ");
	if ((M_getstat(G_path, WDOG_IRQ_PIN, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("%s\n", val ? "set" : "cleared");
	}

	printf("WDOG_IRQ_REASON (last irq pin reason) : ");
	if ((M_getstat(G_path, WDOG_IRQ_REASON, &val)) < 0) {
		PRINT_ERR
	}
	else {
		switch (val) {
		case 0: str = "not triggered"; break;
		case 2: str = "irq timeout triggered"; break;
		case 3: str = "manually triggered"; break;
		default: str = "*** illegal value";
		}
		printf("%s\n", str);
	}

	printf("WDOG_ERR_PIN (err pin)                : ");
	if ((M_getstat(G_path, WDOG_ERR_PIN, &val)) < 0) {
		PRINT_ERR
	}
	else {
		printf("%s\n", val ? "set" : "cleared");
	}

	return ERR_OK;
}

/***************************************************************************/
/** Signal handler
*
*  \param  sig    \IN   received signal
*/
static void __MAPILIB SignalHandler(u_int32 sig)
{
	if (sig == UOS_SIG_USR1) {
		++G_sigCount;

		printf("==> interrupt signal #%d received\n", G_sigCount);

		if (G_rst != -1) {
			UOS_Delay(G_rst);
			M_setstat(G_path, WDOG_RESET_CTRL, 0);
			printf("    watchdog reset after %dms\n", G_rst);
		}
	}
}

/***************************************************************************/
/** Print MDIS error message
 *
 *  \param info       \IN  info string
 *
 *  \return           ERR_FUNC
 */
static int PrintError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring (UOS_ErrnoGet()));
	return ERR_FUNC;
}


