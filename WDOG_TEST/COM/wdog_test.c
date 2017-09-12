/****************************************************************************
 ************                                                    ************
 ************                    WDOG_TEST                       ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: ds
 *        $Date: 2009/08/31 12:17:57 $
 *    $Revision: 1.5 $
 *
 *  Description: Configure and serve Watchdog
 *
 *     Required: libraries: mdis_api, usr_oss, usr_utl
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: wdog_test.c,v $
 * Revision 1.5  2009/08/31 12:17:57  MRoth
 * R: Porting to MIDS5
 * M: added support for 64bit (MDIS_PATH)
 *
 * Revision 1.4  2004/09/02 15:14:15  dpfeuffer
 * minor modifications for MDIS4/2004 conformity
 *
 * Revision 1.3  1999/08/23 11:08:23  Schmidt
 * cosmetics
 *
 * Revision 1.2  1999/08/03 16:43:28  Schmidt
 * option -o (GetStat WDOG_SHOT) added
 *
 * Revision 1.1  1999/06/23 12:07:46  Schmidt
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1999 by MEN mikro elektronik GmbH, Nuremberg, Germany
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

#include <stdio.h>
#include <stdlib.h>
#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/wdog.h>

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void usage(void);
static void PrintMdisError(char *info);


/********************************* usage ************************************
 *
 *  Description: Print program usage
 *
 *---------------------------------------------------------------------------
 *  Input......: -
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void usage(void)
{
	printf("Usage: wdog_test [<opts>] <device> [<opts>]\n");
	printf("Function: Configure and serve Watchdog\n");
	printf("Options:\n");
	printf("  device     device name.............................. [none]\n");
	printf("  -w=<msec>  Start watchdog and trigger all msec. .... [none]\n");
	printf("             On keypress the trigger will be aborted         \n");
	printf("             and the watchdog be stopped (if supported       \n");
	printf("             by the used driver).                            \n");
	printf("             !!! CAUTION:                             !!!    \n");
	printf("             !!! THE SYSTEM WILL BE RESET IF THE      !!!    \n");
	printf("             !!! SPECIFIED TRIGGER TIME IS LONGER THAN!!!    \n");
	printf("             !!! THE WATCHDOG TIME OR IF THE WATCHDOG !!!    \n");
	printf("             !!! CANNOT BE STOPPED                    !!!    \n");
	printf("  -t=<msec>  Test watchdog time. ..................... [none]\n");
	printf("             The watchdog will be started and triggered.     \n");
	printf("             The time between the triggers will be           \n");
	printf("             increasing from msec up to x msec on which      \n");
	printf("             the watchdog causes a system reset.             \n");
	printf("             !!! CAUTION:                             !!!    \n");
	printf("             !!! THE SYSTEM WILL BE RESET DEFINITELY  !!!    \n");
	printf("  -s=<msec>  set watchdog-time to <msec> msec......... [none]\n");
	printf("             disable the watchdog if <msec> is 0 (-s=0)      \n");
	printf("  -g         get watchdog-time........................ [none]\n");
	printf("  -i         get watchdog-status...................... [none]\n");
	printf("  -o         check if watchdog had shot off the system [none]\n");
	printf("\n");
	printf("(c) 1999 by MEN mikro elektronik GmbH\n\n");
}

/********************************* main *************************************
 *
 *  Description: Program main function
 *
 *---------------------------------------------------------------------------
 *  Input......: argc,argv	argument counter, data ..
 *  Output.....: return	    success (0) or error (1)
 *  Globals....: -
 ****************************************************************************/
