#include "SkyModel.h"
#include "Math/Constants.h"
#include "Utilities/HosekDataRGB.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace adria
{
	namespace
	{
		inline double EvaluateSpline(double const* spline, uint64 stride, double value)
		{
			return
				1 * pow(1 - value, 5) * spline[0 * stride] +
				5 * pow(1 - value, 4) * pow(value, 1) * spline[1 * stride] +
				10 * pow(1 - value, 3) * pow(value, 2) * spline[2 * stride] +
				10 * pow(1 - value, 2) * pow(value, 3) * spline[3 * stride] +
				5 * pow(1 - value, 1) * pow(value, 4) * spline[4 * stride] +
				1 * pow(value, 5) * spline[5 * stride];
		}
		double Evaluate(double const* dataset, uint64 stride, float turbidity, float albedo, float sun_theta)
		{
			// splines are functions of elevation^1/3
			double elevationK = pow(std::max<float>(0.f, 1.f - sun_theta / (pi<float> / 2.f)), 1.f / 3.0f);

			// table has values for turbidity 1..10
			int turbidity0		= std::clamp(static_cast<int>(turbidity), 1, 10);
			int turbidity1		= std::min(turbidity0 + 1, 10);
			float turbidityK	= std::clamp(turbidity - turbidity0, 0.f, 1.f);

			double const* datasetA0 = dataset;
			double const* datasetA1 = dataset + stride * 6 * 10;

			double a0t0 = EvaluateSpline(datasetA0 + stride * 6 * (turbidity0 - 1), stride, elevationK);
			double a1t0 = EvaluateSpline(datasetA1 + stride * 6 * (turbidity0 - 1), stride, elevationK);
			double a0t1 = EvaluateSpline(datasetA0 + stride * 6 * (turbidity1 - 1), stride, elevationK);
			double a1t1 = EvaluateSpline(datasetA1 + stride * 6 * (turbidity1 - 1), stride, elevationK);

			return a0t0 * (1.0f - albedo) * (1.0f - turbidityK) + a1t0 * albedo * (1.0f - turbidityK) + a0t1 * (1.0f - albedo) * turbidityK + a1t1 * albedo * turbidityK;
		}

		Vector3 HosekWilkie(float cos_theta, float gamma, float cos_gamma, 
			Vector3 const& A, Vector3 const& B, Vector3 const& C, Vector3 const& D, Vector3 const& E, Vector3 const& F, Vector3 const& G, Vector3 const& H, Vector3 const& I)
		{
			XMVECTOR _A = XMLoadFloat3(&A);
			XMVECTOR _B = XMLoadFloat3(&B);
			XMVECTOR _C = XMLoadFloat3(&C);
			XMVECTOR _D = XMLoadFloat3(&D);
			XMVECTOR _E = XMLoadFloat3(&E);
			XMVECTOR _F = XMLoadFloat3(&F);
			XMVECTOR _G = XMLoadFloat3(&G);
			XMVECTOR _H = XMLoadFloat3(&H);
			XMVECTOR _I = XMLoadFloat3(&I);

			XMVECTOR chi = XMVectorDivide(XMVectorReplicate(1.f + cos_gamma * cos_gamma), XMVectorPow(_H * _H + XMVectorReplicate(1.0f) - XMVectorScale(_H, 2.0f * cos_gamma), XMVectorReplicate(1.5)));
			XMVECTOR temp1 = _A * XMVectorExp(XMVectorScale(_B, 1.0f/(cos_theta + 0.01f)));
			XMVECTOR temp2 = _C + _D * XMVectorExp(XMVectorScale(_E, gamma)) + XMVectorScale(_F, gamma * gamma) + chi * _G + XMVectorScale(_I, (float)sqrt(std::max(0.f, cos_theta)));
			XMVECTOR result = temp1 * temp2;
			return result;
		}
	}

	SkyParameters CalculateSkyParameters(float turbidity, float albedo, Vector3 const& sun_direction)
	{
		float sun_theta = std::acos(std::clamp(-sun_direction.y, 0.f, 1.f));

		SkyParameters params{};
		for (uint64 i = 0; i < SkyParam_Z; ++i)
		{
			auto& param = params[i];
			param.x = (float)Evaluate(datasetsRGB[0] + i, 9, turbidity, albedo, sun_theta);
			param.y = (float)Evaluate(datasetsRGB[1] + i, 9, turbidity, albedo, sun_theta);
			param.z = (float)Evaluate(datasetsRGB[2] + i, 9, turbidity, albedo, sun_theta);
		}

		auto& paramZ = params[SkyParam_Z];
		paramZ.x = (float)Evaluate(datasetsRGBRad[0], 1, turbidity, albedo, sun_theta);
		paramZ.y = (float)Evaluate(datasetsRGBRad[1], 1, turbidity, albedo, sun_theta);
		paramZ.z = (float)Evaluate(datasetsRGBRad[2], 1, turbidity, albedo, sun_theta);

		Vector3 S = HosekWilkie(std::cos(sun_theta), 0.0f, 1.0f,
			params[SkyParam_A],
			params[SkyParam_B],
			params[SkyParam_C],
			params[SkyParam_D],
			params[SkyParam_E],
			params[SkyParam_F],
			params[SkyParam_G],
			params[SkyParam_H],
			params[SkyParam_I]);
		S = S * params[SkyParam_Z];
		params[SkyParam_Z] = params[SkyParam_Z] / (S.Dot(Vector3(0.2126f, 0.7152f, 0.0722f)));
		return params;
	}

}
