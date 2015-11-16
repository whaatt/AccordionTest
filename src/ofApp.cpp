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

  // initialize synthesizer
  synth.init(44100, 256, true);
  synth.load("data/primary.sf2");
  synth.setInstrument(1, 21);
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
    numFrames += 1;

    // get the current time in ms
    int now = ofGetElapsedTimeMillis();
    if (lastTime == -1) lastTime = now;

    float dT = now - lastTime;
    lastTime = now;

    // tau is the decay time constant
    float alpha = 1.0 - exp(-dT / tau);

    lkFlow.calcOpticalFlow(camera);
    if (numFrames % 10 == 0) lkFlow.resetFeaturesToTrack();
    vector<ofVec2f> flows = lkFlow.getMotion();

    float flowX = 0.0;
    float flowY = 0.0;
    float avgFlowX = 0.0;
    float avgFlowY = 0.0;

    // find the absolute average of all flows
    for (int i = 0; i < flows.size(); i += 1) {
      // TODO: better built-in way to do this?
      flowX += abs(flows[i].x);
      flowY += abs(flows[i].y);
    }

    avgFlowX = flowX / (float) flows.size();
    avgFlowY = flowY / (float) flows.size();
    tiltSpeed = avgFlowX; // accordion on Y-axis
    shakeSpeed = avgFlowY; // shaking on X-axis
    if (tiltSpeed != tiltSpeed) return; // NaN

    // formula for exponentially-weighted moving average
    tiltSmooth = alpha * tiltSpeed + (1.0 - alpha) * tiltSmooth;
    shakeSmooth = alpha * shakeSpeed + (1.0 - alpha) * shakeSmooth;

    // use bellow velocity to set the channel velocity
    int volume = std::min(127, (int) (tiltSmooth / 2.0 * 127.0));
    synth.controlChange(1, 7, tiltSmooth > 0.3 ? volume : 0);
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
  lkFlow.draw(400, 100, 640, 480);

  stringstream ss;
  ss << "Velocity: " << tiltSpeed << endl;
  ss << "Smoothed: " << tiltSmooth << endl;
  ss << "Shake: " << shakeSmooth << endl;
  if (shakeSmooth < 1.0 && tiltSmooth > 1.0)
    ss << "Movement Detected!" << endl;
  ofDrawBitmapString(ss.str(), 100, 100);
}

/**
 * Function: keyPressed
 * --------------------
 * Handles key presses.
 */
void ofApp::keyPressed(int key) {
  // start playing a given note
  if (key >= 'a' && key <= 'z') {
    // some really hacky shit I'm doing here to play a blues minor scale
    string qwerty("q.wer..t.y..u.iop..a.s..d.fgh..j.k..l.zxc..v.b..n.m");
    int note = qwerty.find(key) + 35;
    if (playing.count(note)) return;

    // note is not already playing: turn it on
    synth.noteOn(1, note, 127);
    playing.insert(note);
  }
}

/**
 * Function: keyReleased
 * ---------------------
 * Handles key releases.
 */
void ofApp::keyReleased(int key) {
  // stop playing a given note
  if (key >= 'a' && key <= 'z') {
    // some really hacky shit I'm doing here to play a blues minor scale
    string qwerty("q.wer..t.y..u.iop..a.s..d.fgh..j.k..l.zxc..v.b..n.m");
    int note = qwerty.find(key) + 35;
    if (!playing.count(note)) return;

    // note is playing: turn it off
    synth.noteOff(1, note);
    playing.erase(note);
  }
}
