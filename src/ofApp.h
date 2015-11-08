/**
 * File: ofApp.h
 * Author: Sanjay Kannan
 * Author: Aidan Meacham
 * ---------------------
 * Header file for the entire
 * OpenFrameworks application.
 */

#pragma once
#include "ofMain.h"
#include "ofxCv.h"
#include "ofxControlPanel.h"

// master OpenFrameworks runner
class ofApp : public ofBaseApp{
  public:
    void setup();
    void update();
    void draw();

  private:
    // initialize camera
    ofVideoGrabber camera;

    // Farneback is whole image flow
    ofxCv::FlowFarneback farneback;

    float tiltSmooth = 0.0;
    float tiltSpeed = 0.0;
    float shakeSmooth = 0.0;
    float shakeSpeed = 0.0;
    int lastTime = -1;
    float tau = 200;
};
