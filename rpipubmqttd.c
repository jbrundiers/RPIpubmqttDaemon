/*
 * This file is part of RPIpubMqttDaemon.
 *
 * RPIpubMqttDaemon is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * RPIpubMqttDaemon is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with waproamd; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <string.h>

#include <libconfig.h>      /* apt-get install libconfig-dev */
#include <mosquitto.h>      /* apt-get install libmosquitto-dev */ 


static char version[]   = "RPIpubMqttDaemon v1.04 01/13/2019";

/* ------------------------------------------------------------------- 
 * libconfig variables 
 * ------------------------------------------------------------------- */
config_t cfg, *cf;

/* ------------------------------------------------------------------- 
 * MQTT Server parameters 
 * ------------------------------------------------------------------- */
const char *mqttHostname ;
int         mqttPort ;
const char *mqttUser; 
const char *mqttUserpassword; 
const char *mqttTopic; 
const bool  mqttRetain ;
int         mqttConnectretries = 5 ;

struct mosquitto *mosq = NULL;              /* handle mosqitto library */

/* ---------------------------------------------------------------------- 
 * Logging parameters 
 * ------------------------------------------------------------------- */
#define DLOG_INFO  1
#define DLOG_DEBUG 2

static int loglevel = DLOG_INFO;

/* ------------------------------------------------------------------- */
/* Daemon parameters */
/* ------------------------------------------------------------------- */
#define DAEMON_NAME "rpipubmqttd" 
#define LOCK_FILE   "/var/run/"DAEMON_NAME".pid"
int rpi_readinterval = 200 ;
int fpLockfile ;                            /* points to the lock file */


char text[128];

/*--------------------------------------------------------------------------------------------------
    ExitDaemon()
    
---------------------------------------------------------------------------------------------------*/
void ExitDaemon()
{

    syslog( LOG_NOTICE, "shuting down ..\n");

    mosquitto_disconnect (mosq);                    /* Disconnect from MQTT */
    mosquitto_destroy (mosq);
    mosquitto_lib_cleanup();

    close( fpLockfile ) ;                           /* Remove the Lock file */
    remove( LOCK_FILE ) ;

    config_destroy(cf) ;                            /* release config  */

    closelog() ;                                    /* disconnect from syslog */
    exit(EXIT_SUCCESS) ;
}


/*--------------------------------------------------------------------------------------------------
    SignalHandler() 
---------------------------------------------------------------------------------------------------*/
void SignalHandler(int sig) 
{
    if(sig == SIGTERM)                           /* kill -15 shut down the daemon and exit cleanly */
    {
        if ( loglevel == DLOG_DEBUG ) syslog( LOG_NOTICE, "SIGTERM received!\n");
        ExitDaemon() ;
        return;
    }

    else if(sig == SIGHUP)                       /* kill -1 reload the configuration files, if this applies */
    {
        if ( loglevel == DLOG_DEBUG ) syslog( LOG_NOTICE, "SIGHUP received!\n" ) ;
        return;
    }
    else {
        if ( loglevel == DLOG_DEBUG ) syslog( LOG_NOTICE, "unhandled signal received!\n" ) ;
	    return ;
    }
}


/*--------------------------------------------------------------------------------------------------
    readConfig() 
---------------------------------------------------------------------------------------------------*/
int readConfig() 
{
    if (!config_read_file(cf, "/opt/rpipubmqttd/rpipubmqttd.conf")) {
        syslog(LOG_ERR, "can't read config file");
        config_destroy(cf);
        return(EXIT_FAILURE);
    }

    config_lookup_int(cf, "rpi_readinterval", &rpi_readinterval) ;
    config_lookup_int(cf, "log_level", &loglevel) ;
    config_lookup_string(cf, "mqtt.host", &mqttHostname) ;
    config_lookup_string(cf, "mqtt.user", &mqttUser) ;
    config_lookup_string(cf, "mqtt.user_pw", &mqttUserpassword) ;
    config_lookup_string(cf, "mqtt.topic", &mqttTopic) ;
    config_lookup_bool(cf, "mqtt.retain", &mqttRetain) ;
    config_lookup_int(cf, "mqtt.port", &mqttPort) ;
    config_lookup_int( cf, "mqtt.connectretries", &mqttConnectretries ) ;

    return(EXIT_SUCCESS) ;
}


double DiskUsage()
{
  struct statvfs buf;
  double usage = 0.0;

  if (!statvfs("/etc/rc.local", &buf)) {
    unsigned long hd_used;
    hd_used = buf.f_blocks - buf.f_bfree;
    usage = ((double) hd_used) / ((double) buf.f_blocks) * 100;
  }

  return( round(usage) ) ;
}

double CPUTemp()
{
  FILE *temperatureFile;
  double T = 0.0;

  temperatureFile = fopen ("/sys/class/thermal/thermal_zone0/temp", "r");
  if (temperatureFile) {
    fscanf (temperatureFile, "%lf", &T);
    fclose (temperatureFile);
  }
  T = T/1000.0;

  return( T ) ; 
}




