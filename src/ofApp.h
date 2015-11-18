/**
 * File: ofApp.h
 * Author: Sanjay Kannan
 * Author: Aidan Meacham
 * ---------------------
 * Header file for the entire
 * OpenFrameworks application.
 */

#pragma once
#include <set>

#include "ofMain.h"
#include "ofxCv.h"
#include "synthesizer.h"

// master OpenFrameworks runner
class ofApp : public ofBaseApp {
  public:
    void setup();
    void update();
    void draw();

    // some usual boilerplate
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);

  private:
    // FluidSynth wrapper
    Synthesizer synth;

    // initialize camera
    ofVideoGrabber camera;

    // Farneback is whole image flow
    // LK is flow for features
    ofxCv::FlowPyrLK lkFlow;

    // avoid note repeats by
    // tracking playing notes
    set<int> playing;

    float tiltSmooth = 0.0;
    float tiltSpeed = 0.0;
    float shakeSmooth = 0.0;
    float shakeSpeed = 0.0;
    int lastTime = -1;
    int numFrames = 0;
    float tau = 500;

    // GRAPHICS THINGS
    void drawBaffle(float pct);
	void drawKeys();

	// Window stuff
    int wh;
    int ww;
    bool fulscr;

    // Baffle stuff
    float compress;
    float velocity;

    // Keyboard stuff
    float keybPosition;
    bool keybOn;
};
