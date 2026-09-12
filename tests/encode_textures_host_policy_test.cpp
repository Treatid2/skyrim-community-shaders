#include "Features/Upscaling/EncodeTexturesHostPolicy.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

int main()
{
	using EncodeTexturesHostPolicy::NormalizeSamplingDimension;

	for (const auto [input, expected] : {
			 std::pair{ 1.0f, 1u },
			 std::pair{ 6.999999523162842f, 7u },
			 std::pair{ 7.000000476837158f, 7u },
			 std::pair{ 2023.0001220703125f, 2023u },
			 std::pair{ 16384.0f, 16384u },
		 }) {
		const auto actual = NormalizeSamplingDimension(input);
		if (!actual || *actual != expected)
			throw std::runtime_error("valid sampling dimension was rejected");
	}

	for (const auto [screenDimension, scale] : {
			 std::pair{ 3440.0f, 1.0f / 1.7f },
			 std::pair{ 720.0f, 1.0f / 1.7f },
			 std::pair{ 3840.0f, 1.0f / 3.0f },
		 }) {
		const auto renderDimension =
			static_cast<std::uint32_t>(screenDimension * scale);
		const float storedRatio =
			static_cast<float>(renderDimension) / screenDimension;
		const auto normalized = NormalizeSamplingDimension(
			screenDimension * storedRatio);
		if (!normalized || *normalized != renderDimension)
			throw std::runtime_error("dynamic-resolution round trip changed pixel extent");
	}

	for (const float input : {
			 0.0f,
			 -1.0f,
			 7.25f,
			 16385.0f,
			 std::numeric_limits<float>::infinity(),
			 std::numeric_limits<float>::quiet_NaN(),
		 }) {
		if (NormalizeSamplingDimension(input))
			throw std::runtime_error("invalid sampling dimension was accepted");
	}
}
