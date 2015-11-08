/**
 * File: ofApp.cpp
 * Author: Sanjay Kannan
 * Author: Aidan Meacham
 * ---------------------
 * Implentation for the entire
 * OpenFrameworks application.
 */

#include "ofApp.h"
#include <sstream>
#include <math.h>

using namespace ofxCv;
using namespace cv;

/**
 * Function: setup
 * ---------------
 * Initializes the camera.
 */
void ofApp::setup() {
  // initialize camera
  camera.initGrabber(640, 480);
}

/**
 * Function: update
 * ----------------
 * Grabs frame and updates optical
 * flow values. Performs exponential
 * smoothing on flow values.
 */
void ofApp::update() {
  // get new frame
  camera.update();

  // new frame found
  if (camera.isFrameNew()) {
    // get the current time in ms
    int now = ofGetElapsedTimeMillis();
    if (lastTime == -1) lastTime = now;

    float dT = now - lastTime;
    lastTime = now;

    // tau is the decay time constant
    float alpha = 1.0 - exp(-dT / tau);

    farneback.calcOpticalFlow(camera);
    ofVec2f avgFlow = farneback.getAverageFlow();
    tiltSpeed = abs(avgFlow.y); // accordion on Y-axis
    shakeSpeed = abs(avgFlow.x); // shaking on X-axis

    // formula for exponentially-weighted moving average
    tiltSmooth = alpha * tiltSpeed + (1.0 - alpha) * tiltSmooth;
    shakeSmooth = alpha * shakeSpeed + (1.0 - alpha) * shakeSmooth;
  }
}

/**
 * Function: draw
 * --------------
 * Just some testing code.
 */
void ofApp::draw() {
  ofBackground(0);
  ofSetColor(255);
  camera.draw(400, 100, 640, 480);
  farneback.draw(400, 100, 640, 480);

  stringstream ss;
  ss << "Velocity: " << tiltSpeed << endl;
  ss << "Smoothed: " << tiltSmooth << endl;
  ss << "Shake: " << shakeSmooth << endl;
  if (shakeSmooth < 1.0 && tiltSmooth > 1.0)
    ss << "Movement Detected!" << endl;
  ofDrawBitmapString(ss.str(), 100, 100);
}
