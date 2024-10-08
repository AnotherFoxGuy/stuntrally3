#include "pch.h"
#include "Def_Str.h"
#include "carcontrolmap_local.h"
// #include "CGame.h"
// #include "CGui.h"
#include "par.h"


//*  speed sensitive steering
float CARCONTROLMAP_LOCAL::GetSSScoeff(
	float speed, float steer_range, float sss_velfactor, float sss_effect)
{								//  m/s
	float coeff = 1.f, mph = speed * 2.23693629f;
	if (mph > 1.f)
		coeff = std::max(1.f - sss_effect, 1.f - sss_velfactor * speed * 0.02f);

	//LogO("vel: "+fToStr(carspeed*3.6f,1,5)+"  sss: "+fToStr(coeff));
	//val = val >= 0.f ? powf(val,1.5f) : -powf(-val,1.5f);
	return steer_range * coeff;
}


///  Process Input
const std::vector <float> & CARCONTROLMAP_LOCAL::ProcessInput(
	const float* channels, int player,
	float carspeed,
	float sss_range, float sss_effect, float sss_velfactor,
	bool oneAxisThrBrk, bool forceBrake,
	bool bPerfTest, EPerfTest iPerfTestStage)
{
	assert(inputs.size() == CARINPUT::ALL);

	//-----------------------------------------------------------------
	if (bPerfTest)  // Perf test, automatic car input
	{
  		//CLUTCH //?-
		inputs[CARINPUT::THROTTLE] = iPerfTestStage == PT_Accel ? 1.f : 0.f;
		inputs[CARINPUT::BRAKE]    = iPerfTestStage == PT_Brake ? 1.f : 0.f;

		inputs[CARINPUT::STEER_RIGHT] = 0.f;
		inputs[CARINPUT::STEER_LEFT]  = 0.f;
		
		inputs[CARINPUT::SHIFT_UP]   = 0.f;
		inputs[CARINPUT::SHIFT_DOWN] = 0.f;
		
		inputs[CARINPUT::HANDBRAKE] = iPerfTestStage == PT_StartWait ? 1.f : 0.f;
		inputs[CARINPUT::BOOST]     = 0.f;
		inputs[CARINPUT::FLIP]      = 0.f;
		
		inputs[CARINPUT::PREV_CAM]	= channels[A_PrevCamera];
		inputs[CARINPUT::NEXT_CAM]	= channels[A_NextCamera];

		inputs[CARINPUT::LAST_CHK]	= 0.f;
		inputs[CARINPUT::REWIND]	= 0.f;

		return inputs;
	}
	//-----------------------------------------------------------------
	
	//  throttle, brake
	if (forceBrake)
	{
		inputs[CARINPUT::THROTTLE] = 0.f;
		inputs[CARINPUT::BRAKE]    = 0.f;
	}else
	{
		float thr = channels[A_Throttle], brk = channels[A_Brake];
		if (oneAxisThrBrk)  // 1 axis for both
		{	float val = thr * 2.f;
			thr = val > 1.f ? (val - 1.f) : 0.f;
			brk = val < 1.f ? (1.f - val) : 0.f;
		}
		inputs[CARINPUT::THROTTLE] = thr;
		const float deadzone = 0.0001f;  // sensible deadzone for braking
		inputs[CARINPUT::BRAKE]    = brk < deadzone ? 0.f : brk;
	}

	//  steering
	float val = forceBrake ? 0.f : (channels[A_Steering] * 2.f - 1.f);
	
	val *= GetSSScoeff(fabs(carspeed), sss_range, sss_velfactor, sss_effect);

	inputs[CARINPUT::STEER_RIGHT] = val > 0.f ?  val : 0.f;
	inputs[CARINPUT::STEER_LEFT]  = val < 0.f ? -val : 0.f;
	
	//  shift
	bool grUp = channels[A_ShiftUp], grDn = channels[A_ShiftDown];
	inputs[CARINPUT::SHIFT_UP]   = grUp && !grUpOld[player];
	inputs[CARINPUT::SHIFT_DOWN] = grDn && !grDnOld[player];
	grUpOld[player] = grUp;  grDnOld[player] = grDn;
	
	//  other
	inputs[CARINPUT::HANDBRAKE] = forceBrake ? 0.f : channels[A_HandBrake];  // 1.f
	inputs[CARINPUT::BOOST]     = forceBrake ? 0.f : channels[A_Boost];
	inputs[CARINPUT::FLIP]      = forceBrake ? 0.f : channels[A_Flip] * 2.f - 1.f;
	
	//  cam
	inputs[CARINPUT::PREV_CAM]	= channels[A_PrevCamera];
	inputs[CARINPUT::NEXT_CAM]	= channels[A_NextCamera];
	//  last chk
	inputs[CARINPUT::LAST_CHK]	= forceBrake ? false : channels[A_LastChk];
	inputs[CARINPUT::REWIND]	= forceBrake ? false : channels[A_Rewind];

	//  lights
	inputs[CARINPUT::LIGHTS]	= channels[A_Lights];

	return inputs;
}
