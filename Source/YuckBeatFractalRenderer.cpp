#include "YuckBeatFractalRenderer.h"

#include <algorithm>
#include <cmath>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Vec3
{
	float x {};
	float y {};
	float z {};
};

Vec3 operator+ (Vec3 lhs, Vec3 rhs) noexcept { return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z}; }
Vec3 operator- (Vec3 lhs, Vec3 rhs) noexcept { return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z}; }
Vec3 operator- (Vec3 value) noexcept { return {-value.x, -value.y, -value.z}; }
Vec3 operator* (Vec3 lhs, Vec3 rhs) noexcept { return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z}; }
Vec3 operator* (Vec3 lhs, float rhs) noexcept { return {lhs.x * rhs, lhs.y * rhs, lhs.z * rhs}; }
Vec3 operator* (float lhs, Vec3 rhs) noexcept { return rhs * lhs; }
Vec3 operator/ (Vec3 lhs, float rhs) noexcept { return {lhs.x / rhs, lhs.y / rhs, lhs.z / rhs}; }

float saturate (float value) noexcept
{
	return std::clamp (value, 0.0f, 1.0f);
}

float mix (float a, float b, float t) noexcept
{
	return a + (b - a) * t;
}

Vec3 mix (Vec3 a, Vec3 b, float t) noexcept
{
	return a + (b - a) * t;
}

