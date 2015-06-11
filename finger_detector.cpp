#include "ext/HandyAR/FingerTip.h"
#include "ext/HandyAR/HandRegion.h"
#include "sys/time.h"

extern "C" {

FingerTip *ftman = NULL;
HandRegion *hrman = NULL;
void detect_finger_init() {
	ftman = new FingerTip();
	hrman = new HandRegion();
	hrman->LoadSkinColorProbTable();
}
void detect_finger_deinit() {
	delete ftman;
	delete hrman;
}
IplImage *get_YCrCb_image(unsigned char *imageData, int width, int widthStep,
		int height) {
	IplImage frame_y = { };
	frame_y.nSize = sizeof(IplImage);
	frame_y.imageDataOrigin = (char*) imageData;
	frame_y.imageData = (char*) frame_y.imageDataOrigin;
	frame_y.nChannels = 1;
	frame_y.width = width;
	frame_y.height = height;
	frame_y.widthStep = widthStep;

	IplImage frame_u = { };
	frame_u.nSize = sizeof(IplImage);
	frame_u.imageDataOrigin = (char*) imageData + 5 * height * widthStep / 4;
	frame_u.imageData = (char*) frame_u.imageDataOrigin;
	frame_u.nChannels = 1;
	frame_u.width = width / 2;
	frame_u.height = height / 2;
	frame_u.widthStep = widthStep / 2;

	IplImage frame_v = { };
	frame_v.nSize = sizeof(IplImage);
	frame_v.imageDataOrigin = (char*) imageData + height * widthStep;
	frame_v.imageData = (char*) frame_v.imageDataOrigin;
	frame_v.nChannels = 1;
	frame_v.width = width / 2;
	frame_v.height = height / 2;
	frame_v.widthStep = widthStep / 2;

	IplImage *img = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
	IplImage *img_u = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
	IplImage *img_v = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
	cvResize(&frame_u, img_u, CV_INTER_CUBIC);
	cvResize(&frame_v, img_v, CV_INTER_CUBIC);
	cvMerge(&frame_y, img_u, img_v, NULL, img);
	cvReleaseImage(&img_u);
	cvReleaseImage(&img_v);

	return img;
}
int detect_finger(unsigned char *imageData, int width, int widthStep,
		int height, int nChannels, struct timeval time, int nFrame) {
	int debug = 0;

	IplImage frame = { };
	frame.nSize = sizeof(IplImage);
	frame.imageData = (char*) imageData;
	frame.imageDataOrigin = (char*) imageData;
	frame.nChannels = 1;
	frame.width = width;
	frame.height = height;
	frame.widthStep = widthStep;

	struct timezone tzone;
	struct timeval start, now;
	double elapsedTime = 0.0;
	gettimeofday(&start, &tzone);

	IplImage *src_img = get_YCrCb_image(imageData, width, widthStep, height);
	cvCvtColor(src_img, src_img, CV_YCrCb2BGR);

	IplImage *hand_ref = hrman->GetHandRegion(src_img, NULL, debug);

	//cvThreshold(&frame, src_img, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//	cvSaveImage("finger_origin.png", &frame);
//	cvSaveImage("finger_color.png", src_img);
//	cvSaveImage("finger_gray.png", gray_img);
//	cvSaveImage("finger_hand.png", hand_ref);
	//cvSaveImage("finger_bin.png", src_img);
	int nFingertipCandidates = ftman->FindFingerTipCandidatesByCurvature(hand_ref, debug);
	cvReleaseImage(&src_img);

	gettimeofday(&now, &tzone);
	elapsedTime = (double) (now.tv_sec - start.tv_sec)
			+ (double) (now.tv_usec - start.tv_usec) / 1000000.0;
	fprintf(stderr, "detect_finger: num=%d, time=%4.6f\n", nFingertipCandidates, elapsedTime);

	if(nFingertipCandidates > 8)
	{
		exit(0);
	}
	return 0;
}

}
