#include <stdio.h>
#include <iostream>
#include <sstream>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
//------------------------
// g++ subs.cpp -o subs $(pkg-config --cflags --libs opencv) -llept -ltesseract 
//------------------------
using namespace cv;
using namespace std;
long frame = 0;

int wLoB = 253;
int wHiB = 255;
int wLoG = 253; 
int wHiG = 255;
int wLoR = 253;
int wHiR = 255;

int bLoB = 0;
int bHiB = 12;
int bLoG = 0; 
int bHiG = 12;
int bLoR = 0;
int bHiR = 12;
#define entre(x, y, z) ((y <= x) && (x <= z))
#define whitishB(x)     entre(x, wLoB, wHiB)
#define whitishG(x)     entre(x, wLoG, wHiG)
#define whitishR(x)     entre(x, wLoR, wHiR)
#define blackishB(x)     entre(x, bLoB, bHiB)
#define blackishG(x)     entre(x, bLoG, bHiG)
#define blackishR(x)     entre(x, bLoR, bHiR)

#define WHITISH(B,G,R)	( whitishB(B) && whitishG(G) && whitishR(R) )
#define BLACKISH(B,G,R)	( blackishB(B) && blackishG(G) && blackishR(R) )


#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

int ocr2(const char *filename, char *outtext)
{
	tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
	if (api->Init(NULL, "nld")) {
		fprintf(stderr, "Could not initialize tesseract.\n");
		exit(1);
	}
	Pix *image = pixRead(filename);
	api->SetImage(image);
	api->SetRectangle(300, 860, 590, 100);
	*outtext = 0;
	char *s;
	if((s = api->GetUTF8Text()) != NULL) {
		strcpy(outtext,s);
		fprintf(stderr,"OUTTEXT %s\n",outtext);
	}
	api->End();
	pixDestroy(&image);
	return(strlen(outtext));
}

int th1 = 200;
int th2 = 255;
int ocr(Mat img, char *outtext)
{
	Mat crop,crop2;
	Rect myROI(240, 590, 800, 90);	 // (240,590) -> (240+800,590+90) = (1040,680)
	Mat(img, myROI).copyTo(crop);

	cvtColor(crop, crop2, CV_BGR2GRAY);
	//adaptiveThreshold(crop2,crop,255,CV_ADAPTIVE_THRESH_GAUSSIAN_C,CV_THRESH_BINARY,3,5);
	threshold(crop2,crop,th1,th2,THRESH_BINARY);

	tesseract::TessBaseAPI tess;
	tess.Init(NULL,"nld",tesseract::OEM_DEFAULT);
	tess.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
	tess.SetImage((uchar *)crop.data,crop.cols,crop.rows,1,crop.cols);
	imshow("control", crop);
	char *s;
	if((s = tess.GetUTF8Text()) != NULL) {
		//fprintf(stderr,"OCR: %s\n",s);
		strcpy(outtext,s);
		return strlen(outtext);
	}
	return 0;
}


int controlshow(Mat img)
{
	Rect myROI(240, 590, 800, 90);
	// (240,590) -> (240+800,590+90) = (1040,680)
	Mat crop;
	Mat(img, myROI).copyTo(crop);

	//cvtColor(crop, crop, CV_RGB2GRAY);
	//adaptiveThreshold(crop,crop,255,CV_ADAPTIVE_THRESH_GAUSSIAN_C,CV_THRESH_BINARY,3,5);
	//GaussianBlur(crop,crop,Size(0,0),3);
	//addWeighted(crop,1.5,crop,-0.5,0,crop);
	imshow("control", crop);

}

int cropwrite(string f, Mat img)
{
	Rect myROI(240, 590, 800, 90);
	// (240,590) -> (240+800,590+90) = (1040,680)
	Mat crop;
	Mat(img, myROI).copyTo(crop);
	imwrite(f,crop);
}
	

// Protocolo para haysubs()
// Si hay subs
// 	computa signature nueva y compara con anterior
// 0 = no hay cambio respecto del frame anterior
// 1 = no habia subs en el frame anterior y este frame es el primero de un nuevo sub 
// 2 = finalizaron los subs del frame anterior y en este frame no hay sub
// 3 = finalizaron los subs del frame anterior y este frame es el primero de un nuevo sub
// signature: pixel 

enum {SAME = 0, START = 1, END = 2, CHANGE = 3};

int haysubs(Mat img, int xinf, int xsup, int yinf, int ysup)  
{
        int subs = 0;
        int run;
        int x, y;
        uchar B, G, R;

	static long oldsig = 0;
	long newsig = 0;
	int status; 

        for(x = xinf; x < xsup; x++) {
                run = 0;
                for(y = yinf; y < ysup; y++) {
                        B = img.at<Vec3b>(x, y)[0];
                        G = img.at<Vec3b>(x, y)[1];
                        R = img.at<Vec3b>(x, y)[2];
                        if( WHITISH(B,G,R) ) {
                                run++;
				// Color area to know where we're looking at
                                //img.at<Vec3b>(x, y)[0] = 0;
                                //img.at<Vec3b>(x, y)[1] = 255;
                                //img.at<Vec3b>(x, y)[2] = 0;
				newsig++;
                        } else { 
                                //if(BLACKISH(B,G,R))
					if(run && (run < 5)) 
						subs++;
				run = 0;
                        }
                        
                }
		// subs found
                if(subs) {
                        //imshow("control", img);
			status = (oldsig == 0) ? START : (oldsig == newsig) ? SAME : CHANGE;
			goto CHAU;
                }
        }

	// no subs
	status = (oldsig == 0) ? SAME : END;


CHAU:	oldsig = newsig;
	return status;
	
}