int main(int argc, char *argv[])
{
	MDIS_PATH path=0;
	int32	n, count=0,incrTime=1;
	int32	trigTime,testTime,setTime,getTime,status,shot;
	char	*device,*str,*errstr,buf[40];

	/*--------------------+
    |  check arguments    |
    +--------------------*/
	if ((errstr = UTL_ILLIOPT("w=t=s=gio?", buf))) {	/* check args */
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {						/* help requested ? */
		usage();
		return(1);
	}

	/*--------------------+
    |  get arguments      |
    +--------------------*/
	for (device=NULL, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if ( (!device) || (argc < 3) ) {
		usage();
		return(1);
	}

	trigTime = ((str = UTL_TSTOPT("w=")) ? atoi(str) : 0);
	testTime = ((str = UTL_TSTOPT("t=")) ? atoi(str) : 0);
	setTime  = ((str = UTL_TSTOPT("s=")) ? atoi(str) : -1);
	getTime  = (UTL_TSTOPT("g") ? 1 : 0);
	status   = (UTL_TSTOPT("i") ? 1 : 0);
	shot     = (UTL_TSTOPT("o") ? 1 : 0);

	/*--------------------+
    |  open path          |
    +--------------------*/
	if ((path = M_open(device)) < 0) {
		PrintMdisError("open");
		return(1);
	}

	/*--------------------+
    |  get wdog-status    |
    +--------------------*/
	if (status) {
		if ((M_getstat(path, WDOG_STATUS, &status)) < 0) {
			PrintMdisError("getstat WDOG_STATUS");
			goto abort;
		}
		printf("Watchdog is %s\n", status ? "active" : "inactive");
	}

	/*--------------------+
    |  get shot info      |
    +--------------------*/
	if (shot) {
		if ((M_getstat(path, WDOG_SHOT, &shot)) < 0) {
			PrintMdisError("getstat WDOG_SHOT");
			goto abort;
		}
		printf("Did the watchdog initiates the last system reset ?\n");
		switch (shot) {
			case   0: printf(" NO\n"); break;
			case   1: printf(" YES\n"); break;
			case 255: printf(" NOT IDENTIFIABLE\n"); break;
			default : printf(" *** ERROR: DRIVER RETURNS AN ILLEGAL VALUE\n"); break;
		}
	}

	/*--------------------+
    |  get wdog-time      |
    +--------------------*/
	if (getTime) {
		if ((M_getstat(path, WDOG_TIME, &getTime)) < 0) {
			PrintMdisError("getstat WDOG_TIME");
			goto abort;
		}
		printf("Watchdog time: %dmsec\n", getTime);
	}

	/*--------------------+
    |  set wdog-time      |
    +--------------------*/
	if (0 < setTime) {
		if ((M_setstat(path, WDOG_TIME, setTime)) < 0) {
			PrintMdisError("setstat WDOG_TIME");
			goto abort;
		}
		printf("Watchdog time set to %dmsec\n", setTime);
	}

	/*--------------------+
    |  stop watchdog      |
    +--------------------*/
	if (0 == setTime) {
		if ((M_setstat(path, WDOG_STOP, 0)) < 0) {
			PrintMdisError("setstat WDOG_STOP");
			goto abort;
		}
		printf("Watchdog stopped\n");
	}

	/*--------------------+
    |  watch              |
    +--------------------*/
	if (trigTime) {
		/* start watchdog */
		if ((M_setstat(path, WDOG_START, 0)) < 0) {
			PrintMdisError("setstat WDOG_START");
			goto abort;
		}
		printf("Watchdog started - trigger all %dmsec\n", trigTime);

		/* trigger loop */
		do {
			UOS_Delay(trigTime);
			count++;
			printf("  (%6d) Trigger watchdog - Press any key to abort\n", count);
			if ((M_setstat(path, WDOG_TRIG, 0)) < 0) {
				PrintMdisError("setstat WDOG_TRIG");
				goto abort;
			}
		} while(UOS_KeyPressed() == -1);

		/* try to stop watchdog */
		if ((M_setstat(path, WDOG_STOP, 0)) < 0) {
			PrintMdisError("setstat WDOG_STOP");
			goto abort;
		}
		printf("Watchdog stopped\n");
	}

	/*--------------------+
    |  test wdog-time     |
    +--------------------*/
	if (testTime) {
		/* start watchdog */
		if ((M_setstat(path, WDOG_START, 0)) < 0) {
			PrintMdisError("setstat WDOG_START");
			goto abort;
		}
		printf("Watchdog started - force system reset\n", trigTime);

		/* trigger loop */
		do {
			UOS_Delay(testTime);
			printf("  Trigger watchdog after %6dmsec\n", testTime);
			if ((M_setstat(path, WDOG_TRIG, 0)) < 0) {
				PrintMdisError("setstat WDOG_TRIG");
				goto abort;
			}
			if (testTime >=   100) incrTime =   10;
			if (testTime >=  1000) incrTime =  100;
			if (testTime >= 10000) incrTime = 1000;
			testTime += incrTime;
		} while(1);
	}

	/*--------------------+
    |  cleanup            |
    +--------------------*/
	abort:
	if (M_close(path) < 0)
		PrintMdisError("close");

	return(0);
}

/********************************* PrintMdisError ***************************
 *
 *  Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintMdisError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}


