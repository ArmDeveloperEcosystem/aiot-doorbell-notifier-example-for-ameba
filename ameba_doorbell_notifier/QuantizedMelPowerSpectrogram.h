// 
// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: MIT
//

extern "C" {
#undef A0
#undef A1
#undef A2

#include <arm_math.h>
}

class QuantizedMelPowerSpectrogram {
  public:
    QuantizedMelPowerSpectrogram(int width, int numMelBins, int frameLength, int frameStep, int fftSize, int topDb, const void* weightMatrix) :
      _width(width),
      _numMelBins(numMelBins),
      _frameLength(frameLength),
      _frameStep(frameStep),
      _fftSize(fftSize),
      _topDb(topDb),
      _weightMatrix((const float*)weightMatrix),
      _fftMagSize(fftSize / 2 + 1),
      _inputScale(1.0),
      _outputScale(1.0),
      _outputZeroPoint(0),
      _data(nullptr),
      _audioBuffer(nullptr),
      _audioBufferIndex(0),
      _window(nullptr)
    {
    }

    ~QuantizedMelPowerSpectrogram()
    {
    }

    int begin()
    {
      _data = new float[_width * _numMelBins];
      if (_data == nullptr) {
        return 0;
      }
      memset(_data, 0x00, _width * _numMelBins);

      _audioBuffer = new float[_frameLength];
      if (_audioBuffer == nullptr) {
        return 0;
      }
      memset(_audioBuffer, 0x00, sizeof(_audioBuffer[0]) * _frameStep);
      _audioBufferIndex = 0;

      _window = new float[_frameLength];
      if (_window == nullptr) {
        return 0;
      }
      // calulate the Hanning Window
      for (int i = 0; i < _frameLength; i++) {
        _window[i] = 0.5 * (1.0 - arm_cos_f32(2 * PI * i / _frameLength));
      }

      if (arm_rfft_fast_init_f32(&_rfft, _fftSize) != ARM_MATH_SUCCESS) {
        return 0;
      }

      return 1;
    }

    void end()
    {
      if (_window != nullptr) {
        delete [] _window;

        _window = nullptr;
      }

      if (_audioBuffer != nullptr) {
        delete [] _audioBuffer;

        _audioBuffer = nullptr;
      }

      if (_data != nullptr) {
        delete [] _data;

        _data = nullptr;
      }
    }

    void setInputScale(float inputScale) {
      _inputScale = inputScale;
    }

    void setOutputScale(float scale) {
      _outputScale = scale;
    }

    void setOutputZeroPoint(int zeroPoint) {
      _outputZeroPoint = zeroPoint;
    }

    void write(const int16_t samples[], int count) {
      float fSamples[count + _frameLength];

      // copy the last samples from last write to the start
      memcpy(fSamples, _audioBuffer, sizeof(_audioBuffer[0]) * _audioBufferIndex);

      // copy the new samples with scaling
      const int16_t* in16 = samples;
      float* outF = fSamples + _audioBufferIndex;

      for (int i = 0; i < count; i++) {
        *outF++ = (*in16++ / float(1 << 15)) * _inputScale;
      }

      // calculate the number of columns to shift the previous spectrogram data by, and shift the data
      int shiftColumns = (count + _audioBufferIndex - (_frameLength - _frameStep)) / _frameStep;
      memmove(_data, _data + (shiftColumns * _numMelBins), sizeof(_data[0]) * (_width - shiftColumns) * _numMelBins);

      const float* fIn = fSamples;
      outF = _data + (_width - shiftColumns) * _numMelBins;

      // calculate the new spectrogram column values
      for (int i = 0; i < shiftColumns; i++) {
        float windowedInput[_fftSize];
        float fft[_fftSize * 2];
        float fftMag[_fftMagSize];

        // apply the Hanning Window to the input
        arm_mult_f32(_window, (float*)fIn, windowedInput, _fftSize);

        // calculate the FFT of the windowed input
        arm_rfft_fast_f32(&_rfft, windowedInput, fft, 0);

        // calculate the magnitude of the FFT
        arm_cmplx_mag_f32(fft, fftMag, _fftMagSize);
        fftMag[0] = fabs(fft[0]);
        fftMag[_fftSize / 2] = fabs(fft[1]);

        const float* fMelWeightMatrix = _weightMatrix;

        // calculate the Mel value for each Mel bin
        for (int j = 0; j < _numMelBins; j++) {
          const float* fFftMag = fftMag;
          float mel = 0;

          // perform a dot product of the FFT magniture with the Mel weight matrix
          for (int k = 0; k < _fftMagSize; k++) {
            mel += (*fMelWeightMatrix * *fFftMag);

            fMelWeightMatrix++;
            fFftMag++;
          }

          // calculate the mel power and cap above the threshold
          float melPower = mel * mel;
          if (melPower < 1e-6) {
            melPower = 1e-6;
          }

          *outF++ = 10.0 * logf(melPower) / logf(10.0);
        }

        fIn += _frameStep;
      }

      // copy the left over samples for the next write
      _audioBufferIndex = (count + _audioBufferIndex) - (shiftColumns * _frameStep);

      memcpy(_audioBuffer, fSamples + (shiftColumns * _frameStep), sizeof(fSamples[0]) * _audioBufferIndex);
    }

    void read(int8_t* buffer, size_t count) const {
      float maxDb;
      uint32_t maxDbIndex;

      const float* fIn = _data;
      arm_max_f32(_data, _width * _numMelBins, &maxDb, &maxDbIndex);

      float minOut = maxDb - _topDb;

      for (size_t i = 0; i < count; i++) {
        float out = *fIn++;
        if (out < minOut) {
          out = minOut;
        }

        *buffer++ = __SSAT((out / _outputScale) + _outputZeroPoint, 8);
      }
    }

    void clear() {
      // reset the spectrogram data to 0.0's
      memset(_data, 0x00, _width * _numMelBins);

      // reset the audio buffer index to 0
      _audioBufferIndex = 0;
    }

  private:
    int _width;
    int _numMelBins;
    int _frameLength;
    int _frameStep;
    int _fftSize;
    int _topDb;
    const float* _weightMatrix;
    int _fftMagSize;

    float _inputScale;
    float _outputScale;
    int32_t _outputZeroPoint;

    float* _data;
    float* _audioBuffer;
    int _audioBufferIndex;
    float* _window;

    arm_rfft_fast_instance_f32 _rfft;
};