int sub = 1;

string filename()
{
        stringstream ss;

        string name = "cropped_";
        string type = ".jpg";

        ss<<name<<(sub)<<type;

        string fn = ss.str();
        ss.str("");
        return fn;
}

VideoCapture cap("video.mkv");
int StartFrame, EndFrame;
void onSFtb(int, void *) { cap.set(CV_CAP_PROP_POS_FRAMES, StartFrame); } 

enum {CHRON_START = 0, CHRON_END = 1};
int printchron(int chron, long c)
{
        int h, m, s, ms;

        s = c / 1000; 
        ms = c % 1000;
        m = s / 60; 
        s = s % 60;
        h = m / 60;
        m = m % 60;
//00:02:28,560 --> 00:02:30,639
        if(chron == CHRON_START)
                printf("\n%d\n%02d:%02d:%02d,%03d --> ", sub++, h, m, s, ms);
        else
                printf("%02d:%02d:%02d,%03d\n\n", h, m, s, ms);
}
        
long chronstart;

int setchron(long c)
{
	chronstart = c;
}

char *cap2chron(long c, char *chron)
{
        int h, m, s, ms;

        s = c / 1000; 
        ms = c % 1000;
        m = s / 60; 
        s = s % 60;
        h = m / 60;
        m = m % 60;
	sprintf(chron, "%02d:%02d:%02d,%03d", h, m, s, ms);
	return chron;
}

char *getchron(long curchron, char *chronline)
{
	char chron1[100], chron2[100];

	cap2chron(chronstart, chron1);
	cap2chron(curchron, chron2);
	sprintf(chronline, "%d\n%s --> %s", sub++, chron1, chron2);
	//fprintf(stderr,"CHRONLINE %s\n",chronline);
	return chronline;
}


        
int printest(Mat img, int x, int y)
{
	//fprintf(stderr,"RECT (%d,%d) -> (%d,%d)\n",x, y, x+100, y+100);
	Rect myROI(x, y, 100, 100);
	Mat croppedImg;
	Mat(img, myROI).copyTo(croppedImg);
	imshow("control", croppedImg);
}


int main(int argc, char *argv[])
{
        if(!cap.isOpened()) {
                cout << "Cannot open file" << endl;
                return -1;
        }

        StartFrame = 0;
        EndFrame = cap.get(CV_CAP_PROP_FRAME_COUNT); 
        namedWindow("Control", CV_WINDOW_AUTOSIZE);
        createTrackbar("SF", "Control", &StartFrame, cap.get(CV_CAP_PROP_FRAME_COUNT), onSFtb, 0);
        createTrackbar("EF", "Control", &EndFrame, cap.get(CV_CAP_PROP_FRAME_COUNT), 0, 0);
        
        createTrackbar("T1", "Control", &th1,255,NULL,0);
        createTrackbar("T2", "Control", &th2,255,NULL,0);

        int xmax = cap.get(CV_CAP_PROP_FRAME_WIDTH); 
        int ymax = cap.get(CV_CAP_PROP_FRAME_HEIGHT); 
	//fprintf(stderr,"CAP   (%d %d) -> (%d %d)\n", 0, xmax, 0, ymax);

	int x = xmax/2 - 50; int y = ymax - 110; 
	int xw = 100; int yh = 100;
	// 800x90+240+590 convert
	// (240,590) -> (240+800,590+90) = (1040,680)
	//fprintf(stderr,"FRAME (%d %d) -> (%d %d)\n", x, y, x+xw, y+yh);

        Mat img;
        int subs;
        int frame = 0;
	char subtext[1024] = "";
	char same[] = " .   ";
	string f;
	char chronline[500];
        while(true) {
                if(!cap.read(img)) { 
                        cout << "Cannot read a frame from video stream" << endl;
                        break;
                }
                if((frame = cap.get(CV_CAP_PROP_POS_FRAMES)) >= EndFrame)
                        break;
                ////fprintf(stderr,"%ld\r",frame);

                subs = haysubs(img, x, x + xw, y, y + yh);
		switch(subs) {
			case SAME:
				//fprintf(stderr,"%s           \r", same + frame % 4);
				break;
			case START:
				if(ocr(img, subtext))
					setchron(cap.get(CV_CAP_PROP_POS_MSEC));
				//fprintf(stderr, "STR frame %ld\n",frame);
				break;
			case END:
				//fprintf(stderr,"END\n");
				getchron(cap.get(CV_CAP_PROP_POS_MSEC), chronline);
                                printf("%s\n%s\n\n",chronline,subtext);
				//fprintf(stderr, "END frame %d %s\n",frame,subtext);
				break;
			case CHANGE:
				//fprintf(stderr,"CHANGE\n");
                                //string s = getchron();
                                //setchron(cap.get(CV_CAP_PROP_POS_MSEC);
                                //imwrite(f, img);
                                //chron = cap.get(CV_CAP_PROP_POS_MSEC);
                                //intchron(CHRON_START,chron); 
				//fprintf(stderr, "CHG frame %d\n",frame);
				break;
			default:
				fprintf(stderr,"ERROR SUBS\n");
                }
                if (waitKey(30) == 27) {
                    cout << "esc key pressed by user" << endl;
                    break; 
               	}
        }
        return 0;
}
