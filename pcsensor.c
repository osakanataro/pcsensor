/*
 * pcsensor.c by Juan Carlos Perez (c) 2011 (cray@isp-sl.com)
 * based on Temper.c by Robert Kavaler (c) 2009 (relavak.com)
 * All rights reserved.
 *
 * Temper driver for linux. This program can be compiled either as a library
 * or as a standalone program (-DUNIT_TEST). The driver will work with some
 * TEMPer usb devices from RDing (www.PCsensor.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY Juan Carlos Perez ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Robert kavaler BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */



#include <usb.h>
#include <stdio.h>
#include <time.h>

#include <string.h>
#include <errno.h>
#include <signal.h> 
 
 
#define VERSION "1.0.3"
 
#define VENDOR_ID  0x0c45
#define PRODUCT_ID 0x7401
 
#define INTERFACE1 0x00
#define INTERFACE2 0x01

/* If you want to support more devices, please change following */
#define MAX_DEV 4
/* If you want to support more sensors on 1device, please change following */
#define MAX_SENSOR 2
 
const static int reqIntLen=8;
const static int reqBulkLen=8;
const static int endpoint_Int_in=0x82; /* endpoint 0x81 address for IN */
const static int endpoint_Int_out=0x00; /* endpoint 1 address for OUT */
const static int endpoint_Bulk_in=0x82; /* endpoint 0x81 address for IN */
const static int endpoint_Bulk_out=0x00; /* endpoint 1 address for OUT */
const static int timeout=5000; /* timeout in ms */
 
