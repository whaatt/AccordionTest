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
#include <dirent.h>
#include "MIDI/MidiFile.h"

using namespace ofxCv;
using namespace cv;

/**
 * Function: getMIDIFiles
 * ----------------------
 * Gets the MIDI files in a directory
 */
bool getMIDIFiles(vector<string>& list, string dirName) {
  DIR *dir; struct dirent *ent;
  // ugly shit that looks like C code
  if ((dir = opendir(dirName.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      char* name = ent -> d_name;
      size_t len = strlen(name);

      if (len > 4 && strcmp(name + len - 4, ".mid") == 0)
        list.push_back(dirName + "/" + name); // add MIDI to list
    }

    // clean up
    closedir(dir);
    return true;
  }

  // could not open
  return false;
}

/**
 * Function: buildSongVector
 * -------------------------
 * Builds a vector of vectors representing
 * all of the notes in a song. Each inner
 * vector represents notes played at a
 * particular time together.
 */
void buildSongVector(vector<vector<Note>>& song, string fileName) {
  MidiFile songMIDI; // from Midifile library
  songMIDI.read(fileName);

  songMIDI.linkNotePairs();
  songMIDI.doTimeAnalysis();
  songMIDI.joinTracks();

  MidiEvent* event;
  int simulIndex = 0;
  song.clear(); // empty old song
  song.push_back(vector<Note>());
  int deltaTick = (&songMIDI[0][0]) -> tick;

  for (int evIdx = 0; evIdx < songMIDI[0].size(); evIdx += 1) {
    event = &songMIDI[0][evIdx];
    if (!event -> isNoteOn()) continue;

    if (event -> tick != deltaTick) {
      deltaTick = event -> tick;
      song.push_back(vector<Note>());
      simulIndex += 1;
    }

    Note newNote;
    newNote.note = (int) (*event)[1];
    newNote.duration = event -> getDurationInSeconds();
    song[simulIndex].push_back(newNote);
  }
}

/**
 * Function: setup
 * ---------------
 * Initializes the camera.
 */
void ofApp::setup() {
  // initialize camera
  camera.initGrabber(640, 480);
  ofSetWindowTitle("Accordion");

  // modes just contains keyboard modes [irrelevant here]
  mapper.init("data/scales.txt", "data/modes.txt");

  if (getMIDIFiles(filesMIDI, "data/MIDI"))
    loadedMIDI = true; // successful load

  // get UI listing variables
  scales = mapper.getScales();
  keys = mapper.getKeys();
  modes = mapper.getModes();

  // initialize synthesizer
  synth = new Synthesizer();
  synth -> init(44100, 256, true);
  synth -> load("data/primary.sf2");
  synth -> setInstrument(1, 21);

  // initialize graphics
  ofBackground(190,30,45);
  wh = ofGetWindowHeight();
  ww = ofGetWindowWidth();
  position = 0.25;
  velocity = 0;

  keybPosition = -ww;
  keybOn = false;
  fulscr = false;
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
    float flowYDir = 0.0;
    float avgFlowX = 0.0;
    float avgFlowY = 0.0;

    // find the absolute average of all flows
    for (int i = 0; i < flows.size(); i += 1) {
      // TODO: better built-in way to do this?
      flowX += abs(flows[i].x);
      flowY += abs(flows[i].y);
      flowYDir += flows[i].y;
    }

    avgFlowX = flowX / (float) flows.size();
    avgFlowY = flowY / (float) flows.size();
    tiltSpeed = avgFlowY; // accordion on Y-axis
    shakeSpeed = avgFlowX; // shaking on X-axis
    if (tiltSpeed != tiltSpeed) return; // NaN

    // formula for exponentially-weighted moving average
    tiltSmooth = alpha * tiltSpeed + (1.0 - alpha) * tiltSmooth;
    shakeSmooth = alpha * shakeSpeed + (1.0 - alpha) * shakeSmooth;
    tiltDir = flowYDir;

    // use bellow velocity to update the channel synth velocity
    int volume = std::min(127, 31 + (int) (tiltSmooth / 7.5 * 96.0));
    int maxIncrement = copysign(1, volume) * (synthVol == 0 || tiltSmooth <= 1.5) ? 5 : 20;
    int diffIncrement = volume - synthVol; // avoid jumpy changes with a max increment

    // update the synth volume by an increment in direction of tilt velocity
    synthVol += diffIncrement > maxIncrement ? maxIncrement : diffIncrement;
    synth -> controlChange(1, 7, tiltSmooth > 1.5 ? synthVol : 0);
    sounding = tiltSmooth <= 1.5;
  }

  // slew keyboard on and offscreen
  if (keybOn) keybPosition = 0; //keybPosition * .9;
  else if (fulscrToggled) keybPosition = -ww * 2;
  else keybPosition = -ww; //keybPosition + (-ww - keybPosition) * .1;

  // reset toggle state
  if (fulscrToggled)
    fulscrToggled = false;

  // compress bellows between 0.25 and 0.5 using a linear easing function
  if (tiltDir > 0) position = position + (1.0 - position) * tiltSmooth * .0025;
  else position = position - position * tiltSmooth * .005;
  compress = position * 0.25 + 0.25;
}

