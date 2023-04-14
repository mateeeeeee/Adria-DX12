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
		inline double EvaluateSpline(double const* spline, size_t stride, double value)
		{
			return
				1 * pow(1 - value, 5) * spline[0 * stride] +
				5 * pow(1 - value, 4) * pow(value, 1) * spline[1 * stride] +
				10 * pow(1 - value, 3) * pow(value, 2) * spline[2 * stride] +
				10 * pow(1 - value, 2) * pow(value, 3) * spline[3 * stride] +
				5 * pow(1 - value, 1) * pow(value, 4) * spline[4 * stride] +
				1 * pow(value, 5) * spline[5 * stride];
		}

		double Evaluate(double const* dataset, size_t stride, float turbidity, float albedo, float sun_theta)
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

		XMFLOAT3 HosekWilkie(float cos_theta, float gamma, float cos_gamma, XMFLOAT3 A, XMFLOAT3 B, XMFLOAT3 C, XMFLOAT3 D, XMFLOAT3 E, XMFLOAT3 F, XMFLOAT3 G, XMFLOAT3 H, XMFLOAT3 I)
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
			XMVECTOR temp = temp1 * temp2;

			XMFLOAT3 result;
			XMStoreFloat3(&result, temp);
			return result;
		}
	}

	SkyParameters CalculateSkyParameters(float turbidity, float albedo, XMFLOAT3 sun_direction)
	{
		float sun_theta = std::acos(std::clamp(sun_direction.y, 0.f, 1.f));

		SkyParameters params{};
		for (size_t i = 0; i < ESkyParam_Z; ++i)
		{
			auto& param = params[i];
			param.x = (float)Evaluate(datasetsRGB[0] + i, 9, turbidity, albedo, sun_theta);
			param.y = (float)Evaluate(datasetsRGB[1] + i, 9, turbidity, albedo, sun_theta);
			param.z = (float)Evaluate(datasetsRGB[2] + i, 9, turbidity, albedo, sun_theta);
		}

		auto& paramZ = params[ESkyParam_Z];
		paramZ.x = (float)Evaluate(datasetsRGBRad[0], 1, turbidity, albedo, sun_theta);
		paramZ.y = (float)Evaluate(datasetsRGBRad[1], 1, turbidity, albedo, sun_theta);
		paramZ.z = (float)Evaluate(datasetsRGBRad[2], 1, turbidity, albedo, sun_theta);

		XMFLOAT3 S = HosekWilkie(std::cos(sun_theta), 0.0f, 1.0f,
			params[ESkyParam_A],
			params[ESkyParam_B],
			params[ESkyParam_C],
			params[ESkyParam_D],
			params[ESkyParam_E],
			params[ESkyParam_F],
			params[ESkyParam_G],
			params[ESkyParam_H],
			params[ESkyParam_I]);

		XMVECTOR _S = XMLoadFloat3(&S);
		XMVECTOR _Z = XMLoadFloat3(&params[ESkyParam_Z]);
		_S = _S * _Z;

		_Z = XMVectorDivide(_Z, XMVector3Dot(_S, XMVectorSet(0.2126f, 0.7152f, 0.0722f, 0.0f)));
		XMStoreFloat3(&params[ESkyParam_Z], _Z);

		return params;
	}

}
