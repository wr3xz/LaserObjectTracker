/* INCLUDE */
#include <sys/resource.h>
#include <raspicam/raspicam_cv.h>
#include "opencv2/opencv.hpp"
#include <wiringPi.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
//#include <fstream>
//#include <sstream>
//#include <string>

using namespace std; 
using namespace cv;

/* DEFINE */

#define DELAY_VIA_PWM   0
// Default subcycle time
#define SUBCYCLE_TIME_US_DEFAULT 20000
// Default pulse-width-increment-granularity
#define PULSE_WIDTH_INCREMENT_GRANULARITY_US_DEFAULT 10

#define GPIO 17
#define DELAY_INIT 1 * 1000000
#define DELAY 1 * 1000000
#define CHANNEL 0

#define FACTEUR_DECIMAL 100000
#define PIN_SWITCH 4
/* PROTOTYPE */

extern "C" {
int setup(int pw_incr_us, int hw);
void shutdown(void);
int print_channel(int channel);
int init_channel(int channel, int subcycle_time_us);
int add_channel_pulse(int channel, int gpio, int width_start, int width);
int clear_channel_gpio(int channel, int gpio);
}

void set_servo_angle( signed int angle, int gpio);
void setup_servo();
float getParamVal ( string id,int argc,char **argv,float defvalue );

/* GLOBAL */

int first_use_0 = 1;
int first_use_1 = 1;

/* MAIN */

