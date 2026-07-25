# PaddleOCR C++ Inference

[![中文](https://img.shields.io/badge/README-中文-blue?style=for-the-badge)](README.md)

A C++ inference implementation of PaddleOCR using onnxruntime and opencv, runnable on Windows x64.

**Two OCR capabilities**
1. Full-image detection (text location) + recognition (text content)
2. Recognition on a selected ROI

`api_server` is a single long-running exe that provides both:
- An HTTP API (`POST /ocr_detect`, `POST /ocr_recognize`) for other services to call
- A built-in web UI (see Frontend below) — open it in a browser to try OCR on an image

## Project layout

```
src/
  core/        text_det / text_rec shared OCR logic
  api_server/  main.cpp - HTTP API server (POST /ocr_detect, POST /ocr_recognize)
frontend/      Vue 3 + Vite web UI; once built, it's served by api_server itself (see below)
weights/       onnx models & dictionaries
images/        sample images, handy for testing the API manually with curl/Postman (see "Using api_server")
third_party/   httplib.h (single-header HTTP library used by api_server)
vs2022/        Visual Studio 2022 solution (.slnx + api_server's .vcxproj)
cmake/         CMakeLists.txt, for building via VSCode (CMake Tools extension) or any other IDE
packages/      opencv / onnxruntime third-party SDKs (download yourself, not version-controlled, see below)
```

## Building

**Visual Studio 2022**: open `vs2022/PaddleOCR-cpp.slnx`, set `api_server` as the startup project, and hit F5. Or just run `vs2022/build_and_run.bat`, which builds and launches it without opening the VS GUI.

**VSCode / CMake**: install the CMake Tools extension, open the folder, and configure using `cmake/CMakeLists.txt`; this produces the `api_server` target. The opencv/onnxruntime paths default to the folders under `packages/`, and can be overridden with `-DOPENCV_DIR=...` / `-DONNXRUNTIME_DIR=...`.

Both build methods automatically copy the opencv/onnxruntime DLLs and `weights/` into the output directory after building.

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

## Frontend

`frontend/` is a Vue 3 + Vite web page: pick an image, hit Detect/Recognize, and the detected text boxes get drawn on top of the image. It isn't a standalone web app — the built static files are served by `api_server` itself via httplib's `set_mount_point()`, so the page and the API share the same port, with no separate server and no CORS to deal with.

**While developing** (frontend and backend run separately, so UI edits show up instantly):
```bash
cd frontend
npm install
npm run dev
```
`vite.config.js` already proxies `/health`, `/ocr_detect`, `/ocr_recognize` to `http://127.0.0.1:8080` (start `api_server.exe` separately). Open the URL Vite prints (default `http://localhost:5173`).

**To have `api_server.exe` serve the page itself** (for regular use / packaging):
```bash
cd frontend
npm run build
```
This produces `frontend/dist`. When `api_server` is then built (VS2022 or CMake), the build script copies `frontend/dist` next to the output exe automatically; open `http://127.0.0.1:8080/` in a browser and the web UI is right there — no `npm run dev` needed.

> Note: with CMake, `frontend/dist` needs to exist **before the first `cmake` configure** for this copy step to be added to the build. Build the frontend first, then the C++ project, and you're fine.

## C++ Packages

- onnxruntime-win-x64-gpu-1.21.0 [ https://github.com/microsoft/onnxruntime/releases ]
- opencv 4.5.0 windows [ https://opencv.org/releases/ ]

Placement path (same directory level as `vs2022/` and `cmake/`, at the project root):
./packages  
 ├─ onnxruntime-win-x64-gpu-1.21.0  
 └─ opencv

## Choosing a model (converting to .onnx)

List of PP-OCR models
- [Official model list](https://www.paddleocr.ai/latest/en/version3.x/module_usage/module_overview.html)

Python environment setup
- python 3.10.10
- pip install paddle2onnx-2.0.2rc3
- Download the inference model and unzip it
- Run the following command to convert the model to onnx and place it at the path below (`./weights/`), edit the paths yourself:
- C:\Python31010-OCR-2onnx\Scripts\paddle2onnx.exe --model_dir "C:\Users\Users\Downloads\en_PP-OCRv5_mobile_rec_infer\en_PP-OCRv5_mobile_rec_infer" --model_filename inference.json --params_filename inference.pdiparams --save_file "C:\Users\Users\Downloads\en_PP-OCRv5_mobile_rec_infer\en_PP-OCRv5_mobile_rec_infer\model.onnx"

Code to edit

- `src/api_server/main.cpp` -
- TextDetector detect_model("det onnx model path");
- TextRecognizer rec_model("rec onnx model path", "rec dict.txt path");

## Downloading the recognition dictionary

Example: PP-OCRv6_medium_rec

1. Download the inference model and go through the .onnx conversion steps  [https://www.paddleocr.ai/latest/en/version3.x/module_usage/text_detection.html]  
   | [Model page](https://www.paddleocr.ai/latest/en/version3.x/module_usage/text_detection.html) |
   | :---: |
   | <img width="1084" height="692" alt="image" src="https://github.com/user-attachments/assets/fd42b456-bf52-46bc-99ad-e248753c8c45" /> |  

2. Find `character_dict_path` in the .yml file and download the dict.txt needed for recognition  [https://github.com/PaddlePaddle/PaddleOCR/blob/main/configs/rec/PP-OCRv6/PP-OCRv6_medium_rec.yml]  
   | [File page](https://github.com/PaddlePaddle/PaddleOCR/blob/main/configs/rec/PP-OCRv6/PP-OCRv6_medium_rec.yml) |
   | :---: |
   | <img width="1113" height="766" alt="image" src="https://github.com/user-attachments/assets/a80119e7-5dca-47a5-80e2-ff2afa1f37a8" /> |
