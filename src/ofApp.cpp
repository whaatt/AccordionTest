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
void buildSongVector(vector<vector<Note>>& song, vector<char>& songKeys,
  vector<Note>& topNotes, string fileName) {
  MidiFile songMIDI; // from Midifile library
  songMIDI.read(fileName);

  songMIDI.linkNotePairs();
  songMIDI.doTimeAnalysis();
  songMIDI.joinTracks();

  MidiEvent* event;
  int simulIndex = -1;
  song.clear(); // empty old song
  int deltaTick = -1;

  // iterate through list of note on events and add them to song
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

  if (song.size() == 0) return; // empty
  string hardKeys("fghj"); // six notes guitar hero style
  Note lastNote = *max_element(song[0].begin(), song[0].end());
  int lastKeyIndex = 3; // corresponds to j
  songKeys.push_back(hardKeys[lastKeyIndex]);
  topNotes.push_back(lastNote);

  // determine hard mode key mappings [jank]
  for (int i = 1; i < song.size(); i += 1) {
    Note currNote = *max_element(song[i].begin(), song[i].end());
    int diff = currNote.note - lastNote.note; // determines key interval

    int nextKeyIndex;
    if (diff == 0) nextKeyIndex = lastKeyIndex; // same note
    else if (diff > 0 && diff < 3) nextKeyIndex = (lastKeyIndex + 1) % hardKeys.size();
    else if (diff > 2 && diff < 5) nextKeyIndex = (lastKeyIndex + 2) % hardKeys.size();
    else if (diff < 0 && diff > -3) nextKeyIndex = (lastKeyIndex - 1) % hardKeys.size();
    else if (diff < -2 && diff > -5) nextKeyIndex = (lastKeyIndex - 2) % hardKeys.size();
    else if (diff > 4) nextKeyIndex = (lastKeyIndex + 3) % hardKeys.size();
    else nextKeyIndex = (lastKeyIndex - 3) % hardKeys.size();

    // normalize negative mods to positive before append
    if (nextKeyIndex < 0) nextKeyIndex += hardKeys.size();
    songKeys.push_back(hardKeys[nextKeyIndex]);
    topNotes.push_back(currNote);

    // update last values
    lastNote = currNote;
    lastKeyIndex = nextKeyIndex;
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

    // start with base values on first call
    if (lastX == -1 || lastY == -1) {
      lastX = ofGetMouseX();
      lastY = ofGetMouseY();
      return;
    }

    // get the current time in ms
    int now = ofGetElapsedTimeMillis();
    if (lastTimeM == -1) lastTimeM = now;

    float dT = now - lastTimeM;
    if (dT == 0) return;
    lastTimeM = now;

    // tau is the decay time constant
    float alpha = 1.0 - exp(-dT / vTau);

    // update raw values
    int newX = ofGetMouseX();
    int newY = ofGetMouseY();
    xVel = (float) (newX - lastX) / dT;
    yVel = (float) (newY - lastY) / dT;
    lastX = newX;
    lastY = newY;

    // smooth the raw velocity values and update accel
    float xVelSmNew = alpha * xVel + (1.0 - alpha) * xVelSm;
    float yVelSmNew = alpha * yVel + (1.0 - alpha) * yVelSm;
    xAcc = (xVelSmNew - xVelSm) / dT;
    yAcc = (yVelSmNew - yVelSm) / dT;
    xVelSm = xVelSmNew;
    yVelSm = yVelSmNew;

    // TODO: decide if we want this
    // synth -> pitchBend(1, -yVelSm);

    // below this line is
    // smoothing shit for
    // tilt detection

    // get the current time in ms
    if (lastTime == -1) lastTime = now;

    dT = now - lastTime;
    lastTime = now;

    // tau is the decay time constant
    alpha = 1.0 - exp(-dT / tau);

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
    sounding = tiltSmooth > 1.5;
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

  if (!keybToggled)
    ofDrawBitmapString("Welcome to Laptop Accordion 0.0.1!\n" + // welcome
      string("Toggle Keyboard With Backslash (\\)"), ww / 2 - 130, 20, 2);

  ofPushMatrix();
    // draw keys with alpha
    ofTranslate(keybPosition, 0);

    ofPushStyle();
      ofEnableAlphaBlending();
      ofSetColor(255, 255, 255, 180);
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
      // avoid multiple key presses
      // even those we are not handling
      if (pressed.count(key)) return;
      pressed.insert(key);

      // bellows not moving [hard mode only]
      if (hardMode && !sounding) return;

      if (keyPosMap.find(key) != keyPosMap.end())
        return; // already handling this key press

      int now = ofGetElapsedTimeMillis();
      if (now - lastPressTime < debounceTime)
        return; // likely an accidental key mash

      if (hardMode && key != highlight)
        return; // wrong key played

      keyPosMap[key] = songPosition; // turn off shit by the key
      for (int i = 0; i < song[songPosition].size(); i += 1) {
        int note = song[songPosition][i].note;
        synth -> noteOn(1, note, 127);
      }

      // colorings
      if (hardMode) {
        previews.clear();
        if (songPosition + 1 < song.size()) highlight = songKeys[songPosition + 1];
        if (songPosition + 2 < song.size()) previews.push_back(songKeys[songPosition + 2]);
        if (songPosition + 3 < song.size()) previews.push_back(songKeys[songPosition + 3]);
        if (songPosition + 4 < song.size()) previews.push_back(songKeys[songPosition + 4]);
        if (songPosition + 5 < song.size()) previews.push_back(songKeys[songPosition + 5]);
        if (songPosition + 6 < song.size()) previews.push_back(songKeys[songPosition + 6]);
        if (songPosition + 7 < song.size()) previews.push_back(songKeys[songPosition + 7]);
      }

      // move to next
      lastPressTime = now;
      songPosition += 1;
    }

    int red = 170;
    int green = (255 + 221 + rand() % 34) / 2;
    int blue = (200 + 200 + rand() % 55) / 2;
    ofColor random(red, green, blue);
    // ofColor green(170, 255, 170);
    color[key] = random;
  }

  // press backslash for
  // onscreen keyboard
  if (key == '\\') {
    keybOn = !keybOn;
    keybToggled = true;
  }

  // ` for fullscreen
  if (key == '`') {
    fulscr = !fulscr;
    fulscrToggled = true;
    ofSetFullscreen(fulscr);
  }

  // toggle hard mode for play through
  if (key == '0' && !playThrough)
    hardMode = !hardMode;

  // toggle play through
  if (key == '=') {
    // toggle off
    if (playThrough) {
      playThrough = false;
      synth -> allNotesOff(1);
      pressed.clear();
      previews.clear();
      highlight = -1;
      return;
    }

    // TODO: error message this
    if (!loadedMIDI) return;
    playThrough = true;
    songPosition = 0;

    // calculate note lengths and positions
    buildSongVector(song, songKeys, topNotes, filesMIDI[filesIndex]);

    // bad song passed
    if (!song.size()) {
      playThrough = false;
      return;
    }

    // highlights
    if (hardMode) {
      previews.clear();
      if (song.size() > 0) highlight = songKeys[0];
      if (song.size() > 1) previews.push_back(songKeys[1]);
      if (song.size() > 2) previews.push_back(songKeys[2]);
      if (song.size() > 3) previews.push_back(songKeys[3]);
      if (song.size() > 4) previews.push_back(songKeys[4]);
      if (song.size() > 5) previews.push_back(songKeys[5]);
      if (song.size() > 6) previews.push_back(songKeys[6]);
    }
  }

  // change scale [e.g. major] with [ and key [e.g. C#] with ]
  if (key == '[') mapper.setKeyIndex(keyIndex = ++keyIndex % keys.size());
  if (key == ']') mapper.setScaleIndex(scaleIndex = ++scaleIndex % scales.size());

  // change mode [keyboard layout schematic, e.g. inc by rows] with '
  if (key == '\'') mapper.setModeIndex(modeIndex = ++modeIndex % modes.size());

  // change the selected song in directory with - when not in playthrough mode
  if (key == '-' && !playThrough) filesIndex = ++filesIndex % filesMIDI.size();
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
      // do nothing if key pressed but was initially ignored
      if (pressed.count(key) && !keyPosMap.count(key)) {
        pressed.erase(key); // reset key state
        return;
      }

      // turn off all notes in the time vector for the given key
      for (int i = 0; i < song[keyPosMap[key]].size(); i += 1) {
        int note = song[keyPosMap[key]][i].note;
        synth -> noteOff(1, note);
      }

      // remove the key from map
      pressed.erase(key);
      keyPosMap.erase(keyPosMap.find(key));

      // song is over so disable play through
      if (songPosition >= song.size()) {
        synth -> allNotesOff(1);
        playThrough = false;
        pressed.clear();
        previews.clear();
        highlight = -1;
      }
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
  ofSetColor(255, 222, 23);
    ofBeginShape();
      ofVertex(0, 0);
      ofVertex(ww, 0);
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
                     string("Toggle Fullscreen With Tick (`)\n\n") +
                     string("Current Scale: ") + scales[scaleIndex] + " (])\n" +
                     string("Current Key: ") + keys[keyIndex] + " ([)\n" +
                     string("Current Mode: ") + modes[modeIndex] + " (')\n\n" +
                     string("Selected Song: ") + filesMIDI[filesIndex].substr(10, filesMIDI[filesIndex].size() - 14) +
                     string(" (-)\nPlay Through Mode: ") + (playThrough ? string("Running") : string("Stopped")) +
                     string(" (=)\nHard Mode: ") + (hardMode ? string("On") : string("Off")) + " (0)", 10, 20, 2);

  if (!hardMode) {
    string topChars = "qwertyuiop";
    string midChars = "asdfghjkl;";
    string botChars = "zxcvbnm,./";

    // print out the top chars
    for (int i = 1; i < 11; i += 1) {
      if (pressed.count(topChars[i - 1])) ofSetColor(color[topChars[i - 1]]);
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
      if (pressed.count(midChars[i - 1])) ofSetColor(color[midChars[i - 1]]);
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
      if (pressed.count(botChars[i - 1])) ofSetColor(color[botChars[i - 1]]);
      else ofSetColor(ofColor(255, 255, 255)); // default is white

      ofRectRounded(i * ww / 12 + 25, wh / 2 + keyHeight / 2 + keyHeight * .1,
        2, keyWidth, keyHeight, 10, 10, 10, 10); // position and size
      string letter(1, botChars[i - 1]); // for drawing bitmap string

      ofPushStyle();
        ofSetColor(ofColor::black);
        ofDrawBitmapString(letter, i * ww / 12 + 25 + keyWidth / 2,
          wh / 2 + keyHeight + keyHeight *.1, 2);
      ofPopStyle();
    }
  }

  else {
    ofPushStyle();
      ofTranslate(ww / 2, wh / 2);
      ofRotateZ(90); // easy view
      ofTranslate(-ww / 2, -wh / 2);

      string hardLetters = "fghj";
      // print Guitar Hero note grid
      for (int i = 4; i < 8; i += 1) {
        string letter(1, hardLetters[i - 4]);
        ofColor faded(240, 240, 240);

        ofSetColor(faded);
        if (previews.size() > 5 && hardLetters[i - 4] == previews[5]) ofSetColor(ofColor(170, 170, 255));
        ofRectRounded(i * ww / 12, wh / 2 - 7 * keyHeight / 2 - keyHeight * .3, 2,
            keyWidth, keyHeight, 10, 10, 10, 10);

        ofSetColor(faded);
        if (previews.size() > 4 && hardLetters[i - 4] == previews[4]) ofSetColor(ofColor(170, 170, 255));
        ofRectRounded(i * ww / 12, wh / 2 - 5 * keyHeight / 2 - keyHeight * .2, 2,
            keyWidth, keyHeight, 10, 10, 10, 10);

        ofSetColor(faded);
        if (previews.size() > 3 && hardLetters[i - 4] == previews[3]) ofSetColor(ofColor(170, 170, 255));
        ofRectRounded(i * ww / 12, wh / 2 - 3 * keyHeight / 2 - keyHeight * .1, 2,
            keyWidth, keyHeight, 10, 10, 10, 10);

        ofSetColor(faded);
        if (previews.size() > 2 && hardLetters[i - 4] == previews[2]) ofSetColor(ofColor(170, 170, 255));
        ofRectRounded(i * ww / 12, wh / 2 - keyHeight / 2, 2,
            keyWidth, keyHeight, 10, 10, 10, 10);

        ofSetColor(faded);
        if (previews.size() > 1 && hardLetters[i - 4] == previews[1]) ofSetColor(ofColor(170, 170, 255));
        ofRectRounded(i * ww / 12, wh / 2 + keyHeight / 2 + keyHeight * .1, 2,
            keyWidth, keyHeight, 10, 10, 10, 10);

        ofSetColor(faded);
        if (previews.size() > 0 && hardLetters[i - 4] == previews[0]) ofSetColor(ofColor(170, 170, 255));
        ofRectRounded(i * ww / 12, wh / 2 + 3 * keyHeight / 2 + keyHeight * .2, 2,
            keyWidth, keyHeight, 10, 10, 10, 10);

        ofSetColor(ofColor(255, 255, 255));
        if (highlight != -1 && hardLetters[i - 4] == highlight) ofSetColor(ofColor(125, 125, 255));
        ofRectRounded(i * ww / 12, wh / 2 + 5 * keyHeight / 2 + keyHeight * .3, 2,
            keyWidth, keyHeight, 10, 10, 10, 10);

        ofPushStyle();
          ofSetColor(ofColor::black);
          ofDrawBitmapString(letter, i * ww / 12 + keyWidth / 2, wh / 2 + keyHeight * 3.3, 2);
        ofPopStyle();
      }

    // end rotate
    ofPopStyle();
  }
}

/**
 * Function: windowResized
 * -----------------------
 * Handle window resizing.
 */
void ofApp::windowResized(int width, int height) {
  // override custom window sizing
  // ofSetWindowShape(1024, 768);
}
