/*
TwinGlider: Dual channel slew limiter
Copyright (C) 2019 Pablo Delaloza.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https:www.gnu.org/licenses/>.
*/
#include "moDllz.hpp"

struct TwinGlider : Module {
	enum ParamIds {
		RISE_PARAM,
		FALL_PARAM=2,
		LINK_PARAM=4,
		RISEMODE_PARAM=6,
		FALLMODE_PARAM=8,
		TRIG_PARAM=10,
		SMPNGLIDE_PARAM=12,
		NUM_PARAMS=14
	};
	enum InputIds {
		RISE_INPUT,
		FALL_INPUT=2,
		GATE_INPUT=4,
		CLOCK_INPUT=6,
		IN_INPUT=8,
		NUM_INPUTS=10
	};
	enum OutputIds {
		TRIGRISE_OUTPUT,
		TRIG_OUTPUT=2,
		TRIGFALL_OUTPUT=4,
		GATERISE_OUTPUT=6,
		GATEFALL_OUTPUT=8,
		OUT_OUTPUT=10,
		NUM_OUTPUTS=12
	};
	enum LightIds {
		RISING_LIGHT,
		FALLING_LIGHT=2,
		NUM_LIGHTS=4
	};
 
	struct gliderObj{
		float out = 0.0f;
		float in = 0.0f;
		int risemode = 0.0f;
		int fallmode = 0.0f;
		float riseval = 0.0f;
		float fallval = 0.0f;
		float prevriseval = 0.0f;
		float prevfallval = 0.0f;
		float riseramp = 0.0f;
		float fallramp = 0.0f;
		bool rising = false;
		bool falling = false;
		bool newgate = false;
		bool pulse = false;
		bool newin = false;
		bool trigR = false;
		bool trigF = false;
		int clocksafe = 0;
		dsp::PulseGenerator risePulse;
		dsp::PulseGenerator fallPulse;
		
	};
	
	gliderObj glider[2];
	
	const float threshold = 0.01f;
	
	TwinGlider() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i=0; i<2; i++){
			configParam(RISE_PARAM + i, 0.f, 1.f, 0.f);
			configParam(FALL_PARAM + i, 0.f, 1.f, 0.f);
			configParam(LINK_PARAM + i, 0.f, 1.f, 0.f);
			configParam(RISEMODE_PARAM + i, 0.f, 2.f, 0.f);
			configParam(FALLMODE_PARAM + i, 0.f, 2.f, 0.f);
			configParam(SMPNGLIDE_PARAM + i, 0.f, 1.f, 0.f);
		}
	}
	
	void process(const ProcessArgs &args) override;
	void onReset() override {
		for (int ix = 0; ix < 2 ; ix++){
		outputs[OUT_OUTPUT + ix].setVoltage(inputs[IN_INPUT + ix].getVoltage());
		lights[RISING_LIGHT + ix].value = 0;
		lights[FALLING_LIGHT + ix].value = 0;
		outputs[GATERISE_OUTPUT + ix].setVoltage(0);
		outputs[GATEFALL_OUTPUT + ix].setVoltage(0);
		}
	}
	void onRandomize() override{};
};

///////////////////////////////////////////
///////////////STEP //////////////////
/////////////////////////////////////////////

