# PaddleOCR C++ Inference

[![English](https://img.shields.io/badge/README-English-blue?style=for-the-badge)](README-EN.md)

PaddleOCR 的 C++ 推理實作，使用 onnxruntime 與 opencv，可運行 Windows x64 版本。

**提供兩種 OCR 功能**
1. 全圖識別（文字位置＋文字內容）
2. 選擇 ROI 範圍進行辨識

**提供兩種執行方式**
1. `demo` - 有 GUI 視窗、可框選 ROI 的互動式主程式，適合用 Visual Studio 直接按 F5 呼叫 main() 測試
2. `api_server` - 無 GUI 的常駐 exe，啟動後開 HTTP API（`POST /ocr_detect`、`POST /ocr_recognize`），方便其他服務用 API 呼叫辨識結果

## 專案結構

```
src/
  core/        text_det / text_rec 共用 OCR 邏輯 (demo 與 api_server 共用同一份)
  demo/        main.cpp - GUI 互動式 demo
  api_server/  main.cpp - HTTP API server (POST /ocr)
weights/       onnx 模型與字典 (demo、api_server 共用)
images/        測試圖片 (僅 demo 使用)
third_party/   httplib.h (api_server 用的單一標頭 HTTP 函式庫)
vs2022/        Visual Studio 2022 解決方案 (.slnx + 兩個 .vcxproj)
cmake/         CMakeLists.txt，供 VSCode (CMake Tools 擴充套件) 或其他 IDE 建置
packages/      opencv / onnxruntime 第三方 SDK (需自行下載，不進版控，見下方)
```

## 建置方式

**Visual Studio 2022**：開啟 `vs2022/PaddleOCR-cpp.slnx`，裡面有 `demo` 與 `api_server` 兩個專案，選其一設為啟動專案即可 F5。

**VSCode / CMake**：安裝 CMake Tools 擴充套件，開啟資料夾後選擇 `cmake/CMakeLists.txt` 設定，會產生 `demo` 與 `api_server` 兩個 target。opencv/onnxruntime 路徑預設抓 `packages/` 下的資料夾，也可用 `-DOPENCV_DIR=...` / `-DONNXRUNTIME_DIR=...` 覆寫。

兩種方式都會在編譯後自動把 opencv/onnxruntime 的 DLL 與 `weights/`（demo 另外加 `images/`）複製到輸出目錄。

## api_server 使用方式

```
api_server.exe [host] [port]   # 預設 host 0.0.0.0（監聽所有網卡）, port 8080
```

範例：`api_server.exe 127.0.0.1 9000` 只監聽本機的 9000 port。

- `GET /health` - 健康檢查
- `POST /ocr_detect` - body 放整張圖的原始 bytes（jpg/png/bmp...），只做文字偵測，回傳找到的文字框陣列：`[[[x,y],[x,y],[x,y],[x,y]], ...]`
- `POST /ocr_recognize` - body 放已裁切好的單行文字圖，只做文字辨識，回傳該圖辨識出的文字：`{"text":"..."}`

兩個 API 的 body 都必須是**圖片檔案的原始 binary 內容**，不能是 form-data、也不能是 base64 或包成 JSON。

- curl：用 `--data-binary @檔案路徑`（不要用 `-d`/`--data`，會把內容當文字處理），並且要用 `-H "Content-Type: application/octet-stream"` 明確指定內容類型——curl 沒指定的話預設會送 `application/x-www-form-urlencoded`，httplib 對這個 content type 有 8KB 大小限制，圖片超過就會被擋掉回 413

  ```bash
  # 文字偵測 (test_detection.bmp 是整張圖)
  curl -X POST -H "Content-Type: application/octet-stream" --data-binary @images/test_detection.bmp http://127.0.0.1:8080/ocr_detect

  # 文字辨識 (test_recognition.bmp 是已裁切好的單行文字圖)
  curl -X POST -H "Content-Type: application/octet-stream" --data-binary @images/test_recognition.bmp http://127.0.0.1:8080/ocr_recognize
  ```

- Postman / Insomnia：Body 分頁選 **binary**（不要選 form-data 或 raw），然後選檔案
- PowerShell：

  ```powershell
  # 文字偵測
  $bytes = [System.IO.File]::ReadAllBytes("images/test_detection.bmp")
  Invoke-RestMethod -Uri "http://127.0.0.1:8080/ocr_detect" -Method Post -Body $bytes -ContentType "application/octet-stream"

  # 文字辨識
  $bytes = [System.IO.File]::ReadAllBytes("images/test_recognition.bmp")
  Invoke-RestMethod -Uri "http://127.0.0.1:8080/ocr_recognize" -Method Post -Body $bytes -ContentType "application/octet-stream"
  ```

- Python：

  ```python
  import requests

  # 文字偵測
  with open("images/test_detection.bmp", "rb") as f:
      r = requests.post("http://127.0.0.1:8080/ocr_detect", data=f.read())
  print(r.json())

  # 文字辨識
  with open("images/test_recognition.bmp", "rb") as f:
      r = requests.post("http://127.0.0.1:8080/ocr_recognize", data=f.read())
  print(r.json())
  ```

body 格式不對時（例如送 JSON 或 form-data）server 會回 HTTP 400，並在 `hint` 欄位提示要送原始 binary：

```json
{
  "error": "could not decode image",
  "hint": "body must be the raw image bytes (jpg/png/bmp/...), not form-data or base64, e.g. curl -H \"Content-Type: application/octet-stream\" --data-binary @file.jpg"
}
```

## C++ Packages

- onnxruntime-win-x64-gpu-1.21.0 [ https://github.com/microsoft/onnxruntime/releases ]
- opencv 4.5.0 windows [ https://opencv.org/releases/ ]

放置路徑（與 `vs2022/`、`cmake/` 同層的專案根目錄）  
./packages  
 ├─ onnxruntime-win-x64-gpu-1.21.0  
 └─ opencv

## 模型選擇 (轉 .onnx)

PP-OCR系列模型列表
- [官方模型列表](http://www.paddleocr.ai/latest/version3.x/module_usage/module_overview.html)

Python環境設定
- python 3.10.10
- pip install paddle2onnx-2.0.2rc3
- 下載推理模型並解壓縮
- 執行以下指令將model轉onnx並放置到下列路徑(放置路徑 ./weights/)，自行修改路徑
- C:\Python31010-OCR-2onnx\Scripts\paddle2onnx.exe --model_dir "C:\Users\Users\Downloads\en_PP-OCRv5_mobile_rec_infer\en_PP-OCRv5_mobile_rec_infer" --model_filename inference.json --params_filename inference.pdiparams --save_file "C:\Users\Users\Downloads\en_PP-OCRv5_mobile_rec_infer\en_PP-OCRv5_mobile_rec_infer\model.onnx"

修改程式

- `src/demo/main.cpp`（GUI demo）或 `src/api_server/main.cpp`（API server）-
- TextDetector detect_model("det onnx model path");
- TextRecognizer rec_model("rec onnx model path", "rec dict.txt path");

## 文字辨識文本下載

範例：PP-OCRv6_medium_rec

1. 下載推理模型並執行轉 .onnx 步驟  [https://www.paddleocr.ai/latest/version3.x/module_usage/text_detection.html#_2]
   | [模型網址](https://www.paddleocr.ai/latest/version3.x/module_usage/text_detection.html#_2) |
   | :---: |
   | <img width="1084" height="692" alt="image" src="https://github.com/user-attachments/assets/b5f3e947-4b70-4580-9031-3656b2e57369" /> |

2. 尋找 .yml 檔案中的 character_dict_path 下載 recognition 需要的 dict.txt  [https://github.com/PaddlePaddle/PaddleOCR/blob/main/configs/rec/PP-OCRv6/PP-OCRv6_medium_rec.yml]
   | [檔案網址](https://github.com/PaddlePaddle/PaddleOCR/blob/main/configs/rec/PP-OCRv6/PP-OCRv6_medium_rec.yml) |
   | :---: |
   | <img width="1113" height="766" alt="image" src="https://github.com/user-attachments/assets/a80119e7-5dca-47a5-80e2-ff2afa1f37a8" /> |