const static char uTemperatura[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni1[] = { 0x01, 0x82, 0x77, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni2[] = { 0x01, 0x86, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };

static int bsalir=1;
static int debug=0;
static int seconds=5;
static int formato=0;
static int mrtg=0;
static int calibration=0;
static int devlist=0;
static int devnum=-1;

static usb_dev_handle *handles[MAX_DEV];
static char *devlist_bus[MAX_DEV];
static char *devlist_device[MAX_DEV];

void bad(const char *why) {
        fprintf(stderr,"Fatal error> %s\n",why);
        exit(17);
}
 
 
int find_lvr_winusb();
 
void usb_detach(usb_dev_handle *lvr_winusb, int iInterface) {
        int ret;
 
	ret = usb_detach_kernel_driver_np(lvr_winusb, iInterface);
	if(ret) {
		if(errno == ENODATA) {
			if(debug) {
				printf("Device already detached\n");
			}
		} else {
			if(debug) {
				printf("Detach failed: %s[%d]\n",
				       strerror(errno), errno);
				printf("Continuing anyway\n");
			}
		}
	} else {
		if(debug) {
			printf("detach successful\n");
		}
	}
} 

int setup_libusb_access() {
     usb_dev_handle *lvr_winusb[MAX_DEV];
     int i = 0;

     if(debug) {
        usb_set_debug(255);
     } else {
        usb_set_debug(0);
     }
     usb_init();
     usb_find_busses();
     usb_find_devices();
             
 
     if(!find_lvr_winusb()) {
                printf("Couldn't find the USB device, Exiting\n");
                return 0;
        }
        
       
     for (i = 0; handles[i] != NULL && i < MAX_DEV; i++) {
            usb_detach(handles[i], INTERFACE1);
        

            usb_detach(handles[i], INTERFACE2);
        
 
            if (usb_set_configuration(handles[i], 0x01) < 0) {
                printf("Could not set configuration 1 on device %d\n", i);
                return 0;
            }
 

        // Microdia tiene 2 interfaces
        if (usb_claim_interface(handles[i], INTERFACE1) < 0) {
                printf("Could not claim interface\n");
                return 0;
        }
 
        if (usb_claim_interface(handles[i], INTERFACE2) < 0) {
                printf("Could not claim interface\n");
                return 0;
        }
     }
 
     return i;
}
 
 
 
int find_lvr_winusb() {
 
     struct usb_bus *bus;
        struct usb_device *dev;
	int i;

	memset(handles, 0, sizeof(handles)); 
	i = 0;
        for (bus = usb_busses; bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
                        if (dev->descriptor.idVendor == VENDOR_ID && 
                                dev->descriptor.idProduct == PRODUCT_ID ) {
                                usb_dev_handle *handle;
                                if(debug) {
                                  printf("lvr_winusb with Vendor Id: %x and Product Id: %x found.\n", VENDOR_ID, PRODUCT_ID);
                                }
 
                                if (!(handle = usb_open(dev))) {
                                        printf("Could not open USB device\n");
                                        continue;
                                }
                                //handles[i++] = handle;
                                handles[i] = handle;
				devlist_bus[i] = bus->dirname;
				devlist_device[i] = dev->filename;
				i++;
				if (i == MAX_DEV)
					break;
                        }
                }
        }
        return i;
}
 
 
void ini_control_transfer(usb_dev_handle *dev) {
    int r,i;

    char question[] = { 0x01,0x01 };

    r = usb_control_msg(dev, 0x21, 0x09, 0x0201, 0x00, (char *) question, 2, timeout);
    if( r < 0 )
    {
          perror("USB control write"); bad("USB write failed"); 
    }


    if(debug) {
      for (i=0;i<reqIntLen; i++) printf("%02x ",question[i] & 0xFF);
      printf("\n");
    }
}
 
void control_transfer(usb_dev_handle *dev, const char *pquestion) {
    int r,i;

    char question[reqIntLen];
    
    memcpy(question, pquestion, sizeof question);

    r = usb_control_msg(dev, 0x21, 0x09, 0x0200, 0x01, (char *) question, reqIntLen, timeout);
    if( r < 0 )
    {
          perror("USB control write"); bad("USB write failed"); 
    }

    if(debug) {
        for (i=0;i<reqIntLen; i++) printf("%02x ",question[i]  & 0xFF);
        printf("\n");
    }
}

void interrupt_transfer(usb_dev_handle *dev) {
 
    int r,i;
    char answer[reqIntLen];
    char question[reqIntLen];
    for (i=0;i<reqIntLen; i++) question[i]=i;
    r = usb_interrupt_write(dev, endpoint_Int_out, question, reqIntLen, timeout);
    if( r < 0 )
    {
          perror("USB interrupt write"); bad("USB write failed"); 
    }
    r = usb_interrupt_read(dev, endpoint_Int_in, answer, reqIntLen, timeout);
    if( r != reqIntLen )
    {
          perror("USB interrupt read"); bad("USB read failed"); 
    }

    if(debug) {
       for (i=0;i<reqIntLen; i++) printf("%i, %i, \n",question[i],answer[i]);
    }
 
    usb_release_interface(dev, 0);
}

void interrupt_read(usb_dev_handle *dev) {
 
    int r,i;
    unsigned char answer[reqIntLen];
    bzero(answer, reqIntLen);
    
    r = usb_interrupt_read(dev, 0x82, answer, reqIntLen, timeout);
    if( r != reqIntLen )
    {
          perror("USB interrupt read"); bad("USB read failed"); 
    }

    if(debug) {
       for (i=0;i<reqIntLen; i++) printf("%02x ",answer[i]  & 0xFF);
    
       printf("\n");
    }
}

void interrupt_read_temperatura(usb_dev_handle *dev, float *tempC, int nr_of_sensors) {
 
    int r,i, temperature[MAX_SENSOR];
    unsigned char answer[reqIntLen];
    bzero(answer, reqIntLen);
    
    r = usb_interrupt_read(dev, 0x82, answer, reqIntLen, timeout);

    // If reading failed, retry once more...
    if( r != reqIntLen )
    {
          r = usb_interrupt_read(dev, 0x82, answer, reqIntLen, timeout);
    }

    if( r != reqIntLen )
    {
          perror("USB interrupt read"); bad("USB read failed"); 
    }


    if(debug) {
      for (i=0;i<reqIntLen; i++) printf("%02x ",answer[i]  & 0xFF);
    
      printf("\n");
    }
    
    for (i=0; i<nr_of_sensors; i++) {
        temperature[i] = (answer[3+i*2] & 0xFF) + ((signed char)answer[2+i*2] << 8);
        temperature[i] += calibration;
        *tempC = temperature[i] * (125.0 / 32000.0);
    }

}

void bulk_transfer(usb_dev_handle *dev) {
 
    int r,i;
    char answer[reqBulkLen];

    r = usb_bulk_write(dev, endpoint_Bulk_out, NULL, 0, timeout);
    if( r < 0 )
    {
          perror("USB bulk write"); bad("USB write failed"); 
    }
    r = usb_bulk_read(dev, endpoint_Bulk_in, answer, reqBulkLen, timeout);
    if( r != reqBulkLen )
    {
          perror("USB bulk read"); bad("USB read failed"); 
    }


    if(debug) {
      for (i=0;i<reqBulkLen; i++) printf("%02x ",answer[i]  & 0xFF);
    }
 
    usb_release_interface(dev, 0);
}
 

void ex_program(int sig) {
      bsalir=1;
 
      (void) signal(SIGINT, SIG_DFL);
}
 
int main( int argc, char **argv) {
 
     float tempc[MAX_SENSOR];
     int c, i, j;
     struct tm *local;
     time_t t;
     int nr_of_sensors = 1;

     memset(handles, 0, sizeof(handles));
     while ((c = getopt (argc, argv, "mfcvhl::a:dD::n:")) != -1)
     switch (c)
       {
       case 'v':
         debug = 1;
         break;
       case 'c':
         formato=formato+1; //Celsius
         break;
       case 'f':
         formato=formato+10; //Fahrenheit
         break;
       case 'm':
         mrtg=1;
         break;
       case 'd':
         devlist=1;
         break;
       case 'D':
         if (optarg!=NULL){
	   if (!sscanf(optarg,"%i",&devnum)==1) {
	     fprintf (stderr, "Error: '%s' is not numeric.\n", optarg);
	     exit(EXIT_FAILURE);
	   }
	 }else{
	   devnum=-10;
	 }
	 break;
       case 'l':
         if (optarg!=NULL){
           if (!sscanf(optarg,"%i",&seconds)==1) {
             fprintf (stderr, "Error: '%s' is not numeric.\n", optarg);
             exit(EXIT_FAILURE);
           } else {           
              bsalir = 0;
              break;
           }
         } else {
           bsalir = 0;
           seconds = 5;
           break;
         }
       case 'a':
         if (!sscanf(optarg,"%i",&calibration)==1) {
             fprintf (stderr, "Error: '%s' is not numeric.\n", optarg);
             exit(EXIT_FAILURE);
         } else {           
              break;
         }
       case 'n':
         if (!sscanf(optarg,"%i",&nr_of_sensors)==1) {
             fprintf (stderr, "Error: '%s' is not numeric.\n", optarg);
             exit(EXIT_FAILURE);
         } 
         else 
         {           
           if ((nr_of_sensors > MAX_SENSOR) || (nr_of_sensors < 1))
           {
             fprintf (stderr, "Error: '%s' is not in range [1..%i].\n", optarg,MAX_SENSOR);
             exit(EXIT_FAILURE);
           }
              break;
         }
       case '?':
       case 'h':
         printf("pcsensor version %s\n",VERSION);
	 printf("      Aviable options:\n");
	 printf("          -h help\n");
	 printf("          -v verbose\n");
	 printf("          -l[n] loop every 'n' seconds, default value is 5s\n");
	 printf("          -c output only in Celsius\n");
	 printf("          -f output only in Fahrenheit\n");
	 printf("          -a[n] increase or decrease temperature in 'n' degrees for device calibration\n");
	 printf("          -m output for mrtg integration\n");
         printf("          -n[n] read number of sensors [1..%i], default value is 1\n",MAX_SENSOR);
	 printf("          -d output with Bus and Device number\n");
	 printf("          -D display device list\n");
	 printf("          -D[n] specific device number\n");

	 exit(EXIT_FAILURE);
       default:
         if (isprint (optopt))
           fprintf (stderr, "Unknown option `-%c'.\n", optopt);
         else
           fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
         exit(EXIT_FAILURE);
       }

     if (optind < argc) {
        fprintf(stderr, "Non-option ARGV-elements, try -h for help.\n");
        exit(EXIT_FAILURE);
     }
 
     if (setup_libusb_access() == 0) {
         exit(EXIT_FAILURE);
     } 

     (void) signal(SIGINT, ex_program);

     for (i = 0; handles[i] != NULL && i < MAX_DEV; i++) {
       if(devnum == -10){
	 printf("%i is Bus %s Device %s \n",i,devlist_bus[i],devlist_device[i]);
       }else if(i == devnum || devnum == -1 ){
	 ini_control_transfer(handles[i]);
       
	 control_transfer(handles[i], uTemperatura );
	 interrupt_read(handles[i]);
 
	 control_transfer(handles[i], uIni1 );
	 interrupt_read(handles[i]);
 
	 control_transfer(handles[i], uIni2 );
	 interrupt_read(handles[i]);
	 interrupt_read(handles[i]);

	 control_transfer(handles[i], uTemperatura );
	 interrupt_read_temperatura(handles[i], tempc, nr_of_sensors);

	 t = time(NULL);
	 local = localtime(&t);
	 if (mrtg) {
	   if (formato>=10) {
             for (j=0;j<nr_of_sensors; j++)
             {
	       printf("%.2f\n", (9.0 / 5.0 * tempc[j] + 32.0));
	       printf("%.2f\n", (9.0 / 5.0 * tempc[j] + 32.0));
             }
	   } else {
             for (j=0;j<nr_of_sensors; j++)
             {
	       printf("%.2f\n", tempc);
	       printf("%.2f\n", tempc);
             }
	   }
         
	   printf("%02d:%02d\n", 
		  local->tm_hour,
		  local->tm_min);
	 
	   printf("pcsensor\n");
	 } else {
           for (j=0;j<nr_of_sensors; j++)
           {
	     printf("%04d/%02d/%02d %02d:%02d:%02d ", 
		  local->tm_year +1900, 
		  local->tm_mon + 1, 
		  local->tm_mday,
		  local->tm_hour,
		  local->tm_min,
		  local->tm_sec);
	   
	     if(devlist>0){
	       printf("Bus %s Device %s ",devlist_bus[i],devlist_device[i]);
	     }
	     printf("Temperature%d", j);
	     if (formato>=10 || formato==0) {
	       printf(" %.2fF", (9.0 / 5.0 * tempc[j] + 32.0));
	     }
	     if ((formato%10)==1 || formato==0) {
	       printf(" %.2fC", tempc[j]);
	     }
	     printf("\n");
           }
	 }

	 usb_release_interface(handles[i], INTERFACE1);
	 usb_release_interface(handles[i], INTERFACE2);
     
	 usb_close(handles[i]); 
       }
     }
     
     return 0; 
}

