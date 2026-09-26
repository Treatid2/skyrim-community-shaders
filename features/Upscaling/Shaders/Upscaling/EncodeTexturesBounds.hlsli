#ifndef ENCODE_TEXTURES_BOUNDS_HLSLI
#define ENCODE_TEXTURES_BOUNDS_HLSLI

static const float kEncodeTextureMax2DDimension = 16384.0;

/// @brief Validate the dispatch-uniform full-eye sampling contract once.
bool IsEncodeTextureSourceSamplingContractValid(
	float2 trueSamplingDim,
	uint2 sourceSamplingXBounds)
{
	if (!all(isfinite(trueSamplingDim)) ||
		any(trueSamplingDim < 1.0) ||
		any(trueSamplingDim > kEncodeTextureMax2DDimension) ||
		any(trueSamplingDim != floor(trueSamplingDim))) {
		return false;
	}

	uint2 samplingDim = uint2(trueSamplingDim);
	bool fullWidth = sourceSamplingXBounds.x == 0 && sourceSamplingXBounds.y == samplingDim.x;
	uint eyeWidth = samplingDim.x >> 1;
	bool packedEye = (samplingDim.x & 1) == 0 && eyeWidth != 0 &&
	                 ((sourceSamplingXBounds.x == 0 && sourceSamplingXBounds.y == eyeWidth) ||
						 (sourceSamplingXBounds.x == eyeWidth && sourceSamplingXBounds.y == samplingDim.x));
	return sourceSamplingXBounds.x < sourceSamplingXBounds.y && (fullWidth || packedEye);
}

/// @brief Test full-eye source bounds, including valid donors outside a foveated crop.
/// @pre contractValid proves integral dimensions and a full-width or packed-eye interval.
bool IsEncodeTextureSourceSampleInBounds(
	int2 samplePos,
	int2 trueSamplingDim,
	uint2 sourceSamplingXBounds,
	bool contractValid)
{
	if (!contractValid)
		return false;

	// Unsigned deltas fold each lower/upper pair into one comparison.
	return uint(samplePos.y) < uint(trueSamplingDim.y) &&
	       uint(samplePos.x - int(sourceSamplingXBounds.x)) <
	           sourceSamplingXBounds.y - sourceSamplingXBounds.x;
}

#endif  // ENCODE_TEXTURES_BOUNDS_HLSLI