int main ( int argc,char *argv[] ) {
	
	int Low_Teinte = getParamVal ( "-lT",argc,argv,205 );
	int High_Teinte = getParamVal ( "-hT",argc,argv,220 );
	int Low_Sat = getParamVal ( "-lS",argc,argv, 90 );
	int High_Sat = getParamVal ( "-hS",argc,argv,100 );
	int Low_Val = getParamVal ( "-lV",argc,argv,0 );
	int High_Val = getParamVal ( "-hV",argc,argv,100 );
	
	printf("Low_Teinte= %d\n", Low_Teinte);
	printf("High_Teinte= %d\n", High_Teinte);
	printf("Low_Sat= %d\n", Low_Sat);
	printf("High_Sat= %d\n", High_Sat);
	printf("Low_Val= %d\n", Low_Val);
	printf("High_Val= %d\n", High_Val);
	
    setpriority(PRIO_PROCESS, getpid(), -20);
    raspicam::RaspiCam_Cv Camera;
    Mat im_bgr_origin, im_bgr, im_hsv, im_gray;
    int y =0 ; 
	int x =0;  
	unsigned int nbPixels=0;
	unsigned int somme_x = 0;
	unsigned int somme_y = 0;
	
	int ecart_pixel_x;
	int ecart_pixel_y;
	int ecart_angle_x;
	int ecart_angle_y;
	int relative_angle_x;
	int relative_angle_y;
	int tmp_relative_angle_x;
	int tmp_relative_angle_y;
	
	Point center;
	center.x = -1;
	center.y = -1;	 
	Mat element = getStructuringElement(MORPH_ELLIPSE, Size(3,3));
	
	
    //set camera params
    Camera.set( CV_CAP_PROP_FORMAT, CV_8UC3 );
    //réglage du temps d’exposition.
    Camera.set( CV_CAP_PROP_EXPOSURE, 50);
		
	//init GPIO SWITCH
	wiringPiSetup();
	pinMode(PIN_SWITCH, INPUT);
		
	//init servo
	setup_servo(); 
  
	while(true)
	{
		relative_angle_x = 0;
		relative_angle_y = 0;
		tmp_relative_angle_x = 0;
		tmp_relative_angle_y = 0;
		//Open camera
		printf("Opening Camera..\n");    
		if (!Camera.open()) 
		{
			printf("Error opening the camera.\n");
			return -1;
		}

		set_servo_angle(0,17);
		set_servo_angle(0,18);
		usleep(DELAY_INIT);
							
		//Start capture
		printf("Capturing frames..\n");    
			
		while(!digitalRead(PIN_SWITCH))
		{

			
				
			somme_x = 0;
			somme_y = 0;
			nbPixels = 0;
				
				
			Camera.grab();
			Camera.retrieve ( im_bgr_origin); 
				
			//resize image
			resize(im_bgr_origin,im_bgr,Size(),0.125,0.125,CV_INTER_AREA);
			//save acquired image 
			
			//
			imwrite( "./Image_Resized.jpg", im_bgr );	
			//
			
			cvtColor(im_bgr,im_gray,COLOR_BGR2GRAY);	
			cvtColor(im_bgr,im_hsv,COLOR_BGR2HSV);  
				
			//inRange(im_hsv, Scalar((205/2), 220, 0), Scalar((220/2), 255, 255), im_gray);//Teinte, Saturation, Valeur
			inRange(im_hsv, Scalar((Low_Teinte/2), ((Low_Sat/100)*255), ((Low_Val/100)*255)), Scalar((High_Teinte/2), ((High_Sat/100)*255), ((High_Val/100)*255)), im_gray);//Teinte, Saturation, Valeur	
			
			//
			imwrite( "./Image_filtered.jpg", im_gray );
			//
			
			//fermeture
			erode(im_gray,im_gray,element);
			dilate(im_gray,im_gray,element);
				
			//ouverture
			dilate(im_gray,im_gray,element);
			erode(im_gray,im_gray,element); 
			
			for( x = 0; x < im_gray.cols; x++)
			{
				for( y = 0; y < im_gray.rows; y++)
				{
					if (im_gray.at<uchar>(Point(x,y))==255)
					{
						somme_x += x;
						somme_y += y;
						nbPixels++;
					}
				}
			}	
				
			if(nbPixels > 8)
			{	
				center.x = somme_x / nbPixels;
				center.y = somme_y / nbPixels;
					
				//circle( im_bgr, center, 5, Scalar(0,255,0), -1);	
				//imwrite( "./Image_Cercle.jpg", im_bgr );
				//printf("Cercle image saved: Image_Cercle.jpg.\n");  
			 
				ecart_pixel_x = (center.x -(im_gray.cols/2+1));
				ecart_pixel_y = ((im_gray.rows-center.y) -(im_gray.rows/2+1));
					
				//printf("écart sur x en pixel: %d\n", ecart_pixel_x);
				//printf("écart sur y en pixel: %d\n", ecart_pixel_y);
					
				if((abs(ecart_pixel_x) > 8) or (abs(ecart_pixel_y) > 8))
				{
					ecart_angle_x = (ecart_pixel_x * ((FACTEUR_DECIMAL*54)/ im_gray.cols))/FACTEUR_DECIMAL;				
					ecart_angle_y = (ecart_pixel_y * ((FACTEUR_DECIMAL*41)/ im_gray.rows))/FACTEUR_DECIMAL;
						
					tmp_relative_angle_x = relative_angle_x + ecart_angle_x;
					tmp_relative_angle_y = relative_angle_y + ecart_angle_y;
						
					if ((tmp_relative_angle_x >=-90) and (tmp_relative_angle_x <=90)and (tmp_relative_angle_y <= 30) and (tmp_relative_angle_y >=-90))// pas en butée
					{
						relative_angle_x = tmp_relative_angle_x;
						relative_angle_y = tmp_relative_angle_y;	
							
						set_servo_angle(relative_angle_x,17);
						set_servo_angle(relative_angle_y,18);
								
						usleep(150*1000);
						//usleep((abs(relative_angle_y)>abs(relative_angle_x))?(5000*abs(relative_angle_y)):(5000*abs(relative_angle_x)));	
					}
				}
			}
			
		}
		
		printf("Stop camera..\n");    
		Camera.release();
				
		imwrite( "./Image_Morpho.jpg", im_gray );
		printf("Morpho image saved: Image_Morpho.jpg.\n"); 
		if( nbPixels > 8 )
		{   
			circle( im_bgr, center, 2, Scalar(0,255,0), -1);
			imwrite( "./Image_Cercle.jpg", im_bgr );
			printf("Cercle image saved: Image_Cercle.jpg.\n");  
		}
		imwrite( "./Image_High_Res.jpg", im_bgr_origin );
		printf("High res. image saved: Image_High_Res.jpg.\n"); 
				
		//release servo
		clear_channel_gpio(0, 17);
		clear_channel_gpio(0, 18);	
		//shutdown();
		while(digitalRead(PIN_SWITCH))
		{
			//do nothing
			sleep(1);
		}
		
	}
	
	
}


/* FUNCTION */

void set_servo_angle(signed int angle, int gpio) // si gpio = 17 alors commande horizontale, si gpio=18, alors commande verticale.
{
	if ( gpio ==17)
	{	
		if(first_use_0 == 0) 
		{
			clear_channel_gpio(0, gpio);
		}
		add_channel_pulse(CHANNEL, gpio, 0, 1850-angle); //fonctionne de -90° à +90°
		first_use_0 = 0;		
	}
	else if (gpio == 18)
	{			
		if(first_use_1 == 0) 
		{
			clear_channel_gpio(0, gpio);
		}
		add_channel_pulse(CHANNEL, gpio, 0, 1932+angle); //fonctionne de -90° à 0°
		first_use_1 = 0;		
	}
	else{
		printf(" valeur du servo non-correct\n");
	}
}

void setup_servo(void)
{
	setup(PULSE_WIDTH_INCREMENT_GRANULARITY_US_DEFAULT, DELAY_VIA_PWM);
    init_channel(CHANNEL, SUBCYCLE_TIME_US_DEFAULT);
    //print_channel(CHANNEL);	
}


//Returns the value of a param. If not present, returns the defvalue
float getParamVal ( string id,int argc,char **argv,float defvalue ) {
for ( int i=0; i<argc; i++ )
if ( id== argv[i] )
return atof ( argv[i+1] );
return defvalue;
}

