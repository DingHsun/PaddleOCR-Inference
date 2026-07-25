#define _CRT_SECURE_NO_WARNINGS
#define CPPHTTPLIB_THREAD_POOL_COUNT 4
#include <iostream>
#include <sstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <httplib.h>
#include "../core/text_det.h"
#include "../core/text_rec.h"

using namespace cv;
using namespace std;

namespace {

string escape_json(const string& text)
{
	string out;
	out.reserve(text.size());
	for (char c : text)
	{
		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		default: out += c;
		}
	}
	return out;
}

string boxes_to_json(const vector< vector<Point2f> >& boxes)
{
	ostringstream json;
	json << "[";
	for (size_t i = 0; i < boxes.size(); i++)
	{
		if (i > 0) json << ",";
		json << "[";
		for (size_t p = 0; p < boxes[i].size(); p++)
		{
			if (p > 0) json << ",";
			json << "[" << boxes[i][p].x << "," << boxes[i][p].y << "]";
		}
		json << "]";
	}
	json << "]";
	return json.str();
}

Mat decode_request_image(const httplib::Request& req)
{
	vector<uchar> buf(req.body.begin(), req.body.end());
	return imdecode(buf, IMREAD_COLOR);
}

const char* kDecodeErrorJson =
	"{\"error\":\"could not decode image\","
	"\"hint\":\"body must be the raw image bytes (jpg/png/bmp/...), not form-data or base64, "
	"e.g. curl -H \\\"Content-Type: application/octet-stream\\\" --data-binary @file.jpg\"}";

} // namespace

int main(int argc, char** argv)
{
	string det_model_path = "weights/PP-OCRv6_medium_det_infer.onnx";
	string rec_model_path = "weights/en_PP-OCRv5_mobile_rec_infer.onnx";
	string rec_dict_path = "weights/ppocrv5_en_dict.txt";
	string host = "0.0.0.0";
	string local = "127.0.0.1";
	int port = 8080;
	if (argc > 1) host = argv[1];
	if (argc > 2) port = atoi(argv[2]);

	cout << "Loading models..." << endl;
	TextDetector detect_model(det_model_path);
	TextRecognizer rec_model(rec_model_path, rec_dict_path);

	httplib::Server svr;

	// Serve the built Vue frontend (see frontend/README) from the same exe/port.
	// `npm run build` in frontend/ produces frontend/dist; run that before this
	// picks it up. /ocr_detect, /ocr_recognize, /health below still take priority
	// over static files with the same path.
	if (!svr.set_mount_point("/", "frontend/dist"))
	{
		cout << "warning: frontend/dist not found, web UI will not be served" << endl;
	}

	svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
		res.set_content("{\"status\":\"ok\"}", "application/json");
	});

	// POST /ocr_detect - raw image bytes in, JSON array of detected text boxes out.
	// box = [[x,y],[x,y],[x,y],[x,y]] (four corner points, no recognition run).
	svr.Post("/ocr_detect", [&](const httplib::Request& req, httplib::Response& res) {
		cout << "[/ocr_detect] received " << req.body.size() << " bytes" << endl;
		Mat srcimg = decode_request_image(req);
		if (srcimg.empty())
		{
			cout << "[/ocr_detect] could not decode image" << endl;
			res.status = 400;
			res.set_content(kDecodeErrorJson, "application/json");
			return;
		}
		try
		{
			vector< vector<Point2f> > boxes = detect_model.detect(srcimg);
			cout << "[/ocr_detect] found " << boxes.size() << " box(es)" << endl;
			res.set_content(boxes_to_json(boxes), "application/json");
		}
		catch (const std::exception& e)
		{
			cout << "[/ocr_detect] exception: " << e.what() << endl;
			res.status = 500;
			res.set_content("{\"error\":\"" + escape_json(e.what()) + "\"}", "application/json");
		}
	});

	// POST /ocr_recognize - raw image bytes of a single cropped text line in,
	// the recognized text out. No detection is run; the whole image is recognized.
	svr.Post("/ocr_recognize", [&](const httplib::Request& req, httplib::Response& res) {
		cout << "[/ocr_recognize] received " << req.body.size() << " bytes" << endl;
		Mat srcimg = decode_request_image(req);
		if (srcimg.empty())
		{
			cout << "[/ocr_recognize] could not decode image" << endl;
			res.status = 400;
			res.set_content(kDecodeErrorJson, "application/json");
			return;
		}
		try
		{
			string text = rec_model.predict_text(srcimg);
			cout << "[/ocr_recognize] text: " << text << endl;
			res.set_content("{\"text\":\"" + escape_json(text) + "\"}", "application/json");
		}
		catch (const std::exception& e)
		{
			cout << "[/ocr_recognize] exception: " << e.what() << endl;
			res.status = 500;
			res.set_content("{\"error\":\"" + escape_json(e.what()) + "\"}", "application/json");
		}
	});

	cout << "PaddleOCR API server listening on " << host << ":" << port << endl;
	cout << "PaddleOCR API server listening on " << local << ":" << port << endl;
	svr.listen(host.c_str(), port);
	return 0;
}