/**
 * Function: draw
 * --------------
 * Just some testing code.
 */
void ofApp::draw() {
  // get window params
  wh = ofGetWindowHeight();
  ww = ofGetWindowWidth();

  // draw baffles
  ofPushMatrix();
    for (int i = 0; i < 10; i += 1) {
      ofPushMatrix();
        // translate based on compression factor
        ofTranslate(0, i * wh / 5 * compress * 2);
        drawBaffle(compress);
      ofPopMatrix();
    }

    // translate all baffles
    ofTranslate(0, wh, 0);
  ofPopMatrix();

  ofPushMatrix();
    // draw keys with alpha
    ofTranslate(keybPosition, 0);

    ofPushStyle();
      ofEnableAlphaBlending();
      ofSetColor(255, 255, 255, 160);
      ofRect(0, 0, ww, wh);
      ofDisableAlphaBlending();
    ofPopStyle();

    drawKeys();
  ofPopMatrix();
}

/**
 * Function: keyPressed
 * --------------------
 * Handles key presses.
 */
void ofApp::keyPressed(int key) {
  // start playing a given note
  if (key >= 'a' && key <= 'z' || key == ';' ||
      key == ',' || key == '.' || key == '/') {
    if (!playThrough) {
      int note = mapper.getNote(key);
      if (playing.count(note)) return;

      // note is not already playing: turn it on
      synth -> noteOn(1, note, 127);
      playing.insert(note);
      pressed.insert(key);
    }

    else {
      if (songPosition >= song.size()) {
        playThrough = false;
        return;
      }

      if (keyPosMap.find(key) != keyPosMap.end())
        return; // already handling this key press

      keyPosMap[key] = songPosition; // turn off shit by the key
      for (int i = 0; i < song[songPosition].size(); i += 1) {
        int note = song[songPosition][i].note;
        synth -> noteOn(1, note, 127);
      }

      // move to next
      pressed.insert(key);
      songPosition += 1;
    }
  }

  // backslash for onscreen keyboard
  if (key == '\\') keybOn = !keybOn;

  // ` for fullscreen
  if (key == '`') {
    fulscr = !fulscr;
    fulscrToggled = true;
    ofSetFullscreen(fulscr);
  }

  // toggle play through
  if (key == '=') {
    // toggle off
    if (playThrough) {
      playThrough = false;
      synth -> allNotesOff(1);
      return;
    }

    // TODO: error message this
    if (!loadedMIDI) return;
    playThrough = true;
    songPosition = 0;

    // calculate note lengths and positions
    buildSongVector(song, filesMIDI[filesIndex]);
  }

  // change scale [e.g. major] with [ and key [e.g. C#] with ]
  if (key == '[') mapper.setKeyIndex(keyIndex = ++keyIndex % keys.size());
  if (key == ']') mapper.setScaleIndex(scaleIndex = ++scaleIndex % scales.size());

  // change mode [keyboard layout schematic, e.g. inc by rows] with '
  if (key == '\'') mapper.setModeIndex(modeIndex = ++modeIndex % modes.size());

  // change the selected song in directory with -
  if (key == '-') filesIndex = ++filesIndex % filesMIDI.size();
}

/**
 * Function: keyReleased
 * ---------------------
 * Handles key releases.
 */
void ofApp::keyReleased(int key) {
  // stop playing a given note
  if (key >= 'a' && key <= 'z' || key == ';' ||
      key == ',' || key == '.' || key == '/') {
    if (!playThrough) {
      int note = mapper.getNote(key);
      if (!playing.count(note)) return;

      // note is playing: turn it off
      synth -> noteOff(1, note);
      playing.erase(note);
      pressed.erase(key);
    }

    else {
      for (int i = 0; i < song[keyPosMap[key]].size(); i += 1) {
        int note = song[keyPosMap[key]][i].note;
        synth -> noteOff(1, note);
      }

      // remove the key from map
      pressed.erase(key);
      keyPosMap.erase(keyPosMap.find(key));
    }
  }
}