int main(int argc, char* argv[])
{
    char str[10];

	pid_t process_id = 0;
	pid_t sid = 0;

    fprintf( stdout, "%s\n", version ) ;

	process_id = fork();                            /* Create child process */

	if (process_id < 0) {                           /* Indication of fork() failure */
		fprintf( stderr, "fork failed!\n");
		exit(EXIT_FAILURE);                         /* Return failure in exit status */
	}

	if (process_id > 0) {                           /* Success: Let the parent terminate */
		if ( loglevel == DLOG_DEBUG ) 
            fprintf( stdout, "process_id of child process %d \n", process_id);

		exit(EXIT_SUCCESS);                                    /* return success in exit status */
	}

    /* ------------------------------------------------------------------------------
     * child (daemon) continues 
     * ------------------------------------------------------------------------------ */
	sid = setsid();                                 /* The child process becomes session leader */
	if(sid < 0) {
	    exit(EXIT_FAILURE);                         /* Return failure */
	}

    umask(0);                                       /* unmask the file mode */
	chdir("/");                                     /* Change the current working directory to root. */
    for (int x = sysconf(_SC_OPEN_MAX); x>=0; x--){ /* Close all open file descriptors */
        close (x);
    }

                                                    /* Create the lock file */
    fpLockfile = open( LOCK_FILE, O_RDWR|O_CREAT, 0640 ); 
    if ( fpLockfile < 0 ) 
        exit(EXIT_FAILURE);                         /* can not open */

    if ( lockf( fpLockfile, F_TLOCK, 0 ) < 0 ) 
        exit(EXIT_FAILURE);                         /* can not lock */

    sprintf(str,"%d\n",getpid());                   /* record pid to lockfile */
    write(fpLockfile,str,strlen(str)); 

	signal(SIGTERM,SignalHandler);		            /* shut down the daemon and exit cleanly */
	signal(SIGHUP,SignalHandler);			        


	                                                /* open syslog to log some messages */
	openlog ( DAEMON_NAME, LOG_PID | LOG_CONS| LOG_NDELAY, LOG_LOCAL0 );


 	syslog( LOG_NOTICE, "Daemon started.\n");

    
    cf = &cfg;                                      /* Init libconfig */
    config_init(cf);

    if ( readConfig() != EXIT_SUCCESS) 
        exit(EXIT_FAILURE);

    mosquitto_lib_init();                           /* Initialize the Mosquitto library */

    /* Create a new Mosquitto runtime instance with a random client ID, */
    /* and no application-specific callback data.                       */
    mosq = mosquitto_new (NULL, true, NULL);
    if (!mosq)
    {
        syslog(LOG_ERR, "Initialising  Mosquitto library failed");
        exit(EXIT_FAILURE);
    }

    mosquitto_username_pw_set (mosq, mqttUser, mqttUserpassword);


    /* try to connect to MQTT server */
    int ret ;
    do 
    {
        syslog(LOG_NOTICE, "try connect to Mosquitto server...." ) ;

        /* connect to the MQTT server, do not use a keep-alive ping */
        ret = mosquitto_connect (mosq, mqttHostname, mqttPort, 0); 
        if (ret)
        {
            syslog(LOG_NOTICE, "connect to Mosquitto server failed." ) ;
            mqttConnectretries-- ;
            sleep ( 1 ) ;
        }  
 
    } while ((ret != 0) && mqttConnectretries ) ;

    if ( ! mqttConnectretries ) 
    {
            syslog(LOG_ERR, "Can't connect to Mosquitto server." ) ;
            exit(EXIT_FAILURE);
    }
    else
        syslog(LOG_NOTICE, "Connected to Mosquitto server." ) ;

    /*------------------------------------------------------------------*/
    /* the main loop for the daemon                                     */
    /*------------------------------------------------------------------*/
	while (1)
	{
		/* Dont block context switches, let the process sleep for some time */
        sleep( 1 ) ;

 		if ( loglevel == DLOG_DEBUG ) syslog( LOG_NOTICE, "reading data from RPI..\n");

       
        sprintf (text, "{\"CPU_Temp\":\"%.2f\",\"Disk_Usage\":\"%.2f\"}", CPUTemp(), DiskUsage());

        if ( loglevel == DLOG_DEBUG ) syslog( LOG_NOTICE, text );

        /* Publish the message to the topic */
        int ret = mosquitto_publish (mosq, NULL, mqttTopic,strlen (text), text, 0, mqttRetain ) ;

        if (ret)
        {
             syslog(LOG_ERR, "Can't publish to Mosquitto server." ) ;
        }

		sleep(rpi_readinterval) ;

	}

 	syslog( LOG_NOTICE, "Daemon ended.");
	return (EXIT_SUCCESS);
}

