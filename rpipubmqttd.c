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
#include <sys/sysinfo.h>
#include <string.h>

#include <libconfig.h>      /* apt-get install libconfig-dev */
#include <mosquitto.h>      /* apt-get install libmosquitto-dev */ 

#include "rpipubmqttd.h" 


static char version[]   = "RPIpubMqttDaemon v1.10 06/27/2020";


/* ------------------------------------------------------------------- 
 * MQTT parameters 
 * ------------------------------------------------------------------- */

struct mosq_config cfg;             /* holds the client configuration  */

struct mosquitto *mosq = NULL;      /* handle for the mosqitto library */

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


int         mqttConnectretries = 5 ;
char text[128];

struct sysinfo systemInfo ;

/*--------------------------------------------------------------------------------------------------
    ExitDaemon()
    
---------------------------------------------------------------------------------------------------*/
void ExitDaemon()
{
    syslog( LOG_NOTICE, "disconnecting from server ..\n");
    mosquitto_disconnect(mosq);                     /* Disconnect from MQTT */
    mosquitto_loop_stop(mosq, true) ;               /* stop the network thread previously created with mosquitto_loop_start. */

    mosquitto_destroy (mosq);
    mosquitto_lib_cleanup();

    syslog( LOG_NOTICE, "clear lock ..\n");
    close( fpLockfile ) ;                           /* Remove the Lock file */
    remove( LOCK_FILE ) ;

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
        syslog( LOG_NOTICE, "SIGTERM received!\n");
        ExitDaemon() ;
        return;
    }

    else if(sig == SIGHUP)                       /* kill -1 reload the configuration files, if this applies */
    {
        syslog( LOG_NOTICE, "SIGHUP received!\n" ) ;
        return;
    }
    else {
        syslog( LOG_NOTICE, "unhandled signal received!\n" ) ;
	    return ;
    }
}


/*--------------------------------------------------------------------------------------------------
    readConfigFile() 
---------------------------------------------------------------------------------------------------*/
int readConfigFile() 
{
    config_t lib_cfg ; /* *lib_cf; */

    //lib_cf = &lib_cfg;                              /* Init libconfig to read config file */

    config_init(&lib_cfg);

    const char *strBuffer = NULL ;
    int  tmp = 0 ;
    

    if (!config_read_file(&lib_cfg, "/opt/rpipubmqttd/rpipubmqttd.conf")) {
        syslog(LOG_ERR, "error reading config %s line %d: %s", config_error_file(&lib_cfg), config_error_line(&lib_cfg), config_error_text(&lib_cfg));
        config_destroy(&lib_cfg);
        return(EXIT_FAILURE);
    }

    /* */
    config_lookup_int(&lib_cfg, "rpi_readinterval", &rpi_readinterval) ;
    config_lookup_int(&lib_cfg, "log_level", &loglevel) ;
    config_lookup_int(&lib_cfg, "mqtt.connect_retries", &mqttConnectretries ) ;

    /* MQTT client configuration */
    config_lookup_int(&lib_cfg, "mqtt.port", &cfg.port) ;
    config_lookup_bool(&lib_cfg, "mqtt.retain", &tmp) ;
    cfg.retain = tmp ;

    config_lookup_string(&lib_cfg, "mqtt.user_pw", &strBuffer) ;
    cfg.password = strdup(strBuffer);

    config_lookup_string(&lib_cfg, "mqtt.user", &strBuffer) ;
    cfg.username = strdup(strBuffer);

    config_lookup_string(&lib_cfg, "mqtt.host", &strBuffer) ;
    cfg.host = strdup(strBuffer);

    config_lookup_string(&lib_cfg, "mqtt.topic", &strBuffer) ;
    cfg.topic = strdup(strBuffer);

    config_destroy(&lib_cfg) ;                        /* release config  */

    return(EXIT_SUCCESS) ;
}

