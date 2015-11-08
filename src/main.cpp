/**
 * File: main.cpp
 * Author: Sanjay Kannan
 * Author: Aidan Meacham
 * ---------------------
 * Initializes OpenFrameworks
 * and runs the windowed app.
 */

#include "ofMain.h"
#include "ofApp.h"

/**
 * Function: main
 * --------------
 * Sets up OpenFrameworks
 * and runs the window thread.
 */
int main() {
  // set up the OpenGL context in window
  ofSetupOpenGL(1024, 768, OF_WINDOW);
  // TODO: log text to console

  // this kicks off the running of my app
  // can be OF_WINDOW or OF_FULLSCREEN
  // pass in width and height too:
  ofRunApp(new ofApp());
}
