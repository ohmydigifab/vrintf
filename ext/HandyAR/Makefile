CFLAGS = -g -O0 -fpermissive
LDFLAGS = -L$(SDKSTAGE)/opt/vc/lib \
		-lvcos -lbcm_host -lvchiq_arm -lpthread -ldl -lm -lrt -lstdc++ -lz -lglib-2.0 \
		-ljpeg -lpng12 -ltiff -ljbig \
		-lopencv_legacy -lopencv_highgui -lopencv_core -lopencv_ml -lopencv_video -lopencv_flann \
		-lopencv_imgproc -lopencv_calib3d -lopencv_objdetect -lopencv_features2d \
		-lglut -lGLU -lGL
INCLUDES = -I$(SDKSTAGE)/opt/vc/include -I$(SDKSTAGE)/opt/vc/include/interface/vcos/pthreads \
		-I$(SDKSTAGE)/opt/vc/include/interface/vmcs_host/linux \
		-I$(SDKSTAGE)/usr/include -I$(SDKSTAGE)/usr/include/opencv -I$(SDKSTAGE)/usr/include/opencv2
BIN_DIR = ../../bin
TARGET = libHandyAR.so
SRC = ColorDistribution.cpp ColorHistogram.cpp FingerTip.cpp FingertipPoseEstimation.cpp FingertipTracker.cpp HandRegion.cpp MixGaussian.cpp PoseEstimation.cpp cvFindExtrinsicCameraParams3.cpp
OBJS = ColorDistribution.o ColorHistogram.o FingerTip.o FingertipPoseEstimation.o FingertipTracker.o HandRegion.o MixGaussian.o PoseEstimation.o cvFindExtrinsicCameraParams3.o

all: $(TARGET) $(SRC) $(SUBDIRS)

%.o: %.c
	@rm -f $@ 
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations
	
%.o: %.cpp
	@rm -f $@ 
	$(CXX) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

$(TARGET): $(OBJS)
	$(CXX) -shared -fPIC $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET) && cp $(TARGET) $(BIN_DIR)

$(SUBDIRS):
	$(MAKE) -C $@
     
.PHONY: clean rebuild $(SUBDIRS)

clean:
	rm -f $(TARGET) *.o $(CLEAN)

rebuild:
	make -f Makefile-shared clean && make -f Makefile-shared