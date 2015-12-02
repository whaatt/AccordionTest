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
#include "mapper.h"
#include "synthesizer.h"

/**
 * Type: Note
 * ----------
 * A simple struct to hold
 * notes derived from MIDI.
 */
struct Note {
  int note;
  // in seconds
  double duration;
};

// master OpenFrameworks runner
class ofApp : public ofBaseApp {
  public:
    void setup();
    void update();
    void draw();

    // some usual boilerplate
    void keyPressed(int key);
    void keyReleased(int key);

  private:
    // FluidSynth wrapper class
    Synthesizer* synth = NULL;
    int synthVol = 0;

    // initialize camera
    ofVideoGrabber camera;

    // Farneback is whole image flow
    // LK is flow for features
    ofxCv::FlowPyrLK lkFlow;

    // avoid note repeats by
    // tracking playing notes
    set<int> playing;

    // map keys to scales
    vector<string> scales;
    vector<string> keys;
    vector<string> modes;
    Mapper mapper;

    // play through files
    vector<string> filesMIDI;
    bool loadedMIDI = false;
    bool playThrough = false;
    int filesIndex = 0;
    int songPosition = 0;
    map<int, int> keyPosMap;
    vector<vector<Note>> song;

    // mapping state
    int scaleIndex = 0;
    int keyIndex = 0;
    int modeIndex = 0;

    bool sounding = false;
    float tiltSmooth = 0.0;
    float tiltSpeed = 0.0;
    float shakeSmooth = 0.0;
    float shakeSpeed = 0.0;
    float tiltDir = 0.0;
    int lastTime = -1;
    int numFrames = 0;
    float tau = 250;

    // graphics-related functions
    void drawBaffle(float pct);
    void drawKeys();

    // window-related stuff
    int wh; // window height
    int ww; // window width
    bool fulscr; // fullscreen
    bool fulscrToggled;

    // baffle stuff
    float position;
    float compress;
    float velocity;

    // keyboard stuff
    float keybPosition;
    set<int> pressed;
    bool keybOn;
};
