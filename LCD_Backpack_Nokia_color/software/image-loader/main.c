//Image sender for NOKIA

// reference link http://dangerousprototypes.com/forum/index.php?topic=1056.0
// wikimedia      http://dangerousprototypes.com/docs/Mathieu:_Another_LCD_backpack
// update v0.3 based on http://dangerousprototypes.com/forum/index.php?topic=1056.msg11294#msg11294
// i,v,c,T,a,d

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef WIN32

#include <conio.h>
#include <windef.h>
#include <windows.h>
#include <wingdi.h>
#include <WinDef.h>

#else
#include <stdbool.h>
#include <curses.h>
#define  Sleep(x) usleep(x);
#define HANDLE int;
#endif
#include "serial.h"
char *dumpfile;

#define FREE(x) if(x) free(x);

#define MAX_BUFFER 16384   //16kbytes

//structure of bmp header
#ifndef _WIN32
//taken from msdn so that it will compile under debian not yet done!!
typedef unsigned char BYTE;
//typedef int HANDLE;
typedef struct tagBITMAPFILEHEADER {
  unsigned short bfType;
  unsigned long bfSize;
  unsigned short bfReserved1;
  unsigned short bfReserved2;
  unsigned long bfOffBits;
} BITMAPFILEHEADER;
typedef struct tagBITMAPINFOHEADER {
  unsigned long biSize;
  long  biWidth;
  long biHeight;
  unsigned short  biPlanes;
  unsigned short  biBitCount;
  unsigned long biCompression;
  unsigned long biSizeImage;
  long  biXPelsPerMeter;
  long  biYPelsPerMeter;
  unsigned long biClrUsed;
  unsigned long biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBQUAD {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD bmiColors[1];
} BITMAPINFO;
typedef bool BOOL;
#else
HANDLE dumphandle;
#endif

int modem =FALSE;   // use by command line switch -m to set to true
#  define BF_TYPE 0x4D42                   // MB
int print_usage(char * appname)
{
	//print usage
	printf("\n");
	printf("\n");
	printf(" Help Menu\n");
	printf(" Usage:              \n");
	printf("   %s  -p device -f bmpfile -s speed -b bytes -V\n ",appname);
	printf("\n");
#ifdef _WIN32
	printf("   Example Usage:   %s -d COM1 -s 921600 -f sample.bmp -v\n",appname);
	printf("                    %s -d COM1 -s 921600 -v -a \n",appname);
	printf("                    %s -d COM1 -s 921600 -i -d 20\n",appname);
#else
	printf("   Example Usage:   %s -d /dev/ttyS0 -s 921600 -f sample.bmp -v\n",appname);
	printf("                    %s -d /dev/ttyS0 -s 921600 -v -a \n",appname);
	printf("                    %s -d /dev/ttyS0 -s 921600 -i -d 20\n",appname);

#endif
	printf("\n");
	printf("           Where: -p device is port e.g.  COM1 (windows) or /dev/ttyS0 (*nix) \n");
	printf("                  -s Speed is port Speed  default is 921600 \n");
	printf("                  -f Filename of BMP file \n");
	printf("                  -B bytes, No. Of chunks of bytes to send to the  port. MAX 5460   \n");
	printf("                  -d percent, dim the backlight, Range (0-100)  \n");
	printf("                  -a LCD Backligh voltage reading  \n");
	printf("                  -v version information  \n");
	printf("                  -i Initialized and Reset LCD  \n");
	printf("                  -V verbose mode- show something on screen  \n");
 	printf("                  -T Test pattern (full Screen)  \n");
 	printf("                  -h help menu \n");
    printf("\n");
    printf("           Note that the option -p is mandatory\n");

	printf("\n");

	printf("-----------------------------------------------------------------------------\n");

	return 0;
}

int main(int argc, char** argv)
{
		int opt,adc;
		char buffer[MAX_BUFFER]={0};
		int fd;
		int res,c,counter,i,in;
		FILE *fp;
		float v_in,v_out;
		char *param_port = NULL;
		char *param_speed = NULL;
		char *param_imagefile=NULL;
		char *param_mode=NULL;
		char *param_backlight=NULL;
		BITMAPFILEHEADER header;
		BITMAPINFO *headerinfo;
		int headersize,bitmapsize;
		uint8_t *i_bits=NULL,*o_bits=NULL;  // input bits array from original bitmap file, output bit as converted
		int chunksize=300 * 3 ;  //default chunk of bytes to send must be set to max the device can handle without loss of data
		uint8_t b[3] ={0};
		BOOL breakout=FALSE;
		BOOL verbose_mode=FALSE;
		BOOL   has_more_data=TRUE;
		BOOL param_init=FALSE;
		BOOL  show_image=FALSE;
		BOOL show_version=FALSE;
		BOOL test_pattern_full=FALSE;
        BOOL  ADC_read=FALSE;
		printf("-----------------------------------------------------------------------------\n");
		printf("\n");
		printf(" BMP image sender for Nokia LCD Backpack V.0.3 \n");
		printf(" Wiki Docs: http://dangerousprototypes.com/docs/Mathieu:_Another_LCD_backpack\n");
		printf(" http://www.dangerousprototypes.com\n");
		printf("\n");
		printf("-----------------------------------------------------------------------------\n");

		if (argc <= 1)  {
			print_usage(argv[0]);
			exit(-1);
		}

		while ((opt = getopt(argc, argv, "iTtvVas:p:f:d:B:")) != -1) {

			switch (opt) {
				case 'p':  // device   eg. com1 com12 etc
					if ( param_port != NULL){
						printf(" Device/PORT error!\n");
						exit(-1);
					}
					param_port = strdup(optarg);
					break;
				case 'f':
					if (param_imagefile != NULL) {
						printf(" Invalid Parameter after Option -f \n");
						exit(-1);
					}
					param_imagefile = strdup(optarg);
					break;
				case 's':
					if (param_speed != NULL) {
						printf(" Speed should be set: eg  921600 \n");
						exit(-1);
					}
					param_speed = strdup(optarg);
					break;
				case 'B':    // added new: chuck size should be multiply by 3
				    // 1 * 3 up to max of file size or max port speed?
				    if ((atol(optarg) < MAX_BUFFER/3 )&&(atol(optarg)!=0) )
				         chunksize=atol(optarg)*3;
				    else {
				       printf(" Invalid chunk size parameter: using default: %i x 3 = %i Bytes\n",chunksize/3,chunksize);

				    }
				    break;
				case 'd':  // dim the backlight
					if ( param_backlight != NULL){
						printf(" Error: Parameter required to dim the backlight, Range: 0-100\n");
						exit(-1);
					}
					param_backlight = strdup(optarg);
					break;
                case 'i':    //  initialize
				    if (optarg!=NULL) {
				        printf("Invalid option in -i\n");
				    } else {
				        param_init=TRUE;
				    }
					break;
                case 'V':    //talk show some display
				    if (optarg !=NULL) {
				        printf("Invalid option in -V\n");
				    }
				    else {
				        verbose_mode=TRUE;
				    }
					break;
                case 'v':    //
				    if (optarg !=NULL) {
				        printf("Invalid option in -v\n");
				    }
				    else {
				        show_version=TRUE;
				    }
					break;
			    case 'T':    //
				    if (optarg !=NULL) {
				        printf("Invalid option in -T\n");
				    }
				    else {
				        test_pattern_full=TRUE;
				    }
					break;
				 case 'a':    //
				    if (optarg !=NULL) {
				        printf("Invalid option in -a\n");
				    }
				    else {
				        ADC_read=TRUE;
				    }
					break;
				case 'h':
					print_usage(argv[0]);
					exit(-1);
					break;
				default:
					printf(" Invalid argument %c", opt);
					print_usage(argv[0]);
					exit(-1);
					break;
			}
		}

		if (param_port==NULL){
			printf(" No serial port specified\n");
			print_usage(argv[0]);
			exit(-1);
		}

		if (param_speed==NULL)
           param_speed=strdup("921600");  //default is 921600kbps

        fd = serial_open(param_port);
		if (fd < 0) {
			fprintf(stderr, " Error opening serial port\n");
			return -1;
		}

		//setup port and speed
		serial_setup(fd,(speed_t) param_speed);
        if(param_init==TRUE) {
            // i Reset and initialize the LCD.
            buffer[0]='i';
            serial_write( fd, buffer,1 );
		    printf(" LCD Initialized and reset\n");

        }
         if(show_version==TRUE) {
            // i Reset and initialize the LCD.
            buffer[0]='v';
            serial_write( fd, buffer,1 );
            Sleep(1);
			res= serial_read(fd, buffer, sizeof(buffer));
		    printf(" Hardware/Software Version %s\n",buffer);

        }
        if(test_pattern_full==TRUE) {
            // i Reset and initialize the LCD.
            buffer[0]='T';
            serial_write( fd, buffer,1 );
		    printf(" Test Pattern Command Send %c\n",buffer[0]);
		    printf(" Hit any key to end test \n");
		    if (getch()) {
		        buffer[0]=0x00;   //any bytes to end
                serial_write( fd, buffer,1 );
		    }
		    Sleep(1);
            res= serial_read(fd, buffer, sizeof(buffer));
            printf(" Full Screen Test Ends: ");
            if(res==1 && buffer[0]==0x01 ){
                   printf(" Success!\n");
				}
				else {
				    printf(" Reply received: ");
                    for (i=0;i <res;i++)
                      printf(" %02x",(uint8_t) buffer[i]);
                   printf("\n");
				}


        }

        if( ADC_read==TRUE) {
            // LCD backlight voltage reading
            //Raw reading: 0x182 (386)
            //Actual voltage: (386/1024)*3.3volts=1.24volts
            //Scale for resistor divider: Vin = (Vout*(R1+R2))/R2 = (1.24volts*(18K+3.9K))/3.9K = 7.01volts (ideal is 7volts)
            buffer[0]='a';
            serial_write( fd, buffer,1);

		    printf(" ADC read command  Send: %c \n",buffer[0]);
            Sleep(1);
			res= serial_read(fd, buffer, sizeof(buffer));
			//expects 2 bytes at buffer[0] and buffer[1]
			printf(" Voltage Reading : ");
            for (i=0;i< res;i++)
                printf(" %02x",buffer[i]);
            adc = buffer[0] << 8;
            adc += buffer[1];
            v_out=((float)(adc)/1024.0)*3.3;
			v_in = (1.24*(18000.0+3900.0))/3900.0;
			printf("= %2.2f",v_in);
            if((v_in>6.5) && (v_in<7.5)){
                printf(" **PASS**\n");
            }else{
                printf(" FAIL!!!! :(\n");
            }

        }
        if(param_backlight !=NULL) {
          // convert 0-100 to bytes, 0x00 - 0xff
            buffer[0]='d';
            char dx=(atol(param_backlight)*255)/100;
            buffer[1]=dx;
            serial_write( fd, buffer,2 );

            printf(" Dimlight Command  Send: %c %c\n",buffer[0],buffer[1]);
            Sleep(1);
            res= serial_read(fd, buffer, sizeof(buffer));
            printf(" Dim command reply: ");
            if(res==1 && buffer[0]==0x01 ){
               printf(" Success!\n");
			}
			else {
			   printf(" Reply received: ");
               for (i=0;i <res;i++)
                  printf(" %02x",(uint8_t) buffer[i]);
               printf("\n");
			}

        }
		if (param_imagefile !=NULL) {
		    show_image=TRUE;
		}
		if (show_image==TRUE) {
		 //checks is needed to make sure this is a valid bmp file
			//open the Imagefile  file
			   if ((fp = fopen(param_imagefile, "rb")) == NULL) {
				   printf(" Cannot open image file: %s\n",param_imagefile);
				   exit(-1);
			   }
			    if (fread(&header, sizeof(BITMAPFILEHEADER), 1, fp) < 1){
                    printf(" Invalid Image file.. requires BMP file \n");
                    fclose(fp);
                    exit(-1);
               }
                if (header.bfType != BF_TYPE) {    //BM as signature
                     printf("File: %s is not a valid bitmap file! \n ",param_imagefile);
                     fclose(fp);
                     exit(-1);
               }
               headersize = header.bfOffBits - sizeof(BITMAPFILEHEADER);
            //   printf("Header.bfoffbits: %lu  Sizeof Bitmapfilehader: %i Headersize is : %i\n",header.bfOffBits,sizeof(BITMAPFILEHEADER),headersize);
               if ((headerinfo = (BITMAPINFO *)malloc(headersize)) == NULL){
                    printf("Error allocating memory\n");
                    fclose(fp);
                    exit(-1);
               }
               //error in debian here-> might be because I am using 64 bit
               if ((res=fread(headerinfo, 1, headersize, fp)) < headersize){
                    printf("Error: Headersize Error %i < %i\n",res,headersize );
                    fclose(fp);
                    exit(-1);
               }
               printf(" Header size is %lu\n",headerinfo->bmiHeader.biSize);
               printf(" Image size is  %lupx x %lupx\n",headerinfo->bmiHeader.biWidth,headerinfo->bmiHeader.biHeight);
               printf(" Number of colour planes is %d\n",headerinfo->bmiHeader.biPlanes);
               printf(" Bits per pixel is %d\n",headerinfo->bmiHeader.biBitCount);
             //  printf(" Compression type is %lu\n",headerinfo->bmiHeader.biCompression);
             //  printf(" Image size is %lu\n",headerinfo->bmiHeader.biSizeImage);
             //  printf(" Number of colours is %lu\n",headerinfo->bmiHeader.biClrUsed);
              // printf(" Number of required colours is %lu\n",headerinfo->bmiHeader.biClrImportant);


               if ((bitmapsize = headerinfo->bmiHeader.biSizeImage) == 0){
                    bitmapsize = (headerinfo->bmiHeader.biWidth * headerinfo->bmiHeader.biBitCount + 7) / 8 * abs(headerinfo->bmiHeader.biHeight);
                }
               if ((i_bits = malloc(bitmapsize)) == NULL){  //allocate enough memory
					printf(" Error: Cannot allocate enough memory \n");
					fclose(fp);
					exit(-1);
                }
                if ((o_bits = malloc(bitmapsize/2+1)) == NULL){  //allocate enough memory
					printf(" Error: Cannot allocate enough memory \n");
					fclose(fp);
					exit(-1);
                }
               for (i=0;i < bitmapsize/2+1;i++)
                     o_bits[i]=0x00;    // make sure padding is there

				fclose(fp);
				fp = fopen(param_imagefile, "rb");
				printf(" Offset to image data is %lu bytes\n",header.bfOffBits);
				//close and reopen bmp file


				//send the command 	//Send 'p' or 'P' send page of image data.
				printf(" Setting image mode \n");
				buffer[0]='P';
				serial_write( fd, buffer,1 );

				fseek(fp,header.bfOffBits,SEEK_SET); //and disable this if above fgets is enabled

				if ((res=fread(i_bits,sizeof(unsigned char),bitmapsize,fp)) != bitmapsize) {
					 printf(" Header information error: image data size inconsistent. %i %i\n",res,bitmapsize);
					 fclose(fp);
					 exit(-1);

				}
				printf(" Opening Port on %s at %sbps, using image file %s chunksize = %i X %i \n", param_port, param_speed,param_imagefile,chunksize/3,3);
				printf(" Ready to convert %d bit into 12 bit image data.. \n",headerinfo->bmiHeader.biBitCount);
				printf(" Press any key when ready...\n");
				getch();
				switch (headerinfo->bmiHeader.biBitCount) {
                    case 1:
                        printf(" 1 bit (black and white) not yet done");
                        fclose(fp);
                        exit(-1);
                        break;
                    case 4:
                        printf(" 4-bit (16 colors) not yet done");
                          fclose(fp);
                        exit(-1);
                        break;
                    case 8:
                        printf(" 8 bit (256 colors) not yet done");
                        fclose(fp);
                        exit(-1);
                        break;
                    case 16:
                        printf(" 16-bit not yet done");
                        fclose(fp);
                        exit(-1);
                        break;
                    case 24:  //convert i_bits to 12bits into o_bits

                        counter=0; // counter for original bitmap data
                        in=0;   //counter for putting bytes in o_bits
                        breakout=FALSE;
                        while(1) {

                                //the LCD needs 12bit color, this is 24bit
                                //we need to discard the lower 4 bits of each byte
                                //and pack six half bytes into 3 bytes to send the LCD
                                //if we only have one pixel to process (3bytes) instead of 2, then the last should be 0
                                //need to do checks for end of data, make this more effecient
                                //BMP byte order of BGR...
                               //rrrrgggg	bbbbrrrr	ggggbbbb	...	ggggbbbb

                                //grab six bytes
                                for (i=0;i < 6;i++){

                                   if (counter > bitmapsize-1){
                                       buffer[i]=0x00; //pad remaining then exit the loop
                                       breakout=TRUE;
                                   } else {


                                   buffer[i]=i_bits[counter++];
                                   }
                                   if(verbose_mode==TRUE)
                                      printf(" %02x",(unsigned char) buffer[i]);

                                }

                                // convert to 4 bits each RGB
                                if(verbose_mode==TRUE)
                                    printf("--->");

                                //R G
                                b[0]=((buffer[2]&0xf0)|((buffer[1]>>4)&0x0f));
                                //B R2
                                b[1]=(buffer[0]&0xf0)|((buffer[5]>>4)&0x0f);
                                //G2B2
                                b[2]=(buffer[4]&0xf0)|((buffer[3]>>4)&0x0f);
                               // put in o_bits
                                o_bits[in++] =b[0];
                                o_bits[in++] =b[1];
                                o_bits[in++] =b[2];
                                if(verbose_mode==TRUE)
                                   printf("%02X %02X %02X \n",b[0],b[1],b[2]);
                                if (breakout==TRUE) {
                                    //try to clean
                                    if ((b[0]==0x00)&& (b[1] == 0x00)&& (b[2]==0x00)) {
                                       //remove excess padding
                                       in-=3;
                                       break;
                                    }
                                }
                            }

                        break;
                    case 32:
                         printf(" 32-bit not yet done");
                        fclose(fp);
                        exit(-1);
                        break;

                    default:
                        printf(" Unrecognize Bit count \n");
                        fclose(fp);
                        exit(-1);
				}

				printf(" sending image data..\n ");
				has_more_data=TRUE;
				i=0;
				while(i< in){

					for (c=0; c < chunksize; c++) {
						if (i+c < in){
							buffer[c]= o_bits[i+c];
						}
						else {
			            // no more data
						has_more_data=FALSE;
						break;   //breakout of for loop, c has the last chunk
						}
					}
					if(verbose_mode==TRUE) {
						for(counter=0;counter < c;counter++) {
							printf(" [%i] %02X \n", i+counter, (uint8_t) buffer[counter]);
					//        printf(" %02X \n",  (uint8_t) buffer[counter]);
						}

					}
					serial_write( fd, buffer,c);
					i=i+c;
					if(has_more_data==FALSE)
					   break;
				}
				printf(" Total Bytes Sent: %i\n",i);
				Sleep(1);
				res= serial_read(fd, buffer, sizeof(buffer));

				if(res==1 && buffer[0]==0x01 ){
                   printf(" Success!\n");
				}
				else {
				    printf(" Reply received: ");
                    for (i=0;i <res;i++)
                      printf(" %02x",(uint8_t) buffer[i]);
                   printf("\n");
				}

                printf(" Done! :-)\n\n");
				//close lcd
				fclose(fp);
		}
		serial_close(fd);
		FREE(param_port);
		FREE(param_speed);
		FREE(param_imagefile);
		FREE(param_mode);
		return 0;
 }  //end main()
