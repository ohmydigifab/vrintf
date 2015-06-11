APP = vrintf
SRC = face_detector.cpp finger_detector.cpp
OBJS = face_detector.o finger_detector.o
CLEAN =
SUBDIRS = ext/hidclient ext/HandyAR

include ./Makefile.include
