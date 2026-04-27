#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Yin.h"

static void Yin_difference(Yin *yin, int16_t* buffer){
	int16_t i;
	int16_t tau;
	float delta;

	for(tau = 0 ; tau < yin->halfBufferSize; tau++){
		for(i = 0; i < yin->halfBufferSize; i++){
			delta = (float)buffer[i] - (float)buffer[i + tau];
			yin->yinBuffer[tau] += delta * delta;
		}
	}
}

static void Yin_cumulativeMeanNormalizedDifference(Yin *yin){
	int16_t tau;
	float runningSum = 0;
	yin->yinBuffer[0] = 1;

	for (tau = 1; tau < yin->halfBufferSize; tau++) {
		runningSum += yin->yinBuffer[tau];
		if (runningSum == 0) {
			yin->yinBuffer[tau] = 1;
		} else {
			yin->yinBuffer[tau] *= tau / runningSum;
		}
	}
}

static int16_t Yin_absoluteThreshold(Yin *yin){
	int16_t tau;

	for (tau = 2; tau < yin->halfBufferSize ; tau++) {
		if (yin->yinBuffer[tau] < yin->threshold) {
			while (tau + 1 < yin->halfBufferSize && yin->yinBuffer[tau + 1] < yin->yinBuffer[tau]) {
				tau++;
			}
			yin->probability = 1 - yin->yinBuffer[tau];
			break;
		}
	}

	if (tau == yin->halfBufferSize || yin->yinBuffer[tau] >= yin->threshold) {
		tau = -1;
		yin->probability = 0;
	}

	return tau;
}

static float Yin_parabolicInterpolation(Yin *yin, int16_t tauEstimate) {
	float betterTau;
	int16_t x0;
	int16_t x2;

	if (tauEstimate < 1) x0 = tauEstimate;
	else x0 = tauEstimate - 1;

	if (tauEstimate + 1 < yin->halfBufferSize) x2 = tauEstimate + 1;
	else x2 = tauEstimate;

	if (x0 == tauEstimate) {
		betterTau = (yin->yinBuffer[tauEstimate] <= yin->yinBuffer[x2]) ? tauEstimate : x2;
	} 
	else if (x2 == tauEstimate) {
		betterTau = (yin->yinBuffer[tauEstimate] <= yin->yinBuffer[x0]) ? tauEstimate : x0;
	} 
	else {
		float s0 = yin->yinBuffer[x0];
		float s1 = yin->yinBuffer[tauEstimate];
		float s2 = yin->yinBuffer[x2];
		betterTau = tauEstimate + (s2 - s0) / (2 * (2 * s1 - s2 - s0));
	}

	return betterTau;
}

void Yin_init(Yin *yin, int16_t bufferSize, float threshold){
	yin->bufferSize = bufferSize;
	yin->halfBufferSize = bufferSize / 2;
	yin->probability = 0.0f;
	yin->threshold = threshold;

	yin->yinBuffer = (float *) malloc(sizeof(float) * (size_t)yin->halfBufferSize);
	if (yin->yinBuffer) {
		memset(yin->yinBuffer, 0, sizeof(float) * (size_t)yin->halfBufferSize);
	}
}

float Yin_getPitch(Yin *yin, int16_t* buffer){
	int16_t tauEstimate = -1;
	float pitchInHertz = -1;

	if (!yin || !yin->yinBuffer) return -1;

	// CRITICAL: clear working buffer each run (original code accumulated forever)
	memset(yin->yinBuffer, 0, sizeof(float) * (size_t)yin->halfBufferSize);

	Yin_difference(yin, buffer);
	Yin_cumulativeMeanNormalizedDifference(yin);
	tauEstimate = Yin_absoluteThreshold(yin);

	if(tauEstimate != -1){
		pitchInHertz = YIN_SAMPLING_RATE / Yin_parabolicInterpolation(yin, tauEstimate);
	}

	return pitchInHertz;
}

float Yin_getProbability(Yin *yin){
	return yin ? yin->probability : 0.0f;
}
