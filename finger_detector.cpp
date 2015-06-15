#include "ext/HandyAR/FingerTip.h"
#include "ext/HandyAR/HandRegion.h"
#include "sys/time.h"
#include <sys/stat.h>
#include <vector>

extern "C" {

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv/cxcore.h>

void HSV2RGB(double *rr, double *gg, double *bb, double H, double S, double V) {
	int in;
	double fl;
	double m, n;
	in = (int) floor(H / 60);
	fl = (H / 60) - in;
	if (!(in & 1))
		fl = 1 - fl; // if i is even

	m = V * (1 - S);
	n = V * (1 - S * fl);
	switch (in) {
	case 0:
		*rr = V;
		*gg = n;
		*bb = m;
		break;
	case 1:
		*rr = n;
		*gg = V;
		*bb = m;
		break;
	case 2:
		*rr = m;
		*gg = V;
		*bb = n;
		break;
	case 3:
		*rr = m;
		*gg = n;
		*bb = V;
		break;
	case 4:
		*rr = n;
		*gg = m;
		*bb = V;
		break;
	case 5:
		*rr = V;
		*gg = m;
		*bb = n;
		break;
	}
}

float get_distance(CvPoint *p1, CvPoint *p2) {
	float dx = p1->x - p2->x;
	float dy = p1->y - p2->y;
	return sqrt(dx * dx + dy * dy);
}

typedef struct _Fingertip {
	int start_index;
	int end_index;
	int peek_index;
	int cross_index;
} Fingertip;

Fingertip find_finger(CvConvexityDefect *defect, CvSeq *contours,
		CvSeq *defects, int orientation) {
	Fingertip slice = { -1, -1, -1, -1 };
	//find start point
	for (int i = 0; i < contours->total; i++) {
		CvPoint* point = CV_GET_SEQ_ELEM(CvPoint, contours, i);
		if (point == defect->depth_point) {
			slice.start_index = i;
			break;
		}
	}
	//find crossing point
	CvPoint *peek_point =
			(orientation == CV_CLOCKWISE) ? defect->end : defect->start;
	float vx = peek_point->x - defect->depth_point->x;
	float vy = peek_point->y - defect->depth_point->y;
	float theta = atan2(vx, vy);
	float d_theta = theta - (theta + M_PI / 2);
	for (int _i = 1; _i < contours->total + 1; _i++) {
		int i;
		if (orientation == CV_CLOCKWISE) {
			i = (slice.start_index + _i) % contours->total;
		} else {
			i = (slice.start_index - _i) % contours->total;
			if (i < 0) {
				i = contours->total + i;
			}
		}
		CvPoint* point = CV_GET_SEQ_ELEM(CvPoint, contours, i);
		if (point == peek_point) {
			slice.peek_index = i;
		}
		float vx2 = point->x - defect->depth_point->x;
		float vy2 = point->y - defect->depth_point->y;
		float theta2 = atan2(vx2, vy2);
		float d_theta2 = theta2 - (theta + M_PI / 2);
		if (fabs(d_theta2) > M_PI) {
			if (d_theta2 > 0) {
				d_theta2 -= 2 * M_PI;
			} else {
				d_theta2 = 2 * M_PI;
			}
		}
		//check if defect point
		int defect_flag = 0;
		for (int j = 0; j < defects->total; j++) {
			CvConvexityDefect* defect = CV_GET_SEQ_ELEM(CvConvexityDefect,
					defects, j);
			if (point == defect->depth_point) {
				printf("defect\n");
				slice.end_index = i;
				defect_flag = 1;
				break;
			}
		}
		if (defect_flag) {
			break;
		}
		//check if crossing point
		if (slice.cross_index < 0) {
			if (d_theta * d_theta2 <= 0) {
				//crossing
				printf("crossing\n");
				slice.cross_index = i;
			} else {
				d_theta = d_theta2;
			}
		}
	}
	return slice;
}

int get_hull_and_defects(IplImage *gray, IplImage *rgb) {
	CvSeq* seqhull;
	CvSeq* defects;
	CvSeq* contours;
	int* hull;
	int hullsize;
	CvPoint* PointArray;
	CvMemStorage* stor02;
	CvMemStorage* stor03;
	stor02 = cvCreateMemStorage(0);
	stor03 = cvCreateMemStorage(0);

	cvFindContours(gray, stor02, &contours, sizeof(CvContour), CV_RETR_EXTERNAL,
			CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));
	if (contours)
		contours = cvApproxPoly(contours, sizeof(CvContour), stor02,
				CV_POLY_APPROX_DP, 3, 1);

//	int i = 0;
//	int area = 0;
//	int selected = -1;

//busquem el contorn mes gran
	CvSeq* first_contour;
	first_contour = contours;
//	for (; contours != 0; contours = contours->h_next) {
//		CvRect rect;
//		//int count = contours->total;
//		rect = cvContourBoundingRect(contours, 1);
//		if ((rect.width * rect.height) > area) {
//			selected = i;
//			area = rect.width * rect.height;
//		}
//		i++;
//	}

	contours = first_contour;

	srand(0);
	int k = 0;
	for (; contours != 0; contours = contours->h_next) {
		int count = contours->total;          // This is number point in contour
		//CvPoint center;
		//CvSize size;
		CvRect rect;
		std::vector<Fingertip> fingertip_candidates;

		rect = cvContourBoundingRect(contours, 1);
		//if ((k == selected))
		{

			//fprintf(stderr,"malloc\n");
			// Alloc memory for contour point set.
			PointArray = (CvPoint*) malloc(count * sizeof(CvPoint));

			// Alloc memory for indices of convex hull vertices.
			hull = (int*) malloc(sizeof(int) * count);

			// Get contour point set.
			//fprintf(stderr,"cvCvtSeqToArray\n");
			cvCvtSeqToArray(contours, PointArray, CV_WHOLE_SEQ);

			// Find convex hull for curent contour.
			//fprintf(stderr,"cvConvexHull\n");
			cvConvexHull(PointArray, count,
			NULL, CV_COUNTER_CLOCKWISE, hull, &hullsize);

			// Find convex hull for current contour.
			// This required for cvConvexityDefects().
			//fprintf(stderr,"cvConvexHull2\n");
			seqhull = cvConvexHull2(contours, 0, CV_COUNTER_CLOCKWISE, 0);

			// This required for cvConvexityDefects().
			// Otherwise cvConvexityDefects() falled.
			if (hullsize < 4)
				continue;

			// Find defects of convexity of current contours.
			//fprintf(stderr,"cvConvexityDefects\n");
			defects = cvConvexityDefects(contours, seqhull, stor03);
			for (int j = 0; j < defects->total; j++) {
				CvConvexityDefect* defect = CV_GET_SEQ_ELEM(CvConvexityDefect,
						defects, j);
				Fingertip slice1 = find_finger(defect, contours, defects,
						CV_CLOCKWISE);
				if (slice1.start_index >= 0 && slice1.end_index >= 0) {
					fingertip_candidates.push_back(slice1);

				}
				Fingertip slice2 = find_finger(defect, contours, defects,
						CV_COUNTER_CLOCKWISE);
				if (slice2.start_index >= 0 && slice2.end_index >= 0) {
					int start_index = slice2.end_index;
					int end_index = slice2.start_index;
					slice2.start_index = start_index;
					slice2.end_index = end_index;
					fingertip_candidates.push_back(slice2);
				}
			}
			CvPoint peek_y_max = { };
			for (int i = 0; i < (int) fingertip_candidates.size(); i++) {
				Fingertip slice = fingertip_candidates[i];
				CvPoint* point = CV_GET_SEQ_ELEM(CvPoint, contours,
						slice.peek_index);
				if (point->y > peek_y_max.y) {
					peek_y_max = *point;
				}
			}
			if (peek_y_max.x != 0 && peek_y_max.y != 0) {
				cvCircle(rgb, peek_y_max, 5, CV_RGB(255, 0, 0), -1, 8, 0);
			}
//			for (int i = 0; i < (int) fingertip_candidates.size(); i++) {
//				Fingertip slice = fingertip_candidates[i];
//				int h = rand() % 360;
//				double r, g, b;
//				HSV2RGB(&r, &g, &b, h, 1, 255);
//				printf("%lf,%lf,%lf\n", r, g, b);
//
//				cvCircle(rgb,
//						*CV_GET_SEQ_ELEM(CvPoint, contours, slice.start_index),
//						5, CV_RGB(255, 0, 0), -1, 8, 0);
//				cvCircle(rgb,
//						*CV_GET_SEQ_ELEM(CvPoint, contours, slice.end_index), 5,
//						CV_RGB(0, 255, 0), -1, 8, 0);
//				for (int i = slice.start_index; i != slice.end_index;
//						i = (i == contours->total - 1) ? 0 : i + 1) {
//					CvPoint* point = CV_GET_SEQ_ELEM(CvPoint, contours, i);
//					CvPoint* point2 = CV_GET_SEQ_ELEM(CvPoint, contours,
//							(i == contours->total - 1) ? 0 : i + 1);
//					cvLine(rgb, *point, *point2, CV_RGB(r, g, b), 1, CV_AA, 0);
//				}
//			}

			// Draw current contour.
			//cvDrawContours(x->cnt_img,contours,CV_RGB(255,255,255),CV_RGB(255,255,255),0,1, 8);
//			cvDrawContours(rgb, contours, CV_RGB(255, 0, 0), CV_RGB(0, 255, 0),
//					2, 2, CV_AA, cvPoint(0, 0));

			// Draw convex hull for current contour.
//			for (i = 0; i < hullsize - 1; i++) {
//				cvLine(rgb, PointArray[hull[i]], PointArray[hull[i + 1]],
//						CV_RGB(255, 255, 255), 1, CV_AA, 0);
//			}
//			cvLine(rgb, PointArray[hull[hullsize - 1]], PointArray[hull[0]],
//					CV_RGB(255, 255, 255), 1, CV_AA, 0);

			// Free memory.
			free(PointArray);
			free(hull);
			/* replace CV_FILLED with 1 to see the outlines */
			//cvDrawContours( x->cnt_img, contours, CV_RGB(255,0,0), CV_RGB(0,255,0), x->levels, 3, CV_AA, cvPoint(0,0)  );
			//cvConvexityDefects( contours, cvConvexHull2( contours, 0, CV_CLOCKWISE, 0 ), stor022 );
		}
		k++;
	}

	cvReleaseMemStorage(&stor03);
	cvReleaseMemStorage(&stor02);
//if (defects) cvClearSeq(defects);
//if (seqhull) cvClearSeq(seqhull);

//cvCvtColor(rgb, gray, CV_RGB2GRAY);

//copy back the processed frame to image
//memcpy(image.data, gray->imageData, image.xsize * image.ysize);
	return 0;
}

