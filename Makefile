subsextractor: subsextractor.cpp
	g++ subsextractor.cpp -fpermissive -o subsextractor `pkg-config --cflags --libs opencv` -llept -ltesseract