/*--------------------------------------------------------------------------------------------------
    Init_mosq_config()  Init mosq_config with default values
---------------------------------------------------------------------------------------------------*/
void Init_mosq_config(struct mosq_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = -1;
    cfg->keepalive = 60;
    cfg->clean_session = true;
    cfg->protocol_version = MQTT_PROTOCOL_V311;
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

unsigned long RAMfree()
{
 //printf( "Free Ram : %ld\n" ,systemInfo.freeram / 1024L ) ;
 return( systemInfo.freeram / 1024L ) ;
}

unsigned long RAMused()
{
 //printf( "Used Ram : %ld\n" ,(systemInfo.totalram - systemInfo.freeram) / 1024L ) ;
 return( (systemInfo.totalram - systemInfo.freeram) / 1024L ) ;
}


unsigned long SWAPused()
{
 //printf( "Used Swap: %ld\n" ,(systemInfo.totalswap - systemInfo.freeswap) / 1024L ) ;
 return( (systemInfo.totalswap - systemInfo.freeswap) / 1024L ) ;
}


char *Uptime()
{

    int days, hours, mins ;
    static char strText[128];

 //printf( "Processes : %d\n"  ,systemInfo.procs ) ;
 //printf( "Uptime    : %ld\n"  ,systemInfo.uptime ) ;

    days  =   systemInfo.uptime / 86400;                       // 24 x 60 x 60
    hours = ( systemInfo.uptime / 3600) - (days * 24);
    mins  = ( systemInfo.uptime / 60) - (days * 1440) - (hours * 60);

    sprintf( strText, "%dd%2dh%2dm",days, hours, mins);
    return( strText) ;
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

    /* ---- Get all program parameter   ----------*/
    Init_mosq_config( &cfg ) ;                      /* Init Mosquitto client configuration default values */

    if ( readConfigFile() != EXIT_SUCCESS)          /* Read the configuration file */
    {
        syslog(LOG_ERR, "read config file failed");
        exit(EXIT_FAILURE);
    }

    syslog(LOG_NOTICE, "read config file\n");
    
    /* ---- Setup the connection to the brocker ----------*/

    mosquitto_lib_init() ;                          /* Initialize the Mosquitto library */
                                                    /* always returns MOSQ_ERR_SUCCESS  */ 


    int major, minor, revision;
    mosquitto_lib_version(&major, &minor, &revision);
    syslog(LOG_NOTICE, "Mosquitto lib version %d.%d.%d\n", major, minor, revision ) ;


    /* Create a new Mosquitto runtime instance with a random client ID, */
    /* and no application-specific callback data.                       */
    mosq = mosquitto_new (cfg.id, cfg.clean_session, NULL);
    if (mosq == NULL )
    {
        syslog(LOG_ERR, "initialising Mosquitto instance failed");
        exit(EXIT_FAILURE);
    }

    if ( mosquitto_username_pw_set (mosq, cfg.username, cfg.password) != MOSQ_ERR_SUCCESS )
    {
        syslog(LOG_ERR, "set user/password for Mosquitto instance failed");
        exit(EXIT_FAILURE);
    }

    /* try to connect to MQTT server */
    int ret ;
    do 
    {
        syslog(LOG_NOTICE, "try connect to Mosquitto server %s ...", cfg.host ) ;

        /* connect to the MQTT server, do not use a keep-alive ping */
        ret = mosquitto_connect_bind_v5(mosq, cfg.host, cfg.port, cfg.keepalive, cfg.bind_address, cfg.connect_props);
        if (ret != MOSQ_ERR_SUCCESS )
        {
            syslog(LOG_NOTICE, "connect to Mosquitto server failed (%s).", mosquitto_strerror(ret) ) ;
            mqttConnectretries-- ;
            sleep ( 1 ) ;
        }  
 
    } while ((ret != MOSQ_ERR_SUCCESS) && mqttConnectretries ) ;

    

    if ( ret == MOSQ_ERR_SUCCESS ) 
    {
        syslog(LOG_NOTICE, "Connected to Mosquitto server: %s.", cfg.host ) ;

        mosquitto_loop_start(mosq) ;    /* this also handels the keepalive handshake with the server ! */

        /*------------------------------------------------------------------*/
        /* the main loop for the daemon                                     */
        /*------------------------------------------------------------------*/
	    while (1)
	    {
		    /* Dont block context switches, let the process sleep for some time */
            //sleep( 10 ) ;

 		    syslog( LOG_NOTICE, "reading data from RPI..\n");
       
            sysinfo( &systemInfo) ;   //get the infos

            sprintf (text, "{\"CPU_Temp\":\"%.2f\",\"Disk_Usage\":\"%.2f\",\"RAM_used\":\"%ld\",\"RAM_free\":\"%ld\",\"SWAP_used\":\"%ld\",\"Uptime\":\"%s\"}", 
             CPUTemp(), DiskUsage(), RAMused(), RAMfree(), SWAPused(), Uptime() );

            syslog( LOG_NOTICE, "publish to mqtt: %s.", text );

            /* Publish the message to the topic */
            ret = mosquitto_publish_v5(mosq, NULL, cfg.topic, strlen(text), text, 0, cfg.retain, cfg.publish_props);
//            ret = mosquitto_publish (mosq, NULL, mqttTopic,strlen (text), text, 0, mqttRetain ) ;

            if (ret)
            {
                syslog(LOG_ERR, "Can't publish to Mosquitto server (%s).", mosquitto_strerror(ret) ) ;
                break ;
            }      
            
            //sleep(5) ;
		    sleep(rpi_readinterval) ;

	    }
    }
    else {

        syslog(LOG_ERR, "Can't connect to Mosquitto server." ) ;

    }
 	syslog( LOG_NOTICE, "Daemon ended.");
    ExitDaemon() ;
	return (EXIT_SUCCESS);
}


