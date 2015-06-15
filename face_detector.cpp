//#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include "highgui.h"
#include "cv.h"
#include <sys/time.h>

//#define DEBUG_WINDOW

extern "C" {

static int in_detected = 0;
static CvRect last_detected_rect = { };
CvHaarClassifierCascade* cvHCC = NULL;
CvMemStorage* cvMStr = NULL;
void detect_face_init() {
	const char* cascade =
			"/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml";
#ifdef DEBUG_WINDOW
	//cvNamedWindow("debug", CV_WINDOW_AUTOSIZE);
#endif
	// 正面顔検出器の読み込み
	cvHCC = (CvHaarClassifierCascade*) cvLoad(cascade);
	// 検出に必要なメモリストレージを用意する
	cvMStr = cvCreateMemStorage(0);
}
void detect_face_deinit() {
	// 用意したメモリストレージを解放
	cvReleaseMemStorage(&cvMStr);
	// カスケード識別器の解放
	cvReleaseHaarClassifierCascade(&cvHCC);
#ifdef DEBUG_WINDOW
	cvDestroyWindow("debug");
#endif
}
int detect_face(unsigned char *imageData, int width, int widthStep, int height,
		int nChannels, struct timeval time, int nFrame) {
	IplImage frame = { };
	frame.nSize = sizeof(IplImage);
	frame.imageData = (char*) imageData;
	frame.imageDataOrigin = (char*) imageData;
	frame.nChannels = 1;
	frame.width = width;
	frame.height = height;
	frame.widthStep = widthStep;

	//printf("detect_face\n");

	struct timezone tzone;
	struct timeval start, now;
	double elapsedTime = 0.0;
	gettimeofday(&start, &tzone);

	// 検出情報を受け取るためのシーケンスを用意する
	CvSeq* face;

	int src_w = 80;
	int src_h = 60;
	IplImage *src_gray = cvCreateImage(cvSize(src_w, src_h), IPL_DEPTH_8U, 1);
	cvResize(&frame, src_gray); //, CV_INTER_CUBIC);
	cvEqualizeHist(src_gray, src_gray);
	if (in_detected) {
		cvSetImageROI(src_gray,
				cvRect(fmax(0, last_detected_rect.x - 10),
						fmax(0, last_detected_rect.y - 10),
						fmin(src_w, last_detected_rect.width + 20),
						fmin(src_w, last_detected_rect.height + 20)));
	}

	gettimeofday(&now, &tzone);
	elapsedTime = (double) (now.tv_sec - start.tv_sec)
			+ (double) (now.tv_usec - start.tv_usec) / 1000000.0;
	fprintf(stderr, "cvEqualizeHist: %4.6f\n", elapsedTime);

	// 画像中から検出対象の情報を取得する
	face = cvHaarDetectObjects(src_gray, cvHCC, cvMStr);

	gettimeofday(&now, &tzone);
	elapsedTime = (double) (now.tv_sec - start.tv_sec)
			+ (double) (now.tv_usec - start.tv_usec) / 1000000.0;
	fprintf(stderr, "cvHaarDetectObjects: %4.6f\n", elapsedTime);

	in_detected = (face->total > 0);

	for (int i = 0; i < face->total; i++) {
		// 検出情報から顔の位置情報を取得
		CvRect* faceRect = (CvRect*) cvGetSeqElem(face, i);
		// 取得した顔の位置情報に基づき、矩形描画を行う
		cvRectangle(src_gray, cvPoint(faceRect->x, faceRect->y),
				cvPoint(faceRect->x + faceRect->width,
						faceRect->y + faceRect->height), CV_RGB(255, 0, 0), 2,
				CV_AA);
		last_detected_rect = *faceRect;
		printf("%d,%d\n", faceRect->x, faceRect->y);
	}
#ifdef DEBUG_WINDOW
	cvShowImage("debug", src_gray);
	cvWaitKey(1);
#endif

	cvReleaseImage(&src_gray);

	return 0;
}

}