void TwinGlider::process(const ProcessArgs &args) {
	for (int ix = 0; ix < 2; ix++){
		if (inputs[IN_INPUT + ix].isConnected()) {
			if (std::abs(glider[ix].in - inputs[IN_INPUT + ix].getVoltage()) > threshold){
				glider[ix].newin = true;
			}
		if (params[SMPNGLIDE_PARAM + ix].getValue() > 0.5f){
			bool allowin = false;
			if (inputs[CLOCK_INPUT + ix].isConnected()){
			 // External clock
				if ((glider[ix].clocksafe > 8) && (inputs[CLOCK_INPUT + ix].getVoltage() > 2.5f)){
					allowin = true;
					glider[ix].in = inputs[IN_INPUT + ix].getVoltage();
					glider[ix].clocksafe = 0;
				}else if ((glider[ix].clocksafe <10) && (inputs[CLOCK_INPUT + ix].getVoltage() < 0.01f)) glider[ix].clocksafe ++;
			}  else allowin = ((!glider[ix].rising) && (!glider[ix].falling));
			if (allowin) if (glider[ix].newin) glider[ix].in = inputs[IN_INPUT + ix].getVoltage();
		}
		else if (glider[ix].newin) glider[ix].in = inputs[IN_INPUT + ix].getVoltage();
	 //Check for legato from Gate
	bool glideMe = false;
	if (inputs[GATE_INPUT + ix].isConnected()){
		if (inputs[GATE_INPUT + ix ].getVoltage() < 0.5f) {
			glider[ix].newgate = true;
			glider[ix].out = glider[ix].in;
		}else if (glider[ix].newgate) {
				glider[ix].out = glider[ix].in ;
				glider[ix].newgate = false;
		}else glideMe= true;/// GLIDE !!!!!
	}else glideMe = true;//// GLIDE !!!!

			if (glideMe) {
				//////////////// GLIDE FUNCTION ////////////>>>>>>>>>>>>>>>>>>>>>

					glider[ix].risemode = static_cast<int> (params[RISEMODE_PARAM + ix].getValue());
					glider[ix].fallmode = static_cast<int> (params[FALLMODE_PARAM + ix].getValue());
					if (glider[ix].in  > glider[ix].out){
						if (inputs[RISE_INPUT + ix].isConnected())
							glider[ix].riseval = inputs[RISE_INPUT + ix].getVoltage() / 10.0f * params[RISE_PARAM + ix].getValue();
						else glider[ix].riseval = params[RISE_PARAM + ix].getValue();

						if (glider[ix].riseval > 0.0f) {
							switch (glider[ix].risemode) {
								case 0: /// Hi Rate
									glider[ix].riseramp = 1.0f/(1.0f + glider[ix].riseval * 0.005f * args.sampleRate);
									break;
								case 1: /// Rate
									glider[ix].riseramp = 1.0f/(1.0f + glider[ix].riseval * 2.0f * args.sampleRate);
									break;
								case 2: /// Time
									if ((glider[ix].newin)||(glider[ix].riseval != glider[ix].prevriseval)){
										glider[ix].prevriseval = glider[ix].riseval;
										glider[ix].newin = false;
										glider[ix].riseramp =  (glider[ix].in - glider[ix].out) * args.sampleTime /(glider[ix].riseval * glider[ix].riseval * 10.0f);
										if  (glider[ix].riseramp< 1e-6)  glider[ix].riseramp = 1e-6;
									}
									break;
								default:
									break;
							}
							glider[ix].out += glider[ix].riseramp;
							glider[ix].rising = true;
							glider[ix].falling = false;
							if (glider[ix].out >= glider[ix].in) {///////REACH RISE
								glider[ix].out = glider[ix].in;
								glider[ix].rising = false;
								glider[ix].trigR = true;
							}
						} else {
							glider[ix].rising = false;
							glider[ix].out = glider[ix].in;
							glider[ix].trigR = true;
						}

					} else  if (glider[ix].in  < glider[ix].out){
						if (params[LINK_PARAM + ix].getValue() > 0.5f) {
							glider[ix].fallmode  = glider[ix].risemode;
							glider[ix].fallval  = glider[ix].riseval;
						}else{
							if (inputs[FALL_INPUT + ix].isConnected())
								glider[ix].fallval = inputs[FALL_INPUT + ix].getVoltage() / 10.0f * params[FALL_PARAM + ix].getValue();
							else glider[ix].fallval = params[FALL_PARAM + ix].getValue();
						}
						if (glider[ix].fallval > 0.0f) {
							switch (glider[ix].fallmode) {
								case 0:
									glider[ix].fallramp = 1.0f/(1.0f + glider[ix].fallval * 0.005f * args.sampleRate);
									break;
								case 1:
									glider[ix].fallramp = 1.0f/(1.0f + glider[ix].fallval * 2.0f * args.sampleRate);
									break;
								case 2:
									if ((glider[ix].newin) || (glider[ix].fallval != glider[ix].prevfallval)){
										glider[ix].prevfallval = glider[ix].fallval;
										glider[ix].newin = false;
										glider[ix].fallramp =  (glider[ix].out - glider[ix].in) * args.sampleTime /(glider[ix].fallval * glider[ix].fallval * 10.0f);
										if  (glider[ix].fallramp < 1e-6) glider[ix].fallramp= 1e-6;
									}
									break;
								default:
									break;
							}
							glider[ix].out -= glider[ix].fallramp;
							glider[ix].falling = true;
							glider[ix].rising = false;
							if (glider[ix].out <= glider[ix].in){////////REACH FALL
								glider[ix].out = glider[ix].in;
								glider[ix].falling = false;
								glider[ix].trigF = true;
							}
						}else{
							glider[ix].falling = false;
							glider[ix].out = glider[ix].in;
							glider[ix].trigF = true;
						}
					} else {
						glider[ix].rising = false;
						glider[ix].falling = false;
						glider[ix].out = glider[ix].in;
					}
			}else{
				glider[ix].rising = false;
				glider[ix].falling = false;
				glider[ix].out = glider[ix].in;
			}

	lights[RISING_LIGHT + ix].value = glider[ix].rising? 1.0 : 0.0f;
	lights[FALLING_LIGHT + ix].value = glider[ix].falling? 1.0 : 0.0f;
	outputs[GATERISE_OUTPUT + ix].setVoltage(glider[ix].rising? 10.0 : 0.0f);
	outputs[GATEFALL_OUTPUT + ix].setVoltage(glider[ix].falling? 10.0 : 0.0f);
	outputs[OUT_OUTPUT + ix].setVoltage(glider[ix].out);

		//// do triggers and reset flags
		   if (outputs[TRIGRISE_OUTPUT + ix].isConnected()||outputs[TRIG_OUTPUT + ix].isConnected()||outputs[TRIGFALL_OUTPUT + ix].isConnected()) {
			   if (glider[ix].trigR)  {
				   glider[ix].risePulse.trigger(1e-3f);
				   glider[ix].trigR = false;
				}
			   if (glider[ix].trigF)  {
				   glider[ix].fallPulse.trigger(1e-3f);
				   glider[ix].trigF = false;
			   }
			bool pulseR = glider[ix].risePulse.process(1.0f / args.sampleRate);
			outputs[TRIGRISE_OUTPUT + ix].setVoltage(pulseR ? 10.0 : 0.0);
			bool pulseF = glider[ix].fallPulse.process(1.0f / args.sampleRate);
			outputs[TRIGFALL_OUTPUT + ix].setVoltage(pulseF ? 10.0 : 0.0);
			outputs[TRIG_OUTPUT + ix].setVoltage((pulseR || pulseF) ? 10.0 : 0.0);
			}
	 /// else from if input ACTIVE....
 }else{
	 //disconnected in...reset Output if connected...
	 outputs[GATERISE_OUTPUT + ix].setVoltage(0.0f);
	 outputs[GATEFALL_OUTPUT + ix].setVoltage(0.0f);
	 lights[RISING_LIGHT + ix].value = 0.0f;
	 lights[FALLING_LIGHT + ix].value = 0.0f;
	 glider[ix].out = 0.0f;
	 glider[ix].in = 0.0f;
	 glider[ix].newgate = false;

 }/// Closing if input  ACTIVE

} //for loop ix
  
}//closing STEP

