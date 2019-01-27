# RPIpubMqttDaemon for Raspberry PI

A daemon written in C for reading system data from Raspberry PI. The read data will be published to an MQTT server with a given topic.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine. 

### Hardware prerequisites

You need a [Raspberry PI board](http://www.raspberrypi.org/).

### Software prerequisites

The daemon uses 2 non standard libraries which must be installed first:

**libconfig-dev** - This library allows parsing, manipulating and writing structured configuration files.
``` code
apt-get install libconfig-dev
```
**libmosquitto-dev** - Library for implementing MQTT version 3.1/3.1.1 clients.
``` code
apt-get install libmosquitto-dev
```
**MQTT server** - MQTT server running and reachable form the Raspberry.

### Installing
1. Build the executable file by running "make"
``` code
cd ~/rpipubmqttd
make
```
2. Edit the config file rpipubmqttd.conf according to your needs
``` code
nano rpipubmqttd.conf
```
3. Copy the binary rpipubmqttd and the config file rpipubmqttd.conf to /opt/rpipubmqttd/
``` code
sudo mkdir /opt/rpipubmqttd
sudo cp rpipubmqttd /opt/rpipubmqttd
sudo cp rpipubmqttd.conf /opt/rpipubmqttd
```
4. Copy the service configuration file rpipubmqttd.service to /lib/systemd/system/
``` code
sudo cp rpipubmqttd.service /lib/systemd/system/
```
5. Enable RPIpubMqttDaemon service
``` code
sudo systemctl enable rpipubmqttd.service
```
6. Start RPIpubMqttDaemon.service
``` code
sudo systemctl start rpipubmqttd.service
```
7. check syslog for errors
``` code
sudo tail -f /var/log/syslog
```
8. Start an MQTT client and subscripe the topic configured in the config file. You should get a string in JSON format with the read data.
``` code
{"CPU_Temp":"45.08","Disk_Usage":"20.10"}
```


## License

This project is licensed under the GNU General Public License - see the [LICENSE](LICENSE) file for details