float dot (Vec3 lhs, Vec3 rhs) noexcept
{
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float length (Vec3 value) noexcept
{
	return std::sqrt (std::max (0.0f, dot (value, value)));
}

Vec3 normalize (Vec3 value) noexcept
{
	const auto len = length (value);
	if (len <= 0.000001f)
		return {};

	return value / len;
}

Vec3 absVec (Vec3 value) noexcept
{
	return {std::fabs (value.x), std::fabs (value.y), std::fabs (value.z)};
}

Vec3 rotateX (Vec3 value, float angle) noexcept
{
	const auto c = std::cos (angle);
	const auto s = std::sin (angle);
	return {value.x, value.y * c - value.z * s, value.y * s + value.z * c};
}

Vec3 rotateY (Vec3 value, float angle) noexcept
{
	const auto c = std::cos (angle);
	const auto s = std::sin (angle);
	return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

Vec3 rotateZ (Vec3 value, float angle) noexcept
{
	const auto c = std::cos (angle);
	const auto s = std::sin (angle);
	return {value.x * c - value.y * s, value.x * s + value.y * c, value.z};
}

Vec3 hsvToRgb (float hue, float saturation, float value) noexcept
{
	hue -= std::floor (hue);
	const auto r = saturate (std::fabs (hue * 6.0f - 3.0f) - 1.0f);
	const auto g = saturate (2.0f - std::fabs (hue * 6.0f - 2.0f));
	const auto b = saturate (2.0f - std::fabs (hue * 6.0f - 4.0f));
	return mix ({1.0f, 1.0f, 1.0f}, {r, g, b}, saturation) * value;
}

float fractalPattern (Vec3 point, const FractalRenderParams& params) noexcept
{
	const auto fold = 0.28f + saturate (params.fold) * 1.15f;
	const auto power = 2.8f + saturate (params.power) * 7.2f;
	const auto scale = 1.22f + saturate (params.scale) * 1.65f;
	const auto shape = saturate (params.shape);
	Vec3 q = point;
	float signal = 0.0f;
	float amplitude = 0.5f;

	for (int i = 0; i < 7; ++i)
	{
		q = absVec (q) - Vec3 {fold, fold * (0.82f + shape * 0.42f),
		                       fold * (1.16f - shape * 0.32f)};
		q = rotateY (q, 0.23f + shape * 0.71f + params.time * 0.035f);
		q = rotateZ (q, 0.31f + static_cast<float> (i) * 0.29f + params.bpmPulse * 0.14f);

		const auto radius = length (q);
		signal += std::sin (radius * power + static_cast<float> (i) * 1.37f +
		                    params.audioDrive * 2.6f) *
		          amplitude;

		const auto foldBoost = 1.0f / std::max (0.22f, dot (q, q));
		q = q * (scale * (0.72f + foldBoost * 0.10f)) + point * (0.20f + shape * 0.23f);
		amplitude *= 0.55f;
	}

	return signal;
}

float distanceField (Vec3 point, const FractalRenderParams& params) noexcept
{
	const auto time = params.time * (0.18f + saturate (params.spin) * 1.45f);
	point = rotateY (point, time + params.bpmPulse * 0.35f);
	point = rotateX (point, time * 0.37f + saturate (params.shape) * 0.5f);
	point = rotateZ (point, saturate (params.fold) * 1.15f);

	const auto objectSize = 0.72f + saturate (params.size) * 0.72f;
	const auto pattern = fractalPattern (point / objectSize, params);
	const auto surfaceRipple = pattern * (0.08f + saturate (params.power) * 0.055f);
	const auto sphere = length (point) - (objectSize + surfaceRipple);
	const auto box =
	    std::max ({std::fabs (point.x), std::fabs (point.y), std::fabs (point.z)}) -
	    (objectSize * 0.72f + surfaceRipple * 0.55f);

	return mix (sphere, box, saturate (params.shape) * 0.42f);
}

Vec3 estimateNormal (Vec3 point, const FractalRenderParams& params) noexcept
{
	constexpr float epsilon = 0.004f;
	const auto x = distanceField (point + Vec3 {epsilon, 0.0f, 0.0f}, params) -
	               distanceField (point - Vec3 {epsilon, 0.0f, 0.0f}, params);
	const auto y = distanceField (point + Vec3 {0.0f, epsilon, 0.0f}, params) -
	               distanceField (point - Vec3 {0.0f, epsilon, 0.0f}, params);
	const auto z = distanceField (point + Vec3 {0.0f, 0.0f, epsilon}, params) -
	               distanceField (point - Vec3 {0.0f, 0.0f, epsilon}, params);
	return normalize ({x, y, z});
}

float ambientOcclusion (Vec3 point, Vec3 normal, const FractalRenderParams& params) noexcept
{
	float occlusion = 0.0f;
	float scale = 1.0f;
	for (int i = 1; i <= 5; ++i)
	{
		const auto distance = 0.035f * static_cast<float> (i);
		const auto sample = distanceField (point + normal * distance, params);
		occlusion += (distance - sample) * scale;
		scale *= 0.55f;
	}

	return saturate (1.0f - occlusion * (0.65f + saturate (params.ao) * 3.2f));
}

Vec3 shadeSurface (Vec3 point, Vec3 normal, Vec3 rayDirection, float travel,
                   const FractalRenderParams& params) noexcept
{
	const auto hue = saturate (params.hue);
	const auto pattern = fractalPattern (point * (1.35f + saturate (params.scale)), params);
	const auto paletteA = hsvToRgb (hue + pattern * 0.025f, 0.72f, 0.94f);
	const auto paletteB = hsvToRgb (hue + 0.38f + saturate (params.shape) * 0.20f, 0.55f, 0.72f);
	const auto albedo = mix (paletteA, paletteB, 0.35f + 0.35f * saturate (params.audioDrive));

	const auto lightOrbit = params.time * 0.23f + saturate (params.light) * kPi * 1.2f;
	const auto lightDirection =
	    normalize ({std::cos (lightOrbit) * 0.65f, 0.75f + saturate (params.light) * 0.70f,
	                -0.55f + std::sin (lightOrbit) * 0.35f});
	const auto viewDirection = normalize (-rayDirection);
	const auto halfVector = normalize (lightDirection + viewDirection);

	const auto roughness = 0.08f + saturate (params.roughness) * 0.82f;
	const auto metallic = 0.03f + saturate (params.shape) * 0.35f;
	const auto nDotL = saturate (dot (normal, lightDirection));
	const auto nDotV = saturate (dot (normal, viewDirection));
	const auto nDotH = saturate (dot (normal, halfVector));
	const auto vDotH = saturate (dot (viewDirection, halfVector));

	const auto specPower = mix (96.0f, 7.0f, roughness);
	const auto normalDistribution = std::pow (nDotH, specPower) * (1.0f - roughness * 0.62f);
	const auto fresnel = std::pow (1.0f - vDotH, 5.0f);
	const auto specular = (0.04f + metallic * 0.66f + fresnel * 0.44f) * normalDistribution;
	const auto ao = ambientOcclusion (point, normal, params);

	const auto bounce = saturate (normal.y * 0.5f + 0.5f);
	const auto ambient = mix (hsvToRgb (hue + 0.58f, 0.45f, 0.10f),
	                          hsvToRgb (hue + 0.08f, 0.38f, 0.26f), bounce) *
	                     ao;
	const auto diffuse = albedo * (nDotL * (0.85f + saturate (params.light) * 1.25f)) * ao;
	const auto rim = std::pow (1.0f - nDotV, 2.2f) * (0.20f + saturate (params.bloom) * 0.45f);
	const auto fog = saturate (travel / 7.0f) * (0.10f + saturate (params.rays) * 0.18f);

	return ambient + diffuse * (1.0f - metallic * 0.35f) + Vec3 {specular, specular, specular} +
	       hsvToRgb (hue + 0.18f, 0.65f, rim) + hsvToRgb (hue + 0.08f, 0.30f, fog);
}

std::uint32_t packRgb (Vec3 color, float bypass) noexcept
{
	color.x = color.x / (1.0f + color.x);
	color.y = color.y / (1.0f + color.y);
	color.z = color.z / (1.0f + color.z);

	color.x = std::pow (saturate (color.x), 1.0f / 2.2f);
	color.y = std::pow (saturate (color.y), 1.0f / 2.2f);
	color.z = std::pow (saturate (color.z), 1.0f / 2.2f);

	if (bypass > 0.5f)
	{
		const auto gray = dot (color, {0.299f, 0.587f, 0.114f});
		color = mix (color, {gray, gray, gray}, 0.78f) * 0.55f;
	}

	const auto r = static_cast<std::uint32_t> (saturate (color.x) * 255.0f + 0.5f);
	const auto g = static_cast<std::uint32_t> (saturate (color.y) * 255.0f + 0.5f);
	const auto b = static_cast<std::uint32_t> (saturate (color.z) * 255.0f + 0.5f);
	return (r << 16U) | (g << 8U) | b;
}

} // namespace

void renderFractal (const FractalRenderParams& params, std::uint32_t* pixels) noexcept
{
	if (!pixels)
		return;

	const auto hue = saturate (params.hue);
	const auto lightScreen =
	    Vec3 {std::cos (params.time * 0.19f + params.light * kPi) * 0.42f,
	          0.22f + std::sin (params.time * 0.13f) * 0.18f, 0.0f};

	for (int y = 0; y < FractalRenderHeight; ++y)
	{
		for (int x = 0; x < FractalRenderWidth; ++x)
		{
			const auto uv =
			    Vec3 {(static_cast<float> (x) + 0.5f) / static_cast<float> (FractalRenderWidth) *
			              2.0f -
			          1.0f,
			          1.0f - (static_cast<float> (y) + 0.5f) /
			                     static_cast<float> (FractalRenderHeight) * 2.0f,
			          0.0f};

			const auto vignette = 1.0f - saturate (length ({uv.x, uv.y, 0.0f}) * 0.72f);
			auto color = mix (Vec3 {0.010f, 0.014f, 0.016f}, hsvToRgb (hue + 0.60f, 0.58f, 0.16f),
			                  vignette * 0.65f);

			const auto lightDelta = uv - lightScreen;
			const auto rayComb =
			    std::pow (saturate (1.0f - length (lightDelta) * 0.58f), 2.7f) *
			    (0.45f + 0.55f * std::fabs (std::sin ((uv.x * 19.0f - uv.y * 15.0f) +
			                                           params.time * 0.75f)));
			color = color + hsvToRgb (hue + 0.10f, 0.42f, rayComb * saturate (params.rays) * 0.35f);

			const auto zoom = 3.85f + (1.0f - saturate (params.size)) * 0.55f;
			const auto rayOrigin = Vec3 {0.0f, 0.0f, -zoom};
			auto rayDirection = normalize ({uv.x * 1.05f, uv.y, 1.42f});
			rayDirection = rotateZ (rayDirection, params.bpmPulse * 0.030f);

			float travel = 0.0f;
			float glow = 0.0f;
			bool hit = false;
			Vec3 hitPoint {};

			for (int step = 0; step < 54; ++step)
			{
				const auto point = rayOrigin + rayDirection * travel;
				const auto distance = distanceField (point, params);
				glow += std::exp (-std::fabs (distance) * 18.0f) * 0.0065f *
				        (0.45f + saturate (params.bloom));

				if (distance < 0.0027f)
				{
					hit = true;
					hitPoint = point;
					break;
				}

				travel += std::clamp (distance * 0.72f, 0.006f, 0.16f);
				if (travel > 7.2f)
					break;
			}

			if (hit)
			{
				const auto normal = estimateNormal (hitPoint, params);
				color = color + shadeSurface (hitPoint, normal, rayDirection, travel, params);
			}

			color = color + hsvToRgb (hue + 0.18f, 0.52f,
			                          glow * (0.70f + saturate (params.bloom) * 1.75f));
			color = color * (0.70f + vignette * 0.45f);

			pixels[y * FractalRenderWidth + x] = packRgb (color, params.bypass);
		}
	}
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