struct TwinGliderWidget : ModuleWidget {
	
	TwinGliderWidget(TwinGlider *module){
		setModule(module);
	setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TwinGlider.svg")));
	//Screws
	addChild(createWidget<ScrewBlack>(Vec(0, 0)));
	addChild(createWidget<ScrewBlack>(Vec(box.size.x - 15, 0)));
	addChild(createWidget<ScrewBlack>(Vec(0, 365)));
	addChild(createWidget<ScrewBlack>(Vec(box.size.x - 15, 365)));
	
	float Ystep = 30.0f ;
	float Ypos = 0.0f;
	
	for (int i=0; i<2; i++){
			
		Ypos = 25.0f + static_cast<float>(i* 173);  //2nd panel offset

		// Gliding Leds
		addChild(createLight<TinyLight<RedLight>>(Vec(6.75, Ypos+1), module, TwinGlider::RISING_LIGHT + i));
		addChild(createLight<TinyLight<RedLight>>(Vec(154.75, Ypos+1), module, TwinGlider::FALLING_LIGHT + i));
		/// Glide Knobs
		addParam(createParam<moDllzKnobM>(Vec(19, Ypos-4), module, TwinGlider::RISE_PARAM + i));
		addParam(createParam<moDllzKnobM>(Vec(102, Ypos-4), module, TwinGlider::FALL_PARAM + i));

		Ypos += Ystep; //55

		// LINK SWITCH//CKSS
		addParam(createParam<moDllzSwitchLedH>(Vec(73, Ypos-12), module, TwinGlider::LINK_PARAM + i));
		/// Glides CVs
		addInput(createInput<moDllzPort>(Vec(23, Ypos+5.5),  module, TwinGlider::RISE_INPUT + i));
		addInput(createInput<moDllzPort>(Vec(117.5, Ypos+5.5),  module, TwinGlider::FALL_INPUT + i));

		Ypos += Ystep; //85

		/// Mode switches
		addParam(createParam<moDllzSwitchT>(Vec(55, Ypos-7), module, TwinGlider::RISEMODE_PARAM + i));
		addParam(createParam<moDllzSwitchT>(Vec(100, Ypos-7), module, TwinGlider::FALLMODE_PARAM + i));

		/// GATES OUT
		addOutput(createOutput<moDllzPort>(Vec(10.5, Ypos+14),  module, TwinGlider::GATERISE_OUTPUT + i));
		addOutput(createOutput<moDllzPort>(Vec(130.5, Ypos+14),  module, TwinGlider::GATEFALL_OUTPUT + i));

		Ypos += Ystep; //115

		/// TRIGGERS OUT
		addOutput(createOutput<moDllzPort>(Vec(43, Ypos-4.5),  module, TwinGlider::TRIGRISE_OUTPUT + i));
		addOutput(createOutput<moDllzPort>(Vec(71, Ypos-4.5),  module, TwinGlider::TRIG_OUTPUT + i));
		addOutput(createOutput<moDllzPort>(Vec(98, Ypos-4.5),  module, TwinGlider::TRIGFALL_OUTPUT + i));

		Ypos += Ystep; //145

		// GATE IN
		addInput(createInput<moDllzPort>(Vec(44, Ypos+7),  module, TwinGlider::GATE_INPUT + i));
		// CLOCK IN
		addInput(createInput<moDllzPort>(Vec(75, Ypos+7),  module, TwinGlider::CLOCK_INPUT + i));
		// Sample&Glide SWITCH
		addParam(createParam<moDllzSwitchLed>(Vec(108, Ypos+19), module, TwinGlider::SMPNGLIDE_PARAM + i));
		// IN OUT
		addInput(createInput<moDllzPort>(Vec(13.5, Ypos+6.5),  module, TwinGlider::IN_INPUT + i));
		addOutput(createOutput<moDllzPort>(Vec(128.5, Ypos+6.5),  module, TwinGlider::OUT_OUTPUT + i));

		}
	}
};

Model *modelTwinGlider = createModel<TwinGlider, TwinGliderWidget>("TwinGlider");
