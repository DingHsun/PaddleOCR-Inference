#include"text_rec.h"

TextRecognizer::TextRecognizer(string model_path, string dict_path)
{
	std::wstring widestr = std::wstring(model_path.begin(), model_path.end());
	//OrtStatus* status = OrtSessionOptionsAppendExecutionProvider_CUDA(sessionOptions, 0);
	sessionOptions.SetGraphOptimizationLevel(ORT_ENABLE_BASIC);
	net = new Session(env, widestr.c_str(), sessionOptions);
	size_t numInputNodes = net->GetInputCount();
	size_t numOutputNodes = net->GetOutputCount();
	AllocatorWithDefaultOptions allocator;
	for (int i = 0; i < numInputNodes; i++)
	{
		Ort::AllocatedStringPtr input_name_ptr = net->GetInputNameAllocated(i, allocator);
		input_names.push_back(std::string(input_name_ptr.get()));
	}

	for (int i = 0; i < numOutputNodes; i++)
	{
		Ort::AllocatedStringPtr output_name_ptr = net->GetOutputNameAllocated(i, allocator);
		output_names.push_back(std::string(output_name_ptr.get()));
	}

	ifstream ifs(dict_path);
	string line;
	while (getline(ifs, line))
	{
		this->alphabet.push_back(line);
	}
	this->alphabet.push_back(" ");
	names_len = this->alphabet.size();
}

Mat TextRecognizer::preprocess(Mat srcimg)
{
	Mat dstimg;
	int h = srcimg.rows;
	int w = srcimg.cols;
	const float ratio = w / float(h);
	int resized_w = int(ceil((float)this->inpHeight * ratio));
	if (ceil(this->inpHeight * ratio) > this->inpWidth)
	{
		resized_w = this->inpWidth;
	}

	resize(srcimg, dstimg, Size(resized_w, this->inpHeight), INTER_LINEAR);
	return dstimg;
}

void TextRecognizer::normalize_(Mat img)
{
	//    img.convertTo(img, CV_32F);
	int row = img.rows;
	int col = img.cols;
	this->input_image_.resize(this->inpHeight * this->inpWidth * img.channels());
	for (int c = 0; c < 3; c++)
	{
		for (int i = 0; i < row; i++)
		{
			for (int j = 0; j < inpWidth; j++)
			{
				if (j < col)
				{
					float pix = img.ptr<uchar>(i)[j * 3 + c];
					this->input_image_[c * row * inpWidth + i * inpWidth + j] = (pix / 255.0 - 0.5) / 0.5;
				}
				else
				{
					this->input_image_[c * row * inpWidth + i * inpWidth + j] = 0;
				}
			}
		}
	}
}

string TextRecognizer::predict_text(Mat cv_image)
{
	Mat dstimg = this->preprocess(cv_image);
	this->normalize_(dstimg);

	array<int64_t, 4> input_shape_{ 1, 3, this->inpHeight, this->inpWidth };

	auto allocator_info = MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
	Value input_tensor_ = Value::CreateTensor<float>(allocator_info, input_image_.data(), input_image_.size(), input_shape_.data(), input_shape_.size());

	// 1. 建立專屬的 IoBinding 物件
	Ort::IoBinding io_binding(*net);

	// 2. 綁定輸入
	io_binding.BindInput(input_names[0].c_str(), input_tensor_);

	// 3. ✨ 修正：使用你擁有的 GetTensorMemoryInfo 取得記憶體資訊指標
	// 注意：因為它回傳的是 const OrtMemoryInfo*，我們可以直接拿去餵給 BindOutput
	const OrtMemoryInfo* memory_info = input_tensor_.GetTensorMemoryInfo();

	for (const auto& out_name : output_names) {
		// 改用 BindOutput 的基本多載，傳入輸出名稱與指定的記憶體資訊
		io_binding.BindOutput(out_name.c_str(), memory_info);
	}

	// 4. 呼叫 1.21.0 的 2 參數 Run 函式
	net->Run(Ort::RunOptions{ nullptr }, io_binding);

	// 5. 取出結果
	std::vector<Ort::Value> ort_outputs = io_binding.GetOutputValues();

	const float* pdata = ort_outputs[0].GetTensorMutableData<float>();

	int i = 0, j = 0;
	int h = ort_outputs.at(0).GetTensorTypeAndShapeInfo().GetShape().at(2);
	int w = ort_outputs.at(0).GetTensorTypeAndShapeInfo().GetShape().at(1);

	preb_label.resize(w);
	for (i = 0; i < w; i++)
	{
		int one_label_idx = 0;
		float max_data = -10000;
		for (j = 0; j < h; j++)
		{
			float data_ = pdata[i * h + j];
			if (data_ > max_data)
			{
				max_data = data_;
				one_label_idx = j;
			}
		}
		preb_label[i] = one_label_idx;
	}

	vector<int> no_repeat_blank_label;
	for (size_t elementIndex = 0; elementIndex < w; ++elementIndex)
	{
		if (preb_label[elementIndex] != 0 && !(elementIndex > 0 && preb_label[elementIndex - 1] == preb_label[elementIndex]))
		{
			no_repeat_blank_label.push_back(preb_label[elementIndex] - 1);
		}
	}

	int len_s = no_repeat_blank_label.size();
	string plate_text;
	for (i = 0; i < len_s; i++)
	{
		plate_text += alphabet[no_repeat_blank_label[i]];
	}

	return plate_text;
}