bool file_exists(const char *name) {
	struct stat buffer;
	return (stat(name, &buffer) == 0);
}

CvVideoWriter *debug_viewer = NULL;

FingerTip *ftman = NULL;
HandRegion *hrman = NULL;
void detect_finger_init() {
	ftman = new FingerTip();
	hrman = new HandRegion();
	hrman->LoadSkinColorProbTable();
	if (file_exists("debug.mjpg")) {
		fprintf(stderr, "debug movie created\n");
		debug_viewer = cvCreateVideoWriter("debug.mjpg",
				CV_FOURCC('M', 'J', 'P', 'G'), 1, cvSize(160, 120));
	}
}
void detect_finger_deinit() {
	delete ftman;
	delete hrman;
	if (debug_viewer) {
		fprintf(stderr, "debug movie released\n");
		cvReleaseVideoWriter(&debug_viewer);
	}
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

	IplImage *img = cvCreateImage(cvSize(width / 2, height / 2), IPL_DEPTH_8U,
			3);
	IplImage *img_y = cvCreateImage(cvSize(width / 2, height / 2), IPL_DEPTH_8U,
			1);
	cvResize(&frame_y, img_y);
	cvMerge(img_y, &frame_u, &frame_v, NULL, img);
	cvReleaseImage(&img_y);

	return img;
}
int detect_finger(unsigned char *imageData, int width, int widthStep,
		int height, int nChannels, struct timeval time, int nFrame) {
	int debug = 0;

//	IplImage frame = { };
//	frame.nSize = sizeof(IplImage);
//	frame.imageData = (char*) imageData;
//	frame.imageDataOrigin = (char*) imageData;
//	frame.nChannels = 1;
//	frame.width = width;
//	frame.height = height;
//	frame.widthStep = widthStep;

	struct timezone tzone;
	struct timeval start, now;
	double elapsedTime = 0.0;
	gettimeofday(&start, &tzone);

	IplImage *src_img = get_YCrCb_image(imageData, width, widthStep, height);
	cvCvtColor(src_img, src_img, CV_YCrCb2BGR);

	IplImage *hand_ref = hrman->GetHandRegion(src_img, NULL, debug);
	get_hull_and_defects(hand_ref, src_img);

	if (debug_viewer) {
		fprintf(stderr, "debug frame\n");

//		IplImage *tempImage = cvCreateImage(cvGetSize(hand_ref), IPL_DEPTH_8U,
//				3);
//		cvMerge(hand_ref, hand_ref, hand_ref, NULL, tempImage);
//		cvWriteFrame(debug_viewer, tempImage);
//		cvReleaseImage(&tempImage);

//		// dist transform image
//		IplImage * tempImage = cvCreateImage(cvGetSize(ftman->_pDistImage), 8,
//				1);
//		for (int i = 0; i < ftman->_pDistImage->height; i++)
//			for (int j = 0; j < ftman->_pDistImage->width; j++) {
//				int value = 4 * cvGetReal2D(ftman->_pDistImage, i, j);
//				if (value > 255)
//					value = 255;
//				cvSetReal2D(tempImage, i, j, value);
//			}
//		IplImage *img = cvCreateImage(cvGetSize(tempImage), IPL_DEPTH_8U, 3);
//		cvMerge(tempImage, tempImage, tempImage, NULL, img);
//		cvWriteFrame(debug_viewer, img);
//		cvReleaseImage(&img);
//		cvReleaseImage(&tempImage);

//		IplImage * tempImage = cvCloneImage(src_img);
//
//		cvCircle(tempImage,
//				cvPoint(ftman->_maxDistPoint.x, ftman->_maxDistPoint.y), 2,
//				CV_RGB(255, 0, 0), -1, 8, 0);
//		cvCircle(tempImage,
//				cvPoint(ftman->_maxDistPoint.x, ftman->_maxDistPoint.y),
//				ftman->_maxDistValue, CV_RGB(255, 255, 255), 1, 8, 0);/**/
//		CvMemStorage * pStorage = cvCreateMemStorage(0);
//		CvSeq * pContours;
//		cvFindContours(ftman->_pHandImage, pStorage, &pContours,
//				sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE,
//				cvPoint(0, 0));
//		for (; pContours; pContours = pContours->h_next) {
//			cvDrawContours(tempImage, pContours, CV_RGB(255, 255, 0),
//					CV_RGB(0, 0, 0), 0, 2, 8);
//		}
//		cvReleaseMemStorage(&pStorage);
//		cvWriteFrame(debug_viewer, tempImage);
//		cvReleaseImage(&tempImage);

		cvWriteFrame(debug_viewer, src_img);
	}

	cvReleaseImage(&src_img);

	gettimeofday(&now, &tzone);
	elapsedTime = (double) (now.tv_sec - start.tv_sec)
			+ (double) (now.tv_usec - start.tv_usec) / 1000000.0;
	fprintf(stderr, "detect_finger: num=%d, time=%4.6f\n", 0, elapsedTime);

	return 0;
}

}
