# PaddleOCR C++ Inference

[![中文](https://img.shields.io/badge/README-中文-blue?style=for-the-badge)](README.md)

A C++ inference implementation of PaddleOCR using onnxruntime and opencv, runnable on Windows x64.

**Two OCR capabilities**
1. Full-image detection (text location) + recognition (text content)
2. Recognition on a selected ROI

**Two ways to run it**
1. `demo` - an interactive GUI console app with ROI selection, handy for running via Visual Studio F5 straight into `main()`
2. `api_server` - a headless, long-running exe that exposes an HTTP API (`POST /ocr_detect`, `POST /ocr_recognize`) so other services can call OCR over the network

## Project layout

```
src/
  core/        text_det / text_rec shared OCR logic (used by both demo and api_server)
  demo/        main.cpp - interactive GUI demo
  api_server/  main.cpp - HTTP API server (POST /ocr_detect, POST /ocr_recognize)
weights/       onnx models & dictionaries (shared by demo and api_server)
images/        sample images (demo only)
third_party/   httplib.h (single-header HTTP library used by api_server)
vs2022/        Visual Studio 2022 solution (.slnx + two .vcxproj)
cmake/         CMakeLists.txt, for building via VSCode (CMake Tools extension) or any other IDE
packages/      opencv / onnxruntime third-party SDKs (download yourself, not version-controlled, see below)
```

## Building

**Visual Studio 2022**: open `vs2022/PaddleOCR-cpp.slnx`. It contains two projects, `demo` and `api_server` — pick one, set it as the startup project, and hit F5.

**VSCode / CMake**: install the CMake Tools extension, open the folder, and configure using `cmake/CMakeLists.txt`; this produces two targets, `demo` and `api_server`. The opencv/onnxruntime paths default to the folders under `packages/`, and can be overridden with `-DOPENCV_DIR=...` / `-DONNXRUNTIME_DIR=...`.

Both build methods automatically copy the opencv/onnxruntime DLLs and `weights/` (plus `images/` for demo) into the output directory after building.

## Using api_server

```
api_server.exe [host] [port]   # defaults: host 0.0.0.0 (listen on all interfaces), port 8080
```

Example: `api_server.exe 127.0.0.1 9000` listens only on localhost, port 9000.

- `GET /health` - health check
- `POST /ocr_detect` - body is the raw bytes of a full image (jpg/png/bmp...); detection only, returns the array of detected text boxes: `[[[x,y],[x,y],[x,y],[x,y]], ...]`
- `POST /ocr_recognize` - body is the raw bytes of an already-cropped single text line image; recognition only, returns the recognized text: `{"text":"..."}`

The body for both endpoints must be the **raw binary content of an image file** — not form-data, not base64, not wrapped in JSON.

- curl: use `--data-binary @path/to/file` (not `-d`/`--data`, which treats the content as text), and explicitly set `-H "Content-Type: application/octet-stream"` — curl defaults to `application/x-www-form-urlencoded` when unspecified, and httplib caps that content type at 8KB, so anything bigger gets rejected with 413.

  ```bash
  # detection (test_detection.bmp is a full image)
  curl -X POST -H "Content-Type: application/octet-stream" --data-binary @images/test_detection.bmp http://127.0.0.1:8080/ocr_detect

  # recognition (test_recognition.bmp is an already-cropped single text line)
  curl -X POST -H "Content-Type: application/octet-stream" --data-binary @images/test_recognition.bmp http://127.0.0.1:8080/ocr_recognize
  ```

- Postman / Insomnia: on the Body tab, choose **binary** (not form-data or raw), then pick the file
- PowerShell:

  ```powershell
  # detection
  $bytes = [System.IO.File]::ReadAllBytes("images/test_detection.bmp")
  Invoke-RestMethod -Uri "http://127.0.0.1:8080/ocr_detect" -Method Post -Body $bytes -ContentType "application/octet-stream"

  # recognition
  $bytes = [System.IO.File]::ReadAllBytes("images/test_recognition.bmp")
  Invoke-RestMethod -Uri "http://127.0.0.1:8080/ocr_recognize" -Method Post -Body $bytes -ContentType "application/octet-stream"
  ```

- Python:

  ```python
  import requests

  # detection
  with open("images/test_detection.bmp", "rb") as f:
      r = requests.post("http://127.0.0.1:8080/ocr_detect", data=f.read())
  print(r.json())

  # recognition
  with open("images/test_recognition.bmp", "rb") as f:
      r = requests.post("http://127.0.0.1:8080/ocr_recognize", data=f.read())
  print(r.json())
  ```

When the body isn't in the right format (e.g. JSON or form-data), the server responds with HTTP 400 and a `hint` field telling you to send raw binary:

```json
{
  "error": "could not decode image",
  "hint": "body must be the raw image bytes (jpg/png/bmp/...), not form-data or base64, e.g. curl -H \"Content-Type: application/octet-stream\" --data-binary @file.jpg"
}
```

## C++ Packages

- onnxruntime-win-x64-gpu-1.21.0 [ https://github.com/microsoft/onnxruntime/releases ]
- opencv 4.5.0 windows [ https://opencv.org/releases/ ]

Placement path (same directory level as `vs2022/` and `cmake/`, at the project root):
./packages  
 ├─ onnxruntime-win-x64-gpu-1.21.0  
 └─ opencv

## Choosing a model (converting to .onnx)

List of PP-OCR models

[https://github.com/PaddlePaddle/PaddleOCR/blob/release/2.7/doc/doc_ch/models_list.md](http://www.paddleocr.ai/latest/version3.x/module_usage/module_overview.html)

- python 3.10.10
- pip install paddle2onnx-2.0.2rc3
- Download the inference model and unzip it
- Run the following command to convert the model to onnx and place it at the path below (`./weights/`), edit the paths yourself:
- C:\Python31010-OCR-2onnx\Scripts\paddle2onnx.exe --model_dir "C:\Users\Users\Downloads\en_PP-OCRv5_mobile_rec_infer\en_PP-OCRv5_mobile_rec_infer" --model_filename inference.json --params_filename inference.pdiparams --save_file "C:\Users\Users\Downloads\en_PP-OCRv5_mobile_rec_infer\en_PP-OCRv5_mobile_rec_infer\model.onnx"

Code to edit

- `src/demo/main.cpp` (GUI demo) or `src/api_server/main.cpp` (API server) -
- TextDetector detect_model("det onnx model path");
- TextRecognizer rec_model("rec onnx model path", "rec dict.txt path");

## Downloading the recognition dictionary

Example: PP-OCRv6_medium_rec https://github.com/PaddlePaddle/PaddleOCR/blob/main/configs/rec/PP-OCRv6/PP-OCRv6_medium_rec.yml

1. Download the inference model and go through the .onnx conversion steps
   <img width="1084" height="692" alt="image" src="https://github.com/user-attachments/assets/b5f3e947-4b70-4580-9031-3656b2e57369" />

2. Find `character_dict_path` in the .yml file and download the dict.txt needed for recognition  
   <img width="1113" height="766" alt="image" src="https://github.com/user-attachments/assets/a80119e7-5dca-47a5-80e2-ff2afa1f37a8" />
