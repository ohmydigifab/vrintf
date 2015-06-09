#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include "highgui.h"
#include "cv.h"

int detect_face( int argc, char** argv ) {
    const char* cascade = "/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml";
    double w = 160;
    double h = 120;
    cvNamedWindow( "Example2", CV_WINDOW_AUTOSIZE );
    CvCapture* capture = NULL;
    if (argc > 1){
    	capture = cvCreateFileCapture( argv[1] );
    }
    else {
    	capture = cvCreateCameraCapture( 0 );
        // (2)キャプチャサイズを設定する．
        cvSetCaptureProperty (capture, CV_CAP_PROP_FRAME_WIDTH, w);
        cvSetCaptureProperty (capture, CV_CAP_PROP_FRAME_HEIGHT, h);
    }
    IplImage* frame;
    // 正面顔検出器の読み込み
    CvHaarClassifierCascade* cvHCC = (CvHaarClassifierCascade*)cvLoad(cascade);
    // 検出に必要なメモリストレージを用意する
    CvMemStorage* cvMStr = cvCreateMemStorage(0);
    // 検出情報を受け取るためのシーケンスを用意する
    CvSeq* face;
    while(1) {
        frame = cvQueryFrame( capture );
        if( !frame ) break;
        // 画像中から検出対象の情報を取得する
        face = cvHaarDetectObjects(frame, cvHCC, cvMStr);
        for (int i = 0; i < face->total; i++) {
            // 検出情報から顔の位置情報を取得
            CvRect* faceRect = (CvRect*)cvGetSeqElem(face, i);
            // 取得した顔の位置情報に基づき、矩形描画を行う
            cvRectangle(frame,
                        cvPoint(faceRect->x, faceRect->y),
                        cvPoint(faceRect->x + faceRect->width, faceRect->y + faceRect->height),
                        CV_RGB(255, 0 ,0),
                        2, CV_AA);
        }
        cvShowImage( "Example2", frame );
        char c = cvWaitKey(33);
        if( c == 27 ) break;
    }
    // 用意したメモリストレージを解放
    cvReleaseMemStorage(&cvMStr);
    // カスケード識別器の解放
    cvReleaseHaarClassifierCascade(&cvHCC);
    cvReleaseCapture( &capture );
    cvDestroyWindow( "Example2" );

    return 0;
}
