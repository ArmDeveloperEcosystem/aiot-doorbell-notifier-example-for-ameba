// 
// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: MIT
//

#include <TensorFlowLite.h>

#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>

class Model {
  public:
    Model(const unsigned char* tfLiteModel, int tensorArenaSize) :
      _tfLiteModel(tfLiteModel),
      _tensorArenaSize(tensorArenaSize),
      _tensorArena(nullptr),
      _tfInterpreter(nullptr)
    {
    }

    ~Model() {
    }

    int begin() {
      _tensorArena = new unsigned char[_tensorArenaSize];
      if (_tensorArena == nullptr) {
        return 0;
      }

      _tfInterpreter = new tflite::MicroInterpreter(
        tflite::GetModel(_tfLiteModel),
        _tfOpsResolver,
        _tensorArena,
        _tensorArenaSize,
        &_tfMicroErrorReporter
      );

      if (_tfInterpreter->AllocateTensors() != kTfLiteOk) {
        return 0;
      }

      return 1;
    }

    void end() {
      if (_tfInterpreter != nullptr) {
        delete _tfInterpreter;

        _tfInterpreter = nullptr;
      }

      if (_tensorArena != nullptr) {
        delete [] _tensorArena;

        _tensorArena = nullptr;
      }
    }

    float inputScale() const {
      return _tfInterpreter->input(0)->params.scale;
    }

    int32_t inputZeroPoint() const {
      return _tfInterpreter->input(0)->params.zero_point;
    }

    int8_t* input() const {
      TfLiteTensor* inputTensor = _tfInterpreter->input(0);

      return inputTensor->data.int8;
    }

    size_t inputBytes() const {
      TfLiteTensor* inputTensor = _tfInterpreter->input(0);

      return inputTensor->bytes;
    }

    int numOutputs() const {
      TfLiteTensor* outputTensor = _tfInterpreter->output(0);

      return outputTensor->dims->data[1];
    }

    void predict(float predictions[]) {
      TfLiteTensor* outputTensor = _tfInterpreter->output(0);

      if (_tfInterpreter->Invoke() != kTfLiteOk) {
        for (int i = 0; i < _tfInterpreter->output(0)->dims[0].data[1]; i++)  {
          predictions[i] = NAN;
        }
        return;
      }

      for (int i = 0; i < outputTensor->dims->data[1]; i++)  {
        float y_quantized = outputTensor->data.int8[i];
        float y = (y_quantized - outputTensor->params.zero_point) * outputTensor->params.scale;

        predictions[i] = y;
      }
    }

  private:
    const unsigned char* _tfLiteModel;
    int _tensorArenaSize;

    uint8_t* _tensorArena;
    tflite::MicroInterpreter* _tfInterpreter;

    tflite::MicroErrorReporter _tfMicroErrorReporter;
    tflite::AllOpsResolver _tfOpsResolver;
};
