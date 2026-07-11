#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <string>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
//#include <cuda_provider_factory.h>
#include <onnxruntime_cxx_api.h>
#include "text_det.h"
#include "text_rec.h"

using namespace cv;
using namespace std;
using namespace Ort;


int main()
{
	TextDetector detect_model("weights/PP-OCRv6_medium_det_infer.onnx");
	TextRecognizer rec_model("weights/en_PP-OCRv5_mobile_rec_infer.onnx", "weights/ppocrv5_en_dict.txt");

	string imgpath = "images/test.bmp";
	Mat srcimg = imread(imgpath);

	// test hole image
	vector< vector<Point2f> > results = detect_model.detect(srcimg);
	detect_model.draw_pred(srcimg, results);
	imshow("PaddleOCR", srcimg);
	waitKey(0);
	destroyAllWindows();


	// select ROI and test
	srcimg = imread(imgpath);
	cv::Rect img = cv::selectROI(srcimg);
	destroyAllWindows();
	cv::Mat RegionROI = srcimg(cv::Range(img.y, img.y + img.height), cv::Range(img.x, img.x + img.width));
	cv::Mat MaskImg = cv::Mat::zeros(srcimg.size(), srcimg.type());
	Mat imageROI = MaskImg(Rect(img.x, img.y, RegionROI.cols, RegionROI.rows));
	addWeighted(imageROI, 1, RegionROI, 1, 0, imageROI);
	vector< vector<Point2f> > resultss = detect_model.detect(MaskImg);
	for (size_t i = 0; i < resultss.size(); i++)
	{
		Mat textimg = detect_model.get_rotate_crop_image(MaskImg, resultss[i]);
		string text = rec_model.predict_text(textimg);
		cout << text << endl;
	}
	detect_model.draw_pred(srcimg, resultss);
	imshow("PaddleOCR", srcimg);
	waitKey(0);
	destroyAllWindows();
}