/**
 * Function: drawBaffle
 * --------------------
 * Draws a single skeumorphic baffle.
 */
void ofApp::drawBaffle(float pct) {
  ofPushStyle();
  ofSetLineWidth(2);
  ofSetColor(ofColor::black);
    ofBeginShape();
      ofVertex(0,0);
      ofVertex(ww / 10, wh / 5 * pct);
    ofEndShape(false);

    ofBeginShape();
      ofVertex(ww,0);
      ofVertex(ww - ww / 10, wh / 5 * pct);
    ofEndShape(false);

    ofBeginShape();
      ofVertex(ww / 10, wh / 5 * pct);
      ofVertex(0,wh / 5 * pct * 2);
    ofEndShape(false);

    ofBeginShape();
      ofVertex(ww - ww / 10, wh / 5 * pct);
      ofVertex(ww,wh / 5 * pct * 2);
    ofEndShape(false);

    ofBeginShape();
      ofVertex(ww / 10, wh / 5 * pct);
      ofVertex(ww - ww / 10, wh / 5 * pct);
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
 * ------------------
 * Draws onscreen keyboard.
 */
void ofApp::drawKeys(){
  float keyWidth = ww / 12 - (ww / 12) * .1;
  float keyHeight = keyWidth; // squares

  ofSetColor(ofColor(0, 0, 255));
  ofDrawBitmapString("Toggle Keyboard With Backslash (\\)\n" +
                     string("Current Scale: ") + scales[scaleIndex] + " (])\n" +
                     string("Current Key: ") + keys[keyIndex] + " ([)\n" +
                     string("Current Mode: ") + modes[modeIndex] + " (')\n\n" +
                     string("Selected Song: ") + filesMIDI[filesIndex].substr(10, string::npos) + " (-)\n" +
                     string("Play Through Mode: ") + (playThrough ? string("On") : string("Off")) +
                     string(" (=)"), 10, 20, 2); // TODO: clean this shit out of this print

  string topChars = "qwertyuiop";
  string midChars = "asdfghjkl;";
  string botChars = "zxcvbnm,./";

  // print out the top chars
  for (int i = 1; i < 11; i += 1) {
    if (pressed.count(topChars[i - 1])) ofSetColor(ofColor(170, 255, 170));
    else ofSetColor(ofColor(255, 255, 255)); // default is white

    ofRectRounded(i * ww / 12 - 25, wh / 2 - keyHeight / 2 - keyHeight * 1.1,
      2, keyWidth, keyHeight, 10, 10, 10, 10); // position and size
    string letter(1, topChars[i - 1]); // for drawing bitmap string

    ofPushStyle();
      ofSetColor(ofColor::black);
      ofDrawBitmapString(letter, i * ww / 12 - 25 + keyWidth / 2,
        wh / 2 - keyHeight / 2 - keyHeight * 1.1 + keyHeight / 2, 2);
    ofPopStyle();
  }

  // print out the middle chars
  for (int i = 1; i < 11; i += 1) {
    if (pressed.count(midChars[i - 1])) ofSetColor(ofColor(170, 255, 170));
    else ofSetColor(ofColor(255, 255, 255)); // default is white

    ofRectRounded(i * ww / 12, wh / 2 - keyHeight / 2, 2,
      keyWidth, keyHeight, 10, 10, 10, 10); // position and size
    string letter(1, midChars[i - 1]); // for drawing bitmap string

    ofPushStyle();
      ofSetColor(ofColor::black);
      ofDrawBitmapString(letter, i * ww / 12 + keyWidth / 2, wh / 2, 2);
    ofPopStyle();
  }

  // print out the bottom chars
  for (int i = 1; i < 11; i += 1) {
    if (pressed.count(botChars[i - 1])) ofSetColor(ofColor(170, 255, 170));
    else ofSetColor(ofColor(255, 255, 255)); // default is white

    ofRectRounded(i * ww / 12 + 25, wh / 2 + keyHeight / 2 + keyHeight *.1,
      2, keyWidth, keyHeight, 10, 10, 10, 10); // position and size
    string letter(1, botChars[i - 1]); // for drawing bitmap string

    ofPushStyle();
      ofSetColor(ofColor::black);
      ofDrawBitmapString(letter, i * ww / 12 + 25 + keyWidth / 2,
        wh / 2 + keyHeight + keyHeight *.1, 2);
    ofPopStyle();
  }
}
