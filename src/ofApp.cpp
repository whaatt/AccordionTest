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

  // initialize graphics
  ofBackground(190,30,45);
  wh = ofGetWindowHeight();
  ww = ofGetWindowWidth();
  compress=.4;
  velocity=0;
  keybPosition=-ww;
  keybOn=false;
  fulscr=false;
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

  // Slew keyboard on and offscreen
  if (keybOn){
    keybPosition=keybPosition*.9;
  } else {
    keybPosition=keybPosition+(-ww-keybPosition)*.1;
  }

  // Compress bellows based on velocity
  if (velocity>0){
    compress=compress+(.5-compress)*velocity*.1;
  } else {
    compress=compress+(.25-compress)*velocity*-.1;
  }
}

/**
 * Function: draw
 * --------------
 * Just some testing code.
 */
void ofApp::draw() {

  // Get window params
  wh = ofGetWindowHeight();
  ww = ofGetWindowWidth();

  // Draw baffles
  ofPushMatrix();
    for(unsigned int i=0; i<10; i++){
      ofPushMatrix();
        ofTranslate(0,i*wh/5*compress*2);
        drawBaffle(compress);
      ofPopMatrix();
    }
    ofTranslate(0,wh,0);
  ofPopMatrix();

  // Draw keys with alpha
  ofPushMatrix();
    ofTranslate(keybPosition,0);

    ofPushStyle();
      ofEnableAlphaBlending();
      ofSetColor(255,255,255,160);
      ofRect(0,0,1,ww,wh);
      ofDisableAlphaBlending();
    ofPopStyle();

    drawKeys();
  ofPopMatrix();

  // Draw camera, flow, and strings
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

  // Tab activates onscreen keyboard
  if(key==OF_KEY_TAB){
    keybOn=!keybOn;
  }

  // F11 activates fullscreen
  if(key==OF_KEY_F11){
    fulscr=!fulscr;
  }

  ofSetFullscreen(fulscr);
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

/**
 * Function: mouseMoved
 * ---------------------
 * Handles mouse movement.
 */
void ofApp::mouseMoved(int x, int y ){
  velocity = (float)y/wh-.5;
}

/**
 * Function: drawBaffle
 * ---------------------
 * Draws skeumorphic baffle.
 */
void ofApp::drawBaffle(float pct){

  ofPushStyle();
  ofSetLineWidth(2);
  ofSetColor(ofColor::black);
    ofBeginShape();
      ofVertex(0,0);
      ofVertex(ww/10,wh/5*pct);
    ofEndShape(false);

    ofBeginShape();
      ofVertex(ww,0);
      ofVertex(ww-ww/10,wh/5*pct);
    ofEndShape(false);

    ofBeginShape();
      ofVertex(ww/10,wh/5*pct);
      ofVertex(0,wh/5*pct*2);
    ofEndShape(false);

    ofBeginShape();
      ofVertex(ww-ww/10,wh/5*pct);
      ofVertex(ww,wh/5*pct*2);
    ofEndShape(false);

    ofBeginShape();
      ofVertex(ww/10,wh/5*pct);
      ofVertex(ww-ww/10,wh/5*pct);
    ofEndShape(false);
  ofPopStyle();

  ofPushStyle();
  ofSetLineWidth(4);
  ofSetColor(255,222,23);
    ofBeginShape();
      ofVertex(0,0);
      ofVertex(ww,0);
    ofEndShape(false);
  ofPopStyle();
}

/**
 * Function: drawKeys
 * ---------------------
 * Draws onscreen keyboard.
 */
void ofApp::drawKeys(){
  float keyWidth=ww/12-(ww/12)*.1;
  float keyHeight=keyWidth;
  string topChars = "qwertyuiop";
  string midChars = "asdfghjkl;";
  string botChars = "zxcvbnm,./";

  for(unsigned int i=1; i<11; i++){
    ofRectRounded(i*ww/12-25,wh/2-keyHeight/2-keyHeight*1.1,2,keyWidth,keyHeight,10,10,10,10);
    string letter(1,topChars[i-1]);
    ofPushStyle();
      ofSetColor(ofColor::black);
      ofDrawBitmapString(letter,i*ww/12-25+keyWidth/2,wh/2-keyHeight/2-keyHeight*1.1+keyHeight/2,2);
    ofPopStyle();
  }

  for(unsigned int i=1; i<11; i++){
    ofRectRounded(i*ww/12,wh/2-keyHeight/2,2,keyWidth,keyHeight,10,10,10,10);
    string letter(1,midChars[i-1]);
    ofPushStyle();
      ofSetColor(ofColor::black);
      ofDrawBitmapString(letter,i*ww/12+keyWidth/2,wh/2,2);
    ofPopStyle();
  }

  for(unsigned int i=1; i<11; i++){
    ofRectRounded(i*ww/12+25,wh/2+keyHeight/2+keyHeight*.1,2,keyWidth,keyHeight,10,10,10,10);
    string letter(1,botChars[i-1]);
    ofPushStyle();
      ofSetColor(ofColor::black);
      ofDrawBitmapString(letter,i*ww/12+25+keyWidth/2,wh/2+keyHeight+keyHeight*.1,2);
    ofPopStyle();
  }
}
